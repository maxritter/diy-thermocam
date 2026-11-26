//! `cmd_export_media`: images, reports, CSV, video and `.DAT` frames, from the live frame,
//! from pixels the frontend sends, or from a trimmed range of a recording on disk.

use std::fs::{self, File};
use std::io::BufWriter;
use std::path::PathBuf;
use std::sync::Arc;

use serde::Deserialize;
use tauri::{Emitter, State};

use thermocam_core::dat::{format_utc_now_iso8601, SequenceManifest, ThermalDat};
use thermocam_core::export::{
    derive_sequence_fps, encode_gif_streaming, encode_h264_mp4_streaming, encode_jpeg, encode_png,
    encode_radiometric_tiff_16, encode_temperature_csv, generate_html_report,
    generate_markdown_report, render_export_canvas, render_export_canvas_with_scratch,
    ExportOptions, ExportResolution,
};
use thermocam_core::imaging::{DisplayPipeline, RenderScratch};
use thermocam_core::protocol::codec::FrameRaw;
use thermocam_core::radiometry::{RadiometryParams, TempUnit};
use thermocam_core::recording::{
    frame_from_dat, read_recording_frame, write_trimmed_recording, RecordingIndex,
};
use thermocam_core::roi::Roi;
use thermocam_core::series::{series_csv, SeriesRegion};

use crate::error::{CmdError, CmdResult};
use crate::playback::{absolute, get_or_open, series_for};
use crate::state::{resolve_palette, AppState};

/// Inclusive frame range of a recording.
#[derive(Debug, Clone, Copy, Deserialize)]
pub struct FrameRange {
    pub from: usize,
    pub to: usize,
}

fn default_palette() -> String {
    "ironblack".into()
}

fn default_resolution() -> ExportResolution {
    ExportResolution::FullHd1080p
}

#[derive(Deserialize)]
pub struct ExportRequest {
    /// `png`, `jpg`, `tiff`, `csv`, `html`, `md`, `mp4`, `gif`, `dat`, `seq` or `series-csv`.
    #[serde(alias = "kind")]
    pub format: String,
    pub output_path: Option<String>,
    #[serde(default = "default_palette")]
    pub palette_id: String,
    #[serde(default)]
    pub min_raw: u16,
    #[serde(default = "u16_max")]
    pub max_raw: u16,
    #[serde(default)]
    pub rad: RadiometryParams,
    #[serde(default)]
    pub pipeline: DisplayPipeline,
    #[serde(default)]
    pub rois: Vec<Roi>,
    #[serde(default = "default_resolution")]
    pub resolution: ExportResolution,
    #[serde(default)]
    pub show_colorbar: bool,
    #[serde(default)]
    pub show_hud: bool,
    #[serde(default)]
    pub unit: TempUnit,
    pub title: Option<String>,
    pub notes: Option<String>,
    pub pixels: Option<Vec<u16>>,
    pub width: Option<u16>,
    pub height: Option<u16>,
    /// A recording folder on this computer to export from (video, `.DAT` frames, series CSV).
    #[serde(default)]
    pub recording: Option<String>,
    /// Inclusive frame range of `recording` (whole recording when absent).
    #[serde(default)]
    pub range: Option<FrameRange>,
    /// Regions for `series-csv` (`frame,t_ms,<region…>`).
    #[serde(default)]
    pub regions: Option<Vec<SeriesRegion>>,
}

fn u16_max() -> u16 {
    u16::MAX
}

/// The frames a video or sequence export draws from.
enum ExportFrames {
    Memory(Vec<Arc<FrameRaw>>),
    Disk {
        rec: Arc<RecordingIndex>,
        from: usize,
        to: usize,
    },
}

impl ExportFrames {
    fn len(&self) -> usize {
        match self {
            Self::Memory(v) => v.len(),
            Self::Disk { from, to, .. } => to - from + 1,
        }
    }

