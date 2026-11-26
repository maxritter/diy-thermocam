//! Loading a capture's frame (this computer or the camera's card) and rendering pictures.

use std::fs;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};

use serde::Deserialize;
use tauri::ipc::{InvokeResponseBody, Response};
use tauri::State;

use thermocam_core::card::{list_card_dir, read_card_file, CardAccess};
use thermocam_core::dat::ThermalDat;
use thermocam_core::paths::{card_path, confine};
use thermocam_core::picture::{render_png, PictureFrame, RenderSpec};
use thermocam_core::recording::read_recording_frame;
use thermocam_core::sidecar::CaptureRef;

use crate::card::LiveCard;
use crate::error::{CmdError, CmdResult};
use crate::playback::{absolute, get_or_open};
use crate::state::{resolve_palette, ActiveTransport, AppState};

/// A capture's first frame plus the facts that identify this version of it (for caches).
pub struct LoadedFrame {
    pub dat: ThermalDat,
    pub identity: Vec<String>,
}

fn is_dat_name(name: &str) -> bool {
    name.len() > 4 && name[name.len() - 4..].eq_ignore_ascii_case(".dat")
}

/// The first `.DAT` (by name) in a local folder.
pub fn first_local_frame(dir: &Path) -> CmdResult<PathBuf> {
    let mut names: Vec<String> = fs::read_dir(dir)?
        .flatten()
        .filter(|e| e.file_type().map(|t| t.is_file()).unwrap_or(false))
        .filter_map(|e| e.file_name().to_str().map(str::to_string))
        .filter(|n| is_dat_name(n))
        .collect();
    names.sort();
    let first = names
        .first()
        .ok_or_else(|| CmdError::new("bad_file", "The folder holds no .DAT frames."))?;
    Ok(confine(dir, first)?)
}

fn parse_dat(bytes: &[u8]) -> CmdResult<ThermalDat> {
    ThermalDat::from_bytes(bytes).map_err(|e| CmdError::new("bad_file", e.to_string()))
}

/// Where a capture's first frame lives, found without reading it.
pub enum FrameLocation {
    Local(PathBuf),
    /// Normalised card path of the frame file.
    Camera(String),
}

/// A capture's first frame, located, with the facts that identify this version of it.
pub struct IdentifiedFrame {
    pub location: FrameLocation,
    pub identity: Vec<String>,
}

fn identify_local(path: &str) -> CmdResult<IdentifiedFrame> {
    let p = PathBuf::from(path);
    if !p.is_absolute() {
        return Err(CmdError::new("bad_path", "expected an absolute path"));
    }
    let file = if p.is_dir() {
        first_local_frame(&p)?
    } else {
        p
    };
    let meta = fs::metadata(&file)?;
    let mtime = meta
        .modified()
        .ok()
        .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
        .map_or(0, |d| d.as_millis());
    let canonical = fs::canonicalize(&file).unwrap_or(file.clone());
    Ok(IdentifiedFrame {
        identity: vec![
            "local".into(),
            canonical.to_string_lossy().into_owned(),
            mtime.to_string(),
            meta.len().to_string(),
        ],
        location: FrameLocation::Local(file),
    })
}

/// Fail unless `serial` is the camera that is connected now.
pub fn require_camera(serial_slot: &Mutex<Option<String>>, serial: &str) -> CmdResult<()> {
    match serial_slot.lock()?.as_deref() {
        Some(s) if s.eq_ignore_ascii_case(serial) => Ok(()),
        _ => Err(CmdError::not_connected()),
    }
}

fn identify_camera(
    client: &Mutex<Option<ActiveTransport>>,
    serial: &str,
    sd_path: &str,
) -> CmdResult<IdentifiedFrame> {
    let card = LiveCard(client);
    let path = card_path(sd_path)?;
    let stat = card.stat(&path)?;
    let (file, stat) = if stat.is_dir {
        let mut names: Vec<String> = list_card_dir(&card, &path)?
            .into_iter()
            .filter(|e| !e.is_dir && is_dat_name(&e.name))
            .map(|e| e.name)
            .collect();
        names.sort();
        let first = names
            .first()
            .ok_or_else(|| CmdError::new("bad_file", "The folder holds no .DAT frames."))?;
        let file = card_path(&format!("{path}/{first}"))?;
        let stat = card.stat(&file)?;
        (file, stat)
    } else {
        (path, stat)
    };
    Ok(IdentifiedFrame {
        identity: vec![
            "camera".into(),
            serial.to_ascii_lowercase(),
            file.clone(),
            stat.mtime_epoch.to_string(),
            stat.size.to_string(),
        ],
        location: FrameLocation::Camera(file),
    })
}

