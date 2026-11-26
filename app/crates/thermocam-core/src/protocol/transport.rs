//! `Transport` trait, `SerialTransport`, `MockTransport`, and `SimulatorTransport` (realistic radiometric demo).

use std::collections::VecDeque;
use std::io::{self, Read, Write};
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};

use super::codec::{FrameRaw, FrameRawMeta, HelloReq, HelloResp, PongResp, StreamCtrl};
use super::messages::{
    DeviceConfig, Diagnostics, FsEntry, FsInfoResp, FsListResp, FsReadChunk, FsStatResp,
    PerfTelemetry, RadParams,
};
use super::parser::{ParserStats, StreamParser};
use super::{
    capabilities, flags, MsgType, Packet, ProtocolError, StatusCode, PROTO_MAJOR, PROTO_MINOR,
    TEENSY_CDC_MTP_PID, TEENSY_CDC_PID, TEENSY_USB_VID,
};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SerialPortInfo {
    pub name: String,
    pub label: String,
    pub vid: Option<u16>,
    pub pid: Option<u16>,
    pub is_thermocam: bool,
    pub is_simulator: bool,
}

/// Enumerate available serial ports (real DIY-Thermocam devices first, built-in Demo Simulator last).
#[must_use]
pub fn list_ports() -> Vec<SerialPortInfo> {
    let mut thermocams = Vec::new();
    let mut others = Vec::new();
    if let Ok(ports) = serialport::available_ports() {
        let cu_names: std::collections::HashSet<String> = ports
            .iter()
            .filter_map(|p| p.port_name.strip_prefix("/dev/cu.").map(str::to_string))
            .collect();

        for p in ports {
            // On macOS, skip /dev/tty.* when the matching non-blocking /dev/cu.* callout exists.
            if let Some(suffix) = p.port_name.strip_prefix("/dev/tty.") {
                if cu_names.contains(suffix) {
                    continue;
                }
            }
            let (vid, pid, product) = match p.port_type {
                serialport::SerialPortType::UsbPort(u) => {
                    (Some(u.vid), Some(u.pid), u.product.unwrap_or_default())
                }
                _ => (None, None, String::new()),
            };
            let is_thermocam = vid == Some(TEENSY_USB_VID)
                && matches!(pid, Some(TEENSY_CDC_PID) | Some(TEENSY_CDC_MTP_PID));
            let label = if is_thermocam {
                format!("DIY-Thermocam ({})", p.port_name)
            } else if product.is_empty() {
                p.port_name.clone()
            } else {
                format!("{} ({})", p.port_name, product)
            };
            let info = SerialPortInfo {
                name: p.port_name,
                label,
                vid,
                pid,
                is_thermocam,
                is_simulator: false,
            };
            if is_thermocam {
                thermocams.push(info);
            } else {
                others.push(info);
            }
        }
    }

    let mut out = Vec::with_capacity(thermocams.len() + others.len() + 1);
    out.extend(thermocams);
    out.push(SerialPortInfo {
        name: "sim://thermocam-v3".to_string(),
        label: "Simulator (Lepton 3.5)".to_string(),
        vid: Some(TEENSY_USB_VID),
        pid: Some(TEENSY_CDC_PID),
        is_thermocam: false,
        is_simulator: true,
    });
    out.extend(others);
    out
}

/// Abstract byte transport for Protocol v2 sessions.
pub trait Transport: Send {
    fn write_all(&mut self, data: &[u8]) -> io::Result<()>;
    fn read_chunk(&mut self, buf: &mut [u8], timeout: Duration) -> io::Result<usize>;
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

/// Convert a `serialport` error to `io::Error`, keeping its `io::ErrorKind` and description.
#[must_use]
pub fn serialport_error_to_io(e: serialport::Error) -> io::Error {
    let kind = match e.kind() {
        serialport::ErrorKind::Io(k) => k,
        serialport::ErrorKind::InvalidInput => io::ErrorKind::InvalidInput,
        serialport::ErrorKind::NoDevice | serialport::ErrorKind::Unknown => io::ErrorKind::Other,
    };
    io::Error::new(kind, e.description)
}

/// Real USB CDC ACM serial transport using `serialport`.
pub struct SerialTransport {
    port: Box<dyn serialport::SerialPort>,
    last_timeout: Duration,
}

impl SerialTransport {
    pub fn open(port_name: &str) -> Result<Self, ProtocolError> {
        let initial_timeout = Duration::from_millis(10);
        let mut port = serialport::new(port_name, 115_200)
            .timeout(initial_timeout)
            .open()
            .map_err(|e| ProtocolError::Io(serialport_error_to_io(e)))?;
        let _ = port.write_data_terminal_ready(true);
        let _ = port.clear(serialport::ClearBuffer::All);
        Ok(Self {
            port,
            last_timeout: initial_timeout,
        })
    }
}

impl Transport for SerialTransport {
    fn write_all(&mut self, data: &[u8]) -> io::Result<()> {
        Write::write_all(&mut *self.port, data)
    }
    fn read_chunk(&mut self, buf: &mut [u8], timeout: Duration) -> io::Result<usize> {
        let effective = timeout.max(Duration::from_millis(1));
        if effective != self.last_timeout {
            let _ = self.port.set_timeout(effective);
            self.last_timeout = effective;
        }
        match Read::read(&mut *self.port, buf) {
            Ok(0) if !buf.is_empty() => Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "serial port disconnected (EOF)",
            )),
            Ok(n) => Ok(n),
            Err(e) if e.kind() == io::ErrorKind::TimedOut => Ok(0),
            Err(e) => Err(e),
        }
    }
    fn flush(&mut self) -> io::Result<()> {
        Write::flush(&mut *self.port)
    }
}