    fn get(&self, i: usize) -> Result<Arc<FrameRaw>, String> {
        match self {
            Self::Memory(v) => v.get(i).cloned().ok_or_else(|| "frame out of range".into()),
            Self::Disk { rec, from, .. } => {
                let idx = from + i;
                let dat = read_recording_frame(rec, idx).map_err(|e| e.detail)?;
                let t = rec.t_ms.as_ref().and_then(|t| t.get(idx).copied());
                Ok(Arc::new(frame_from_dat(&dat, idx as u32, t)))
            }
        }
    }

    fn timestamps_us(&self) -> Vec<u64> {
        match self {
            Self::Memory(v) => v.iter().map(|f| f.meta.timestamp_us).collect(),
            Self::Disk { rec, from, to } => match &rec.t_ms {
                Some(t) => t[*from..=*to]
                    .iter()
                    .map(|&ms| u64::from(ms) * 1000)
                    .collect(),
                None => {
                    let step = 1_000_000.0 / f64::from(rec.fps_nominal.max(0.1));
                    (0..=(to - from))
                        .map(|i| (i as f64 * step) as u64)
                        .collect()
                }
            },
        }
    }
}

fn checked_range(rec: &RecordingIndex, range: Option<FrameRange>) -> CmdResult<(usize, usize)> {
    let (from, to) = range.map_or((0, rec.frames.saturating_sub(1)), |r| (r.from, r.to));
    if rec.frames == 0 || from > to || to >= rec.frames {
        return Err(CmdError::new(
            "bad_range",
            format!("frames {from}–{to} of a {}-frame recording", rec.frames),
        ));
    }
    Ok((from, to))
}

async fn pick_target(ext: &str, output_path: Option<String>) -> CmdResult<PathBuf> {
    let cancelled = || CmdError::new("cancelled", "No file was chosen.");
    Ok(match output_path {
        Some(p) => PathBuf::from(p),
        None if ext == "seq" => rfd::AsyncFileDialog::new()
            .pick_folder()
            .await
            .ok_or_else(cancelled)?
            .path()
            .to_path_buf(),
        None => {
            let file_ext = if ext == "series-csv" { "csv" } else { ext };
            rfd::AsyncFileDialog::new()
                .set_file_name(format!("thermocam_capture.{file_ext}"))
                .save_file()
                .await
                .ok_or_else(cancelled)?
                .path()
                .to_path_buf()
        }
    })
}

