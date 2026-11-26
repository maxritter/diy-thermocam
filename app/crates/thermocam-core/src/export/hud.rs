//! High-resolution crisp vector/HUD compositor for HD/1440p/4K exports:
//! renders scale bar, temperature ticks, ROI markers, labels, and radiometry HUD
//! directly at target export resolution (never blurry upscaled text!).

use crate::palettes::Palette;
use crate::radiometry::TempUnit;
use crate::roi::{Roi, RoiGeometry, RoiStats};

/// Built-in crisp 5×7 ASCII bitmap glyph rows for high-res integer-scaled text rendering.
fn glyph_5x7(ch: char) -> [u8; 7] {
    match ch {
        '0' => [0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E],
        '1' => [0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E],
        '2' => [0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F],
        '3' => [0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E],
        '4' => [0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02],
        '5' => [0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E],
        '6' => [0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E],
        '7' => [0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08],
        '8' => [0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E],
        '9' => [0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C],
        '.' => [0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C],
        '-' => [0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00],
        '+' => [0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00],
        ':' => [0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00],
        '/' => [0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10],
        '%' => [0x19, 0x1A, 0x02, 0x04, 0x08, 0x0B, 0x13],
        '°' => [0x06, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00],
        'C' | 'c' => [0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E],
        'F' | 'f' => [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10],
        'K' | 'k' => [0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11],
        'A' | 'a' => [0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
        'B' | 'b' => [0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E],
        'D' | 'd' => [0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C],
        'E' | 'e' => [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F],
        'G' | 'g' => [0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F],
        'H' | 'h' => [0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
        'I' | 'i' => [0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E],
        'J' | 'j' => [0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C],
        'L' | 'l' => [0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F],
        'M' | 'm' => [0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11],
        'N' | 'n' => [0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11],
        'O' | 'o' => [0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
        'P' | 'p' => [0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10],
        'Q' | 'q' => [0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D],
        'R' | 'r' => [0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11],
        'S' | 's' => [0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E],
        'T' | 't' => [0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04],
        'U' | 'u' => [0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
        'V' | 'v' => [0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04],
        'W' | 'w' => [0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11],
        'X' | 'x' => [0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11],
        'Y' | 'y' => [0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04],
        'Z' | 'z' => [0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F],
        '#' => [0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A],
        '=' => [0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00],
        'Δ' => [0x04, 0x0A, 0x0A, 0x11, 0x11, 0x1F, 0x00],
        'ε' => [0x00, 0x0E, 0x10, 0x0C, 0x10, 0x0E, 0x00],
        _ => [0x00; 7],
    }
}

pub struct RgbCanvas<'a> {
    pub buf: &'a mut [u8],
    pub w: usize,
    pub h: usize,
}

impl<'a> RgbCanvas<'a> {
    #[inline]
    pub fn blend_px(&mut self, x: isize, y: isize, color: [u8; 3], alpha: f32) {
        if x < 0 || y < 0 || x >= self.w as isize || y >= self.h as isize {
            return;
        }
        let idx = (y as usize * self.w + x as usize) * 3;
        let inv = 1.0 - alpha;
        for (c, &col) in color.iter().enumerate() {
            self.buf[idx + c] = (f32::from(self.buf[idx + c]) * inv + f32::from(col) * alpha)
                .round()
                .clamp(0.0, 255.0) as u8;
        }
    }

    pub fn fill_rect(
        &mut self,
        x: isize,
        y: isize,
        rw: isize,
        rh: isize,
        color: [u8; 3],
        alpha: f32,
    ) {
        for py in y..(y + rh) {
            for px in x..(x + rw) {
                self.blend_px(px, py, color, alpha);
            }
        }
    }

    pub fn stroke_rect(
        &mut self,
        x: isize,
        y: isize,
        rw: isize,
        rh: isize,
        t: isize,
        color: [u8; 3],
    ) {
        self.fill_rect(x, y, rw, t, color, 1.0);
        self.fill_rect(x, y + rh - t, rw, t, color, 1.0);
        self.fill_rect(x, y, t, rh, color, 1.0);
        self.fill_rect(x + rw - t, y, t, rh, color, 1.0);
    }

    pub fn draw_line(
        &mut self,
        x0: isize,
        y0: isize,
        x1: isize,
        y1: isize,
        t: isize,
        color: [u8; 3],
    ) {
        let steps = (x1 - x0).abs().max((y1 - y0).abs()).max(1);
        for i in 0..=steps {
            let f = i as f32 / steps as f32;
            let px = (x0 as f32 + (x1 - x0) as f32 * f).round() as isize;
            let py = (y0 as f32 + (y1 - y0) as f32 * f).round() as isize;
            self.fill_rect(px - t / 2, py - t / 2, t, t, color, 1.0);
        }
    }

    pub fn draw_ellipse(
        &mut self,
        cx: isize,
        cy: isize,
        rx: isize,
        ry: isize,
        t: isize,
        color: [u8; 3],
    ) {
        let rx_f = rx.max(2) as f32;
        let ry_f = ry.max(2) as f32;
        let steps = ((rx_f.max(ry_f) * std::f32::consts::TAU).ceil() as usize).clamp(32, 1024);
        for i in 0..steps {
            let theta = (i as f32 / steps as f32) * std::f32::consts::TAU;
            let px = cx + (theta.cos() * rx_f).round() as isize;
            let py = cy + (theta.sin() * ry_f).round() as isize;
            self.fill_rect(px - t / 2, py - t / 2, t, t, color, 1.0);
        }
    }

    pub fn draw_text(&mut self, x: isize, y: isize, scale: isize, text: &str, color: [u8; 3]) {
        let s = scale.max(1);
        for (ci, ch) in text.chars().enumerate() {
            let rows = glyph_5x7(ch);
            let gx = x + (ci as isize) * 6 * s;
            for (ry, &bits) in rows.iter().enumerate() {
                for rx in 0..5isize {
                    if (bits & (1 << (4 - rx))) != 0 {
                        self.fill_rect(gx + rx * s, y + (ry as isize) * s, s, s, color, 1.0);
                    }
                }
            }
        }
    }

    pub fn draw_pill_label(&mut self, x: isize, y: isize, scale: isize, text: &str, fg: [u8; 3]) {
        let s = scale.max(1);
        let tw = (text.chars().count() as isize) * 6 * s;
        let pad = 3 * s;
        self.fill_rect(
            x - pad,
            y - pad,
            tw + pad * 2,
            7 * s + pad * 2,
            [10, 12, 16],
            0.78,
        );
        self.draw_text(x, y, s, text, fg);
    }
}

/// Composite crisp vector-grade colorbar, ticks, ROI markers, and radiometry HUD onto an exported RGB frame.
#[allow(clippy::too_many_arguments)]
pub fn composite_export_hud(
    canvas_rgb: &mut [u8],
    out_w: usize,
    out_h: usize,
    img_rect: (usize, usize, usize, usize),
    sensor_w: u16,
    sensor_h: u16,
    min_c: f32,
    max_c: f32,
    unit: TempUnit,
    emissivity: f32,
    palette: &Palette,
    rois: &[Roi],
    roi_stats: &[RoiStats],
    show_colorbar: bool,
    show_hud: bool,
) {
    let mut cv = RgbCanvas {
        buf: canvas_rgb,
        w: out_w,
        h: out_h,
    };
    let scale = ((out_h / 420) as isize).clamp(1, 5);
    let (ix, iy, iw, ih) = img_rect;

    // 1. Crisp ROI overlays scaled to exact target image coordinates
    let sx = iw as f32 / f32::from(sensor_w.max(1));
    let sy = ih as f32 / f32::from(sensor_h.max(1));
    let stroke = scale.max(2);
    for roi in rois.iter().filter(|r| r.visible) {
        let stat = roi_stats.iter().find(|s| s.id == roi.id);
        let label = match stat {
            Some(st) => format!("{} {}", roi.label, unit.format(st.mean_c, 1)),
            None => roi.label.clone(),
        };
        match &roi.geometry {
            RoiGeometry::Spot { x, y, .. } => {
                let cx = ix as isize + (f32::from(*x) * sx).round() as isize;
                let cy = iy as isize + (f32::from(*y) * sy).round() as isize;
                let arm = 6 * scale;
                cv.draw_line(cx - arm, cy, cx + arm, cy, stroke, [255, 255, 255]);
                cv.draw_line(cx, cy - arm, cx, cy + arm, stroke, [255, 255, 255]);
                cv.draw_pill_label(cx + arm + 4, cy - 4 * scale, scale, &label, [245, 248, 252]);
            }
            RoiGeometry::Box { x, y, w, h } => {
                let bx = ix as isize + (f32::from(*x) * sx).round() as isize;
                let by = iy as isize + (f32::from(*y) * sy).round() as isize;
                let bw = (f32::from(*w) * sx).round() as isize;
                let bh = (f32::from(*h) * sy).round() as isize;
                cv.stroke_rect(bx, by, bw.max(4), bh.max(4), stroke, [93, 163, 229]);
                cv.draw_pill_label(bx, by - 12 * scale, scale, &label, [245, 248, 252]);
            }
            RoiGeometry::Line { x0, y0, x1, y1 } | RoiGeometry::Delta { x0, y0, x1, y1 } => {
                let p0x = ix as isize + (f32::from(*x0) * sx).round() as isize;
                let p0y = iy as isize + (f32::from(*y0) * sy).round() as isize;
                let p1x = ix as isize + (f32::from(*x1) * sx).round() as isize;
                let p1y = iy as isize + (f32::from(*y1) * sy).round() as isize;
                cv.draw_line(p0x, p0y, p1x, p1y, stroke, [247, 178, 59]);
                cv.draw_pill_label(p0x + 4, p0y - 12 * scale, scale, &label, [247, 178, 59]);
            }
            RoiGeometry::Circle { cx, cy, radius } => {
                let px = ix as isize + (cx * sx).round() as isize;
                let py = iy as isize + (cy * sy).round() as isize;
                let rx = (radius * sx).round() as isize;
                let ry = (radius * sy).round() as isize;
                cv.draw_ellipse(px, py, rx, ry, stroke, [111, 211, 159]);
                cv.draw_pill_label(
                    px - rx,
                    py - ry - 12 * scale,
                    scale,
                    &label,
                    [245, 248, 252],
                );
            }
            RoiGeometry::Polygon { points } if points.len() >= 2 => {
                for pair in points.windows(2) {
                    let a0 = ix as isize + (pair[0][0] * sx).round() as isize;
                    let a1 = iy as isize + (pair[0][1] * sy).round() as isize;
                    let b0 = ix as isize + (pair[1][0] * sx).round() as isize;
                    let b1 = iy as isize + (pair[1][1] * sy).round() as isize;
                    cv.draw_line(a0, a1, b0, b1, stroke, [111, 211, 159]);
                }
                if points.len() >= 3 {
                    let first = points[0];
                    let last = points[points.len() - 1];
                    cv.draw_line(
                        ix as isize + (last[0] * sx).round() as isize,
                        iy as isize + (last[1] * sy).round() as isize,
                        ix as isize + (first[0] * sx).round() as isize,
                        iy as isize + (first[1] * sy).round() as isize,
                        stroke,
                        [111, 211, 159],
                    );
                }
                let p0 = points[0];
                let lx = ix as isize + (p0[0] * sx).round() as isize;
                let ly = iy as isize + (p0[1] * sy).round() as isize;
                cv.draw_pill_label(lx, ly - 12 * scale, scale, &label, [245, 248, 252]);
            }
            _ => {}
        }
    }

    // 2. Crisp right-hand colorbar with 5 ticks
    if show_colorbar {
        let bar_w = (out_w / 42).clamp(18, 56) as isize;
        let bar_h = ((out_h as f32) * 0.72) as isize;
        let bar_x = (out_w as isize) - bar_w - (out_w / 10).clamp(72, 220) as isize;
        let bar_y = ((out_h as isize) - bar_h) / 2;

        cv.stroke_rect(
            bar_x - 2,
            bar_y - 2,
            bar_w + 4,
            bar_h + 4,
            2,
            [210, 215, 222],
        );
        for row in 0..bar_h {
            let t = 1.0 - (row as f32 / (bar_h - 1).max(1) as f32);
            let c = palette.sample(t);
            cv.fill_rect(bar_x, bar_y + row, bar_w, 1, c, 1.0);
        }
        for tick in 0..=4 {
            let frac = tick as f32 / 4.0;
            let ty = bar_y + ((1.0 - frac) * bar_h as f32).round() as isize;
            let temp_c = min_c + frac * (max_c - min_c);
            cv.fill_rect(bar_x + bar_w + 2, ty, 6, 2, [225, 230, 236], 1.0);
            cv.draw_text(
                bar_x + bar_w + 12,
                ty - 3 * scale,
                scale,
                &unit.format(temp_c, 1),
                [237, 237, 237],
            );
        }
    }

    // 3. Bottom radiometric status bar
    if show_hud {
        let hud = format!(
            "DIY-THERMOCAM  MIN {}  MAX {}  ε={:.2}",
            unit.format(min_c, 1),
            unit.format(max_c, 1),
            emissivity
        );
        cv.draw_pill_label(
            18 * scale / 2,
            out_h as isize - 14 * scale,
            scale,
            &hud,
            [237, 237, 237],
        );
    }
}