/// Deterministic in-memory mock transport for unit testing Protocol v2 flows.
#[derive(Default)]
pub struct MockTransport {
    pub rx_queue: VecDeque<u8>,
    pub tx_log: Vec<u8>,
    /// Once the queued bytes are read, every read fails with this (an unplugged cable).
    pub read_error_when_empty: Option<io::ErrorKind>,
}

impl MockTransport {
    pub fn push_rx(&mut self, bytes: &[u8]) {
        self.rx_queue.extend(bytes);
    }
}

impl Transport for MockTransport {
    fn write_all(&mut self, data: &[u8]) -> io::Result<()> {
        self.tx_log.extend_from_slice(data);
        Ok(())
    }
    fn read_chunk(&mut self, buf: &mut [u8], _timeout: Duration) -> io::Result<usize> {
        if self.rx_queue.is_empty() {
            if let Some(kind) = self.read_error_when_empty {
                return Err(io::Error::new(kind, "simulated unplug"));
            }
        }
        let n = buf.len().min(self.rx_queue.len());
        for slot in buf.iter_mut().take(n) {
            *slot = self.rx_queue.pop_front().unwrap_or(0);
        }
        Ok(n)
    }
}

/// Largest `FS_READ` length one request asks for. Every request is bounded, so the client lock
/// is held for one short round trip and no `FS_READ_DATA` arrives after it is released.
pub const MAX_READ_CHUNK: u32 = 32_768;

/// Name of the camera-style video folder seeded on the demo camera's card.
pub const DEMO_VIDEO_FOLDER: &str = "20261126101500";
/// Number of `NNNNNN.DAT` frames in [`DEMO_VIDEO_FOLDER`].
pub const DEMO_VIDEO_FRAMES: u32 = 60;
const DEMO_VIDEO_FIRST_SEQ: u32 = 1_000;

/// Frame `index` of the demo camera's seeded video folder.
#[must_use]
pub fn demo_video_frame(index: u32) -> FrameRaw {
    generate_demo_frame(DEMO_VIDEO_FIRST_SEQ + index, 160, 120)
}

/// Contents of a simulated card file.
enum SdData {
    Bytes(Vec<u8>),
    /// A demo video frame, rendered as a `.DAT` when read (keeps the simulator light).
    DemoVideoFrame(u32),
}

impl SdData {
    fn len(&self) -> usize {
        match self {
            Self::Bytes(b) => b.len(),
            Self::DemoVideoFrame(_) => crate::dat::LEPTON3_BIG_SIZE,
        }
    }

    fn bytes(&self) -> std::borrow::Cow<'_, [u8]> {
        match self {
            Self::Bytes(b) => std::borrow::Cow::Borrowed(b),
            Self::DemoVideoFrame(i) => std::borrow::Cow::Owned(
                crate::dat::ThermalDat::from_frame(&demo_video_frame(*i), 10, 0).to_bytes(),
            ),
        }
    }
}

fn sd_norm(path: &str) -> &str {
    path.trim_matches('/')
}

/// Realistic offline DIY-Thermocam V3 Protocol v2 simulator generating animated radiometric scenes.
pub struct SimulatorTransport {
    parser: StreamParser,
    out_buf: VecDeque<u8>,
    config: DeviceConfig,
    rad_params: RadParams,
    streaming: bool,
    first_frame_pending: bool,
    last_frame_at: Instant,
    frame_seq: u32,
    start: Instant,
    /// Card files by full path without the leading `/` (`DIR/NAME.DAT`).
    sd_files: Vec<(String, SdData)>,
    /// Folders that exist even when empty.
    sd_dirs: Vec<String>,
}

impl Default for SimulatorTransport {
    fn default() -> Self {
        Self::new()
    }
}

impl SimulatorTransport {
    #[must_use]
    pub fn new() -> Self {
        let f0 = generate_demo_frame(0, 160, 120);
        let f1 = generate_demo_frame(12, 160, 120);
        let f2 = generate_demo_frame(28, 160, 120);
        let d0 = crate::dat::ThermalDat::from_frame(&f0, 10, 0).to_bytes();
        let d1 = crate::dat::ThermalDat::from_frame(&f1, 13, 0).to_bytes();
        let d2 = crate::dat::ThermalDat::from_frame(&f2, 11, 0).to_bytes();
        let mut sd_files = vec![
            ("20261125101500.DAT".to_string(), SdData::Bytes(d0)),
            ("20261125102030.DAT".to_string(), SdData::Bytes(d1)),
            ("20261125102845.DAT".to_string(), SdData::Bytes(d2)),
        ];
        for i in 0..DEMO_VIDEO_FRAMES {
            sd_files.push((
                format!("{DEMO_VIDEO_FOLDER}/{i:06}.DAT"),
                SdData::DemoVideoFrame(i),
            ));
        }
        Self {
            parser: StreamParser::new(),
            out_buf: VecDeque::new(),
            config: DeviceConfig::default(),
            rad_params: RadParams::default(),
            streaming: false,
            first_frame_pending: false,
            last_frame_at: Instant::now(),
            frame_seq: 0,
            start: Instant::now(),
            sd_files,
            sd_dirs: Vec::new(),
        }
    }

