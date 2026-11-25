//! Measurement-safe display pipeline: spatial filters, rotation/flip, modern high-resolution
//! upscalers (Nearest, Bicubic Catmull-Rom, Separable Lanczos-3, Edge-Preserving Thermal Enhance
//! with bilateral denoise + Lanczos-3 + Unsharp Mask), and RGB colorization + isotherm overlay.

use serde::{Deserialize, Serialize};

use crate::palettes::Palette;
use crate::radiometry::{raw_to_object_c, RadiometryParams};
use crate::roi::{IsothermConfig, IsothermMode};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "snake_case")]
pub enum SpatialFilter {
    #[default]
    None,
    Box3x3,
    Gaussian3x3,
    Median3x3,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "snake_case")]
pub enum UpscaleMethod {
    Nearest,
    Bicubic,
    Lanczos3,
    #[default]
    EdgePreserve,
}

#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct DisplayPipeline {
    pub filter: SpatialFilter,
    pub upscaler: UpscaleMethod,
    pub sharpen_amount: f32,
    pub rotate_180: bool,
    pub flip_horizontal: bool,
    pub isotherm: IsothermConfig,
}

impl Default for DisplayPipeline {
    fn default() -> Self {
        Self {
            filter: SpatialFilter::None,
            upscaler: UpscaleMethod::EdgePreserve,
            sharpen_amount: 0.45,
            rotate_180: false,
            flip_horizontal: false,
            isotherm: IsothermConfig::default(),
        }
    }
}

#[must_use]
pub fn preprocess_display_scalar(
    pixels: &[u16],
    width: usize,
    height: usize,
    pipeline: &DisplayPipeline,
) -> Vec<f32> {
    let mut oriented = vec![0f32; width * height];
    for y in 0..height {
        let sy = if pipeline.rotate_180 { height - 1 - y } else { y };
        for x in 0..width {
            let mut sx = if pipeline.rotate_180 { width - 1 - x } else { x };
            if pipeline.flip_horizontal {
                sx = width - 1 - sx;
            }
            oriented[y * width + x] = f32::from(pixels[sy * width + sx]);
        }
    }
    match pipeline.filter {
        SpatialFilter::None => oriented,
        SpatialFilter::Box3x3 => convolve_3x3(&oriented, width, height, [1.0; 9], 9.0),
        SpatialFilter::Gaussian3x3 => convolve_3x3(
            &oriented,
            width,
            height,
            [1.0, 2.0, 1.0, 2.0, 4.0, 2.0, 1.0, 2.0, 1.0],
            16.0,
        ),
        SpatialFilter::Median3x3 => median_3x3(&oriented, width, height),
    }
}

fn convolve_3x3(src: &[f32], w: usize, h: usize, k: [f32; 9], div: f32) -> Vec<f32> {
    let mut dst = src.to_vec();
    for y in 1..h.saturating_sub(1) {
        for x in 1..w.saturating_sub(1) {
            let mut acc = 0.0;
            let mut idx = 0;
            for dy in -1isize..=1 {
                for dx in -1isize..=1 {
                    acc += src[(y as isize + dy) as usize * w + (x as isize + dx) as usize] * k[idx];
                    idx += 1;
                }
            }
            dst[y * w + x] = acc / div;
        }
    }
    dst
}

fn median_3x3(src: &[f32], w: usize, h: usize) -> Vec<f32> {
    let mut dst = src.to_vec();
    let mut win = [0f32; 9];
    for y in 1..h.saturating_sub(1) {
        for x in 1..w.saturating_sub(1) {
            let mut i = 0;
            for dy in -1isize..=1 {
                for dx in -1isize..=1 {
                    win[i] = src[(y as isize + dy) as usize * w + (x as isize + dx) as usize];
                    i += 1;
                }
            }
            win.sort_by(|a, b| a.total_cmp(b));
            dst[y * w + x] = win[4];
        }
    }
    dst
}

