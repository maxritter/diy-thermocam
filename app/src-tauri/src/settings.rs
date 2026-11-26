//! Studio's settings file, `app_config_dir()/settings.json`.
//!
//! The frontend owns the keys (unit, theme, Engineering details, PNG on snapshot, button
//! action, material switch, drawer height, last palette and range). Rust stores the object as
//! given and owns one key itself: `captures_dir`, written only by `cmd_set_captures_dir`.

use std::fs;
use std::path::{Path, PathBuf};

use serde_json::{Map, Value};
use tauri::Manager;

use thermocam_core::atomic_file::write_atomic;

use crate::error::{CmdError, CmdResult};

pub const CAPTURES_DIR_KEY: &str = "captures_dir";

pub fn settings_path(app: &tauri::AppHandle) -> CmdResult<PathBuf> {
    Ok(app.path().app_config_dir()?.join("settings.json"))
}

/// Read the settings object; a missing or damaged file gives an empty object.
pub fn read_settings(path: &Path) -> Map<String, Value> {
    fs::read_to_string(path)
        .ok()
        .and_then(|s| serde_json::from_str::<Value>(&s).ok())
        .and_then(|v| match v {
            Value::Object(m) => Some(m),
            _ => None,
        })
        .unwrap_or_default()
}

pub fn write_settings(path: &Path, settings: &Map<String, Value>) -> CmdResult<()> {
    let json = serde_json::to_vec_pretty(settings).map_err(|e| CmdError::other(e.to_string()))?;
    write_atomic(path, &json)?;
    Ok(())
}

pub fn stored_captures_dir(app: &tauri::AppHandle) -> Option<PathBuf> {
    let path = settings_path(app).ok()?;
    read_settings(&path)
        .get(CAPTURES_DIR_KEY)
        .and_then(Value::as_str)
        .filter(|s| !s.is_empty())
        .map(PathBuf::from)
}

pub fn store_captures_dir(app: &tauri::AppHandle, dir: &Path) -> CmdResult<()> {
    let path = settings_path(app)?;
    let mut settings = read_settings(&path);
    settings.insert(
        CAPTURES_DIR_KEY.to_string(),
        Value::String(dir.to_string_lossy().into_owned()),
    );
    write_settings(&path, &settings)
}

/// Returns the stored settings object (`{}` on first start).
#[tauri::command]
pub fn cmd_settings_load(app: tauri::AppHandle) -> CmdResult<Value> {
    Ok(Value::Object(read_settings(&settings_path(&app)?)))
}

/// Replaces the stored settings with `settings` (an object). `captures_dir` is kept as stored.
#[tauri::command]
pub fn cmd_settings_save(app: tauri::AppHandle, settings: Value) -> CmdResult<()> {
    let Value::Object(mut incoming) = settings else {
        return Err(CmdError::new("bad_settings", "settings must be an object"));
    };
    let path = settings_path(&app)?;
    let current = read_settings(&path);
    match current.get(CAPTURES_DIR_KEY) {
        Some(dir) => {
            incoming.insert(CAPTURES_DIR_KEY.to_string(), dir.clone());
        }
        None => {
            incoming.remove(CAPTURES_DIR_KEY);
        }
    }
    write_settings(&path, &incoming)
}