    /// Add an extra virtual SD file; `name` may include folders (`DIR/000000.DAT`).
    pub fn add_sd_file(&mut self, name: String, data: Vec<u8>) {
        let name = sd_norm(&name).to_string();
        self.sd_files.push((name, SdData::Bytes(data)));
    }

    /// Add a (possibly empty) virtual SD folder; `name` may be nested (`A/B`).
    pub fn add_sd_dir(&mut self, name: String) {
        let name = sd_norm(&name).to_string();
        if !name.is_empty() && !self.sd_dirs.contains(&name) {
            self.sd_dirs.push(name);
        }
    }

    fn sd_file(&self, path: &str) -> Option<&SdData> {
        let name = sd_norm(path);
        self.sd_files
            .iter()
            .find(|(n, _)| n == name)
            .map(|(_, d)| d)
    }

    fn sd_is_dir(&self, path: &str) -> bool {
        let name = sd_norm(path);
        if name.is_empty() {
            return true;
        }
        let prefix = format!("{name}/");
        self.sd_dirs
            .iter()
            .any(|d| d == name || d.starts_with(&prefix))
            || self.sd_files.iter().any(|(n, _)| n.starts_with(&prefix))
    }

    /// Direct children of `dir`: files first, then folders, like a FAT directory walk.
    fn sd_list(&self, dir: &str) -> Vec<FsEntry> {
        let dir = sd_norm(dir);
        let prefix = if dir.is_empty() {
            String::new()
        } else {
            format!("{dir}/")
        };
        let mut files = Vec::new();
        let mut dirs: Vec<String> = Vec::new();
        let mut note_dir = |rest: &str| {
            let first = rest.split('/').next().unwrap_or(rest).to_string();
            if !first.is_empty() && !dirs.contains(&first) {
                dirs.push(first);
            }
        };
        for (name, data) in &self.sd_files {
            if let Some(rest) = name.strip_prefix(&prefix) {
                if rest.contains('/') {
                    note_dir(rest);
                } else {
                    files.push(FsEntry {
                        name: rest.to_string(),
                        size: data.len() as u32,
                        mtime_epoch: 1_764_065_700,
                        is_dir: false,
                    });
                }
            }
        }
        for d in &self.sd_dirs {
            if let Some(rest) = d.strip_prefix(&prefix) {
                note_dir(rest);
            }
        }
        files.extend(dirs.into_iter().map(|name| FsEntry {
            name,
            size: 0,
            mtime_epoch: 1_764_065_700,
            is_dir: true,
        }));
        files
    }

    pub fn push_next_frame(&mut self) {
        self.frame_seq = self.frame_seq.wrapping_add(1);
        // Real sample captures in turn (q7); `generate_demo_frame` stays for tests.
        let frame = crate::demo_scenes::demo_scene_frame(self.frame_seq);
        let payload = frame.to_bytes();
        if let Ok(pkt) = Packet::encode(MsgType::FrameRaw, 0, self.frame_seq as u16, 0, &payload) {
            self.out_buf.extend(pkt);
        }
    }