fn bilateral_denoise(src: &[f32], w: usize, h: usize, sigma_range: f32) -> Vec<f32> {
    let inv_2sr2 = 1.0 / (2.0 * sigma_range * sigma_range).max(1e-6);
    let spatial: [f32; 9] = [0.606, 0.778, 0.606, 0.778, 1.0, 0.778, 0.606, 0.778, 0.606];
    let mut dst = vec![0f32; w * h];
    for y in 0..h {
        for x in 0..w {
            let center = src[y * w + x];
            let mut num = 0.0f32;
            let mut den = 0.0f32;
            let mut idx = 0;
            for dy in -1isize..=1 {
                let ny = (y as isize + dy).clamp(0, (h - 1) as isize) as usize;
                for dx in -1isize..=1 {
                    let nx = (x as isize + dx).clamp(0, (w - 1) as isize) as usize;
                    let v = src[ny * w + nx];
                    let diff = v - center;
                    let wt = spatial[idx] * (-(diff * diff) * inv_2sr2).exp();
                    num += v * wt;
                    den += wt;
                    idx += 1;
                }
            }
            dst[y * w + x] = if den > 0.0 { num / den } else { center };
        }
    }
    dst
}

fn unsharp_mask(src: &[f32], w: usize, h: usize, amount: f32) -> Vec<f32> {
    if amount <= 0.001 || w < 4 || h < 4 {
        return src.to_vec();
    }
    let k = [0.0625f32, 0.25, 0.375, 0.25, 0.0625];
    let mut tmp = vec![0f32; w * h];
    for y in 0..h {
        let row = &src[y * w..(y + 1) * w];
        let out = &mut tmp[y * w..(y + 1) * w];
        for (x, slot) in out.iter_mut().enumerate().take(w) {
            let mut s = 0.0;
            for (ki, &kw) in k.iter().enumerate() {
                let nx = (x as isize + ki as isize - 2).clamp(0, (w - 1) as isize) as usize;
                s += row[nx] * kw;
            }
            *slot = s;
        }
    }
    let mut out = vec![0f32; w * h];
    let amt = amount.clamp(0.0, 2.0);
    for y in 0..h {
        for x in 0..w {
            let mut blur = 0.0;
            for (ki, &kw) in k.iter().enumerate() {
                let ny = (y as isize + ki as isize - 2).clamp(0, (h - 1) as isize) as usize;
                blur += tmp[ny * w + x] * kw;
            }
            let orig = src[y * w + x];
            out[y * w + x] = (orig + amt * (orig - blur)).clamp(0.0, 1.0);
        }
    }
    out
}

#[inline]
fn catmull_rom(x: f32) -> f32 {
    let ax = x.abs();
    if ax < 1.0 {
        1.5 * ax * ax * ax - 2.5 * ax * ax + 1.0
    } else if ax < 2.0 {
        -0.5 * ax * ax * ax + 2.5 * ax * ax - 4.0 * ax + 2.0
    } else {
        0.0
    }
}

#[inline]
fn lanczos3(x: f32) -> f32 {
    let ax = x.abs();
    if ax < 1e-6 {
        1.0
    } else if ax < 3.0 {
        let px = std::f32::consts::PI * ax;
        (px.sin() / px) * ((px / 3.0).sin() / (px / 3.0))
    } else {
        0.0
    }
}