/// Exports and returns the written path. `export-progress {stage:"encoding", done, total}`
/// is sent per video frame.
#[tauri::command]
pub async fn cmd_export_media(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    req: ExportRequest,
) -> CmdResult<String> {
    let ext = req.format.to_lowercase();
    let palette =
        resolve_palette(&state, &req.palette_id).map_err(|e| CmdError::new("bad_palette", e))?;
    let opts = ExportOptions {
        resolution: req.resolution,
        show_colorbar: req.show_colorbar,
        show_hud: req.show_hud,
        unit: req.unit,
        jpeg_quality: 94,
    };
    let is_sequence = matches!(ext.as_str(), "mp4" | "gif" | "seq");

    // The recording to read: the one named, or for old callers the last one recorded.
    let recording_path = match &req.recording {
        Some(p) => Some(absolute(p)?),
        None if is_sequence => state.last_recording.lock()?.clone(),
        None => None,
    };
    let recordings = Arc::clone(&state.recordings);
    let series_cache = Arc::clone(&state.series_cache);
    let latest = state.latest_frame.lock()?.clone();
    let target = pick_target(&ext, req.output_path.clone()).await?;

    tauri::async_runtime::spawn_blocking(move || {
        let rec = match &recording_path {
            Some(p) => Some(get_or_open(&recordings, p)?),
            None => None,
        };

        if ext == "series-csv" {
            let path = recording_path
                .as_ref()
                .ok_or_else(|| CmdError::new("no_recording", "series-csv needs a recording"))?;
            let regions = req.regions.clone().unwrap_or_default();
            let rec = rec
                .as_ref()
                .ok_or_else(|| CmdError::new("no_recording", ""))?;
            let (from, to) = checked_range(rec, req.range)?;
            let series = series_for(&recordings, &series_cache, path, &regions, |_, _| {})?;
            let ids: Vec<String> = regions.iter().map(|r| r.id.clone()).collect();
            fs::write(&target, series_csv(&series, &ids, from, to))?;
            return Ok(target.to_string_lossy().into_owned());
        }

        if ext == "dat" {
            if let (Some(rec), Some(_)) = (&rec, &req.recording) {
                // `.DAT` frames of a trimmed range: a recording folder of its own.
                let (from, to) = checked_range(rec, req.range)?;
                write_trimmed_recording(rec, from, to, &target)?;
                return Ok(target.to_string_lossy().into_owned());
            }
        }

        if is_sequence {
            let frames = match &rec {
                Some(rec) => {
                    let (from, to) = checked_range(rec, req.range)?;
                    ExportFrames::Disk {
                        rec: Arc::clone(rec),
                        from,
                        to,
                    }
                }
                None => ExportFrames::Memory(latest.clone().into_iter().collect()),
            };
            export_sequence(&app, &ext, &frames, &target, &req, &palette, &opts)?;
            return Ok(target.to_string_lossy().into_owned());
        }

        // Single-frame formats: the sent pixels, a recording frame, or the live frame.
        let (pixels, w, h, frame) = match (req.pixels.clone(), req.width, req.height) {
            (Some(px), Some(w), Some(h)) => {
                let expected = usize::from(w) * usize::from(h);
                if expected == 0 || px.len() != expected {
                    return Err(CmdError::new(
                        "bad_pixels",
                        format!("pixel buffer length {} does not match {w}x{h}", px.len()),
                    ));
                }
                (px, w, h, None)
            }
            _ => {
                let fr = match &rec {
                    Some(rec) => {
                        let idx = req.range.map_or(0, |r| r.from);
                        let dat = read_recording_frame(rec, idx)?;
                        let t = rec.t_ms.as_ref().and_then(|t| t.get(idx).copied());
                        Arc::new(frame_from_dat(&dat, idx as u32, t))
                    }
                    None => latest.clone().ok_or_else(|| {
                        CmdError::new("no_frame", "No image has arrived from the camera yet.")
                    })?,
                };
                (fr.pixels.clone(), fr.meta.width, fr.meta.height, Some(fr))
            }
        };
        let (min_raw, max_raw, rad, unit) = (req.min_raw, req.max_raw, req.rad, req.unit);
        let (pipeline, rois) = (&req.pipeline, &req.rois);
        let title = req
            .title
            .as_deref()
            .unwrap_or("DIY-Thermocam Inspection Report");
        let notes = req.notes.as_deref().unwrap_or("");
        let err = |e: thermocam_core::export::ExportError| CmdError::other(e.to_string());
        match ext.as_str() {
            "png" | "jpg" | "jpeg" => {
                let (ow, oh, rgb) = render_export_canvas(
                    &pixels, w, h, min_raw, max_raw, &palette, rad, pipeline, rois, &opts,
                );
                let bytes = if ext == "png" {
                    encode_png(ow, oh, &rgb)
                } else {
                    encode_jpeg(ow, oh, &rgb, opts.jpeg_quality)
                }
                .map_err(err)?;
                fs::write(&target, bytes)?;
            }
            "tiff" | "tif" => {
                fs::write(
                    &target,
                    encode_radiometric_tiff_16(&pixels, w, h, rad).map_err(err)?,
                )?;
            }
            "csv" => fs::write(&target, encode_temperature_csv(&pixels, w, h, rad, unit))?,
            "html" => {
                let html = generate_html_report(
                    title, notes, &pixels, w, h, min_raw, max_raw, &palette, rad, pipeline, rois,
                    &opts,
                )
                .map_err(err)?;
                fs::write(&target, html)?;
            }
            "md" => {
                let md = generate_markdown_report(
                    title,
                    notes,
                    "capture.png",
                    &pixels,
                    w,
                    h,
                    &palette,
                    rad,
                    rois,
                    unit,
                );
                fs::write(&target, md)?;
            }
            "dat" => {
                let scheme = palette.firmware_id.unwrap_or(10);
                let dat = match frame {
                    Some(fr) => ThermalDat::from_frame(&fr, scheme, 0),
                    None => ThermalDat::from_pixels(pixels, w, h, rad, scheme, 0),
                };
                fs::write(&target, dat.to_bytes())?;
            }
            other => {
                return Err(CmdError::new(
                    "bad_format",
                    format!("unsupported export format: {other}"),
                ))
            }
        }
        Ok(target.to_string_lossy().into_owned())
    })
    .await
    .map_err(|e| CmdError::other(e.to_string()))?
}

