//! Measurements, ROIs (1×1 & 3×3 Spot with ShowRaw, Box, Circle, Polygon/Lasso, Line Profile,
//! 2-point ΔT), ROI-driven contrast auto-scaling, N×M grid, 256-bin histogram, Isotherms, and TempSwitch alarms.

use serde::{Deserialize, Serialize};

use crate::radiometry::{raw_to_object_c, RadiometryParams};

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum RoiGeometry {
    Spot {
        x: u16,
        y: u16,
        kernel: u8,
        show_raw: bool,
    },
    Box {
        x: u16,
        y: u16,
        w: u16,
        h: u16,
    },
    Circle {
        cx: f32,
        cy: f32,
        radius: f32,
    },
    Polygon {
        points: Vec<[f32; 2]>,
    },
    Line {
        x0: u16,
        y0: u16,
        x1: u16,
        y1: u16,
    },
    Delta {
        x0: u16,
        y0: u16,
        x1: u16,
        y1: u16,
    },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Roi {
    pub id: String,
    pub label: String,
    pub geometry: RoiGeometry,
    pub emissivity_override: Option<f32>,
    pub reflected_temp_override_c: Option<f32>,
    pub visible: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RoiStats {
    pub id: String,
    pub label: String,
    pub pixel_count: usize,
    pub min_raw: u16,
    pub max_raw: u16,
    pub mean_raw: f32,
    pub min_c: f32,
    pub max_c: f32,
    pub mean_c: f32,
    pub std_dev_c: f32,
    pub delta_c: Option<f32>,
    pub min_xy: [u16; 2],
    pub max_xy: [u16; 2],
    pub effective_emissivity: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LineSample {
    pub dist_px: f32,
    pub x: u16,
    pub y: u16,
    pub raw: u16,
    pub temp_c: f32,
}

#[must_use]
pub fn evaluate_roi(
    roi: &Roi,
    pixels: &[u16],
    width: u16,
    height: u16,
    global: RadiometryParams,
) -> RoiStats {
    let params = RadiometryParams {
        emissivity: roi.emissivity_override.unwrap_or(global.emissivity),
        reflected_temp_c: roi
            .reflected_temp_override_c
            .unwrap_or(global.reflected_temp_c),
        ..global
    };
    evaluate_roi_with(roi, pixels, width, height, params.emissivity, |raw| {
        raw_to_object_c(raw, params)
    })
}

/// [`evaluate_roi`] with the raw-to-°C conversion supplied by the caller (the single-correction
/// reading for recordings). `effective_emissivity` is reported as given.
#[must_use]
pub fn evaluate_roi_with<F: Fn(u16) -> f32>(
    roi: &Roi,
    pixels: &[u16],
    width: u16,
    height: u16,
    effective_emissivity: f32,
    to_c: F,
) -> RoiStats {
    let coords = collect_roi_coords(&roi.geometry, width, height);
    let w = usize::from(width);
    let mut min_raw = u16::MAX;
    let mut max_raw = 0u16;
    let mut min_xy = [0u16, 0u16];
    let mut max_xy = [0u16, 0u16];
    let mut sum_raw = 0f64;
    let mut temps = Vec::with_capacity(coords.len());

    for &(x, y) in &coords {
        let raw = pixels[usize::from(y) * w + usize::from(x)];
        if raw < min_raw {
            min_raw = raw;
            min_xy = [x, y];
        }
        if raw > max_raw {
            max_raw = raw;
            max_xy = [x, y];
        }
        sum_raw += f64::from(raw);
        temps.push(to_c(raw));
    }

    let n = coords.len();
    let mean_raw = if n > 0 {
        (sum_raw / n as f64) as f32
    } else {
        0.0
    };
    let mean_c = if n > 0 {
        (temps.iter().map(|&t| f64::from(t)).sum::<f64>() / n as f64) as f32
    } else {
        0.0
    };
    let var = if n > 1 {
        temps
            .iter()
            .map(|&t| {
                let d = f64::from(t - mean_c);
                d * d
            })
            .sum::<f64>()
            / n as f64
    } else {
        0.0
    };
    let min_c = if n > 0 { to_c(min_raw) } else { 0.0 };
    let max_c = if n > 0 { to_c(max_raw) } else { 0.0 };
    let delta_c = match roi.geometry {
        RoiGeometry::Delta { x0, y0, x1, y1 } => {
            let p0 = pixels[usize::from(y0.min(height - 1)) * w + usize::from(x0.min(width - 1))];
            let p1 = pixels[usize::from(y1.min(height - 1)) * w + usize::from(x1.min(width - 1))];
            Some(to_c(p1) - to_c(p0))
        }
        _ => None,
    };

    RoiStats {
        id: roi.id.clone(),
        label: roi.label.clone(),
        pixel_count: n,
        min_raw: if n > 0 { min_raw } else { 0 },
        max_raw,
        mean_raw,
        min_c,
        max_c,
        mean_c,
        std_dev_c: var.sqrt() as f32,
        delta_c,
        min_xy,
        max_xy,
        effective_emissivity,
    }
}

#[must_use]
pub fn roi_contrast_range(
    roi: &Roi,
    pixels: &[u16],
    width: u16,
    height: u16,
    pad_fraction: f32,
) -> Option<(u16, u16)> {
    let coords = collect_roi_coords(&roi.geometry, width, height);
    if coords.is_empty() {
        return None;
    }
    let w = usize::from(width);
    let mut lo = u16::MAX;
    let mut hi = 0u16;
    for (x, y) in coords {
        let v = pixels[usize::from(y) * w + usize::from(x)];
        lo = lo.min(v);
        hi = hi.max(v);
    }
    let span = f32::from(hi.saturating_sub(lo)).max(1.0);
    let pad = (span * pad_fraction.clamp(0.0, 0.5)).round() as u16;
    Some((lo.saturating_sub(pad), hi.saturating_add(pad).max(lo + 1)))
}

#[allow(clippy::too_many_arguments)]
#[must_use]
pub fn sample_line_profile(
    pixels: &[u16],
    width: u16,
    height: u16,
    x0: u16,
    y0: u16,
    x1: u16,
    y1: u16,
    params: RadiometryParams,
) -> Vec<LineSample> {
    let dx = f32::from(x1) - f32::from(x0);
    let dy = f32::from(y1) - f32::from(y0);
    let total_dist = dx.hypot(dy);
    let steps = (total_dist.ceil() as usize).max(1);
    let w = usize::from(width);
    (0..=steps)
        .map(|i| {
            let t = i as f32 / steps as f32;
            let x = (f32::from(x0) + dx * t)
                .round()
                .clamp(0.0, f32::from(width - 1)) as u16;
            let y = (f32::from(y0) + dy * t)
                .round()
                .clamp(0.0, f32::from(height - 1)) as u16;
            let raw = pixels[usize::from(y) * w + usize::from(x)];
            LineSample {
                dist_px: total_dist * t,
                x,
                y,
                raw,
                temp_c: raw_to_object_c(raw, params),
            }
        })
        .collect()
}

#[must_use]
pub fn compute_temp_grid(
    pixels: &[u16],
    width: u16,
    height: u16,
    cols: u16,
    rows: u16,
    params: RadiometryParams,
) -> Vec<Vec<f32>> {
    let cols = cols.clamp(1, 16);
    let rows = rows.clamp(1, 16);
    let w = usize::from(width);
    (0..rows)
        .map(|r| {
            let y = ((u32::from(r) * 2 + 1) * u32::from(height) / (u32::from(rows) * 2))
                .min(u32::from(height - 1)) as usize;
            (0..cols)
                .map(|c| {
                    let x = ((u32::from(c) * 2 + 1) * u32::from(width) / (u32::from(cols) * 2))
                        .min(u32::from(width - 1)) as usize;
                    raw_to_object_c(pixels[y * w + x], params)
                })
                .collect()
        })
        .collect()
}

#[must_use]
pub fn compute_histogram_256(pixels: &[u16], min_raw: u16, max_raw: u16) -> [u32; 256] {
    let mut bins = [0u32; 256];
    let span = f32::from(max_raw.saturating_sub(min_raw)).max(1.0);
    for &px in pixels {
        let norm = ((f32::from(px) - f32::from(min_raw)) / span).clamp(0.0, 1.0);
        bins[((norm * 255.0).round() as usize).min(255)] += 1;
    }
    bins
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum IsothermMode {
    Off,
    Above,
    Below,
    Interval,
}

#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct IsothermConfig {
    pub mode: IsothermMode,
    pub low_c: f32,
    pub high_c: f32,
    pub color: [u8; 3],
}

impl Default for IsothermConfig {
    fn default() -> Self {
        Self {
            mode: IsothermMode::Off,
            low_c: 40.0,
            high_c: 60.0,
            color: [0, 230, 160],
        }
    }
}

impl IsothermConfig {
    #[inline]
    #[must_use]
    pub fn matches(&self, temp_c: f32) -> bool {
        match self.mode {
            IsothermMode::Off => false,
            IsothermMode::Above => temp_c >= self.high_c,
            IsothermMode::Below => temp_c <= self.low_c,
            IsothermMode::Interval => temp_c >= self.low_c && temp_c <= self.high_c,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum AlarmCondition {
    Above,
    Below,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum AlarmAction {
    Notify,
    SaveDat,
    StartRecording,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TempSwitchRule {
    pub id: String,
    pub name: String,
    pub enabled: bool,
    pub target_roi_id: Option<String>,
    pub condition: AlarmCondition,
    pub threshold_c: f32,
    pub cooldown_ms: u64,
    pub action: AlarmAction,
    #[serde(default)]
    pub last_triggered_ms: Option<u64>,
}

impl TempSwitchRule {
    pub fn evaluate(&mut self, measured_c: f32, now_ms: u64) -> bool {
        if !self.enabled {
            return false;
        }
        let hit = match self.condition {
            AlarmCondition::Above => measured_c >= self.threshold_c,
            AlarmCondition::Below => measured_c <= self.threshold_c,
        };
        if !hit {
            return false;
        }
        if let Some(prev) = self.last_triggered_ms {
            if now_ms.saturating_sub(prev) < self.cooldown_ms {
                return false;
            }
        }
        self.last_triggered_ms = Some(now_ms);
        true
    }
}

fn collect_roi_coords(geom: &RoiGeometry, width: u16, height: u16) -> Vec<(u16, u16)> {
    if width == 0 || height == 0 {
        return Vec::new();
    }
    let max_x = width - 1;
    let max_y = height - 1;
    match geom {
        RoiGeometry::Spot { x, y, kernel, .. } => {
            let cx = (*x).min(max_x);
            let cy = (*y).min(max_y);
            if *kernel >= 3 {
                let mut pts = Vec::with_capacity(9);
                for dy in -1i32..=1 {
                    for dx in -1i32..=1 {
                        pts.push((
                            (i32::from(cx) + dx).clamp(0, i32::from(max_x)) as u16,
                            (i32::from(cy) + dy).clamp(0, i32::from(max_y)) as u16,
                        ));
                    }
                }
                pts
            } else {
                vec![(cx, cy)]
            }
        }
        RoiGeometry::Box { x, y, w, h } => {
            let x0 = (*x).min(max_x);
            let y0 = (*y).min(max_y);
            let x1 = x0.saturating_add((*w).max(1) - 1).min(max_x);
            let y1 = y0.saturating_add((*h).max(1) - 1).min(max_y);
            let mut pts = Vec::new();
            for py in y0..=y1 {
                for px in x0..=x1 {
                    pts.push((px, py));
                }
            }
            pts
        }
        RoiGeometry::Circle { cx, cy, radius } => {
            let r2 = radius * radius;
            let x0 = (cx - radius).floor().clamp(0.0, f32::from(max_x)) as u16;
            let x1 = (cx + radius).ceil().clamp(0.0, f32::from(max_x)) as u16;
            let y0 = (cy - radius).floor().clamp(0.0, f32::from(max_y)) as u16;
            let y1 = (cy + radius).ceil().clamp(0.0, f32::from(max_y)) as u16;
            let mut pts = Vec::new();
            for py in y0..=y1 {
                let dy = f32::from(py) - cy;
                for px in x0..=x1 {
                    let dx = f32::from(px) - cx;
                    if dx * dx + dy * dy <= r2 {
                        pts.push((px, py));
                    }
                }
            }
            pts
        }
        RoiGeometry::Polygon { points } => {
            if points.len() < 3 {
                return Vec::new();
            }
            let min_x = points
                .iter()
                .map(|p| p[0])
                .fold(f32::INFINITY, f32::min)
                .floor()
                .clamp(0.0, f32::from(max_x)) as u16;
            let max_px = points
                .iter()
                .map(|p| p[0])
                .fold(f32::NEG_INFINITY, f32::max)
                .ceil()
                .clamp(0.0, f32::from(max_x)) as u16;
            let min_y = points
                .iter()
                .map(|p| p[1])
                .fold(f32::INFINITY, f32::min)
                .floor()
                .clamp(0.0, f32::from(max_y)) as u16;
            let max_py = points
                .iter()
                .map(|p| p[1])
                .fold(f32::NEG_INFINITY, f32::max)
                .ceil()
                .clamp(0.0, f32::from(max_y)) as u16;
            let mut pts = Vec::new();
            for py in min_y..=max_py {
                for px in min_x..=max_px {
                    if point_in_polygon(f32::from(px) + 0.5, f32::from(py) + 0.5, points) {
                        pts.push((px, py));
                    }
                }
            }
            pts
        }
        RoiGeometry::Line { x0, y0, x1, y1 } => {
            let dx = f32::from(*x1) - f32::from(*x0);
            let dy = f32::from(*y1) - f32::from(*y0);
            let steps = (dx.hypot(dy).ceil() as usize).max(1);
            (0..=steps)
                .map(|i| {
                    let t = i as f32 / steps as f32;
                    (
                        (f32::from(*x0) + dx * t)
                            .round()
                            .clamp(0.0, f32::from(max_x)) as u16,
                        (f32::from(*y0) + dy * t)
                            .round()
                            .clamp(0.0, f32::from(max_y)) as u16,
                    )
                })
                .collect()
        }
        RoiGeometry::Delta { x0, y0, x1, y1 } => {
            vec![
                ((*x0).min(max_x), (*y0).min(max_y)),
                ((*x1).min(max_x), (*y1).min(max_y)),
            ]
        }
    }
}

fn point_in_polygon(px: f32, py: f32, poly: &[[f32; 2]]) -> bool {
    let mut inside = false;
    let mut j = poly.len() - 1;
    for i in 0..poly.len() {
        let (xi, yi) = (poly[i][0], poly[i][1]);
        let (xj, yj) = (poly[j][0], poly[j][1]);
        let denom = (yj - yi).abs().max(1e-6) * if yj >= yi { 1.0 } else { -1.0 };
        if ((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / denom + xi) {
            inside = !inside;
        }
        j = i;
    }
    inside
}
