//! The chart's session log under `app_cache_dir()/session/` (see `thermocam_core::session_log`).

use std::path::PathBuf;
use std::sync::Mutex;
use std::time::SystemTime;

use serde::Serialize;
use tauri::{Manager, State};

use thermocam_core::session_log::{LogSample, SessionLog};

use crate::error::{CmdError, CmdResult};
use crate::state::AppState;

fn session_dir(app: &tauri::AppHandle) -> CmdResult<PathBuf> {
    Ok(app.path().app_cache_dir()?.join("session"))
}

fn start(app: &tauri::AppHandle, slot: &Mutex<Option<SessionLog>>) -> CmdResult<String> {
    let log = SessionLog::start(&session_dir(app)?, SystemTime::now())?;
    let path = log.path().to_string_lossy().into_owned();
    *slot.lock()? = Some(log);
    Ok(path)
}

#[derive(Serialize)]
pub struct SessionStarted {
    pub path: String,
}

/// Start a new session log (at connect). Logs older than 7 days are deleted.
#[tauri::command]
pub fn cmd_session_log_start(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
) -> CmdResult<SessionStarted> {
    Ok(SessionStarted {
        path: start(&app, &state.session_log)?,
    })
}

/// Append `[{t_ms, region_id, value_c}]` (about once a second). Starts a log if none is open.
#[tauri::command]
pub fn cmd_session_log_append(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    batch: Vec<LogSample>,
) -> CmdResult<()> {
    if state.session_log.lock()?.is_none() {
        start(&app, &state.session_log)?;
    }
    let mut slot = state.session_log.lock()?;
    let log = slot
        .as_mut()
        .ok_or_else(|| CmdError::other("no session log"))?;
    log.append(&batch)?;
    Ok(())
}

/// Copy the whole session to `out_path` (or where the Save dialog says) and return the path.
#[tauri::command]
pub async fn cmd_session_log_export(
    state: State<'_, AppState>,
    out_path: Option<String>,
) -> CmdResult<String> {
    let target = match out_path {
        Some(p) => PathBuf::from(p),
        None => rfd::AsyncFileDialog::new()
            .add_filter("CSV", &["csv"])
            .set_file_name("thermocam-session.csv")
            .save_file()
            .await
            .ok_or_else(|| CmdError::new("cancelled", "No file was chosen."))?
            .path()
            .to_path_buf(),
    };
    let mut slot = state.session_log.lock()?;
    let log = slot
        .as_mut()
        .ok_or_else(|| CmdError::new("no_session", "Nothing has been logged yet."))?;
    log.export(&target)?;
    Ok(target.to_string_lossy().into_owned())
}