    fn handle_packet(&mut self, pkt: Packet) {
        let seq = pkt.header.seq;
        let Ok(mt) = pkt.msg_type() else {
            if let Ok(b) = super::messages::encode_generic_error_resp(
                seq,
                StatusCode::ErrUnknownMsg as u16,
                "unknown",
            ) {
                self.out_buf.extend(b);
            }
            return;
        };
        let reply: Option<(MsgType, u8, u16, Vec<u8>)> = match mt {
            MsgType::HelloReq => {
                let resp = HelloResp {
                    proto_major: PROTO_MAJOR,
                    proto_minor: PROTO_MINOR,
                    fw_version: 400,
                    fw_build_str: "4.0.0-sim".to_string(),
                    hw_version: 3,
                    lepton_type: 1,
                    sensor_w: 160,
                    sensor_h: 120,
                    display_w: 320,
                    display_h: 240,
                    diagnostic_mask: 0xFF,
                    teensy_uid: 0x0011_2233_4455_6677,
                    capabilities: capabilities::RAW_STREAM
                        | capabilities::SD_FS
                        | capabilities::PERF_TELEMETRY
                        | capabilities::BOOTLOADER
                        | capabilities::LEPTON_RAD_PARAMS
                        | capabilities::GAIN_MODE
                        | capabilities::HOT_COLD
                        | capabilities::RTC_SYNC
                        | capabilities::KEEP_LCD_ON,
                    max_chunk_size: 8192,
                    heartbeat_timeout_ms: 5000,
                };
                Some((MsgType::HelloResp, 0, 0, resp.to_bytes().to_vec()))
            }
            MsgType::GoodbyeReq => {
                self.streaming = false;
                Some((MsgType::GoodbyeResp, 0, 0, Vec::new()))
            }
            MsgType::PingReq => {
                let host_us = if pkt.payload.len() >= 8 {
                    u64::from_le_bytes(pkt.payload[0..8].try_into().unwrap_or([0; 8]))
                } else {
                    0
                };
                let pong = PongResp {
                    host_time_us: host_us,
                    device_uptime_us: self.start.elapsed().as_micros() as u64,
                };
                Some((MsgType::PongResp, 0, 0, pong.to_bytes().to_vec()))
            }
            MsgType::StreamCtrlReq => {
                if let Ok(sc) = StreamCtrl::from_bytes(&pkt.payload) {
                    self.streaming = sc.mode != 0;
                    if self.streaming {
                        self.first_frame_pending = true;
                        self.last_frame_at = Instant::now();
                    }
                    Some((MsgType::StreamCtrlResp, 0, 0, sc.to_bytes().to_vec()))
                } else {
                    Some((
                        MsgType::StreamCtrlResp,
                        flags::ERROR,
                        StatusCode::ErrInvalidArg as u16,
                        Vec::new(),
                    ))
                }
            }
            MsgType::GetConfigReq => Some((
                MsgType::GetConfigResp,
                0,
                0,
                self.config.to_bytes().to_vec(),
            )),
            MsgType::SetConfigReq if pkt.payload.len() >= 36 => {
                if let Ok(cfg) = DeviceConfig::from_bytes(&pkt.payload[4..36]) {
                    self.config = cfg;
                    Some((
                        MsgType::SetConfigResp,
                        0,
                        0,
                        self.config.to_bytes().to_vec(),
                    ))
                } else {
                    Some((
                        MsgType::SetConfigResp,
                        flags::ERROR,
                        StatusCode::ErrInvalidArg as u16,
                        Vec::new(),
                    ))
                }
            }
            MsgType::GetRadParamsReq => Some((
                MsgType::GetRadParamsResp,
                0,
                0,
                self.rad_params.to_bytes().to_vec(),
            )),
            MsgType::SetRadParamsReq => {
                if let Ok(rp) = RadParams::from_bytes(&pkt.payload) {
                    self.rad_params = rp;
                    Some((
                        MsgType::SetRadParamsResp,
                        0,
                        0,
                        self.rad_params.to_bytes().to_vec(),
                    ))
                } else {
                    Some((
                        MsgType::SetRadParamsResp,
                        flags::ERROR,
                        StatusCode::ErrInvalidArg as u16,
                        Vec::new(),
                    ))
                }
            }
            MsgType::TriggerFfcReq => Some((MsgType::TriggerFfcResp, 0, 0, vec![0, 0])),
            MsgType::GetDiagnosticsReq => {
                let d = Diagnostics {
                    diagnostic_mask: 0xFF,
                    bat_pct: 87,
                    usb_power: true,
                    sd_fs_type: 3,
                    bat_mv: 4120,
                    cpu_mhz: 450,
                    mcu_temp_c: 41.2,
                    sd_total_kb: 31_268_864,
                    sd_free_kb: 28_412_000,
                    uptime_us: self.start.elapsed().as_micros() as u64,
                    free_dtcm_bytes: 286_720,
                    free_ocram_bytes: 342_016,
                    lepton_fpa_temp_c: 31.4,
                    lepton_part_num: "500-0771-01-SIM".to_string(),
                };
                Some((MsgType::GetDiagnosticsResp, 0, 0, d.to_bytes().to_vec()))
            }
            MsgType::GetPerfReq => {
                let p = PerfTelemetry {
                    acquire_us: 840,
                    stats_us: 210,
                    filter_us: 390,
                    scale_us: 620,
                    colorize_us: 410,
                    overlay_us: 280,
                    display_us: 3100,
                    sd_write_us: 24_500,
                    usb_tx_us: 2850,
                    loop_total_us: 8700,
                    frame_count: self.frame_seq,
                    frames_dropped: 0,
                    vospi_resyncs: 0,
                    vospi_crc_errors: 0,
                    usb_rx_errors: 0,
                    usb_crc_errors: 0,
                    live_fps_x100: 878,
                    video_fps_x100: 874,
                    stream_fps_x100: if self.streaming { 878 } else { 0 },
                    perf_mode: if self.streaming { 2 } else { 3 },
                    flags: 0x07,
                    display_spi_mhz: 40,
                    lepton_spi_mhz: 20,
                    cpu_mhz: 450,
                    mcu_temp_c: 41.2,
                    frame_period_us: 113_895,
                };
                Some((MsgType::GetPerfResp, 0, 0, p.to_bytes().to_vec()))
            }
            MsgType::FsInfoReq => {
                let info = FsInfoResp {
                    card_present: true,
                    fs_type: 3,
                    total_kb: 31_268_864,
                    free_kb: 28_412_000,
                    root_entry_count: self.sd_list("").len() as u32,
                };
                Some((MsgType::FsInfoResp, 0, 0, info.to_bytes().to_vec()))
            }
            MsgType::FsListReq => {
                let cursor = if pkt.payload.len() >= 4 {
                    u32::from_le_bytes([
                        pkt.payload[0],
                        pkt.payload[1],
                        pkt.payload[2],
                        pkt.payload[3],
                    ]) as usize
                } else {
                    0
                };
                let max_entries = if pkt.payload.len() >= 6 {
                    usize::from(u16::from_le_bytes([pkt.payload[4], pkt.payload[5]])).clamp(1, 64)
                } else {
                    64
                };
                let path_len = if pkt.payload.len() >= 8 {
                    usize::from(u16::from_le_bytes([pkt.payload[6], pkt.payload[7]]))
                } else {
                    0
                };
                let dir = String::from_utf8_lossy(
                    &pkt.payload[8.min(pkt.payload.len())..(8 + path_len).min(pkt.payload.len())],
                )
                .into_owned();
                if !self.sd_is_dir(&dir) {
                    Some((
                        MsgType::FsListResp,
                        flags::ERROR,
                        StatusCode::ErrNotFound as u16,
                        Vec::new(),
                    ))
                } else {
                    let all = self.sd_list(&dir);
                    let start = cursor.min(all.len());
                    let end = (start + max_entries).min(all.len());
                    let next_cursor = if end >= all.len() {
                        u32::MAX
                    } else {
                        end as u32
                    };
                    let list = FsListResp {
                        next_cursor,
                        entries: all[start..end].to_vec(),
                    };
                    Some((MsgType::FsListResp, 0, 0, list.to_bytes()))
                }
            }
            MsgType::FsStatReq if pkt.payload.len() >= 4 => {
                let compute_crc32 = (pkt.payload[0] & 0x01) != 0;
                let path_len = u16::from_le_bytes([pkt.payload[2], pkt.payload[3]]) as usize;
                let path =
                    String::from_utf8_lossy(&pkt.payload[4..(4 + path_len).min(pkt.payload.len())]);
                if let Some(data) = self.sd_file(&path) {
                    let stat = FsStatResp {
                        size: data.len() as u32,
                        mtime_epoch: 1_764_065_700,
                        crc32: if compute_crc32 {
                            super::crc::crc32_ieee(&data.bytes())
                        } else {
                            0
                        },
                        is_dir: false,
                        crc32_valid: compute_crc32,
                    };
                    Some((MsgType::FsStatResp, 0, 0, stat.to_bytes().to_vec()))
                } else if self.sd_is_dir(&path) {
                    let stat = FsStatResp {
                        size: 0,
                        mtime_epoch: 1_764_065_700,
                        crc32: 0,
                        is_dir: true,
                        crc32_valid: false,
                    };
                    Some((MsgType::FsStatResp, 0, 0, stat.to_bytes().to_vec()))
                } else {
                    Some((
                        MsgType::FsStatResp,
                        flags::ERROR,
                        StatusCode::ErrNotFound as u16,
                        Vec::new(),
                    ))
                }
            }
            MsgType::FsReadReq if pkt.payload.len() >= 12 => {
                let req_offset = u32::from_le_bytes([
                    pkt.payload[0],
                    pkt.payload[1],
                    pkt.payload[2],
                    pkt.payload[3],
                ]) as usize;
                let req_len = u32::from_le_bytes([
                    pkt.payload[4],
                    pkt.payload[5],
                    pkt.payload[6],
                    pkt.payload[7],
                ]) as usize;
                let req_chunk = u16::from_le_bytes([pkt.payload[8], pkt.payload[9]]) as usize;
                let chunk_sz = if req_chunk == 0 {
                    8192
                } else {
                    req_chunk.clamp(256, 8192)
                };
                let path_len = u16::from_le_bytes([pkt.payload[10], pkt.payload[11]]) as usize;
                let path = String::from_utf8_lossy(
                    &pkt.payload[12..(12 + path_len).min(pkt.payload.len())],
                );
                let data = self.sd_file(&path).map(SdData::bytes);
                match data {
                    Some(data) if req_offset > data.len() => Some((
                        MsgType::FsReadData,
                        flags::ERROR,
                        StatusCode::ErrInvalidArg as u16,
                        Vec::new(),
                    )),
                    Some(data) => {
                        // Firmware semantics: `eof` marks the end of the file, a clear
                        // MORE_CHUNKS flag marks the end of this request's range.
                        let start = req_offset;
                        let end = if req_len == 0 {
                            data.len()
                        } else {
                            start.saturating_add(req_len).min(data.len())
                        };
                        let mut encoded = Vec::new();
                        let mut pos = start;
                        loop {
                            let take = (end - pos).min(chunk_sz);
                            let done = pos + take >= end;
                            let eof = pos + take >= data.len();
                            let mut p = Vec::with_capacity(8 + take);
                            p.extend_from_slice(&(pos as u32).to_le_bytes());
                            p.extend_from_slice(&(take as u16).to_le_bytes());
                            p.push(u8::from(eof));
                            p.push(0);
                            p.extend_from_slice(&data[pos..pos + take]);
                            let pkt_flags = if done { 0 } else { flags::MORE_CHUNKS };
                            if let Ok(bytes) =
                                Packet::encode(MsgType::FsReadData, pkt_flags, seq, 0, &p)
                            {
                                encoded.extend(bytes);
                            }
                            pos += take;
                            if done {
                                break;
                            }
                        }
                        self.out_buf.extend(encoded);
                        None
                    }
                    None => Some((
                        MsgType::FsReadData,
                        flags::ERROR,
                        StatusCode::ErrNotFound as u16,
                        Vec::new(),
                    )),
                }
            }
            MsgType::FsDeleteReq if pkt.payload.len() >= 4 => {
                let path_len = u16::from_le_bytes([pkt.payload[2], pkt.payload[3]]) as usize;
                let path =
                    String::from_utf8_lossy(&pkt.payload[4..(4 + path_len).min(pkt.payload.len())]);
                let name = sd_norm(&path).to_string();
                let prefix = format!("{name}/");
                let before = self.sd_files.len() + self.sd_dirs.len();
                // Like the firmware, deleting a folder removes everything inside it.
                self.sd_files
                    .retain(|(n, _)| *n != name && !n.starts_with(&prefix));
                self.sd_dirs
                    .retain(|d| *d != name && !d.starts_with(&prefix));
                if !name.is_empty() && self.sd_files.len() + self.sd_dirs.len() < before {
                    Some((MsgType::FsDeleteResp, 0, 0, Vec::new()))
                } else {
                    Some((
                        MsgType::FsDeleteResp,
                        flags::ERROR,
                        StatusCode::ErrNotFound as u16,
                        Vec::new(),
                    ))
                }
            }
            MsgType::EnterBootloaderReq => Some((MsgType::EnterBootloaderResp, 0, 0, Vec::new())),
            _ => None,
        };
        if let Some((rtype, rflags, rstatus, rpayload)) = reply {
            if let Ok(bytes) = Packet::encode(rtype, rflags, seq, rstatus, &rpayload) {
                self.out_buf.extend(bytes);
            }
        }
    }
}

