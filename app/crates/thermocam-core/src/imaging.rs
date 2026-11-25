//! Measurement-safe display pipeline: spatial filters, rotation/flip, cached separable Lanczos-3 /
//! Bicubic / Edge-Preserving Thermal Enhance (`rayon`-parallel, zero per-frame scratch allocs),
//! local-range-clamped sensor-space Unsharp Mask, temporal HEQ smoothing, and RGB/isotherm rendering.

use std::collections::HashMap;
use std::sync::{Arc, Mutex, OnceLock};

use rayon::prelude::*;
use serde::{Deserialize, Serialize};

use crate::palettes::Palette;
use crate::radiometry::{raw_to_object_c, RadiometryParams};
use crate::roi::{IsothermConfig, IsothermMode};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Default)]
#[serde(rename_all = "snake_case")]
pub enum SpatialFilter {
    #[default]
    None,
    Box3x3,
    Gaussian3x3,
    Median3x3,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Default)]
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
    #[serde(default)]
    pub histogram_eq: bool,
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
            histogram_eq: false,
            isotherm: IsothermConfig::default(),
        }
    }
}

/// Compact stack-allocated tap weights for one output coordinate (radius <= 3 -> at most 7 taps).
#[derive(Debug, Clone, Copy)]
struct TapRow {
    indices: [u32; 8],
    weights: [f32; 8],
    len: u8,
}

/// Precomputed separable resampling tap tables for `(sw, sh) -> (dw, dh)`.
#[derive(Debug)]
pub struct ResamplePlan {
    sw: usize,
    sh: usize,
    dw: usize,
    dh: usize,
    method: UpscaleMethod,
    h_taps: Vec<TapRow>,
    v_taps: Vec<TapRow>,
}

impl ResamplePlan {
    #[must_use]
    pub fn new(sw: usize, sh: usize, dw: usize, dh: usize, method: UpscaleMethod) -> Self {
        let (h_taps, v_taps) = match method {
            UpscaleMethod::Nearest => (Vec::new(), Vec::new()),
            UpscaleMethod::Bicubic => (
                precompute_taps(sw, dw, 2, catmull_rom),
                precompute_taps(sh, dh, 2, catmull_rom),
            ),
            UpscaleMethod::Lanczos3 | UpscaleMethod::EdgePreserve => (
                precompute_taps(sw, dw, 3, lanczos3),
                precompute_taps(sh, dh, 3, lanczos3),
            ),
        };
        Self { sw, sh, dw, dh, method, h_taps, v_taps }
    }

    #[must_use]
    pub fn cached(sw: usize, sh: usize, dw: usize, dh: usize, method: UpscaleMethod) -> Arc<Self> {
        type ResampleKey = (usize, usize, usize, usize, UpscaleMethod);
        type ResampleCache = Mutex<HashMap<ResampleKey, Arc<ResamplePlan>>>;
        static CACHE: OnceLock<ResampleCache> = OnceLock::new();
        let cache = CACHE.get_or_init(|| Mutex::new(HashMap::new()));
        let key = (sw, sh, dw, dh, method);
        if let Ok(guard) = cache.lock() {
            if let Some(plan) = guard.get(&key) {
                return Arc::clone(plan);
            }
        }
        let plan = Arc::new(Self::new(sw, sh, dw, dh, method));
        if let Ok(mut guard) = cache.lock() {
            if guard.len() >= 32 {
                guard.clear();
            }
            guard.insert(key, Arc::clone(&plan));
        }
        plan
    }

