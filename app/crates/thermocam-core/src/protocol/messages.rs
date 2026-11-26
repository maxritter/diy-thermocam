//! Remaining Protocol v2 codecs: DeviceConfig, Diagnostics, PerfTelemetry, RadParams, Events, SD FS, Bootloader.

use serde::{Deserialize, Serialize};

use super::codec::{
    check_len, read_cstr, read_f32, read_u16, read_u32, read_u64, write_cstr, SparseTempPoint,
};
use super::{flags, MsgType, Packet, ProtocolError, BOOTLOADER_MAGIC};

#[inline]
fn read_i16(b: &[u8], off: usize) -> i16 {
    i16::from_le_bytes([b[off], b[off + 1]])
}

// ── DeviceConfig (32 B) ──────────────────────────────────────────────────────

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct DeviceConfig {
    pub config_ver: u8,
    pub color_scheme: u8,
    pub text_color: u8,
    pub temp_format: u8,
    pub filter_type: u8,
    pub min_max_points: u8,
    pub rotation_vert: u8,
    pub rotation_horiz: u8,
    pub limits_mode: u8,
    pub shutter_mode: u8,
    pub lepton_gain_mode: u8,
    pub hot_cold_mode: u8,
    pub hot_cold_color: u8,
    pub screen_off_time: u8,
    pub overlay_flags: u16,
    pub manual_min_raw: u16,
    pub manual_max_raw: u16,
    pub hot_cold_level_c10: i16,
    pub min_max_preset: u8,
}

impl Default for DeviceConfig {
    fn default() -> Self {
        Self {
            config_ver: 1,
            color_scheme: 10, // Ironblack
            text_color: 0,
            temp_format: 0,
            filter_type: 1, // Gaussian
            min_max_points: 3,
            rotation_vert: 0,
            rotation_horiz: 0,
            limits_mode: 0,
            shutter_mode: 1,
            lepton_gain_mode: 0,
            hot_cold_mode: 0,
            hot_cold_color: 2,
            screen_off_time: 0,
            overlay_flags: 0x003F,
            manual_min_raw: 29315,
            manual_max_raw: 31315,
            hot_cold_level_c10: 350,
            min_max_preset: 0,
        }
    }
}

impl DeviceConfig {
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 32] {
        let mut b = [0u8; 32];
        b[0] = self.config_ver;
        b[1] = self.color_scheme;
        b[2] = self.text_color;
        b[3] = self.temp_format;
        b[4] = self.filter_type;
        b[5] = self.min_max_points;
        b[6] = self.rotation_vert;
        b[7] = self.rotation_horiz;
        b[8] = self.limits_mode;
        b[9] = self.shutter_mode;
        b[10] = self.lepton_gain_mode;
        b[11] = self.hot_cold_mode;
        b[12] = self.hot_cold_color;
        b[13] = self.screen_off_time;
        b[14..16].copy_from_slice(&self.overlay_flags.to_le_bytes());
        b[16..18].copy_from_slice(&self.manual_min_raw.to_le_bytes());
        b[18..20].copy_from_slice(&self.manual_max_raw.to_le_bytes());
        b[20..22].copy_from_slice(&self.hot_cold_level_c10.to_le_bytes());
        b[22] = self.min_max_preset;
        b
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 32)?;
        Ok(Self {
            config_ver: b[0],
            color_scheme: b[1],
            text_color: b[2],
            temp_format: b[3],
            filter_type: b[4],
            min_max_points: b[5],
            rotation_vert: b[6],
            rotation_horiz: b[7],
            limits_mode: b[8],
            shutter_mode: b[9],
            lepton_gain_mode: b[10],
            hot_cold_mode: b[11],
            hot_cold_color: b[12],
            screen_off_time: b[13],
            overlay_flags: read_u16(b, 14),
            manual_min_raw: read_u16(b, 16),
            manual_max_raw: read_u16(b, 18),
            hot_cold_level_c10: read_i16(b, 20),
            min_max_preset: b[22],
        })
    }

    #[must_use]
    pub fn to_set_req_bytes(&self, persist: bool) -> [u8; 36] {
        let mut b = [0u8; 36];
        b[0] = u8::from(persist);
        b[4..36].copy_from_slice(&self.to_bytes());
        b
    }
}