#[must_use]
pub fn resample_scalar(
    src: &[f32],
    sw: usize,
    sh: usize,
    dw: usize,
    dh: usize,
    method: UpscaleMethod,
) -> Vec<f32> {
    if sw == dw && sh == dh {
        return src.to_vec();
    }
    if method == UpscaleMethod::Nearest {
        let mut out = vec![0f32; dw * dh];
        for y in 0..dh {
            let sy = ((y as f32 + 0.5) * sh as f32 / dh as f32)
                .floor()
                .clamp(0.0, (sh - 1) as f32) as usize;
            for x in 0..dw {
                let sx = ((x as f32 + 0.5) * sw as f32 / dw as f32)
                    .floor()
                    .clamp(0.0, (sw - 1) as f32) as usize;
                out[y * dw + x] = src[sy * sw + sx];
            }
        }
        return out;
    }

    let (radius, kernel): (isize, fn(f32) -> f32) = match method {
        UpscaleMethod::Bicubic => (2, catmull_rom),
        UpscaleMethod::Lanczos3 | UpscaleMethod::EdgePreserve => (3, lanczos3),
        UpscaleMethod::Nearest => unreachable!(),
    };

    let h_taps = precompute_taps(sw, dw, radius, kernel);
    let mut mid = vec![0f32; dw * sh];
    for y in 0..sh {
        let row = &src[y * sw..(y + 1) * sw];
        let mrow = &mut mid[y * dw..(y + 1) * dw];
        for (x, taps) in h_taps.iter().enumerate() {
            let mut acc = 0.0f32;
            for &(idx, w) in taps {
                acc += row[idx] * w;
            }
            mrow[x] = acc;
        }
    }

    let v_taps = precompute_taps(sh, dh, radius, kernel);
    let mut dst = vec![0f32; dw * dh];
    for (y, taps) in v_taps.iter().enumerate() {
        let drow = &mut dst[y * dw..(y + 1) * dw];
        for &(sy, wy) in taps {
            let mrow = &mid[sy * dw..(sy + 1) * dw];
            for x in 0..dw {
                drow[x] += mrow[x] * wy;
            }
        }
    }
    dst
}

fn precompute_taps(
    src_len: usize,
    dst_len: usize,
    radius: isize,
    kernel: fn(f32) -> f32,
) -> Vec<Vec<(usize, f32)>> {
    let scale = src_len as f32 / dst_len as f32;
    let mut all = Vec::with_capacity(dst_len);
    for i in 0..dst_len {
        let center = (i as f32 + 0.5) * scale - 0.5;
        let left = center.floor() as isize - radius + 1;
        let right = center.floor() as isize + radius;
        let mut taps = Vec::with_capacity((2 * radius + 1) as usize);
        let mut wsum = 0.0f32;
        for s in left..=right {
            let w = kernel(center - s as f32);
            if w.abs() > 1e-6 {
                taps.push((s.clamp(0, (src_len - 1) as isize) as usize, w));
                wsum += w;
            }
        }
        if wsum.abs() > 1e-6 {
            for (_, w) in &mut taps {
                *w /= wsum;
            }
        }
        all.push(taps);
    }
    all
}

#[allow(clippy::too_many_arguments)]
#[must_use]
pub fn render_thermal_rgb(
    pixels: &[u16],
    sw: usize,
    sh: usize,
    dw: usize,
    dh: usize,
    min_raw: u16,
    max_raw: u16,
    palette: &Palette,
    rad: RadiometryParams,
    pipeline: &DisplayPipeline,
) -> Vec<u8> {
    let scalar = preprocess_display_scalar(pixels, sw, sh, pipeline);
    let lo = f32::from(min_raw);
    let span = f32::from(max_raw.saturating_sub(min_raw)).max(1.0);
    let mut norm: Vec<f32> = scalar.iter().map(|&v| ((v - lo) / span).clamp(0.0, 1.0)).collect();

    if pipeline.upscaler == UpscaleMethod::EdgePreserve {
        norm = bilateral_denoise(&norm, sw, sh, 0.08);
    }

    let mut upscaled = resample_scalar(&norm, sw, sh, dw, dh, pipeline.upscaler);
    if pipeline.sharpen_amount > 0.001 && pipeline.upscaler != UpscaleMethod::Nearest {
        upscaled = unsharp_mask(&upscaled, dw, dh, pipeline.sharpen_amount);
    }

    let iso_active = pipeline.isotherm.mode != IsothermMode::Off;
    let mut rgb = Vec::with_capacity(dw * dh * 3);
    for &t in &upscaled {
        let clamped = t.clamp(0.0, 1.0);
        if iso_active {
            let approx_raw = (lo + clamped * span).round().clamp(0.0, 65_535.0) as u16;
            let temp_c = raw_to_object_c(approx_raw, rad);
            if pipeline.isotherm.matches(temp_c) {
                rgb.extend_from_slice(&pipeline.isotherm.color);
                continue;
            }
        }
        rgb.extend_from_slice(&palette.sample(clamped));
    }
    rgb
}
