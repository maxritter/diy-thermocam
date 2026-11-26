//! Explicit little-endian wire encoders/decoders for all Protocol v2 payloads (§4).

use serde::{Deserialize, Serialize};

use super::{MsgType, Packet, ProtocolError, PROTO_MAJOR, PROTO_MINOR};

// Byte readers and writers shared with `messages.rs`.
#[inline]
pub(super) fn read_u16(b: &[u8], off: usize) -> u16 {
    u16::from_le_bytes([b[off], b[off + 1]])
}
#[inline]
pub(super) fn read_u32(b: &[u8], off: usize) -> u32 {
    u32::from_le_bytes([b[off], b[off + 1], b[off + 2], b[off + 3]])
}
#[inline]
pub(super) fn read_u64(b: &[u8], off: usize) -> u64 {
    u64::from_le_bytes([
        b[off],
        b[off + 1],
        b[off + 2],
        b[off + 3],
        b[off + 4],
        b[off + 5],
        b[off + 6],
        b[off + 7],
    ])
}
#[inline]
pub(super) fn read_f32(b: &[u8], off: usize) -> f32 {
    f32::from_le_bytes([b[off], b[off + 1], b[off + 2], b[off + 3]])
}
pub(super) fn read_cstr(slice: &[u8]) -> String {
    let end = slice.iter().position(|&c| c == 0).unwrap_or(slice.len());
    String::from_utf8_lossy(&slice[..end]).into_owned()
}
pub(super) fn write_cstr<const N: usize>(s: &str) -> [u8; N] {
    let mut buf = [0u8; N];
    let bytes = s.as_bytes();
    let len = bytes.len().min(N);
    buf[..len].copy_from_slice(&bytes[..len]);
    buf
}
pub(super) fn check_len(payload: &[u8], expected: usize) -> Result<(), ProtocolError> {
    if payload.len() < expected {
        return Err(ProtocolError::TruncatedPayload {
            expected,
            actual: payload.len(),
        });
    }
    Ok(())
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct HelloReq {
    pub host_proto_major: u8,
    pub host_proto_minor: u8,
    pub host_features: u32,
}

impl Default for HelloReq {
    fn default() -> Self {
        Self {
            host_proto_major: PROTO_MAJOR,
            host_proto_minor: PROTO_MINOR,
            host_features: 0x02,
        }
    }
}

impl HelloReq {
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 8] {
        let mut b = [0u8; 8];
        b[0] = self.host_proto_major;
        b[1] = self.host_proto_minor;
        b[4..8].copy_from_slice(&self.host_features.to_le_bytes());
        b
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 8)?;
        Ok(Self {
            host_proto_major: b[0],
            host_proto_minor: b[1],
            host_features: read_u32(b, 4),
        })
    }

    pub fn encode_packet(&self, seq: u16) -> Result<Vec<u8>, ProtocolError> {
        Packet::encode(MsgType::HelloReq, 0, seq, 0, &self.to_bytes())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct HelloResp {
    pub proto_major: u8,
    pub proto_minor: u8,
    pub fw_version: u16,
    pub fw_build_str: String,
    pub hw_version: u8,
    pub lepton_type: u8,
    pub sensor_w: u16,
    pub sensor_h: u16,
    pub display_w: u16,
    pub display_h: u16,
    pub diagnostic_mask: u8,
    pub teensy_uid: u64,
    pub capabilities: u32,
    pub max_chunk_size: u16,
    pub heartbeat_timeout_ms: u16,
}

impl HelloResp {
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 64] {
        let mut b = [0u8; 64];
        b[0] = self.proto_major;
        b[1] = self.proto_minor;
        b[2..4].copy_from_slice(&self.fw_version.to_le_bytes());
        b[4..36].copy_from_slice(&write_cstr::<32>(&self.fw_build_str));
        b[36] = self.hw_version;
        b[37] = self.lepton_type;
        b[38..40].copy_from_slice(&self.sensor_w.to_le_bytes());
        b[40..42].copy_from_slice(&self.sensor_h.to_le_bytes());
        b[42..44].copy_from_slice(&self.display_w.to_le_bytes());
        b[44..46].copy_from_slice(&self.display_h.to_le_bytes());
        b[46] = self.diagnostic_mask;
        b[48..56].copy_from_slice(&self.teensy_uid.to_le_bytes());
        b[56..60].copy_from_slice(&self.capabilities.to_le_bytes());
        b[60..62].copy_from_slice(&self.max_chunk_size.to_le_bytes());
        b[62..64].copy_from_slice(&self.heartbeat_timeout_ms.to_le_bytes());
        b
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 64)?;
        Ok(Self {
            proto_major: b[0],
            proto_minor: b[1],
            fw_version: read_u16(b, 2),
            fw_build_str: read_cstr(&b[4..36]),
            hw_version: b[36],
            lepton_type: b[37],
            sensor_w: read_u16(b, 38),
            sensor_h: read_u16(b, 40),
            display_w: read_u16(b, 42),
            display_h: read_u16(b, 44),
            diagnostic_mask: b[46],
            teensy_uid: read_u64(b, 48),
            capabilities: read_u32(b, 56),
            max_chunk_size: read_u16(b, 60),
            heartbeat_timeout_ms: read_u16(b, 62),
        })
    }
}