impl Transport for SimulatorTransport {
    fn write_all(&mut self, data: &[u8]) -> io::Result<()> {
        for pkt in self.parser.feed(data).into_iter().flatten() {
            self.handle_packet(pkt);
        }
        Ok(())
    }

    fn read_chunk(&mut self, buf: &mut [u8], timeout: Duration) -> io::Result<usize> {
        if self.out_buf.is_empty() && self.streaming {
            const FRAME_PERIOD: Duration = Duration::from_millis(114);
            if self.first_frame_pending || self.last_frame_at.elapsed() >= FRAME_PERIOD {
                self.first_frame_pending = false;
                self.last_frame_at = Instant::now();
                self.push_next_frame();
            } else if !timeout.is_zero() {
                let rem = FRAME_PERIOD.saturating_sub(self.last_frame_at.elapsed());
                std::thread::sleep(timeout.min(rem));
                if self.last_frame_at.elapsed() >= FRAME_PERIOD {
                    self.last_frame_at = Instant::now();
                    self.push_next_frame();
                }
            }
        }
        let n = buf.len().min(self.out_buf.len());
        for slot in buf.iter_mut().take(n) {
            *slot = self.out_buf.pop_front().unwrap_or(0);
        }
        Ok(n)
    }
}

/// Generate a realistic synthetic radiometric thermal scene (PCB with warm MCU, power inductor, VRM, traces, cool aluminum heatsink).
#[must_use]
pub fn generate_demo_frame(frame_seq: u32, width: u16, height: u16) -> FrameRaw {
    let w = usize::from(width);
    let h = usize::from(height);
    let phase = (frame_seq as f32) * 0.14;
    let mut pixels = vec![0u16; w * h];
    let mut min_raw = u16::MAX;
    let mut max_raw = 0u16;
    let mut min_xy = (0u16, 0u16);
    let mut max_xy = (0u16, 0u16);

    for y in 0..h {
        let ny = y as f32 / h as f32;
        for x in 0..w {
            let nx = x as f32 / w as f32;
            // Ambient PCB substrate ~23.8 C with subtle vertical gradient
            let mut temp_c = 23.4 + 1.4 * (1.0 - ny) + 0.4 * (nx * std::f32::consts::TAU).sin();

            // Hot SoC / MCU package near center-left (~47.5 C, gently breathing)
            let dx1 = (nx - 0.38) * 1.33;
            let dy1 = ny - 0.48;
            let r1 = dx1 * dx1 + dy1 * dy1;
            temp_c += (22.6 + 1.2 * phase.sin()) * (-r1 / 0.011).exp();

            // Switching regulator MOSFET hotspot (~41.5 C)
            let dx2 = (nx - 0.68) * 1.33;
            let dy2 = ny - 0.34;
            let r2 = dx2 * dx2 + dy2 * dy2;
            temp_c += (17.2 + 0.9 * (phase * 1.3).cos()) * (-r2 / 0.0055).exp();

            // Warm copper trace connecting regulator to MCU
            let trace_dist =
                ((ny - (0.34 + 0.14 * ((0.68 - nx) / 0.30).clamp(0.0, 1.0))) * 12.0).abs();
            if (0.38..=0.68).contains(&nx) && trace_dist < 0.7 {
                temp_c += 6.8 * (1.0 - trace_dist / 0.7);
            }

            // Secondary linear LDO lower-right (~36 C)
            let dx3 = (nx - 0.72) * 1.33;
            let dy3 = ny - 0.72;
            temp_c += 12.2 * (-(dx3 * dx3 + dy3 * dy3) / 0.007).exp();

            // Cool aluminum heatsink fin block on left (~21.0 C)
            if (0.08..=0.21).contains(&nx) && (0.20..=0.80).contains(&ny) {
                let fin = ((ny * 40.0).sin() * 0.5 + 0.5) * 1.0;
                temp_c -= 3.2 - fin;
            }

            // Subtle sensor NETD noise (~50 mK deterministic hash)
            let hash = ((x as u32).wrapping_mul(73_856_093)
                ^ (y as u32).wrapping_mul(19_349_663)
                ^ frame_seq.wrapping_mul(83_492_791))
                & 0xFF;
            temp_c += ((hash as f32 / 255.0) - 0.5) * 0.14;

            let raw = ((temp_c + 273.15) / 0.01).round().clamp(1000.0, 65000.0) as u16;
            pixels[y * w + x] = raw;
            if raw < min_raw {
                min_raw = raw;
                min_xy = (x as u16, y as u16);
            }
            if raw > max_raw {
                max_raw = raw;
                max_xy = (x as u16, y as u16);
            }
        }
    }

    let spot_raw = pixels[(h / 2) * w + (w / 2)];
    let spot_temp_c = f32::from(spot_raw) * 0.01 - 273.15;

    FrameRaw {
        meta: FrameRawMeta {
            frame_seq,
            timestamp_us: u64::from(frame_seq) * 113_895,
            rtc_epoch: 1_764_066_000 + frame_seq / 9,
            dropped_frames: 0,
            width,
            height,
            pixel_format: 0,
            gain_mode: 0,
            ffc_state: 0,
            flags: 0x02,
            min_raw,
            max_raw,
            min_x: min_xy.0,
            min_y: min_xy.1,
            max_x: max_xy.0,
            max_y: max_xy.1,
            spot_temp_c,
            cal_offset: -273.15,
            cal_slope: 0.01,
            fpa_temp_c: 31.4,
            bat_pct: 87,
            usb_power: true,
            temp_point_count: 0,
        },
        pixels,
        temp_points: Vec::new(),
    }
}