// ── Diagnostics (80 B) & PerfTelemetry (84 B) ────────────────────────────────

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Diagnostics {
    pub diagnostic_mask: u8,
    pub bat_pct: i8,
    pub usb_power: bool,
    pub sd_fs_type: u8,
    pub bat_mv: u16,
    pub cpu_mhz: u16,
    pub mcu_temp_c: f32,
    pub sd_total_kb: u32,
    pub sd_free_kb: u32,
    pub uptime_us: u64,
    pub free_dtcm_bytes: u32,
    pub free_ocram_bytes: u32,
    pub lepton_fpa_temp_c: f32,
    pub lepton_part_num: String,
}

impl Diagnostics {
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 80] {
        let mut b = [0u8; 80];
        b[0] = self.diagnostic_mask;
        b[1] = self.bat_pct as u8;
        b[2] = u8::from(self.usb_power);
        b[3] = self.sd_fs_type;
        b[4..6].copy_from_slice(&self.bat_mv.to_le_bytes());
        b[6..8].copy_from_slice(&self.cpu_mhz.to_le_bytes());
        b[8..12].copy_from_slice(&self.mcu_temp_c.to_le_bytes());
        b[12..16].copy_from_slice(&self.sd_total_kb.to_le_bytes());
        b[16..20].copy_from_slice(&self.sd_free_kb.to_le_bytes());
        b[20..28].copy_from_slice(&self.uptime_us.to_le_bytes());
        b[28..32].copy_from_slice(&self.free_dtcm_bytes.to_le_bytes());
        b[32..36].copy_from_slice(&self.free_ocram_bytes.to_le_bytes());
        b[36..40].copy_from_slice(&self.lepton_fpa_temp_c.to_le_bytes());
        b[40..72].copy_from_slice(&write_cstr::<32>(&self.lepton_part_num));
        b
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 80)?;
        Ok(Self {
            diagnostic_mask: b[0],
            bat_pct: b[1] as i8,
            usb_power: b[2] != 0,
            sd_fs_type: b[3],
            bat_mv: read_u16(b, 4),
            cpu_mhz: read_u16(b, 6),
            mcu_temp_c: read_f32(b, 8),
            sd_total_kb: read_u32(b, 12),
            sd_free_kb: read_u32(b, 16),
            uptime_us: read_u64(b, 20),
            free_dtcm_bytes: read_u32(b, 28),
            free_ocram_bytes: read_u32(b, 32),
            lepton_fpa_temp_c: read_f32(b, 36),
            lepton_part_num: read_cstr(&b[40..72]),
        })
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct PerfTelemetry {
    pub acquire_us: u32,
    pub stats_us: u32,
    pub filter_us: u32,
    pub scale_us: u32,
    pub colorize_us: u32,
    pub overlay_us: u32,
    pub display_us: u32,
    pub sd_write_us: u32,
    pub usb_tx_us: u32,
    pub loop_total_us: u32,
    pub frame_count: u32,
    pub frames_dropped: u32,
    pub vospi_resyncs: u32,
    pub vospi_crc_errors: u32,
    pub usb_rx_errors: u32,
    pub usb_crc_errors: u32,
    pub live_fps_x100: u16,
    pub video_fps_x100: u16,
    pub stream_fps_x100: u16,
    pub perf_mode: u8,
    pub flags: u8,
    pub display_spi_mhz: u8,
    pub lepton_spi_mhz: u8,
    pub cpu_mhz: u16,
    pub mcu_temp_c: f32,
    pub frame_period_us: u32,
}