    pub fn resample_into(&self, src: &[f32], mid: &mut Vec<f32>, dst: &mut Vec<f32>) {
        let (sw, sh, dw, dh) = (self.sw, self.sh, self.dw, self.dh);
        if sw == dw && sh == dh {
            dst.clear();
            dst.extend_from_slice(src);
            return;
        }
        dst.resize(dw * dh, 0.0);
        if self.method == UpscaleMethod::Nearest {
            dst.par_chunks_mut(dw).enumerate().for_each(|(y, drow)| {
                let sy = ((y as f32 + 0.5) * sh as f32 / dh as f32)
                    .floor()
                    .clamp(0.0, (sh.saturating_sub(1)) as f32) as usize;
                let srow = &src[sy * sw..(sy + 1) * sw];
                for (x, slot) in drow.iter_mut().enumerate() {
                    let sx = ((x as f32 + 0.5) * sw as f32 / dw as f32)
                        .floor()
                        .clamp(0.0, (sw.saturating_sub(1)) as f32) as usize;
                    *slot = srow[sx];
                }
            });
            return;
        }

        mid.resize(dw * sh, 0.0);
        let h_taps = &self.h_taps;
        mid.par_chunks_mut(dw).enumerate().for_each(|(y, mrow)| {
            let srow = &src[y * sw..(y + 1) * sw];
            for (x, taps) in h_taps.iter().enumerate() {
                let mut acc = 0.0f32;
                let n = usize::from(taps.len);
                for k in 0..n {
                    acc += srow[taps.indices[k] as usize] * taps.weights[k];
                }
                mrow[x] = acc;
            }
        });

        let v_taps = &self.v_taps;
        let mid_ref: &[f32] = mid;
        dst.par_chunks_mut(dw).enumerate().for_each(|(y, drow)| {
            let taps = &v_taps[y];
            drow.fill(0.0);
            let n = usize::from(taps.len);
            for k in 0..n {
                let sy = taps.indices[k] as usize;
                let wy = taps.weights[k];
                let mrow = &mid_ref[sy * dw..(sy + 1) * dw];
                for (d, &m) in drow.iter_mut().zip(mrow.iter()) {
                    *d += m * wy;
                }
            }
        });
    }
}

/// EMA-smoothed clipped-CDF state so video Histogram Equalization never flickers across frames.
#[derive(Debug, Clone)]
pub struct TemporalHeqState {
    smoothed_cdf: Option<[f32; 256]>,
    alpha: f32,
}

impl Default for TemporalHeqState {
    fn default() -> Self {
        Self::new(0.25)
    }
}

impl TemporalHeqState {
    #[must_use]
    pub fn new(alpha: f32) -> Self {
        Self {
            smoothed_cdf: None,
            alpha: alpha.clamp(0.05, 1.0),
        }
    }

    pub fn reset(&mut self) {
        self.smoothed_cdf = None;
    }
}

/// Reusable scratch buffers for zero-allocation-per-frame video/sequence rendering.
#[derive(Debug, Default)]
pub struct RenderScratch {
    oriented: Vec<f32>,
    filtered: Vec<f32>,
    norm: Vec<f32>,
    processed: Vec<f32>,
    mid: Vec<f32>,
    upscaled: Vec<f32>,
    iso_upscaled: Vec<f32>,
    pub rgb: Vec<u8>,
    pub canvas: Vec<u8>,
    pub heq_state: TemporalHeqState,
    plan: Option<Arc<ResamplePlan>>,
}

impl RenderScratch {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    fn plan_for(&mut self, sw: usize, sh: usize, dw: usize, dh: usize, method: UpscaleMethod) -> Arc<ResamplePlan> {
        if let Some(p) = &self.plan {
            if p.sw == sw && p.sh == sh && p.dw == dw && p.dh == dh && p.method == method {
                return Arc::clone(p);
            }
        }
        let p = ResamplePlan::cached(sw, sh, dw, dh, method);
        self.plan = Some(Arc::clone(&p));
        p
    }
}

