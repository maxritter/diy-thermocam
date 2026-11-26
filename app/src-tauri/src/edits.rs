//! Edits (sidecar v2) for local and camera captures: load, save, reset.

use std::path::PathBuf;

use tauri::Manager;

use thermocam_core::sidecar::{
    load_sidecar, reset_sidecar, save_sidecar, sidecar_path, CaptureRef, SidecarLoad, ThermoSidecar,
};

use crate::error::{CmdError, CmdResult};

fn edits_root(app: &tauri::AppHandle) -> CmdResult<PathBuf> {
    Ok(app.path().app_data_dir()?.join("edits"))
}

fn path_for(app: &tauri::AppHandle, capture: &CaptureRef) -> CmdResult<PathBuf> {
    Ok(sidecar_path(capture, &edits_root(app)?)?)
}

/// `{ sidecar: ThermoSidecar | null, quarantined: string | null }`.
#[tauri::command]
pub fn cmd_sidecar_load(app: tauri::AppHandle, capture: CaptureRef) -> CmdResult<SidecarLoad> {
    Ok(load_sidecar(&path_for(&app, &capture)?))
}

/// Saves atomically and returns the sidecar's path.
#[tauri::command]
pub fn cmd_sidecar_save(
    app: tauri::AppHandle,
    capture: CaptureRef,
    sidecar: ThermoSidecar,
) -> CmdResult<String> {
    let path = path_for(&app, &capture)?;
    save_sidecar(&path, &sidecar).map_err(CmdError::from)?;
    Ok(path.to_string_lossy().into_owned())
}

/// Deletes the sidecar (Reset to original). Returns whether one existed.
#[tauri::command]
pub fn cmd_sidecar_reset(app: tauri::AppHandle, capture: CaptureRef) -> CmdResult<bool> {
    let path = path_for(&app, &capture)?;
    Ok(reset_sidecar(&path)?)
}
