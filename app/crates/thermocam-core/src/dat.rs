//! Lossless V1/V2/V3 `.DAT` reader/writer (all 4 valid sizes + legacy 192-grid point heuristic)
//! and `.thermo.json` non-destructive sidecar format (`firmware/src/thermal/save.cpp` & `load.cpp`).

use serde::{Deserialize, Serialize};
use thiserror::Error;

use crate::protocol::codec::FrameRaw;

pub const LEPTON2_SMALL_SIZE: usize = 9_621;
pub const LEPTON2_BIG_SIZE: usize = 10_005;
pub const LEPTON3_SMALL_SIZE: usize = 38_421;
pub const LEPTON3_BIG_SIZE: usize = 38_805;

#[derive(Debug, Error)]
pub enum DatError {
    #[error("invalid .DAT file size {0} (expected 9621, 10005, 38421, or 38805 bytes)")]
    InvalidSize(usize),
    #[error("pixel count {actual} does not match {width}x{height}")]
    PixelCountMismatch {
        width: u16,
        height: u16,
        actual: usize,
    },
    #[error("sidecar JSON error: {0}")]
    Json(#[from] serde_json::Error),
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct DatTempPoint {
    /// 1-based index on the 160×120 grid (`x + y * 160 + 1`).
    pub index: u16,
    pub x: u16,
    pub y: u16,
    pub raw: u16,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct ThermalDat {
    pub width: u16,
    pub height: u16,
    pub pixels: Vec<u16>,
    pub min_raw: u16,
    pub max_raw: u16,
    pub spot_temp: f32,
    pub color_scheme: u8,
    pub temp_format: u8,
    pub spot_enabled: bool,
    pub colorbar_enabled: bool,
    pub min_max_points: u8,
    pub cal_offset: f32,
    pub cal_slope: f32,
    pub temp_points: Vec<DatTempPoint>,
    pub has_temp_points_block: bool,
}

impl ThermalDat {
    /// Construct a `.DAT` from a Protocol v2 `FrameRaw`.
    #[must_use]
    pub fn from_frame(frame: &FrameRaw, color_scheme: u8, temp_format: u8) -> Self {
        let temp_points = frame
            .temp_points
            .iter()
            .take(96)
            .map(|p| {
                let scale = if frame.meta.width == 80 { 2 } else { 1 };
                let x160 = (u32::from(p.x) * scale).min(159) as u16;
                let y120 = (u32::from(p.y) * scale).min(119) as u16;
                DatTempPoint {
                    index: x160 + y120 * 160 + 1,
                    x: p.x.min(frame.meta.width.saturating_sub(1)),
                    y: p.y.min(frame.meta.height.saturating_sub(1)),
                    raw: p.raw,
                }
            })
            .collect();
        Self {
            width: frame.meta.width,
            height: frame.meta.height,
            pixels: frame.pixels.clone(),
            min_raw: frame.meta.min_raw,
            max_raw: frame.meta.max_raw,
            spot_temp: frame.meta.spot_temp_c,
            color_scheme,
            temp_format,
            spot_enabled: true,
            colorbar_enabled: true,
            min_max_points: 3,
            cal_offset: frame.meta.cal_offset,
            cal_slope: frame.meta.cal_slope,
            temp_points,
            has_temp_points_block: true,
        }
    }

    /// Construct a `.DAT` directly from displayed pixel data & radiometry parameters
    /// (used when exporting a paused frame, time-lapse snapshot, or edited `.DAT`).
    #[must_use]
    pub fn from_pixels(
        pixels: Vec<u16>,
        width: u16,
        height: u16,
        rad: crate::radiometry::RadiometryParams,
        color_scheme: u8,
        temp_format: u8,
    ) -> Self {
        let mut min_raw = u16::MAX;
        let mut max_raw = 0u16;
        for &px in &pixels {
            if px < min_raw {
                min_raw = px;
            }
            if px > max_raw {
                max_raw = px;
            }
        }
        if pixels.is_empty() {
            min_raw = 0;
        }
        let w = usize::from(width);
        let h = usize::from(height);
        let center_idx = (h / 2) * w + (w / 2);
        let spot_raw = pixels.get(center_idx).copied().unwrap_or(min_raw);
        let spot_temp = crate::radiometry::raw_to_object_c(spot_raw, rad);
        Self {
            width,
            height,
            pixels,
            min_raw,
            max_raw,
            spot_temp,
            color_scheme,
            temp_format,
            spot_enabled: true,
            colorbar_enabled: true,
            min_max_points: 3,
            cal_offset: rad.cal_offset,
            cal_slope: rad.cal_slope,
            temp_points: Vec::new(),
            has_temp_points_block: true,
        }
    }

    /// Parse any V1/V2/V3 `.DAT` file (9621, 10005, 38421, or 38805 bytes).
    pub fn from_bytes(bytes: &[u8]) -> Result<Self, DatError> {
        let (width, height, has_points) = match bytes.len() {
            LEPTON2_SMALL_SIZE => (80u16, 60u16, false),
            LEPTON2_BIG_SIZE => (80u16, 60u16, true),
            LEPTON3_SMALL_SIZE => (160u16, 120u16, false),
            LEPTON3_BIG_SIZE => (160u16, 120u16, true),
            other => return Err(DatError::InvalidSize(other)),
        };

        let pixel_count = usize::from(width) * usize::from(height);
        let pixel_bytes = pixel_count * 2;
        let pixels: Vec<u16> = bytes[..pixel_bytes]
            .as_chunks::<2>()
            .0
            .iter()
            .map(|c| u16::from_be_bytes(*c))
            .collect();

        let meta = &bytes[pixel_bytes..pixel_bytes + 21];
        let min_raw = u16::from_be_bytes([meta[0], meta[1]]);
        let max_raw = u16::from_be_bytes([meta[2], meta[3]]);
        let spot_temp = f32::from_le_bytes([meta[4], meta[5], meta[6], meta[7]]);
        let color_scheme = meta[8];
        let temp_format = meta[9];
        let spot_enabled = meta[10] != 0;
        let colorbar_enabled = meta[11] != 0;
        let min_max_points = meta[12];
        let cal_offset = f32::from_le_bytes([meta[13], meta[14], meta[15], meta[16]]);
        let cal_slope = f32::from_le_bytes([meta[17], meta[18], meta[19], meta[20]]);

        let temp_points = if has_points {
            parse_temp_points(&bytes[pixel_bytes + 21..], width)
        } else {
            Vec::new()
        };

        Ok(Self {
            width,
            height,
            pixels,
            min_raw,
            max_raw,
            spot_temp,
            color_scheme,
            temp_format,
            spot_enabled,
            colorbar_enabled,
            min_max_points,
            cal_offset: if cal_offset.is_finite() && cal_offset != 0.0 {
                cal_offset
            } else {
                -273.15
            },
            cal_slope: if cal_slope.is_finite() && cal_slope > 0.0 {
                cal_slope
            } else {
                0.01
            },
            temp_points,
            has_temp_points_block: has_points,
        })
    }

    /// Serialize to exact firmware-compatible `.DAT` bytes (`save.cpp` layout).
    #[must_use]
    pub fn to_bytes(&self) -> Vec<u8> {
        let extra = if self.has_temp_points_block { 384 } else { 0 };
        let mut out = Vec::with_capacity(self.pixels.len() * 2 + 21 + extra);
        for &px in &self.pixels {
            out.extend_from_slice(&px.to_be_bytes());
        }
        out.extend_from_slice(&self.min_raw.to_be_bytes());
        out.extend_from_slice(&self.max_raw.to_be_bytes());
        out.extend_from_slice(&self.spot_temp.to_le_bytes());
        out.push(self.color_scheme);
        out.push(self.temp_format);
        out.push(u8::from(self.spot_enabled));
        out.push(u8::from(self.colorbar_enabled));
        out.push(self.min_max_points);
        out.extend_from_slice(&self.cal_offset.to_le_bytes());
        out.extend_from_slice(&self.cal_slope.to_le_bytes());
        if self.has_temp_points_block {
            for i in 0..96 {
                if let Some(pt) = self.temp_points.get(i) {
                    out.extend_from_slice(&pt.index.to_be_bytes());
                    out.extend_from_slice(&pt.raw.to_be_bytes());
                } else {
                    out.extend_from_slice(&0u16.to_be_bytes());
                    out.extend_from_slice(&0u16.to_be_bytes());
                }
            }
        }
        out
    }
}

/// Exact firmware `readTempPoints()` heuristic (`firmware/src/thermal/load.cpp:192-262`).
fn parse_temp_points(block: &[u8], width: u16) -> Vec<DatTempPoint> {
    let mut arr = [0u16; 192];
    for (i, chunk) in block.as_chunks::<2>().0.iter().take(192).enumerate() {
        let v = u16::from_be_bytes(*chunk);
        arr[i] = if v == 65_535 { 0 } else { v };
    }

    let mut old_format = false;
    for i in 1..191 {
        if arr[i] != 0 && arr[i - 1] == 0 && arr[i + 1] == 0 {
            old_format = true;
            break;
        }
    }

    let div = if width == 80 { 2 } else { 1 };
    let mut out = Vec::new();
    if old_format {
        for (i, &val) in arr.iter().enumerate() {
            if val != 0 {
                let xpos = ((i % 16) * 10) as u16;
                let ypos = ((i / 16) * 10) as u16;
                let index = xpos + ypos * 160 + 1;
                out.push(DatTempPoint {
                    index,
                    x: xpos / div,
                    y: ypos / div,
                    raw: val,
                });
                if out.len() == 96 {
                    break;
                }
            }
        }
    } else {
        for i in 0..96 {
            let idx = arr[i * 2];
            let val = arr[i * 2 + 1];
            if idx != 0 {
                let z = idx.saturating_sub(1);
                let x160 = z % 160;
                let y120 = z / 160;
                out.push(DatTempPoint {
                    index: idx,
                    x: x160 / div,
                    y: y120 / div,
                    raw: val,
                });
            }
        }
    }
    out
}

/// The `.thermo.json` sidecar now lives in [`crate::sidecar`]; re-exported for older callers.
pub use crate::sidecar::ThermoSidecar;

/// Sequence `manifest.json` written alongside lossless `.DAT` frame sequences.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SequenceManifest {
    pub schema_version: u32,
    pub created_utc: String,
    pub sensor_w: u16,
    pub sensor_h: u16,
    pub fps: f32,
    pub frame_count: usize,
    pub frames: Vec<String>,
    pub palette_id: String,
}

/// Format a UNIX timestamp (seconds since 1970-01-01T00:00:00Z) as ISO-8601 UTC (`YYYY-MM-DDTHH:MM:SSZ`).
#[must_use]
pub fn format_utc_iso8601(unix_secs: u64) -> String {
    let days = (unix_secs / 86_400) as i64;
    let rem = unix_secs % 86_400;
    let hour = rem / 3600;
    let minute = (rem % 3600) / 60;
    let second = rem % 60;
    // Howard Hinnant's civil_from_days algorithm (exact Gregorian calendar)
    let z = days + 719_468;
    let era = if z >= 0 { z } else { z - 146_096 } / 146_097;
    let doe = (z - era * 146_097) as u64;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146_096) / 365;
    let y = (yoe as i64) + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let day = doy - (153 * mp + 2) / 5 + 1;
    let month = if mp < 10 { mp + 3 } else { mp - 9 };
    let year = if month <= 2 { y + 1 } else { y };
    format!("{year:04}-{month:02}-{day:02}T{hour:02}:{minute:02}:{second:02}Z")
}

/// Current system time formatted as ISO-8601 UTC (`YYYY-MM-DDTHH:MM:SSZ`).
#[must_use]
pub fn format_utc_now_iso8601() -> String {
    let secs = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_or(0, |d| d.as_secs());
    format_utc_iso8601(secs)
}