impl PerfTelemetry {
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 84] {
        let mut b = [0u8; 84];
        b[0..4].copy_from_slice(&self.acquire_us.to_le_bytes());
        b[4..8].copy_from_slice(&self.stats_us.to_le_bytes());
        b[8..12].copy_from_slice(&self.filter_us.to_le_bytes());
        b[12..16].copy_from_slice(&self.scale_us.to_le_bytes());
        b[16..20].copy_from_slice(&self.colorize_us.to_le_bytes());
        b[20..24].copy_from_slice(&self.overlay_us.to_le_bytes());
        b[24..28].copy_from_slice(&self.display_us.to_le_bytes());
        b[28..32].copy_from_slice(&self.sd_write_us.to_le_bytes());
        b[32..36].copy_from_slice(&self.usb_tx_us.to_le_bytes());
        b[36..40].copy_from_slice(&self.loop_total_us.to_le_bytes());
        b[40..44].copy_from_slice(&self.frame_count.to_le_bytes());
        b[44..48].copy_from_slice(&self.frames_dropped.to_le_bytes());
        b[48..52].copy_from_slice(&self.vospi_resyncs.to_le_bytes());
        b[52..56].copy_from_slice(&self.vospi_crc_errors.to_le_bytes());
        b[56..60].copy_from_slice(&self.usb_rx_errors.to_le_bytes());
        b[60..64].copy_from_slice(&self.usb_crc_errors.to_le_bytes());
        b[64..66].copy_from_slice(&self.live_fps_x100.to_le_bytes());
        b[66..68].copy_from_slice(&self.video_fps_x100.to_le_bytes());
        b[68..70].copy_from_slice(&self.stream_fps_x100.to_le_bytes());
        b[70] = self.perf_mode;
        b[71] = self.flags;
        b[72] = self.display_spi_mhz;
        b[73] = self.lepton_spi_mhz;
        b[74..76].copy_from_slice(&self.cpu_mhz.to_le_bytes());
        b[76..80].copy_from_slice(&self.mcu_temp_c.to_le_bytes());
        b[80..84].copy_from_slice(&self.frame_period_us.to_le_bytes());
        b
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 84)?;
        Ok(Self {
            acquire_us: read_u32(b, 0),
            stats_us: read_u32(b, 4),
            filter_us: read_u32(b, 8),
            scale_us: read_u32(b, 12),
            colorize_us: read_u32(b, 16),
            overlay_us: read_u32(b, 20),
            display_us: read_u32(b, 24),
            sd_write_us: read_u32(b, 28),
            usb_tx_us: read_u32(b, 32),
            loop_total_us: read_u32(b, 36),
            frame_count: read_u32(b, 40),
            frames_dropped: read_u32(b, 44),
            vospi_resyncs: read_u32(b, 48),
            vospi_crc_errors: read_u32(b, 52),
            usb_rx_errors: read_u32(b, 56),
            usb_crc_errors: read_u32(b, 60),
            live_fps_x100: read_u16(b, 64),
            video_fps_x100: read_u16(b, 66),
            stream_fps_x100: read_u16(b, 68),
            perf_mode: b[70],
            flags: b[71],
            display_spi_mhz: b[72],
            lepton_spi_mhz: b[73],
            cpu_mhz: read_u16(b, 74),
            mcu_temp_c: read_f32(b, 76),
            frame_period_us: read_u32(b, 80),
        })
    }
}

// ── Radiometric Params (18 B) ────────────────────────────────────────────────

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct RadParams {
    pub scene_emissivity: u16,
    pub t_bkg_ck: u16,
    pub tau_window: u16,
    pub t_window_ck: u16,
    pub tau_atm: u16,
    pub t_atm_ck: u16,
    pub refl_window: u16,
    pub t_refl_ck: u16,
    pub flags: u8,
}

impl Default for RadParams {
    fn default() -> Self {
        Self {
            scene_emissivity: 8192,
            t_bkg_ck: 29515,
            tau_window: 8192,
            t_window_ck: 29515,
            tau_atm: 8192,
            t_atm_ck: 29515,
            refl_window: 0,
            t_refl_ck: 29515,
            flags: 0,
        }
    }
}

impl RadParams {
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 18] {
        let mut b = [0u8; 18];
        b[0..2].copy_from_slice(&self.scene_emissivity.to_le_bytes());
        b[2..4].copy_from_slice(&self.t_bkg_ck.to_le_bytes());
        b[4..6].copy_from_slice(&self.tau_window.to_le_bytes());
        b[6..8].copy_from_slice(&self.t_window_ck.to_le_bytes());
        b[8..10].copy_from_slice(&self.tau_atm.to_le_bytes());
        b[10..12].copy_from_slice(&self.t_atm_ck.to_le_bytes());
        b[12..14].copy_from_slice(&self.refl_window.to_le_bytes());
        b[14..16].copy_from_slice(&self.t_refl_ck.to_le_bytes());
        b[16] = self.flags;
        b[17] = 0;
        b
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 18)?;
        Ok(Self {
            scene_emissivity: read_u16(b, 0),
            t_bkg_ck: read_u16(b, 2),
            tau_window: read_u16(b, 4),
            t_window_ck: read_u16(b, 6),
            tau_atm: read_u16(b, 8),
            t_atm_ck: read_u16(b, 10),
            refl_window: read_u16(b, 12),
            t_refl_ck: read_u16(b, 14),
            flags: b[16],
        })
    }

    pub fn encode_set_packet(&self, seq: u16) -> Result<Vec<u8>, ProtocolError> {
        Packet::encode(MsgType::SetRadParamsReq, 0, seq, 0, &self.to_bytes())
    }
}

