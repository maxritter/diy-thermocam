//! `Transport` trait, `SerialTransport`, `MockTransport`, and `SimulatorTransport` (realistic radiometric demo).

use std::collections::VecDeque;
use std::io::{self, Read, Write};
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};

use super::codec::{FrameRaw, FrameRawMeta, HelloReq, HelloResp, PongResp, StreamCtrl};
use super::messages::{
    DeviceConfig, Diagnostics, FsEntry, FsInfoResp, FsListResp, FsReadChunk, PerfTelemetry,
    RadParams,
};
use super::parser::StreamParser;
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

/// Enumerate available serial ports (plus the built-in Demo Simulator).
#[must_use]
pub fn list_ports() -> Vec<SerialPortInfo> {
    let mut out = vec![SerialPortInfo {
        name: "sim://thermocam-v3".to_string(),
        label: "DIY-Thermocam V3 Demo Simulator (160×120)".to_string(),
        vid: Some(TEENSY_USB_VID),
        pid: Some(TEENSY_CDC_PID),
        is_thermocam: true,
        is_simulator: true,
    }];
    if let Ok(ports) = serialport::available_ports() {
        for p in ports {
            let (vid, pid, product) = match p.port_type {
                serialport::SerialPortType::UsbPort(u) => {
                    (Some(u.vid), Some(u.pid), u.product.unwrap_or_default())
                }
                _ => (None, None, String::new()),
            };
            let is_thermocam = vid == Some(TEENSY_USB_VID)
                && matches!(pid, Some(TEENSY_CDC_PID) | Some(TEENSY_CDC_MTP_PID));
            let label = if product.is_empty() {
                p.port_name.clone()
            } else {
                format!("{} ({})", p.port_name, product)
            };
            out.push(SerialPortInfo {
                name: p.port_name,
                label,
                vid,
                pid,
                is_thermocam,
                is_simulator: false,
            });
        }
    }
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

/// Real USB CDC ACM serial transport using `serialport`.
pub struct SerialTransport {
    port: Box<dyn serialport::SerialPort>,
}

impl SerialTransport {
    pub fn open(port_name: &str) -> Result<Self, ProtocolError> {
        let mut port = serialport::new(port_name, 115_200)
            .timeout(Duration::from_millis(50))
            .open()
            .map_err(|e| ProtocolError::Io(io::Error::other(e.to_string())))?;
        let _ = port.write_data_terminal_ready(true);
        let _ = port.clear(serialport::ClearBuffer::All);
        Ok(Self { port })
    }
}

impl Transport for SerialTransport {
    fn write_all(&mut self, data: &[u8]) -> io::Result<()> {
        Write::write_all(&mut *self.port, data)
    }
    fn read_chunk(&mut self, buf: &mut [u8], timeout: Duration) -> io::Result<usize> {
        let _ = self.port.set_timeout(timeout);
        match Read::read(&mut *self.port, buf) {
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
        let n = buf.len().min(self.rx_queue.len());
        for slot in buf.iter_mut().take(n) {
            *slot = self.rx_queue.pop_front().unwrap_or(0);
        }
        Ok(n)
    }
}

/// Realistic offline DIY-Thermocam V3 Protocol v2 simulator generating animated radiometric scenes.
pub struct SimulatorTransport {
    parser: StreamParser,
    out_buf: VecDeque<u8>,
    config: DeviceConfig,
    rad_params: RadParams,
    streaming: bool,
    frame_seq: u32,
    start: Instant,
    sd_files: Vec<(String, Vec<u8>)>,
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
        Self {
            parser: StreamParser::new(),
            out_buf: VecDeque::new(),
            config: DeviceConfig::default(),
            rad_params: RadParams::default(),
            streaming: false,
            frame_seq: 0,
            start: Instant::now(),
            sd_files: vec![
                ("20261125101500.DAT".to_string(), d0),
                ("20261125102030.DAT".to_string(), d1),
                ("20261125102845.DAT".to_string(), d2),
            ],
        }
    }

    pub fn push_next_frame(&mut self) {
        self.frame_seq = self.frame_seq.wrapping_add(1);
        let frame = generate_demo_frame(self.frame_seq, 160, 120);
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
                    fw_version: 310,
                    fw_build_str: "3.1.0-sim (2026-11-25)".to_string(),
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
                    Some((MsgType::StreamCtrlResp, 0, 0, sc.to_bytes().to_vec()))
                } else {
                    Some((MsgType::StreamCtrlResp, flags::ERROR, StatusCode::ErrInvalidArg as u16, Vec::new()))
                }
            }
            MsgType::GetConfigReq => {
                Some((MsgType::GetConfigResp, 0, 0, self.config.to_bytes().to_vec()))
            }
            MsgType::SetConfigReq if pkt.payload.len() >= 36 => {
                if let Ok(cfg) = DeviceConfig::from_bytes(&pkt.payload[4..36]) {
                    self.config = cfg;
                    Some((MsgType::SetConfigResp, 0, 0, self.config.to_bytes().to_vec()))
                } else {
                    Some((MsgType::SetConfigResp, flags::ERROR, StatusCode::ErrInvalidArg as u16, Vec::new()))
                }
            }
            MsgType::GetRadParamsReq => {
                Some((MsgType::GetRadParamsResp, 0, 0, self.rad_params.to_bytes().to_vec()))
            }
            MsgType::SetRadParamsReq => {
                if let Ok(rp) = RadParams::from_bytes(&pkt.payload) {
                    self.rad_params = rp;
                    Some((MsgType::SetRadParamsResp, 0, 0, self.rad_params.to_bytes().to_vec()))
                } else {
                    Some((MsgType::SetRadParamsResp, flags::ERROR, StatusCode::ErrInvalidArg as u16, Vec::new()))
                }
            }
            MsgType::TriggerFfcReq => Some((MsgType::TriggerFfcResp, 0, 0, vec![0, 0])),
            MsgType::GetDiagnosticsReq => {
                let d = Diagnostics {
                    diagnostic_mask: 0xFF,
                    bat_pct: 92,
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
                    root_entry_count: self.sd_files.len() as u32,
                };
                Some((MsgType::FsInfoResp, 0, 0, info.to_bytes().to_vec()))
            }
            MsgType::FsListReq => {
                let list = FsListResp {
                    next_cursor: u32::MAX,
                    entries: self
                        .sd_files
                        .iter()
                        .map(|(n, d)| FsEntry {
                            name: n.clone(),
                            size: d.len() as u32,
                            mtime_epoch: 1_764_065_700,
                            is_dir: false,
                        })
                        .collect(),
                };
                Some((MsgType::FsListResp, 0, 0, list.to_bytes()))
            }
            MsgType::FsReadReq if pkt.payload.len() >= 12 => {
                let path_len = u16::from_le_bytes([pkt.payload[10], pkt.payload[11]]) as usize;
                let path = String::from_utf8_lossy(
                    &pkt.payload[12..(12 + path_len).min(pkt.payload.len())],
                );
                let name = path.trim_start_matches('/');
                if let Some((_, data)) = self.sd_files.iter().find(|(n, _)| n == name) {
                    let mut p = Vec::with_capacity(8 + data.len());
                    p.extend_from_slice(&0u32.to_le_bytes());
                    p.extend_from_slice(&(data.len() as u16).to_le_bytes());
                    p.push(1);
                    p.push(0);
                    p.extend_from_slice(data);
                    Some((MsgType::FsReadData, 0, 0, p))
                } else {
                    Some((MsgType::FsReadData, flags::ERROR, StatusCode::ErrNotFound as u16, Vec::new()))
                }
            }
            MsgType::FsDeleteReq if pkt.payload.len() >= 2 => {
                let path_len = u16::from_le_bytes([pkt.payload[0], pkt.payload[1]]) as usize;
                let path = String::from_utf8_lossy(
                    &pkt.payload[2..(2 + path_len).min(pkt.payload.len())],
                );
                let name = path.trim_start_matches('/');
                let before = self.sd_files.len();
                self.sd_files.retain(|(n, _)| n != name);
                if self.sd_files.len() < before {
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

    fn read_chunk(&mut self, buf: &mut [u8], _timeout: Duration) -> io::Result<usize> {
        if self.out_buf.is_empty() && self.streaming {
            self.push_next_frame();
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
            // Ambient PCB substrate ~23.5 C with subtle vertical gradient
            let mut temp_c = 22.8 + 1.8 * (1.0 - ny) + 0.5 * (nx * std::f32::consts::TAU).sin();

            // Hot SoC / MCU package near center-left (~64 C, gently breathing)
            let dx1 = (nx - 0.38) * 1.33;
            let dy1 = ny - 0.48;
            let r1 = dx1 * dx1 + dy1 * dy1;
            temp_c += (41.5 + 2.2 * phase.sin()) * (-r1 / 0.011).exp();

            // Switching regulator MOSFET hotspot (~53 C)
            let dx2 = (nx - 0.68) * 1.33;
            let dy2 = ny - 0.34;
            let r2 = dx2 * dx2 + dy2 * dy2;
            temp_c += (31.0 + 1.5 * (phase * 1.3).cos()) * (-r2 / 0.0055).exp();

            // Warm copper trace connecting regulator to MCU
            let trace_dist = ((ny - (0.34 + 0.14 * ((0.68 - nx) / 0.30).clamp(0.0, 1.0))) * 12.0).abs();
            if (0.38..=0.68).contains(&nx) && trace_dist < 0.7 {
                temp_c += 11.5 * (1.0 - trace_dist / 0.7);
            }

            // Secondary linear LDO lower-right (~44 C)
            let dx3 = (nx - 0.72) * 1.33;
            let dy3 = ny - 0.72;
            temp_c += 21.0 * (-(dx3 * dx3 + dy3 * dy3) / 0.007).exp();

            // Cool aluminum heatsink fin block on left (~17.5 C)
            if (0.08..=0.21).contains(&nx) && (0.20..=0.80).contains(&ny) {
                let fin = ((ny * 40.0).sin() * 0.5 + 0.5) * 1.6;
                temp_c -= 5.4 - fin;
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
            bat_pct: 92,
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
}

impl<T: Transport> ProtocolClient<T> {
    pub fn new(transport: T) -> Self {
        Self {
            transport,
            parser: StreamParser::new(),
            next_seq: 1,
            pending: VecDeque::new(),
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

        let deadline = Instant::now() + timeout;
        let mut buf = [0u8; 4096];
        loop {
            if let Some(pos) = self.pending.iter().position(|p| p.header.seq == seq) {
                let pkt = self.pending.remove(pos).expect("valid idx");
                if pkt.is_error() {
                    return Err(ProtocolError::DeviceError {
                        status: pkt.header.status_or_sub,
                        message: String::from_utf8_lossy(&pkt.payload).into_owned(),
                    });
                }
                return Ok(pkt);
            }
            if Instant::now() >= deadline {
                return Err(ProtocolError::Timeout(seq));
            }
            let n = self
                .transport
                .read_chunk(&mut buf, Duration::from_millis(20))?;
            if n > 0 {
                for pkt in self.parser.feed(&buf[..n]).into_iter().flatten() {
                    self.pending.push_back(pkt);
                }
            }
        }
    }

    pub fn hello(&mut self) -> Result<HelloResp, ProtocolError> {
        let pkt = self.request(
            MsgType::HelloReq,
            &HelloReq::default().to_bytes(),
            Duration::from_millis(1000),
        )?;
        HelloResp::from_bytes(&pkt.payload)
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
        let deadline = Instant::now() + timeout;
        let mut buf = vec![0u8; 65_536];
        loop {
            let rem = deadline.saturating_duration_since(Instant::now());
            let n = self.transport.read_chunk(&mut buf, rem.min(Duration::from_millis(20)))?;
            if n > 0 {
                for pkt in self.parser.feed(&buf[..n]).into_iter().flatten() {
                    out.push(pkt);
                }
            }
            if !out.is_empty() || n == 0 || Instant::now() >= deadline {
                break;
            }
        }
        Ok(out)
    }

    pub fn read_file_bytes(&mut self, path: &str) -> Result<Vec<u8>, ProtocolError> {
        let mut out = Vec::new();
        let mut offset = 0u32;
        loop {
            let req = FsReadChunk::encode_req(offset, 0, 8192, path);
            let pkt = self.request(MsgType::FsReadReq, &req, Duration::from_millis(3000))?;
            let chunk = FsReadChunk::from_bytes(&pkt.payload)?;
            offset = offset.saturating_add(chunk.data.len() as u32);
            out.extend_from_slice(&chunk.data);
            if chunk.eof || chunk.data.is_empty() {
                break;
            }
        }
        Ok(out)
    }

    pub fn delete_file(&mut self, path: &str) -> Result<(), ProtocolError> {
        let pb = path.as_bytes();
        let mut req = Vec::with_capacity(2 + pb.len());
        req.extend_from_slice(&(pb.len() as u16).to_le_bytes());
        req.extend_from_slice(pb);
        let _ = self.request(MsgType::FsDeleteReq, &req, Duration::from_millis(2000))?;
        Ok(())
    }
}
