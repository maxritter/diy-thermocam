//! Record and time-lapse: the stream thread writes each frame to the Captures folder as it
//! arrives (`thermocam_core::recording`); these commands start and stop it.

use std::path::PathBuf;
use std::sync::{Arc, Mutex};
use std::time::Instant;

use serde::Serialize;
use tauri::{Emitter, State};

use thermocam_core::captures::unique_path;
use thermocam_core::protocol::codec::FrameRaw;
use thermocam_core::radiometry::Material;
use thermocam_core::recording::{
    PushOutcome, RecordingInfo, RecordingOptions, RecordingSource, RecordingWriter, StdSink,
    StopReason,
};

use crate::captures::captures_dir;
use crate::error::{CmdError, CmdResult};
use crate::platform::{free_bytes, local_now};
use crate::state::AppState;

pub const RECORDING_STOPPED_EVENT: &str = "recording-stopped";

pub struct ActiveRecording {
    pub writer: RecordingWriter<StdSink>,
    pub started: Instant,
}

impl ActiveRecording {
    pub fn is_timelapse(&self) -> bool {
        self.writer.source() == RecordingSource::Timelapse
    }
}

/// Finish the active recording (if any), remember it, and send `recording-stopped`.
pub fn finish_recording(
    app: &tauri::AppHandle,
    slot: &Mutex<Option<ActiveRecording>>,
    last: &Mutex<Option<PathBuf>>,
    reason: StopReason,
) -> Option<RecordingInfo> {
    let active = slot.lock().ok()?.take()?;
    let info = active.writer.finish(reason);
    if let Ok(mut l) = last.lock() {
        *l = Some(PathBuf::from(&info.path));
    }
    let _ = app.emit(RECORDING_STOPPED_EVENT, &info);
    Some(info)
}

/// Called by the stream thread for every frame.
pub fn record_frame(
    app: &tauri::AppHandle,
    slot: &Mutex<Option<ActiveRecording>>,
    last: &Mutex<Option<PathBuf>>,
    frame: &FrameRaw,
) {
    let stopped = {
        let Ok(mut guard) = slot.lock() else {
            return;
        };
        let Some(active) = guard.as_mut() else {
            return;
        };
        let t_ms = u32::try_from(active.started.elapsed().as_millis()).unwrap_or(u32::MAX);
        matches!(active.writer.push(frame, t_ms), PushOutcome::Stopped(_))
    };
    if stopped {
        // The writer keeps the reason it stopped for (write error, low disk).
        finish_recording(app, slot, last, StopReason::User);
    }
}

/// Called when the camera goes away: a recording stops and keeps its frames; a time-lapse
/// waits and continues when frames arrive again.
pub fn on_camera_lost(
    app: &tauri::AppHandle,
    slot: &Mutex<Option<ActiveRecording>>,
    last: &Mutex<Option<PathBuf>>,
) {
    let is_recording = slot
        .lock()
        .ok()
        .and_then(|g| g.as_ref().map(|a| !a.is_timelapse()))
        .unwrap_or(false);
    if is_recording {
        finish_recording(app, slot, last, StopReason::Disconnected);
    }
}

#[derive(Serialize)]
pub struct RecordingStarted {
    pub path: String,
}

/// Start writing frames to a new `YYYYMMDDHHMMSS/` folder in the Captures folder.
/// `interval_ms` makes it a time-lapse (one frame per interval). `sensor_material` is what the
/// camera's sensor holds now (`null` = factory) and is stored in `recording.json`.
pub fn start_recording(
    app: &tauri::AppHandle,
    state: &AppState,
    interval_ms: Option<u32>,
    sensor_material: Option<Material>,
) -> CmdResult<RecordingStarted> {
    if state.client.lock()?.is_none() {
        return Err(CmdError::not_connected());
    }
    let mut slot = state.recording.lock()?;
    if slot.is_some() {
        return Err(CmdError::new(
            "already_recording",
            "A recording is already running.",
        ));
    }
    let root = captures_dir(app)?;
    let dir = unique_path(&root, &local_now().to_name(), None);
    let opts = match interval_ms {
        Some(ms) => RecordingOptions::timelapse(ms, sensor_material),
        None => RecordingOptions::studio(sensor_material),
    };
    let probe = dir.clone();
    let writer = RecordingWriter::start(
        dir.clone(),
        opts,
        StdSink,
        Box::new(move || free_bytes(&probe)),
    )
    .map_err(|e| CmdError::new(&e.kind, e.detail))?;
    *slot = Some(ActiveRecording {
        writer,
        started: Instant::now(),
    });
    Ok(RecordingStarted {
        path: dir.to_string_lossy().into_owned(),
    })
}

#[tauri::command]
pub fn cmd_record_start(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    interval_ms: Option<u32>,
    sensor_material: Option<Material>,
) -> CmdResult<RecordingStarted> {
    start_recording(&app, &state, interval_ms, sensor_material)
}

#[tauri::command]
pub fn cmd_record_stop(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
) -> CmdResult<RecordingInfo> {
    finish_recording(
        &app,
        &state.recording,
        &state.last_recording,
        StopReason::User,
    )
    .ok_or_else(|| CmdError::new("not_recording", "No recording is running."))
}

#[derive(Serialize)]
pub struct RecordingStatus {
    pub path: String,
    pub frames: usize,
    pub elapsed_ms: u64,
    pub source: RecordingSource,
}

/// The running recording, or `null`.
#[tauri::command]
pub fn cmd_record_status(state: State<'_, AppState>) -> CmdResult<Option<RecordingStatus>> {
    Ok(state.recording.lock()?.as_ref().map(|a| RecordingStatus {
        path: a.writer.dir().to_string_lossy().into_owned(),
        frames: a.writer.frames(),
        elapsed_ms: a.started.elapsed().as_millis() as u64,
        source: a.writer.source(),
    }))
}

/// Shared handles the stream thread needs.
#[derive(Clone)]
pub struct RecorderHandles {
    pub slot: Arc<Mutex<Option<ActiveRecording>>>,
    pub last: Arc<Mutex<Option<PathBuf>>>,
}

impl RecorderHandles {
    pub fn from_state(state: &AppState) -> Self {
        Self {
            slot: Arc::clone(&state.recording),
            last: Arc::clone(&state.last_recording),
        }
    }
}