pub fn identify_capture(
    client: &Mutex<Option<ActiveTransport>>,
    serial_slot: &Mutex<Option<String>>,
    capture: &CaptureRef,
) -> CmdResult<IdentifiedFrame> {
    match capture {
        CaptureRef::Local { path } => identify_local(path),
        CaptureRef::Camera { serial, sd_path } => {
            require_camera(serial_slot, serial)?;
            identify_camera(client, serial, sd_path)
        }
    }
}

pub fn read_identified(
    client: &Mutex<Option<ActiveTransport>>,
    frame: &IdentifiedFrame,
) -> CmdResult<ThermalDat> {
    match &frame.location {
        FrameLocation::Local(p) => parse_dat(&fs::read(p)?),
        FrameLocation::Camera(p) => parse_dat(&read_card_file(&LiveCard(client), p)?),
    }
}

pub fn load_capture(
    client: &Mutex<Option<ActiveTransport>>,
    serial_slot: &Mutex<Option<String>>,
    capture: &CaptureRef,
) -> CmdResult<LoadedFrame> {
    let frame = identify_capture(client, serial_slot, capture)?;
    let dat = read_identified(client, &frame)?;
    Ok(LoadedFrame {
        dat,
        identity: frame.identity,
    })
}

/// One frame of a recording on this computer.
#[derive(Debug, Clone, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum RecordingFrameRef {
    Recording { path: String, index: usize },
}

/// `"latest"`, a capture (`{kind:"local",path}` / `{kind:"camera",serial,sd_path}`), or a
/// recording frame (`{kind:"recording",path,index}`).
#[derive(Debug, Clone, Deserialize)]
#[serde(untagged)]
pub enum RenderSource {
    Keyword(String),
    Capture(CaptureRef),
    Frame(RecordingFrameRef),
}

#[derive(Debug, Clone, Deserialize)]
pub struct RenderRequest {
    pub source: RenderSource,
    #[serde(flatten)]
    pub spec: RenderSpec,
    /// Write the PNG here instead of returning its bytes.
    #[serde(default)]
    pub out_path: Option<String>,
    /// Ask where to save with a Save dialog (Save PNG…).
    #[serde(default)]
    pub dialog: bool,
}

fn json_path(path: &Path) -> Response {
    let body = serde_json::json!({ "path": path.to_string_lossy() }).to_string();
    Response::new(InvokeResponseBody::Json(body))
}

/// PNG bytes (`ArrayBuffer`) when neither `out_path` nor `dialog` is set, otherwise `{ path }`.
#[tauri::command]
pub async fn cmd_render_png(state: State<'_, AppState>, req: RenderRequest) -> CmdResult<Response> {
    let palette = resolve_palette(&state, &req.spec.palette_id)
        .map_err(|e| CmdError::new("bad_palette", e))?;
    let target = if let Some(p) = &req.out_path {
        Some(PathBuf::from(p))
    } else if req.dialog {
        let picked = rfd::AsyncFileDialog::new()
            .add_filter("PNG image", &["png"])
            .set_file_name("thermocam.png")
            .save_file()
            .await
            .ok_or_else(|| CmdError::new("cancelled", "No file was chosen."))?;
        Some(picked.path().to_path_buf())
    } else {
        None
    };
    let latest = Arc::clone(&state.latest_frame);
    let client = Arc::clone(&state.client);
    let serial = Arc::clone(&state.camera_serial);
    let recordings = Arc::clone(&state.recordings);
    tauri::async_runtime::spawn_blocking(move || {
        let png = match &req.source {
            RenderSource::Keyword(k) if k == "latest" => {
                let frame = latest.lock()?.clone().ok_or_else(|| {
                    CmdError::new("no_frame", "No image has arrived from the camera yet.")
                })?;
                render_png(&PictureFrame::from_frame(&frame), &req.spec, &palette)
            }
            RenderSource::Keyword(other) => {
                return Err(CmdError::new(
                    "bad_source",
                    format!("unknown source '{other}'"),
                ));
            }
            RenderSource::Capture(c) => {
                let loaded = load_capture(&client, &serial, c)?;
                render_png(&PictureFrame::from_dat(&loaded.dat), &req.spec, &palette)
            }
            RenderSource::Frame(RecordingFrameRef::Recording { path, index }) => {
                let rec = get_or_open(&recordings, &absolute(path)?)?;
                let dat = read_recording_frame(&rec, *index)?;
                render_png(&PictureFrame::from_dat(&dat), &req.spec, &palette)
            }
        }
        .map_err(|e| CmdError::other(e.to_string()))?
        .2;
        match target {
            Some(path) => {
                thermocam_core::atomic_file::write_atomic(&path, &png)?;
                Ok(json_path(&path))
            }
            None => Ok(Response::new(png)),
        }
    })
    .await
    .map_err(|e| CmdError::other(e.to_string()))?
}
