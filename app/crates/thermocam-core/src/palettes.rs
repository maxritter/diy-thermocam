//! Single-source firmware palettes (generated at build time from `firmware/3.0/src/general/colorschemes.cpp`)
//! plus perceptually uniform 2026 palettes (`Inferno`, `Viridis`, `Magma`, `Plasma`, `Turbo`) and custom 256-px import.

use serde::{Deserialize, Serialize};
use thiserror::Error;

include!(concat!(env!("OUT_DIR"), "/firmware_palettes.rs"));

#[derive(Debug, Error)]
pub enum PaletteError {
    #[error("palette '{0}' not found")]
    NotFound(String),
    #[error("invalid custom palette data: {0}")]
    InvalidCustom(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Palette {
    pub id: String,
    pub label: String,
    pub firmware_id: Option<u8>,
    pub category: String,
    /// Exact RGB entries (192/224/240/256 for firmware palettes, 256 for perceptual/custom).
    pub rgb: Vec<[u8; 3]>,
}

impl Palette {
    /// Sample RGB at normalized `[0.0, 1.0]` using exact firmware indexing semantics.
    #[must_use]
    pub fn sample(&self, t: f32) -> [u8; 3] {
        if self.rgb.is_empty() {
            return [0, 0, 0];
        }
        let clamped = t.clamp(0.0, 1.0);
        let idx = (clamped * (self.rgb.len() - 1) as f32).round() as usize;
        self.rgb[idx.min(self.rgb.len() - 1)]
    }

    /// Resample into a 256-entry RGBA LUT for WebGL2 1D texture upload.
    #[must_use]
    pub fn to_rgba_256(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(256 * 4);
        for i in 0..256 {
            let [r, g, b] = self.sample(i as f32 / 255.0);
            out.extend_from_slice(&[r, g, b, 255]);
        }
        out
    }
}

/// Interpolate keyframe stops `[(t, r, g, b)]` smoothly into a 256-entry palette.
fn lerp_stops(stops: &[(f32, u8, u8, u8)]) -> Vec<[u8; 3]> {
    let mut out = Vec::with_capacity(256);
    for i in 0..256 {
        let t = i as f32 / 255.0;
        let mut s0 = stops[0];
        let mut s1 = stops[stops.len() - 1];
        for w in stops.windows(2) {
            if t >= w[0].0 && t <= w[1].0 {
                s0 = w[0];
                s1 = w[1];
                break;
            }
        }
        let span = (s1.0 - s0.0).max(1e-6);
        let f = ((t - s0.0) / span).clamp(0.0, 1.0);
        let lerp = |a: u8, b: u8| -> u8 {
            (f32::from(a) + (f32::from(b) - f32::from(a)) * f)
                .round()
                .clamp(0.0, 255.0) as u8
        };
        out.push([lerp(s0.1, s1.1), lerp(s0.2, s1.2), lerp(s0.3, s1.3)]);
    }
    out
}

/// Return all 24 built-in palettes: 19 bit-exact firmware LUTs + 5 perceptually uniform palettes.
#[must_use]
pub fn builtin_palettes() -> Vec<Palette> {
    let mut list: Vec<Palette> = FIRMWARE_PALETTES
        .iter()
        .map(|p| Palette {
            id: p.key.to_string(),
            label: p.label.to_string(),
            firmware_id: Some(p.id),
            category: "Firmware".to_string(),
            rgb: p.rgb.to_vec(),
        })
        .collect();

    list.push(Palette {
        id: "inferno".into(),
        label: "Inferno (Perceptual)".into(),
        firmware_id: None,
        category: "Perceptual".into(),
        rgb: lerp_stops(&[
            (0.00, 0, 0, 4),
            (0.15, 40, 11, 84),
            (0.32, 101, 21, 110),
            (0.50, 159, 42, 99),
            (0.68, 212, 72, 66),
            (0.84, 245, 125, 21),
            (0.94, 250, 193, 39),
            (1.00, 252, 255, 164),
        ]),
    });
    list.push(Palette {
        id: "viridis".into(),
        label: "Viridis (Perceptual)".into(),
        firmware_id: None,
        category: "Perceptual".into(),
        rgb: lerp_stops(&[
            (0.00, 68, 1, 84),
            (0.20, 65, 68, 135),
            (0.40, 42, 120, 142),
            (0.60, 34, 168, 132),
            (0.80, 122, 209, 81),
            (1.00, 253, 231, 37),
        ]),
    });
    list.push(Palette {
        id: "magma".into(),
        label: "Magma (Perceptual)".into(),
        firmware_id: None,
        category: "Perceptual".into(),
        rgb: lerp_stops(&[
            (0.00, 0, 0, 4),
            (0.18, 44, 17, 95),
            (0.38, 114, 31, 129),
            (0.58, 182, 54, 121),
            (0.76, 241, 96, 93),
            (0.90, 254, 175, 119),
            (1.00, 252, 253, 191),
        ]),
    });
    list.push(Palette {
        id: "plasma".into(),
        label: "Plasma (Perceptual)".into(),
        firmware_id: None,
        category: "Perceptual".into(),
        rgb: lerp_stops(&[
            (0.00, 13, 8, 135),
            (0.20, 84, 2, 163),
            (0.40, 139, 10, 165),
            (0.60, 185, 50, 137),
            (0.80, 219, 92, 104),
            (0.92, 244, 136, 73),
            (1.00, 240, 249, 33),
        ]),
    });
    list.push(Palette {
        id: "turbo".into(),
        label: "Turbo (High-Contrast)".into(),
        firmware_id: None,
        category: "Perceptual".into(),
        rgb: lerp_stops(&[
            (0.00, 48, 18, 59),
            (0.15, 65, 69, 171),
            (0.30, 46, 145, 224),
            (0.45, 29, 206, 170),
            (0.60, 122, 234, 83),
            (0.75, 232, 207, 45),
            (0.88, 251, 126, 33),
            (1.00, 122, 4, 3),
        ]),
    });
    list
}

/// Look up a built-in palette by string ID or firmware numeric ID.
pub fn find_palette(id: &str) -> Result<Palette, PaletteError> {
    builtin_palettes()
        .into_iter()
        .find(|p| p.id.eq_ignore_ascii_case(id))
        .ok_or_else(|| PaletteError::NotFound(id.to_string()))
}

#[must_use]
pub fn firmware_palette_by_id(fw_id: u8) -> Palette {
    builtin_palettes()
        .into_iter()
        .find(|p| p.firmware_id == Some(fw_id))
        .unwrap_or_else(|| find_palette("ironblack").expect("ironblack built-in"))
}

/// Import a custom 256-entry palette from a 1D/2D PNG image (samples row 0 across width) or CSV/hex text.
pub fn parse_custom_palette_png(name: &str, png_bytes: &[u8]) -> Result<Palette, PaletteError> {
    let img = image::load_from_memory(png_bytes)
        .map_err(|e| PaletteError::InvalidCustom(e.to_string()))?
        .to_rgb8();
    let w = img.width();
    if w < 2 {
        return Err(PaletteError::InvalidCustom(
            "image width must be >= 2".into(),
        ));
    }
    let mut rgb = Vec::with_capacity(256);
    for i in 0..256 {
        let x = ((i as f32 / 255.0) * (w - 1) as f32).round() as u32;
        let px = img.get_pixel(x, img.height() / 2);
        rgb.push([px[0], px[1], px[2]]);
    }
    Ok(Palette {
        id: format!("custom:{}", name.to_lowercase().replace(' ', "_")),
        label: name.to_string(),
        firmware_id: None,
        category: "Custom".into(),
        rgb,
    })
}

/// Import a custom palette from CSV (`r,g,b` per line) or `#RRGGBB` hex lines (resampled to 256 entries).
pub fn parse_custom_palette_text(name: &str, text: &str) -> Result<Palette, PaletteError> {
    let mut entries = Vec::new();
    for line in text.lines() {
        let l = line.trim();
        if l.is_empty() || l.starts_with('#') && l.len() != 7 {
            continue;
        }
        if let Some(hex) = l.strip_prefix('#') {
            if hex.len() == 6 {
                let r = u8::from_str_radix(&hex[0..2], 16)
                    .map_err(|e| PaletteError::InvalidCustom(e.to_string()))?;
                let g = u8::from_str_radix(&hex[2..4], 16)
                    .map_err(|e| PaletteError::InvalidCustom(e.to_string()))?;
                let b = u8::from_str_radix(&hex[4..6], 16)
                    .map_err(|e| PaletteError::InvalidCustom(e.to_string()))?;
                entries.push([r, g, b]);
            }
        } else {
            let parts: Vec<&str> = l.split([',', ';', ' ']).filter(|s| !s.is_empty()).collect();
            if parts.len() >= 3 {
                if let (Ok(r), Ok(g), Ok(b)) = (
                    parts[0].parse::<u8>(),
                    parts[1].parse::<u8>(),
                    parts[2].parse::<u8>(),
                ) {
                    entries.push([r, g, b]);
                }
            }
        }
    }
    if entries.len() < 2 {
        return Err(PaletteError::InvalidCustom(
            "need at least 2 RGB entries".into(),
        ));
    }
    let stops: Vec<(f32, u8, u8, u8)> = entries
        .iter()
        .enumerate()
        .map(|(i, c)| (i as f32 / (entries.len() - 1) as f32, c[0], c[1], c[2]))
        .collect();
    Ok(Palette {
        id: format!("custom:{}", name.to_lowercase().replace(' ', "_")),
        label: name.to_string(),
        firmware_id: None,
        category: "Custom".into(),
        rgb: lerp_stops(&stops),
    })
}