fn export_sequence(
    app: &tauri::AppHandle,
    ext: &str,
    frames: &ExportFrames,
    target: &PathBuf,
    req: &ExportRequest,
    palette: &thermocam_core::palettes::Palette,
    opts: &ExportOptions,
) -> CmdResult<()> {
    let total = frames.len();
    if total == 0 {
        return Err(CmdError::new(
            "no_frame",
            "No frames are available to export.",
        ));
    }
    let fps = derive_sequence_fps(&frames.timestamps_us());
    if ext == "seq" {
        fs::create_dir_all(target)?;
        let scheme = palette.firmware_id.unwrap_or(10);
        let mut names = Vec::with_capacity(total);
        let (mut w, mut h) = (0, 0);
        for i in 0..total {
            let fr = frames.get(i).map_err(CmdError::other)?;
            (w, h) = (fr.meta.width, fr.meta.height);
            let name = format!("{i:06}.DAT");
            fs::write(
                target.join(&name),
                ThermalDat::from_frame(&fr, scheme, 0).to_bytes(),
            )?;
            names.push(name);
        }
        let manifest = SequenceManifest {
            schema_version: 1,
            created_utc: format_utc_now_iso8601(),
            sensor_w: w,
            sensor_h: h,
            fps: fps as f32,
            frame_count: names.len(),
            frames: names,
            palette_id: req.palette_id.clone(),
        };
        let json =
            serde_json::to_string_pretty(&manifest).map_err(|e| CmdError::other(e.to_string()))?;
        fs::write(target.join("manifest.json"), json)?;
        return Ok(());
    }

    let first = frames.get(0).map_err(CmdError::other)?;
    let (out_w, out_h) = opts
        .resolution
        .dimensions(first.meta.width, first.meta.height);
    let mut scratch = RenderScratch::new();
    let writer = BufWriter::new(File::create(target)?);
    let render = |idx: usize, scratch: &mut RenderScratch| -> Result<(), String> {
        let fr = frames.get(idx)?;
        render_export_canvas_with_scratch(
            &fr.pixels,
            fr.meta.width,
            fr.meta.height,
            req.min_raw,
            req.max_raw,
            palette,
            req.rad,
            &req.pipeline,
            &req.rois,
            opts,
            true,
            scratch,
        );
        let _ = app.emit(
            "export-progress",
            serde_json::json!({ "stage": "encoding", "done": idx + 1, "total": total }),
        );
        Ok(())
    };
    let to_export = |e: String| thermocam_core::export::ExportError::Video(e);
    let result = if ext == "mp4" {
        encode_h264_mp4_streaming(writer, out_w, out_h, total, fps, |idx, emit| {
            render(idx, &mut scratch).map_err(to_export)?;
            emit(&scratch.canvas)
        })
    } else {
        encode_gif_streaming(
            writer,
            out_w as u16,
            out_h as u16,
            total,
            fps,
            |idx, emit| {
                render(idx, &mut scratch).map_err(to_export)?;
                emit(&scratch.canvas)
            },
        )
    };
    result.map_err(|e| CmdError::other(e.to_string()))
}