/// High-level synchronous Protocol v2 client operating over any `Transport`.
pub struct ProtocolClient<T: Transport> {
    transport: T,
    parser: StreamParser,
    next_seq: u16,
    pending: VecDeque<Packet>,
    rx_scratch: Vec<u8>,
}

impl<T: Transport> ProtocolClient<T> {
    pub fn new(transport: T) -> Self {
        Self {
            transport,
            parser: StreamParser::new(),
            next_seq: 1,
            pending: VecDeque::new(),
            rx_scratch: vec![0u8; 65_536],
        }
    }

    pub fn into_inner(self) -> T {
        self.transport
    }

    fn alloc_seq(&mut self) -> u16 {
        let s = self.next_seq;
        self.next_seq = self.next_seq.wrapping_add(1).max(1);
        s
    }

    fn is_unsolicited_msg(msg_type: u8) -> bool {
        (0x80..=0xDF).contains(&msg_type)
    }

    fn recv_response_for_seq(
        &mut self,
        seq: u16,
        timeout: Duration,
    ) -> Result<Packet, ProtocolError> {
        let deadline = Instant::now() + timeout;
        loop {
            if let Some(pos) = self
                .pending
                .iter()
                .position(|p| !Self::is_unsolicited_msg(p.header.msg_type) && p.header.seq == seq)
            {
                let pkt = self.pending.remove(pos).expect("valid idx");
                if pkt.is_error() {
                    return Err(ProtocolError::DeviceError {
                        status: pkt.header.status_or_sub,
                        message: String::from_utf8_lossy(&pkt.payload).into_owned(),
                    });
                }
                return Ok(pkt);
            }
            let now = Instant::now();
            if now >= deadline {
                return Err(ProtocolError::Timeout(seq));
            }
            let wait = deadline
                .saturating_duration_since(now)
                .min(Duration::from_millis(10));
            let n = self.transport.read_chunk(&mut self.rx_scratch, wait)?;
            if n > 0 {
                for pkt in self
                    .parser
                    .feed(&self.rx_scratch[..n])
                    .into_iter()
                    .flatten()
                {
                    self.pending.push_back(pkt);
                }
            }
        }
    }

