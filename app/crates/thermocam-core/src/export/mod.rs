//! Pro-grade HD/1440p/4K export pipeline: PNG, JPEG, zero-FFmpeg H.264 MP4 (`openh264` + `mp4`),
//! animated GIF, 16-bit radiometric Kelvin TIFF, temperature matrix CSV, and HTML/Markdown reports.

pub mod hud;

use std::fmt::Write as _;
use std::io::{Cursor, Seek, Write};

use base64::Engine as _;
use image::codecs::jpeg::JpegEncoder;
use image::codecs::png::PngEncoder;
use image::{ExtendedColorType, ImageEncoder};
use openh264::formats::YUVBuffer;
use serde::{Deserialize, Serialize};
use thiserror::Error;

use crate::imaging::{render_thermal_rgb_with_scratch, DisplayPipeline, RenderScratch};
use crate::palettes::Palette;
use crate::radiometry::{raw_to_object_c, RadiometryParams, TempUnit};
use crate::roi::{evaluate_roi, sample_line_profile, LineSample, Roi, RoiGeometry, RoiStats};

#[derive(Debug, Error)]
pub enum ExportError {
    #[error("image encoding error: {0}")]
    Image(String),
    #[error("TIFF encoding error: {0}")]
    Tiff(String),
    #[error("GIF encoding error: {0}")]
    Gif(String),
    #[error("H.264/MP4 encoding error: {0}")]
    Video(String),
    #[error("no frames provided for video export")]
    EmptySequence,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ExportResolution {
    Native,
    Hd720p,
    FullHd1080p,
    Qhd1440p,
    Uhd4k,
    Custom { width: u32, height: u32 },
}

impl ExportResolution {
    #[must_use]
    pub fn dimensions(self, sensor_w: u16, sensor_h: u16) -> (u32, u32) {
        match self {
            Self::Native => (u32::from(sensor_w) * 4, u32::from(sensor_h) * 4),
            Self::Hd720p => (1280, 720),
            Self::FullHd1080p => (1920, 1080),
            Self::Qhd1440p => (2560, 1440),
            Self::Uhd4k => (3840, 2160),
            Self::Custom { width, height } => ((width.max(64) & !1), (height.max(64) & !1)),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExportOptions {
    pub resolution: ExportResolution,
    pub show_colorbar: bool,
    pub show_hud: bool,
    pub unit: TempUnit,
    pub jpeg_quality: u8,
}

impl Default for ExportOptions {
    fn default() -> Self {
        Self {
            resolution: ExportResolution::FullHd1080p,
            show_colorbar: true,
            show_hud: true,
            unit: TempUnit::Celsius,
            jpeg_quality: 94,
        }
    }
}

/// Render a target-resolution RGB frame into `scratch.canvas` with zero per-frame heap churn.
#[allow(clippy::too_many_arguments)]
pub fn render_export_canvas_with_scratch(
    pixels: &[u16],
    sensor_w: u16,
    sensor_h: u16,
    min_raw: u16,
    max_raw: u16,
    palette: &Palette,
    rad: RadiometryParams,
    pipeline: &DisplayPipeline,
    rois: &[Roi],
    opts: &ExportOptions,
    use_temporal_heq: bool,
    scratch: &mut RenderScratch,
) -> (u32, u32) {
    let (out_w, out_h) = opts.resolution.dimensions(sensor_w, sensor_h);
    let ow = out_w as usize;
    let oh = out_h as usize;

    let right_gutter = if opts.show_colorbar {
        (ow / 7).clamp(110, 380)
    } else {
        0
    };
    let avail_w = ow.saturating_sub(right_gutter).max(16);
    let avail_h = oh.max(16);
    let aspect = f64::from(sensor_w) / f64::from(sensor_h.max(1));
    let (img_w, img_h) = if (avail_w as f64 / avail_h as f64) > aspect {
        let h = avail_h;
        let w = ((h as f64 * aspect).round() as usize).min(avail_w);
        (w.max(2), h.max(2))
    } else {
        let w = avail_w;
        let h = ((w as f64 / aspect).round() as usize).min(avail_h);
        (w.max(2), h.max(2))
    };
    let ix = (avail_w - img_w) / 2;
    let iy = (avail_h - img_h) / 2;

    render_thermal_rgb_with_scratch(
        pixels,
        usize::from(sensor_w),
        usize::from(sensor_h),
        img_w,
        img_h,
        min_raw,
        max_raw,
        palette,
        rad,
        pipeline,
        use_temporal_heq,
        scratch,
    );

    scratch.canvas.resize(ow * oh * 3, 14);
    scratch.canvas.fill(14);
    for row in 0..img_h {
        let src_off = row * img_w * 3;
        let dst_off = ((iy + row) * ow + ix) * 3;
        scratch.canvas[dst_off..dst_off + img_w * 3]
            .copy_from_slice(&scratch.rgb[src_off..src_off + img_w * 3]);
    }

    let stats: Vec<RoiStats> = rois
        .iter()
        .map(|r| evaluate_roi(r, pixels, sensor_w, sensor_h, rad))
        .collect();
    let min_c = raw_to_object_c(min_raw, rad);
    let max_c = raw_to_object_c(max_raw, rad);

    hud::composite_export_hud(
        &mut scratch.canvas,
        ow,
        oh,
        (ix, iy, img_w, img_h),
        sensor_w,
        sensor_h,
        min_c,
        max_c,
        opts.unit,
        rad.emissivity,
        palette,
        rois,
        &stats,
        opts.show_colorbar,
        opts.show_hud,
    );
    (out_w, out_h)
}

/// Render a full target-resolution RGB frame with letterboxed aspect-correct thermal image
/// and crisp vector-composited scale bar, ROI overlays, and HUD.
#[allow(clippy::too_many_arguments)]
#[must_use]
pub fn render_export_canvas(
    pixels: &[u16],
    sensor_w: u16,
    sensor_h: u16,
    min_raw: u16,
    max_raw: u16,
    palette: &Palette,
    rad: RadiometryParams,
    pipeline: &DisplayPipeline,
    rois: &[Roi],
    opts: &ExportOptions,
) -> (u32, u32, Vec<u8>) {
    let mut scratch = RenderScratch::new();
    let (out_w, out_h) = render_export_canvas_with_scratch(
        pixels,
        sensor_w,
        sensor_h,
        min_raw,
        max_raw,
        palette,
        rad,
        pipeline,
        rois,
        opts,
        false,
        &mut scratch,
    );
    (out_w, out_h, scratch.canvas)
}

pub fn encode_png(width: u32, height: u32, rgb: &[u8]) -> Result<Vec<u8>, ExportError> {
    let mut out = Vec::new();
    let enc = PngEncoder::new(&mut out);
    enc.write_image(rgb, width, height, ExtendedColorType::Rgb8)
        .map_err(|e| ExportError::Image(e.to_string()))?;
    Ok(out)
}

pub fn encode_jpeg(
    width: u32,
    height: u32,
    rgb: &[u8],
    quality: u8,
) -> Result<Vec<u8>, ExportError> {
    let mut out = Vec::new();
    let mut enc = JpegEncoder::new_with_quality(&mut out, quality.clamp(50, 100));
    enc.encode(rgb, width, height, ExtendedColorType::Rgb8)
        .map_err(|e| ExportError::Image(e.to_string()))?;
    Ok(out)
}

/// Export 16-bit radiometric TIFF storing centi-Kelvin (`raw_k100 = round((T_obj_C + 273.15) * 100)`).
pub fn encode_radiometric_tiff_16(
    pixels: &[u16],
    width: u16,
    height: u16,
    rad: RadiometryParams,
) -> Result<Vec<u8>, ExportError> {
    let ck: Vec<u16> = pixels
        .iter()
        .map(|&raw| {
            let t_c = raw_to_object_c(raw, rad);
            ((f64::from(t_c) + 273.15) * 100.0)
                .round()
                .clamp(0.0, 65_535.0) as u16
        })
        .collect();
    let mut cursor = Cursor::new(Vec::new());
    let mut enc = tiff::encoder::TiffEncoder::new(&mut cursor)
        .map_err(|e| ExportError::Tiff(e.to_string()))?;
    enc.write_image::<tiff::encoder::colortype::Gray16>(u32::from(width), u32::from(height), &ck)
        .map_err(|e| ExportError::Tiff(e.to_string()))?;
    Ok(cursor.into_inner())
}

/// Export full temperature matrix as CSV in the requested `TempUnit`.
#[must_use]
pub fn encode_temperature_csv(
    pixels: &[u16],
    width: u16,
    height: u16,
    rad: RadiometryParams,
    unit: TempUnit,
) -> String {
    let w = usize::from(width);
    let h = usize::from(height);
    let mut out = String::with_capacity(w * h * 7);
    let _ = writeln!(
        out,
        "# DIY-Thermocam Temperature Matrix ({}) emissivity={:.3} reflected_c={:.2}",
        unit.symbol(),
        rad.emissivity,
        rad.reflected_temp_c
    );
    for y in 0..h {
        for x in 0..w {
            if x > 0 {
                out.push(',');
            }
            let val = unit.from_celsius(raw_to_object_c(pixels[y * w + x], rad));
            let _ = write!(out, "{val:.2}");
        }
        out.push('\n');
    }
    out
}

/// Derive true capture FPS from microsecond frame timestamps (falls back to 8.78 Hz Lepton rate).
#[must_use]
pub fn derive_sequence_fps(timestamps_us: &[u64]) -> f64 {
    if timestamps_us.len() < 2 {
        return 8.78;
    }
    let first = timestamps_us[0];
    let last = *timestamps_us.last().unwrap_or(&first);
    if last <= first {
        return 8.78;
    }
    let span_s = (last - first) as f64 / 1_000_000.0;
    if span_s <= 1e-4 {
        return 8.78;
    }
    let fps = (timestamps_us.len() - 1) as f64 / span_s;
    fps.clamp(0.5, 60.0)
}

/// Constant-RAM streaming animated GIF encoder: renders and writes one frame at a time with zero clone.
pub fn encode_gif_streaming<W, F>(
    writer: W,
    width: u16,
    height: u16,
    frame_count: usize,
    fps: f64,
    mut with_frame_rgb: F,
) -> Result<(), ExportError>
where
    W: Write,
    F: FnMut(usize, &mut dyn FnMut(&[u8]) -> Result<(), ExportError>) -> Result<(), ExportError>,
{
    if frame_count == 0 {
        return Err(ExportError::EmptySequence);
    }
    let delay_cs = ((100.0 / fps.clamp(0.5, 30.0)).round() as u16).max(2);
    let mut enc = gif::Encoder::new(writer, width, height, &[])
        .map_err(|e| ExportError::Gif(e.to_string()))?;
    enc.set_repeat(gif::Repeat::Infinite)
        .map_err(|e| ExportError::Gif(e.to_string()))?;
    for idx in 0..frame_count {
        with_frame_rgb(idx, &mut |rgb: &[u8]| {
            let mut frame = gif::Frame::from_rgb_speed(width, height, rgb, 15);
            frame.delay = delay_cs;
            enc.write_frame(&frame)
                .map_err(|e| ExportError::Gif(e.to_string()))
        })?;
    }
    Ok(())
}

/// Encode animated GIF from RGB8 frames.
pub fn encode_gif(
    width: u16,
    height: u16,
    frames_rgb: &[Vec<u8>],
    fps: f32,
) -> Result<Vec<u8>, ExportError> {
    let mut out = Vec::new();
    encode_gif_streaming(
        &mut out,
        width,
        height,
        frames_rgb.len(),
        f64::from(fps),
        |idx, emit| emit(&frames_rgb[idx]),
    )?;
    Ok(out)
}

/// Constant-RAM streaming H.264 MP4 encoder (`openh264` -> AVCC -> `mp4::Mp4Writer`).
///
/// Renders, converts, encodes, and flushes **one frame at a time** directly to `writer` (e.g. a
/// `BufWriter<File>`), keeping peak RAM constant (~1 frame) regardless of sequence length, and
/// preserving exact fractional capture FPS (`timescale = 100_000`).
pub fn encode_h264_mp4_streaming<W, F>(
    writer: W,
    width: u32,
    height: u32,
    frame_count: usize,
    fps: f64,
    mut with_frame_rgb: F,
) -> Result<(), ExportError>
where
    W: Write + Seek,
    F: FnMut(usize, &mut dyn FnMut(&[u8]) -> Result<(), ExportError>) -> Result<(), ExportError>,
{
    if frame_count == 0 {
        return Err(ExportError::EmptySequence);
    }
    let w = (width & !1) as usize;
    let h = (height & !1) as usize;
    let fps_clamped = fps.clamp(0.5, 60.0);
    let timescale: u32 = 100_000;
    let sample_dur: u32 = ((f64::from(timescale) / fps_clamped).round() as u32).max(1);

    let cfg = openh264::encoder::EncoderConfig::new()
        .bitrate(openh264::encoder::BitRate::from_bps(
            ((w * h) as u32 * 4).clamp(1_500_000, 12_000_000),
        ))
        .max_frame_rate(openh264::encoder::FrameRate::from_hz(fps_clamped as f32));
    let api = openh264::OpenH264API::from_source();
    let mut encoder = openh264::encoder::Encoder::with_api_config(api, cfg)
        .map_err(|e| ExportError::Video(e.to_string()))?;

    let mp4_cfg = mp4::Mp4Config {
        major_brand: str::parse("isom").expect("isom"),
        minor_version: 512,
        compatible_brands: vec![
            str::parse("isom").expect("isom"),
            str::parse("iso2").expect("iso2"),
            str::parse("avc1").expect("avc1"),
            str::parse("mp41").expect("mp41"),
        ],
        timescale: 1000,
    };
    let mut mp4_writer = mp4::Mp4Writer::write_start(writer, &mp4_cfg)
        .map_err(|e| ExportError::Video(e.to_string()))?;
    let mut track_added = false;
    let mut sps: Option<Vec<u8>> = None;
    let mut pps: Option<Vec<u8>> = None;
    let mut deferred_samples: Vec<(Vec<u8>, bool)> = Vec::new();
    let mut written_samples: u64 = 0;

    for idx in 0..frame_count {
        let mut avcc = Vec::new();
        let mut is_sync = false;
        with_frame_rgb(idx, &mut |rgb: &[u8]| {
            let rgb_src = openh264::formats::RgbSliceU8::new(rgb, (w, h));
            let yuv = YUVBuffer::from_rgb_source(rgb_src);
            let bitstream = encoder
                .encode(&yuv)
                .map_err(|e| ExportError::Video(e.to_string()))?;
            let annex_b = bitstream.to_vec();
            avcc.reserve(annex_b.len());
            for nalu in split_annex_b(&annex_b) {
                if nalu.is_empty() {
                    continue;
                }
                let nal_type = nalu[0] & 0x1F;
                match nal_type {
                    7 => sps = Some(nalu.to_vec()),
                    8 => pps = Some(nalu.to_vec()),
                    5 => {
                        is_sync = true;
                        avcc.extend_from_slice(&(nalu.len() as u32).to_be_bytes());
                        avcc.extend_from_slice(nalu);
                    }
                    _ => {
                        avcc.extend_from_slice(&(nalu.len() as u32).to_be_bytes());
                        avcc.extend_from_slice(nalu);
                    }
                }
            }
            Ok(())
        })?;

        if !track_added {
            if let (Some(seq_param_set), Some(pic_param_set)) = (sps.clone(), pps.clone()) {
                let track_cfg = mp4::TrackConfig {
                    track_type: mp4::TrackType::Video,
                    timescale,
                    language: String::from("und"),
                    media_conf: mp4::MediaConfig::AvcConfig(mp4::AvcConfig {
                        width: w as u16,
                        height: h as u16,
                        seq_param_set,
                        pic_param_set,
                    }),
                };
                mp4_writer
                    .add_track(&track_cfg)
                    .map_err(|e| ExportError::Video(e.to_string()))?;
                track_added = true;
                for (bytes, sync) in deferred_samples.drain(..) {
                    let sample = mp4::Mp4Sample {
                        start_time: written_samples * u64::from(sample_dur),
                        duration: sample_dur,
                        rendering_offset: 0,
                        is_sync: sync,
                        bytes: bytes.into(),
                    };
                    mp4_writer
                        .write_sample(1, &sample)
                        .map_err(|e| ExportError::Video(e.to_string()))?;
                    written_samples += 1;
                }
            }
        }

        if !avcc.is_empty() {
            if track_added {
                let sample = mp4::Mp4Sample {
                    start_time: written_samples * u64::from(sample_dur),
                    duration: sample_dur,
                    rendering_offset: 0,
                    is_sync,
                    bytes: avcc.into(),
                };
                mp4_writer
                    .write_sample(1, &sample)
                    .map_err(|e| ExportError::Video(e.to_string()))?;
                written_samples += 1;
            } else {
                deferred_samples.push((avcc, is_sync));
            }
        }
    }

    if !track_added {
        return Err(ExportError::Video("missing H.264 SPS/PPS".into()));
    }
    mp4_writer
        .write_end()
        .map_err(|e| ExportError::Video(e.to_string()))?;
    Ok(())
}

/// Zero-FFmpeg H.264 MP4 encoder (`openh264` Annex-B -> AVCC NALUs -> `mp4::Mp4Writer`).
pub fn encode_h264_mp4(
    width: u32,
    height: u32,
    frames_rgb: &[Vec<u8>],
    fps: u32,
) -> Result<Vec<u8>, ExportError> {
    let mut cursor = Cursor::new(Vec::new());
    encode_h264_mp4_streaming(
        &mut cursor,
        width,
        height,
        frames_rgb.len(),
        f64::from(fps.clamp(1, 60)),
        |idx, emit| emit(&frames_rgb[idx]),
    )?;
    Ok(cursor.into_inner())
}

fn split_annex_b(data: &[u8]) -> Vec<&[u8]> {
    let mut starts = Vec::new();
    let mut i = 0;
    while i + 3 <= data.len() {
        if data[i] == 0 && data[i + 1] == 0 {
            if i + 4 <= data.len() && data[i + 2] == 0 && data[i + 3] == 1 {
                starts.push((i, 4));
                i += 4;
                continue;
            }
            if data[i + 2] == 1 {
                starts.push((i, 3));
                i += 3;
                continue;
            }
        }
        i += 1;
    }
    let mut nalus = Vec::with_capacity(starts.len());
    for idx in 0..starts.len() {
        let (pos, sc_len) = starts[idx];
        let body_start = pos + sc_len;
        let body_end = starts.get(idx + 1).map_or(data.len(), |&(next, _)| next);
        if body_start < body_end {
            nalus.push(&data[body_start..body_end]);
        }
    }
    nalus
}

/// Generate a standalone one-click HTML inspection report (embedded HD thermal PNG, inline SVG line profile,
/// radiometric metadata, measurement table, and engineer notes).
#[allow(clippy::too_many_arguments)]
pub fn generate_html_report(
    title: &str,
    notes: &str,
    pixels: &[u16],
    sensor_w: u16,
    sensor_h: u16,
    min_raw: u16,
    max_raw: u16,
    palette: &Palette,
    rad: RadiometryParams,
    pipeline: &DisplayPipeline,
    rois: &[Roi],
    opts: &ExportOptions,
) -> Result<String, ExportError> {
    let (w, h, rgb) = render_export_canvas(
        pixels, sensor_w, sensor_h, min_raw, max_raw, palette, rad, pipeline, rois, opts,
    );
    let png = encode_png(w, h, &rgb)?;
    let b64 = base64::engine::general_purpose::STANDARD.encode(&png);
    let stats: Vec<RoiStats> = rois
        .iter()
        .map(|r| evaluate_roi(r, pixels, sensor_w, sensor_h, rad))
        .collect();

    let line_svg = rois
        .iter()
        .find_map(|r| match r.geometry {
            RoiGeometry::Line { x0, y0, x1, y1 } => {
                Some(sample_line_profile(pixels, sensor_w, sensor_h, x0, y0, x1, y1, rad))
            }
            _ => None,
        })
        .map(|samples| render_line_profile_svg(&samples, opts.unit))
        .unwrap_or_default();

    let mut rows = String::new();
    for s in &stats {
        let _ = writeln!(
            rows,
            "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{:.2}</td><td>{:.2}</td></tr>",
            escape_html(&s.label),
            opts.unit.format(s.min_c, 2),
            opts.unit.format(s.mean_c, 2),
            opts.unit.format(s.max_c, 2),
            s.std_dev_c,
            s.effective_emissivity
        );
    }

    Ok(format!(
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>{}</title>\
         <style>body{{font-family:system-ui,-apple-system,sans-serif;max-width:1040px;margin:2rem auto;padding:0 1.5rem;color:#171717}}\
         img{{width:100%;border-radius:8px;border:1px solid #e6e6e6}}\
         table{{width:100%;border-collapse:collapse;margin:1rem 0;font-variant-numeric:tabular-nums}}\
         th,td{{border-bottom:1px solid #e6e6e6;padding:.5rem .75rem;text-align:left}}\
         .meta{{display:grid;grid-template-columns:repeat(4,1fr);gap:.75rem;background:#fafafa;padding:1rem;border-radius:8px;border:1px solid #e6e6e6}}</style></head>\
         <body><h1>{}</h1>\
         <div class=\"meta\"><div><strong>Palette</strong><br>{}</div><div><strong>Emissivity</strong><br>ε = {:.2}</div>\
         <div><strong>Reflected T</strong><br>{}</div><div><strong>Resolution</strong><br>{}×{} (sensor {}×{})</div></div>\
         <p><img src=\"data:image/png;base64,{}\" alt=\"Thermal Inspection\"></p>\
         {}<h2>Measurements</h2><table><thead><tr><th>ROI</th><th>Min</th><th>Mean</th><th>Max</th><th>σ</th><th>ε</th></tr></thead><tbody>{}</tbody></table>\
         <h2>Notes</h2><p>{}</p></body></html>",
        escape_html(title),
        escape_html(title),
        escape_html(&palette.label),
        rad.emissivity,
        opts.unit.format(rad.reflected_temp_c, 1),
        w,
        h,
        sensor_w,
        sensor_h,
        b64,
        line_svg,
        rows,
        escape_html(notes)
    ))
}

/// Generate a Markdown inspection report with measurement table & radiometry parameters.
#[allow(clippy::too_many_arguments)]
#[must_use]
pub fn generate_markdown_report(
    title: &str,
    notes: &str,
    image_filename: &str,
    pixels: &[u16],
    sensor_w: u16,
    sensor_h: u16,
    palette: &Palette,
    rad: RadiometryParams,
    rois: &[Roi],
    unit: TempUnit,
) -> String {
    let mut md = format!(
        "# {title}\n\n![Thermal Image]({image_filename})\n\n\
         - **Palette:** {}\n- **Sensor:** {sensor_w}×{sensor_h}\n\
         - **Global Emissivity:** {:.2}\n- **Reflected Temp:** {}\n\n\
         ## Measurements\n\n| ROI | Min | Mean | Max | Std Dev | Emissivity |\n|---|---:|---:|---:|---:|---:|\n",
        palette.label,
        rad.emissivity,
        unit.format(rad.reflected_temp_c, 1),
    );
    for r in rois {
        let s = evaluate_roi(r, pixels, sensor_w, sensor_h, rad);
        let _ = writeln!(
            md,
            "| {} | {} | {} | {} | {:.2} | {:.2} |",
            s.label,
            unit.format(s.min_c, 2),
            unit.format(s.mean_c, 2),
            unit.format(s.max_c, 2),
            s.std_dev_c,
            s.effective_emissivity
        );
    }
    if !notes.trim().is_empty() {
        let _ = write!(md, "\n## Notes\n\n{notes}\n");
    }
    md
}

fn render_line_profile_svg(samples: &[LineSample], unit: TempUnit) -> String {
    if samples.len() < 2 {
        return String::new();
    }
    let vals: Vec<f32> = samples.iter().map(|s| unit.from_celsius(s.temp_c)).collect();
    let lo = vals.iter().copied().fold(f32::INFINITY, f32::min);
    let hi = vals.iter().copied().fold(f32::NEG_INFINITY, f32::max);
    let span = (hi - lo).max(0.1);
    let mut pts = String::new();
    for (i, &v) in vals.iter().enumerate() {
        let x = 40.0 + (i as f32 / (vals.len() - 1) as f32) * 560.0;
        let y = 130.0 - ((v - lo) / span) * 100.0;
        let _ = write!(pts, "{x:.1},{y:.1} ");
    }
    format!(
        "<h2>Line Profile</h2><svg viewBox=\"0 0 640 160\" width=\"100%\" style=\"background:#fafafa;border:1px solid #e6e6e6;border-radius:8px\">\
         <polyline fill=\"none\" stroke=\"#2076c5\" stroke-width=\"2.2\" points=\"{pts}\"/>\
         <text x=\"8\" y=\"32\" font-size=\"11\" fill=\"#4d4d4d\">{hi:.1}{}</text>\
         <text x=\"8\" y=\"134\" font-size=\"11\" fill=\"#4d4d4d\">{lo:.1}{}</text></svg>",
        unit.symbol(),
        unit.symbol()
    )
}

fn escape_html(s: &str) -> String {
    s.replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
}