/// Display-only clipped-CDF histogram equalization (CLAHE-style plateau clip) with optional
/// temporal EMA smoothing across video frames (`temporal = Some(...)`).
pub fn apply_clipped_histogram_eq_into(
    norm: &[f32],
    out: &mut Vec<f32>,
    temporal: Option<&mut TemporalHeqState>,
) {
    out.resize(norm.len(), 0.0);
    if norm.is_empty() {
        return;
    }
    const BINS: usize = 256;
    let mut hist = [0u32; BINS];
    for &v in norm {
        let idx = (v.clamp(0.0, 1.0) * 255.0).round() as usize;
        hist[idx.min(BINS - 1)] += 1;
    }
    let clip_limit = ((norm.len() as f32 / BINS as f32) * 3.0).max(4.0) as u32;
    let mut excess: u32 = 0;
    for h in &mut hist {
        if *h > clip_limit {
            excess += *h - clip_limit;
            *h = clip_limit;
        }
    }
    let redist = excess / BINS as u32;
    for h in &mut hist {
        *h += redist;
    }
    let mut cdf = [0f32; BINS];
    let mut running = 0u32;
    for i in 0..BINS {
        running += hist[i];
        cdf[i] = running as f32;
    }
    let total = running.max(1) as f32;
    let cdf_min = cdf.iter().copied().find(|&c| c > 0.0).unwrap_or(0.0);
    let denom = (total - cdf_min).max(1.0);
    let mut lut = [0f32; BINS];
    for i in 0..BINS {
        lut[i] = ((cdf[i] - cdf_min) / denom).clamp(0.0, 1.0);
    }
    if let Some(state) = temporal {
        if let Some(prev) = &mut state.smoothed_cdf {
            let a = state.alpha;
            let inv = 1.0 - a;
            for i in 0..BINS {
                prev[i] = prev[i] * inv + lut[i] * a;
                lut[i] = prev[i];
            }
        } else {
            state.smoothed_cdf = Some(lut);
        }
    }
    for (dst, &v) in out.iter_mut().zip(norm.iter()) {
        let lin = v.clamp(0.0, 1.0);
        let idx = (lin * 255.0).round() as usize;
        let eq = lut[idx.min(BINS - 1)];
        *dst = (eq * 0.75 + lin * 0.25).clamp(0.0, 1.0);
    }
}

/// Display-only clipped-CDF histogram equalization (single-frame helper).
#[must_use]
pub fn apply_clipped_histogram_eq(norm: &[f32]) -> Vec<f32> {
    let mut out = Vec::with_capacity(norm.len());
    apply_clipped_histogram_eq_into(norm, &mut out, None);
    out
}

fn preprocess_into(
    pixels: &[u16],
    width: usize,
    height: usize,
    pipeline: &DisplayPipeline,
    oriented: &mut Vec<f32>,
    filtered: &mut Vec<f32>,
) {
    oriented.resize(width * height, 0.0);
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
        SpatialFilter::None => {
            filtered.clear();
            filtered.extend_from_slice(oriented);
        }
        SpatialFilter::Box3x3 => convolve_3x3_into(oriented, width, height, [1.0; 9], 9.0, filtered),
        SpatialFilter::Gaussian3x3 => convolve_3x3_into(
            oriented,
            width,
            height,
            [1.0, 2.0, 1.0, 2.0, 4.0, 2.0, 1.0, 2.0, 1.0],
            16.0,
            filtered,
        ),
        SpatialFilter::Median3x3 => median_3x3_into(oriented, width, height, filtered),
    }
}

#[must_use]
pub fn preprocess_display_scalar(
    pixels: &[u16],
    width: usize,
    height: usize,
    pipeline: &DisplayPipeline,
) -> Vec<f32> {
    let mut oriented = Vec::new();
    let mut filtered = Vec::new();
    preprocess_into(pixels, width, height, pipeline, &mut oriented, &mut filtered);
    filtered
}

fn convolve_3x3_into(src: &[f32], w: usize, h: usize, k: [f32; 9], div: f32, dst: &mut Vec<f32>) {
    dst.clear();
    dst.extend_from_slice(src);
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
}