    pub fn request(
        &mut self,
        msg_type: MsgType,
        payload: &[u8],
        timeout: Duration,
    ) -> Result<Packet, ProtocolError> {
        let seq = self.alloc_seq();
        let bytes = Packet::encode(msg_type, 0, seq, 0, payload)?;
        self.transport.write_all(&bytes)?;
        self.transport.flush()?;
        self.recv_response_for_seq(seq, timeout)
    }

    /// Send a non-blocking `PING_REQ` heartbeat without stalling the stream/command mutex.
    pub fn send_ping_nowait(&mut self, host_time_us: u64) -> Result<(), ProtocolError> {
        let seq = self.alloc_seq();
        let bytes = Packet::encode(MsgType::PingReq, 0, seq, 0, &host_time_us.to_le_bytes())?;
        self.transport.write_all(&bytes)?;
        self.transport.flush()?;
        Ok(())
    }

    /// One `FS_LIST` page of up to `max_entries` entries starting at `cursor`.
    pub fn list_dir_page(
        &mut self,
        path: &str,
        cursor: u32,
        max_entries: u16,
    ) -> Result<FsListResp, ProtocolError> {
        let req = FsListResp::encode_req(cursor, max_entries, path);
        let pkt = self.request(MsgType::FsListReq, &req, Duration::from_millis(3000))?;
        FsListResp::from_bytes(&pkt.payload)
    }