// ── SD Filesystem & Async Events & Bootloader ────────────────────────────────

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FsInfoResp {
    pub card_present: bool,
    pub fs_type: u8,
    pub total_kb: u32,
    pub free_kb: u32,
    pub root_entry_count: u32,
}

impl FsInfoResp {
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 16] {
        let mut b = [0u8; 16];
        b[0] = u8::from(self.card_present);
        b[1] = self.fs_type;
        b[4..8].copy_from_slice(&self.total_kb.to_le_bytes());
        b[8..12].copy_from_slice(&self.free_kb.to_le_bytes());
        b[12..16].copy_from_slice(&self.root_entry_count.to_le_bytes());
        b
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 16)?;
        Ok(Self {
            card_present: b[0] != 0,
            fs_type: b[1],
            total_kb: read_u32(b, 4),
            free_kb: read_u32(b, 8),
            root_entry_count: read_u32(b, 12),
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FsEntry {
    pub name: String,
    pub size: u32,
    pub mtime_epoch: u32,
    pub is_dir: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FsListResp {
    pub next_cursor: u32,
    pub entries: Vec<FsEntry>,
}

impl FsListResp {
    #[must_use]
    pub fn encode_req(cursor: u32, max_entries: u16, path: &str) -> Vec<u8> {
        let pb = path.as_bytes();
        let mut out = Vec::with_capacity(8 + pb.len());
        out.extend_from_slice(&cursor.to_le_bytes());
        out.extend_from_slice(&max_entries.to_le_bytes());
        out.extend_from_slice(&(pb.len() as u16).to_le_bytes());
        out.extend_from_slice(pb);
        out
    }

    #[must_use]
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut out = Vec::new();
        out.extend_from_slice(&self.next_cursor.to_le_bytes());
        out.extend_from_slice(&(self.entries.len() as u16).to_le_bytes());
        out.extend_from_slice(&0u16.to_le_bytes());
        for e in &self.entries {
            let nb = e.name.as_bytes();
            let nlen = nb.len().min(255);
            out.extend_from_slice(&e.size.to_le_bytes());
            out.extend_from_slice(&e.mtime_epoch.to_le_bytes());
            out.push(u8::from(e.is_dir));
            out.push(nlen as u8);
            out.extend_from_slice(&nb[..nlen]);
        }
        out
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 8)?;
        let next_cursor = read_u32(b, 0);
        let count = usize::from(read_u16(b, 4));
        let mut off = 8usize;
        let mut entries = Vec::with_capacity(count);
        for _ in 0..count {
            check_len(b, off + 10)?;
            let size = read_u32(b, off);
            let mtime_epoch = read_u32(b, off + 4);
            let is_dir = b[off + 8] != 0;
            let name_len = usize::from(b[off + 9]);
            off += 10;
            check_len(b, off + name_len)?;
            let name = String::from_utf8_lossy(&b[off..off + name_len]).into_owned();
            off += name_len;
            entries.push(FsEntry {
                name,
                size,
                mtime_epoch,
                is_dir,
            });
        }
        Ok(Self {
            next_cursor,
            entries,
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FsStatResp {
    pub size: u32,
    pub mtime_epoch: u32,
    pub crc32: u32,
    pub is_dir: bool,
    pub crc32_valid: bool,
}

impl FsStatResp {
    #[must_use]
    pub fn encode_req(compute_crc32: bool, path: &str) -> Vec<u8> {
        let pb = path.as_bytes();
        let mut out = Vec::with_capacity(4 + pb.len());
        out.push(u8::from(compute_crc32));
        out.push(0);
        out.extend_from_slice(&(pb.len() as u16).to_le_bytes());
        out.extend_from_slice(pb);
        out
    }

    #[must_use]
    pub fn to_bytes(&self) -> [u8; 16] {
        let mut b = [0u8; 16];
        b[0..4].copy_from_slice(&self.size.to_le_bytes());
        b[4..8].copy_from_slice(&self.mtime_epoch.to_le_bytes());
        b[8..12].copy_from_slice(&self.crc32.to_le_bytes());
        b[12] = u8::from(self.is_dir);
        b[13] = u8::from(self.crc32_valid);
        b
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 16)?;
        Ok(Self {
            size: read_u32(b, 0),
            mtime_epoch: read_u32(b, 4),
            crc32: read_u32(b, 8),
            is_dir: b[12] != 0,
            crc32_valid: b[13] != 0,
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FsReadChunk {
    pub offset: u32,
    pub eof: bool,
    pub data: Vec<u8>,
}

impl FsReadChunk {
    #[must_use]
    pub fn encode_req(offset: u32, length: u32, chunk_size: u16, path: &str) -> Vec<u8> {
        let pb = path.as_bytes();
        let mut out = Vec::with_capacity(12 + pb.len());
        out.extend_from_slice(&offset.to_le_bytes());
        out.extend_from_slice(&length.to_le_bytes());
        out.extend_from_slice(&chunk_size.to_le_bytes());
        out.extend_from_slice(&(pb.len() as u16).to_le_bytes());
        out.extend_from_slice(pb);
        out
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 8)?;
        let offset = read_u32(b, 0);
        let data_len = usize::from(read_u16(b, 4));
        let eof = b[6] != 0;
        check_len(b, 8 + data_len)?;
        Ok(Self {
            offset,
            eof,
            data: b[8..8 + data_len].to_vec(),
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SaveCaptureResp {
    pub dat_size: u32,
    pub bmp_size: u32,
    pub stem: String,
}

impl SaveCaptureResp {
    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 40)?;
        Ok(Self {
            dat_size: read_u32(b, 0),
            bmp_size: read_u32(b, 4),
            stem: read_cstr(&b[8..40]),
        })
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum AsyncEvent {
    Button {
        action: u8,
        duration_ms: u16,
    },
    Touch {
        action: u8,
        x: u16,
        y: u16,
    },
    Battery {
        bat_pct: i8,
        charging: bool,
        low_warning: bool,
        bat_mv: u16,
    },
    Ffc {
        state: u8,
        duration_ms: u16,
    },
    SessionExit {
        reason: u8,
    },
    Perf(Box<PerfTelemetry>),
}

impl AsyncEvent {
    pub fn from_packet(pkt: &Packet) -> Result<Option<Self>, ProtocolError> {
        let b = &pkt.payload;
        match pkt.msg_type()? {
            MsgType::EvtButton => {
                check_len(b, 4)?;
                Ok(Some(Self::Button {
                    action: b[0],
                    duration_ms: read_u16(b, 2),
                }))
            }
            MsgType::EvtTouch => {
                check_len(b, 6)?;
                Ok(Some(Self::Touch {
                    action: b[0],
                    x: read_u16(b, 2),
                    y: read_u16(b, 4),
                }))
            }
            MsgType::EvtBattery => {
                check_len(b, 6)?;
                Ok(Some(Self::Battery {
                    bat_pct: b[0] as i8,
                    charging: b[1] != 0,
                    low_warning: b[2] != 0,
                    bat_mv: read_u16(b, 4),
                }))
            }
            MsgType::EvtFfc => {
                check_len(b, 4)?;
                Ok(Some(Self::Ffc {
                    state: b[0],
                    duration_ms: read_u16(b, 2),
                }))
            }
            MsgType::EvtSessionExit => {
                check_len(b, 2)?;
                Ok(Some(Self::SessionExit { reason: b[0] }))
            }
            MsgType::EvtPerf => Ok(Some(Self::Perf(Box::new(PerfTelemetry::from_bytes(b)?)))),
            _ => Ok(None),
        }
    }
}

pub fn encode_enter_bootloader_req(seq: u16) -> Result<Vec<u8>, ProtocolError> {
    Packet::encode(
        MsgType::EnterBootloaderReq,
        0,
        seq,
        0,
        &BOOTLOADER_MAGIC.to_le_bytes(),
    )
}

pub fn encode_generic_error_resp(
    seq: u16,
    status: u16,
    message: &str,
) -> Result<Vec<u8>, ProtocolError> {
    let bytes = message.as_bytes();
    let trimmed = &bytes[..bytes.len().min(128)];
    Packet::encode(
        MsgType::GenericErrorResp,
        flags::ERROR,
        seq,
        status,
        trimmed,
    )
}

pub fn decode_temp_points_resp(b: &[u8]) -> Result<Vec<SparseTempPoint>, ProtocolError> {
    check_len(b, 2)?;
    let count = usize::from(read_u16(b, 0));
    check_len(b, 2 + count * 6)?;
    let mut pts = Vec::with_capacity(count);
    for c in b[2..2 + count * 6].as_chunks::<6>().0 {
        pts.push(SparseTempPoint {
            x: read_u16(c, 0),
            y: read_u16(c, 2),
            raw: read_u16(c, 4),
        });
    }
    Ok(pts)
}