fn median_3x3_into(src: &[f32], w: usize, h: usize, dst: &mut Vec<f32>) {
    dst.clear();
    dst.extend_from_slice(src);
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
}

fn bilateral_denoise_into(src: &[f32], w: usize, h: usize, sigma_range: f32, dst: &mut Vec<f32>) {
    let inv_2sr2 = 1.0 / (2.0 * sigma_range * sigma_range).max(1e-6);
    let spatial: [f32; 9] = [0.606, 0.778, 0.606, 0.778, 1.0, 0.778, 0.606, 0.778, 0.606];
    dst.resize(w * h, 0.0);
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
}

/// Scale-invariant, halo-free local-range-clamped Unsharp Mask at sensor resolution.
///
/// Running at sensor resolution guarantees identical visual edge boost across 720p, 1080p, 1440p,
/// and 4K (and matches the WebGL2 live viewport), while clamping to the 3x3 local `[min, max]`
/// neighborhood completely eliminates bright/dark overshoot halos.
fn unsharp_mask_local_clamped_into(src: &[f32], w: usize, h: usize, amount: f32, out: &mut Vec<f32>) {
    if amount <= 0.001 || w < 3 || h < 3 {
        out.clear();
        out.extend_from_slice(src);
        return;
    }
    let amt = amount.clamp(0.0, 2.0);
    out.resize(w * h, 0.0);
    let weights: [f32; 9] = [
        1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0,
        2.0 / 16.0, 4.0 / 16.0, 2.0 / 16.0,
        1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0,
    ];
    for y in 0..h {
        for x in 0..w {
            let orig = src[y * w + x];
            let mut blur = 0.0f32;
            let mut lo = f32::INFINITY;
            let mut hi = f32::NEG_INFINITY;
            let mut ki = 0;
            for dy in -1isize..=1 {
                let ny = (y as isize + dy).clamp(0, (h - 1) as isize) as usize;
                for dx in -1isize..=1 {
                    let nx = (x as isize + dx).clamp(0, (w - 1) as isize) as usize;
                    let v = src[ny * w + nx];
                    blur += v * weights[ki];
                    lo = lo.min(v);
                    hi = hi.max(v);
                    ki += 1;
                }
            }
            let sharpened = orig + amt * (orig - blur);
            out[y * w + x] = sharpened.clamp(lo, hi).clamp(0.0, 1.0);
        }
    }
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

fn precompute_taps(
    src_len: usize,
    dst_len: usize,
    radius: isize,
    kernel: fn(f32) -> f32,
) -> Vec<TapRow> {
    let scale = src_len as f32 / dst_len as f32;
    let mut all = Vec::with_capacity(dst_len);
    for i in 0..dst_len {
        let center = (i as f32 + 0.5) * scale - 0.5;
        let left = center.floor() as isize - radius + 1;
        let right = center.floor() as isize + radius;
        let mut row = TapRow {
            indices: [0; 8],
            weights: [0.0; 8],
            len: 0,
        };
        let mut wsum = 0.0f32;
        for s in left..=right {
            let w = kernel(center - s as f32);
            if w.abs() > 1e-6 && usize::from(row.len) < 8 {
                let k = usize::from(row.len);
                row.indices[k] = s.clamp(0, (src_len.saturating_sub(1)) as isize) as u32;
                row.weights[k] = w;
                row.len += 1;
                wsum += w;
            }
        }
        if wsum.abs() > 1e-6 {
            for k in 0..usize::from(row.len) {
                row.weights[k] /= wsum;
            }
        }
        all.push(row);
    }
    all
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
    let plan = ResamplePlan::cached(sw, sh, dw, dh, method);
    let mut mid = Vec::new();
    let mut dst = Vec::new();
    plan.resample_into(src, &mut mid, &mut dst);
    dst
}

/// Render thermal RGB directly into `scratch.rgb` using cached tap plans, reusable buffers,
/// `rayon` parallel rows, optional temporal HEQ smoothing (`use_temporal_heq`), and local-clamped USM.
#[allow(clippy::too_many_arguments)]
pub fn render_thermal_rgb_with_scratch(
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
    use_temporal_heq: bool,
    scratch: &mut RenderScratch,
) {
    preprocess_into(
        pixels,
        sw,
        sh,
        pipeline,
        &mut scratch.oriented,
        &mut scratch.filtered,
    );
    let lo = f32::from(min_raw);
    let span = f32::from(max_raw.saturating_sub(min_raw)).max(1.0);
    scratch.norm.resize(sw * sh, 0.0);
    for (dst, &v) in scratch.norm.iter_mut().zip(scratch.filtered.iter()) {
        *dst = ((v - lo) / span).clamp(0.0, 1.0);
    }

    let plan = scratch.plan_for(sw, sh, dw, dh, pipeline.upscaler);
    let iso_active = pipeline.isotherm.mode != IsothermMode::Off;
    let need_linear_iso = iso_active
        && (pipeline.histogram_eq
            || pipeline.sharpen_amount > 0.001
            || pipeline.upscaler == UpscaleMethod::EdgePreserve);
    if need_linear_iso {
        plan.resample_into(&scratch.norm, &mut scratch.mid, &mut scratch.iso_upscaled);
    }

    if pipeline.histogram_eq {
        let temporal = if use_temporal_heq {
            Some(&mut scratch.heq_state)
        } else {
            None
        };
        apply_clipped_histogram_eq_into(&scratch.norm, &mut scratch.processed, temporal);
        std::mem::swap(&mut scratch.norm, &mut scratch.processed);
    }

    if pipeline.upscaler == UpscaleMethod::EdgePreserve {
        bilateral_denoise_into(&scratch.norm, sw, sh, 0.08, &mut scratch.processed);
        std::mem::swap(&mut scratch.norm, &mut scratch.processed);
    }

    if pipeline.sharpen_amount > 0.001 && pipeline.upscaler != UpscaleMethod::Nearest {
        unsharp_mask_local_clamped_into(
            &scratch.norm,
            sw,
            sh,
            pipeline.sharpen_amount,
            &mut scratch.processed,
        );
        std::mem::swap(&mut scratch.norm, &mut scratch.processed);
    }

    plan.resample_into(&scratch.norm, &mut scratch.mid, &mut scratch.upscaled);

    scratch.rgb.resize(dw * dh * 3, 0);
    let upscaled: &[f32] = &scratch.upscaled;
    let iso_lin: Option<&[f32]> = if need_linear_iso {
        Some(&scratch.iso_upscaled)
    } else {
        None
    };

    scratch
        .rgb
        .par_chunks_mut(dw * 3)
        .enumerate()
        .for_each(|(y, out_row)| {
            let row_off = y * dw;
            for x in 0..dw {
                let idx = row_off + x;
                let clamped = upscaled[idx].clamp(0.0, 1.0);
                let col = if iso_active {
                    let iso_t = iso_lin.map_or(clamped, |lin| lin[idx].clamp(0.0, 1.0));
                    let approx_raw = (lo + iso_t * span).round().clamp(0.0, 65_535.0) as u16;
                    let temp_c = raw_to_object_c(approx_raw, rad);
                    if pipeline.isotherm.matches(temp_c) {
                        pipeline.isotherm.color
                    } else {
                        palette.sample(clamped)
                    }
                } else {
                    palette.sample(clamped)
                };
                let o = x * 3;
                out_row[o] = col[0];
                out_row[o + 1] = col[1];
                out_row[o + 2] = col[2];
            }
        });
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
    let mut scratch = RenderScratch::new();
    render_thermal_rgb_with_scratch(
        pixels,
        sw,
        sh,
        dw,
        dh,
        min_raw,
        max_raw,
        palette,
        rad,
        pipeline,
        false,
        &mut scratch,
    );
    scratch.rgb
}