pub fn encode_goodbye_req(seq: u16) -> Result<Vec<u8>, ProtocolError> {
    Packet::encode(MsgType::GoodbyeReq, 0, seq, 0, &[])
}

pub fn encode_ping_req(seq: u16, host_time_us: u64) -> Result<Vec<u8>, ProtocolError> {
    Packet::encode(MsgType::PingReq, 0, seq, 0, &host_time_us.to_le_bytes())
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct PongResp {
    pub host_time_us: u64,
    pub device_uptime_us: u64,
}

impl PongResp {
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 16] {
        let mut b = [0u8; 16];
        b[0..8].copy_from_slice(&self.host_time_us.to_le_bytes());
        b[8..16].copy_from_slice(&self.device_uptime_us.to_le_bytes());
        b
    }
    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 16)?;
        Ok(Self {
            host_time_us: read_u64(b, 0),
            device_uptime_us: read_u64(b, 8),
        })
    }
}

// ── Streaming ────────────────────────────────────────────────────────────────

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct StreamCtrl {
    pub mode: u8,
    pub keep_lcd_on: bool,
    pub include_temp_points: bool,
    pub perf_interval_s: u8,
    pub max_fps_x10: u16,
}

impl StreamCtrl {
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 8] {
        let mut b = [0u8; 8];
        b[0] = self.mode;
        b[1] = u8::from(self.keep_lcd_on);
        b[2] = u8::from(self.include_temp_points);
        b[3] = self.perf_interval_s;
        b[4..6].copy_from_slice(&self.max_fps_x10.to_le_bytes());
        b
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 8)?;
        Ok(Self {
            mode: b[0],
            keep_lcd_on: b[1] != 0,
            include_temp_points: b[2] != 0,
            perf_interval_s: b[3],
            max_fps_x10: read_u16(b, 4),
        })
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct SparseTempPoint {
    pub x: u16,
    pub y: u16,
    pub raw: u16,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct FrameRawMeta {
    pub frame_seq: u32,
    pub timestamp_us: u64,
    pub rtc_epoch: u32,
    pub dropped_frames: u32,
    pub width: u16,
    pub height: u16,
    pub pixel_format: u8,
    pub gain_mode: u8,
    pub ffc_state: u8,
    pub flags: u8,
    pub min_raw: u16,
    pub max_raw: u16,
    pub min_x: u16,
    pub min_y: u16,
    pub max_x: u16,
    pub max_y: u16,
    pub spot_temp_c: f32,
    pub cal_offset: f32,
    pub cal_slope: f32,
    pub fpa_temp_c: f32,
    pub bat_pct: i8,
    pub usb_power: bool,
    pub temp_point_count: u16,
}

impl FrameRawMeta {
    #[must_use]
    pub fn to_bytes(&self) -> [u8; 64] {
        let mut b = [0u8; 64];
        b[0..4].copy_from_slice(&self.frame_seq.to_le_bytes());
        b[4..12].copy_from_slice(&self.timestamp_us.to_le_bytes());
        b[12..16].copy_from_slice(&self.rtc_epoch.to_le_bytes());
        b[16..20].copy_from_slice(&self.dropped_frames.to_le_bytes());
        b[20..22].copy_from_slice(&self.width.to_le_bytes());
        b[22..24].copy_from_slice(&self.height.to_le_bytes());
        b[24] = self.pixel_format;
        b[25] = self.gain_mode;
        b[26] = self.ffc_state;
        b[27] = self.flags;
        b[28..30].copy_from_slice(&self.min_raw.to_le_bytes());
        b[30..32].copy_from_slice(&self.max_raw.to_le_bytes());
        b[32..34].copy_from_slice(&self.min_x.to_le_bytes());
        b[34..36].copy_from_slice(&self.min_y.to_le_bytes());
        b[36..38].copy_from_slice(&self.max_x.to_le_bytes());
        b[38..40].copy_from_slice(&self.max_y.to_le_bytes());
        b[40..44].copy_from_slice(&self.spot_temp_c.to_le_bytes());
        b[44..48].copy_from_slice(&self.cal_offset.to_le_bytes());
        b[48..52].copy_from_slice(&self.cal_slope.to_le_bytes());
        b[52..56].copy_from_slice(&self.fpa_temp_c.to_le_bytes());
        b[56] = self.bat_pct as u8;
        b[57] = u8::from(self.usb_power);
        b[58..60].copy_from_slice(&self.temp_point_count.to_le_bytes());
        b
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        check_len(b, 64)?;
        Ok(Self {
            frame_seq: read_u32(b, 0),
            timestamp_us: read_u64(b, 4),
            rtc_epoch: read_u32(b, 12),
            dropped_frames: read_u32(b, 16),
            width: read_u16(b, 20),
            height: read_u16(b, 22),
            pixel_format: b[24],
            gain_mode: b[25],
            ffc_state: b[26],
            flags: b[27],
            min_raw: read_u16(b, 28),
            max_raw: read_u16(b, 30),
            min_x: read_u16(b, 32),
            min_y: read_u16(b, 34),
            max_x: read_u16(b, 36),
            max_y: read_u16(b, 38),
            spot_temp_c: read_f32(b, 40),
            cal_offset: read_f32(b, 44),
            cal_slope: read_f32(b, 48),
            fpa_temp_c: read_f32(b, 52),
            bat_pct: b[56] as i8,
            usb_power: b[57] != 0,
            temp_point_count: read_u16(b, 58),
        })
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct FrameRaw {
    pub meta: FrameRawMeta,
    pub pixels: Vec<u16>,
    pub temp_points: Vec<SparseTempPoint>,
}

impl FrameRaw {
    #[must_use]
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut meta = self.meta.clone();
        if !self.temp_points.is_empty() {
            meta.flags |= 0x04;
            meta.temp_point_count = self.temp_points.len() as u16;
        }
        let mut out = Vec::with_capacity(64 + self.pixels.len() * 2 + self.temp_points.len() * 6);
        out.extend_from_slice(&meta.to_bytes());
        for &px in &self.pixels {
            out.extend_from_slice(&px.to_le_bytes());
        }
        for pt in &self.temp_points {
            out.extend_from_slice(&pt.x.to_le_bytes());
            out.extend_from_slice(&pt.y.to_le_bytes());
            out.extend_from_slice(&pt.raw.to_le_bytes());
        }
        out
    }

    pub fn from_bytes(b: &[u8]) -> Result<Self, ProtocolError> {
        let meta = FrameRawMeta::from_bytes(b)?;
        let pixel_count = usize::from(meta.width) * usize::from(meta.height);
        let pixels_end = 64 + pixel_count * 2;
        check_len(b, pixels_end)?;
        let pixels: Vec<u16> = b[64..pixels_end]
            .as_chunks::<2>()
            .0
            .iter()
            .map(|c| u16::from_le_bytes(*c))
            .collect();
        let mut temp_points = Vec::new();
        if (meta.flags & 0x04) != 0 && meta.temp_point_count > 0 {
            let pts_bytes = usize::from(meta.temp_point_count) * 6;
            check_len(b, pixels_end + pts_bytes)?;
            for chunk in b[pixels_end..pixels_end + pts_bytes].as_chunks::<6>().0 {
                temp_points.push(SparseTempPoint {
                    x: read_u16(chunk, 0),
                    y: read_u16(chunk, 2),
                    raw: read_u16(chunk, 4),
                });
            }
        }
        Ok(Self {
            meta,
            pixels,
            temp_points,
        })
    }
}