    /// Paginate `FS_LIST` all the way until `next_cursor == 0xFFFFFFFF` (no 64-file cap).
    pub fn list_dir_all(&mut self, path: &str) -> Result<Vec<FsEntry>, ProtocolError> {
        const PAGE_SIZE: u16 = 64;
        const MAX_PAGES: usize = 256;
        let mut cursor: u32 = 0;
        let mut all = Vec::new();
        for _ in 0..MAX_PAGES {
            let page = self.list_dir_page(path, cursor, PAGE_SIZE)?;
            let got_any = !page.entries.is_empty();
            all.extend(page.entries);
            if page.next_cursor == u32::MAX || page.next_cursor == cursor || !got_any {
                break;
            }
            cursor = page.next_cursor;
        }
        Ok(all)
    }

    #[must_use]
    pub fn parser_stats(&self) -> ParserStats {
        self.parser.stats()
    }

    pub fn hello(&mut self) -> Result<HelloResp, ProtocolError> {
        let pkt = self.request(
            MsgType::HelloReq,
            &HelloReq::default().to_bytes(),
            Duration::from_millis(1000),
        )?;
        HelloResp::from_bytes(&pkt.payload)
    }

    pub fn ping(&mut self, host_time_us: u64) -> Result<PongResp, ProtocolError> {
        let pkt = self.request(
            MsgType::PingReq,
            &host_time_us.to_le_bytes(),
            Duration::from_millis(1000),
        )?;
        PongResp::from_bytes(&pkt.payload)
    }

    pub fn goodbye(&mut self) -> Result<(), ProtocolError> {
        let _ = self.request(MsgType::GoodbyeReq, &[], Duration::from_millis(1000))?;
        Ok(())
    }

    pub fn fs_stat(
        &mut self,
        path: &str,
        compute_crc32: bool,
    ) -> Result<FsStatResp, ProtocolError> {
        let req = FsStatResp::encode_req(compute_crc32, path);
        let pkt = self.request(MsgType::FsStatReq, &req, Duration::from_millis(3000))?;
        FsStatResp::from_bytes(&pkt.payload)
    }

    pub fn set_stream(&mut self, ctrl: StreamCtrl) -> Result<StreamCtrl, ProtocolError> {
        let pkt = self.request(
            MsgType::StreamCtrlReq,
            &ctrl.to_bytes(),
            Duration::from_millis(1000),
        )?;
        StreamCtrl::from_bytes(&pkt.payload)
    }

    pub fn poll_packets(&mut self, timeout: Duration) -> Result<Vec<Packet>, ProtocolError> {
        let mut out: Vec<Packet> = self.pending.drain(..).collect();
        if !out.is_empty() {
            return Ok(out);
        }
        let deadline = Instant::now() + timeout;
        loop {
            let rem = deadline.saturating_duration_since(Instant::now());
            let wait = rem.min(Duration::from_millis(10));
            let n = self.transport.read_chunk(&mut self.rx_scratch, wait)?;
            if n > 0 {
                for pkt in self
                    .parser
                    .feed(&self.rx_scratch[..n])
                    .into_iter()
                    .flatten()
                {
                    out.push(pkt);
                }
            }
            if !out.is_empty() || Instant::now() >= deadline {
                break;
            }
        }
        Ok(out)
    }

    /// Read `len` bytes (clamped to `1..=MAX_READ_CHUNK`, never "to the end") at `offset`.
    /// Returns the data and whether it reached the end of the file. One bounded round trip.
    pub fn read_file_chunk(
        &mut self,
        path: &str,
        offset: u32,
        len: u32,
    ) -> Result<(Vec<u8>, bool), ProtocolError> {
        let len = len.clamp(1, MAX_READ_CHUNK);
        let seq = self.alloc_seq();
        let req = FsReadChunk::encode_req(offset, len, 8192, path);
        let bytes = Packet::encode(MsgType::FsReadReq, 0, seq, 0, &req)?;
        self.transport.write_all(&bytes)?;
        self.transport.flush()?;

        let mut out = Vec::with_capacity(len as usize);
        let mut eof = false;
        loop {
            let pkt = self.recv_response_for_seq(seq, Duration::from_millis(3000))?;
            let more = (pkt.header.flags & flags::MORE_CHUNKS) != 0;
            let chunk = FsReadChunk::from_bytes(&pkt.payload)?;
            out.extend_from_slice(&chunk.data);
            eof |= chunk.eof;
            if !more {
                break;
            }
        }
        Ok((out, eof))
    }

    /// Read a whole file as a sequence of bounded chunk requests.
    pub fn read_file_bytes(&mut self, path: &str) -> Result<Vec<u8>, ProtocolError> {
        let mut out = Vec::new();
        loop {
            let (chunk, eof) = self.read_file_chunk(path, out.len() as u32, MAX_READ_CHUNK)?;
            let empty = chunk.is_empty();
            out.extend_from_slice(&chunk);
            if eof || empty {
                break;
            }
        }
        Ok(out)
    }

    pub fn delete_file(&mut self, path: &str) -> Result<(), ProtocolError> {
        let pb = path.as_bytes();
        let mut req = Vec::with_capacity(4 + pb.len());
        req.push(0); // flags
        req.push(0); // reserved
        req.extend_from_slice(&(pb.len() as u16).to_le_bytes());
        req.extend_from_slice(pb);
        let _ = self.request(MsgType::FsDeleteReq, &req, Duration::from_millis(2000))?;
        Ok(())
    }
}
