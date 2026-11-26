//! The Captures folder and the Snapshot command.

use std::path::PathBuf;
use std::sync::Arc;

use serde::Serialize;
use tauri::{Manager, State};

use thermocam_core::atomic_file::write_atomic;
use thermocam_core::captures::{
    check_writable, default_captures_dir, list_captures, trash_targets, write_snapshot,
    LocalCapture,
};
use thermocam_core::picture::{render_png, PictureFrame, RenderSpec};
use thermocam_core::sidecar::SIDECAR_EXTENSION;

use crate::error::{CmdError, CmdResult};
use crate::platform::local_now;
use crate::settings::{store_captures_dir, stored_captures_dir};
use crate::state::{resolve_palette, AppState};

/// The Captures folder (stored choice, else `Pictures/Thermocam`), created on first use.
pub fn captures_dir(app: &tauri::AppHandle) -> CmdResult<PathBuf> {
    let dir = match stored_captures_dir(app) {
        Some(d) => d,
        None => {
            let pictures = app.path().picture_dir().ok();
            let home = app.path().home_dir().ok();
            default_captures_dir(pictures.as_deref(), home.as_deref())
                .ok_or_else(|| CmdError::other("this system has no home folder"))?
        }
    };
    std::fs::create_dir_all(&dir)?;
    Ok(dir)
}

#[tauri::command]
pub fn cmd_captures_dir(app: tauri::AppHandle) -> CmdResult<String> {
    Ok(captures_dir(&app)?.to_string_lossy().into_owned())
}

/// Checks that `path` is a writable folder, stores it and returns it.
#[tauri::command]
pub fn cmd_set_captures_dir(app: tauri::AppHandle, path: String) -> CmdResult<String> {
    let dir = PathBuf::from(&path);
    if !dir.is_absolute() {
        return Err(CmdError::new(
            "bad_path",
            "the Captures folder must be an absolute path",
        ));
    }
    check_writable(&dir)?;
    store_captures_dir(&app, &dir)?;
    Ok(dir.to_string_lossy().into_owned())
}

/// The Captures folder's entries for "On this computer":
/// `[{name, path, kind: "snapshot"|"recording"|"other", size, frames|null, mtime_ms}]`.
#[tauri::command]
pub async fn cmd_captures_list(app: tauri::AppHandle) -> CmdResult<Vec<LocalCapture>> {
    let dir = captures_dir(&app)?;
    tauri::async_runtime::spawn_blocking(move || Ok(list_captures(&dir)?))
        .await
        .map_err(|e| CmdError::other(e.to_string()))?
}

#[derive(Serialize)]
pub struct SnapshotInfo {
    pub path: String,
    /// The PNG written beside the `.DAT` when the request asked for one.
    pub png_path: Option<String>,
}

pub fn latest_frame(state: &AppState) -> CmdResult<Arc<thermocam_core::protocol::codec::FrameRaw>> {
    state
        .latest_frame
        .lock()?
        .clone()
        .ok_or_else(|| CmdError::new("no_frame", "No image has arrived from the camera yet."))
}

/// Saves the latest frame as `YYYYMMDDHHMMSS.DAT` in the Captures folder, with no dialog.
/// With `png`, also renders it to `YYYYMMDDHHMMSS.png` beside the `.DAT` (PNG on snapshot).
#[tauri::command]
pub async fn cmd_snapshot(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    png: Option<RenderSpec>,
) -> CmdResult<SnapshotInfo> {
    let frame = latest_frame(&state)?;
    let dir = captures_dir(&app)?;
    let palette = match &png {
        Some(spec) => Some(
            resolve_palette(&state, &spec.palette_id)
                .map_err(|e| CmdError::new("bad_palette", e))?,
        ),
        None => None,
    };
    tauri::async_runtime::spawn_blocking(move || {
        let path = write_snapshot(&dir, &frame, local_now())?;
        let png_path = match (png, palette) {
            (Some(spec), Some(palette)) => {
                let (_, _, bytes) = render_png(&PictureFrame::from_frame(&frame), &spec, &palette)
                    .map_err(|e| CmdError::other(e.to_string()))?;
                let png_path = path.with_extension("png");
                write_atomic(&png_path, &bytes)?;
                Some(png_path.to_string_lossy().into_owned())
            }
            _ => None,
        };
        Ok(SnapshotInfo {
            path: path.to_string_lossy().into_owned(),
            png_path,
        })
    })
    .await
    .map_err(|e| CmdError::other(e.to_string()))?
}

#[derive(Serialize)]
pub struct TrashFailure {
    pub path: String,
    pub kind: String,
    pub detail: String,
}

#[derive(Serialize)]
pub struct TrashResult {
    pub trashed: Vec<String>,
    pub failed: Vec<TrashFailure>,
}

/// Moves captures (files or recording folders) to the Trash / Recycle Bin, with their edits.
/// Every path must lie inside the Captures folder; the others are reported in `failed`.
#[tauri::command]
pub async fn cmd_trash(app: tauri::AppHandle, paths: Vec<String>) -> CmdResult<TrashResult> {
    let root = captures_dir(&app)?;
    tauri::async_runtime::spawn_blocking(move || {
        let mut result = TrashResult {
            trashed: Vec::new(),
            failed: Vec::new(),
        };
        for (given, checked) in paths.iter().zip(trash_targets(&root, &paths)) {
            let target = match checked {
                Ok(t) => t,
                Err(e) => {
                    result.failed.push(TrashFailure {
                        path: given.clone(),
                        kind: e.kind().into(),
                        detail: e.to_string(),
                    });
                    continue;
                }
            };
            let mut items = vec![target.clone()];
            let sidecar = target.with_extension(SIDECAR_EXTENSION);
            if sidecar != target && sidecar.exists() {
                items.push(sidecar);
            }
            match trash::delete_all(&items) {
                Ok(()) => result.trashed.push(given.clone()),
                Err(e) => result.failed.push(TrashFailure {
                    path: given.clone(),
                    kind: "trash".into(),
                    detail: e.to_string(),
                }),
            }
        }
        Ok(result)
    })
    .await
    .map_err(|e| CmdError::other(e.to_string()))?
}
