//! Opening recordings frame by frame (Studio's and the camera's), copying camera videos into
//! the cache, and region values across a whole recording.

use std::collections::VecDeque;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};

use serde::Serialize;
use tauri::ipc::Response;
use tauri::{Emitter, Manager, State};

use thermocam_core::disk_cache::evict_oldest;
use thermocam_core::paths::confine;
use thermocam_core::recording::{
    fetch_card_folder, frame_raw_bytes, open_recording, read_recording_frame, RecordError,
    RecordingIndex,
};
use thermocam_core::series::{compute_series, Series, SeriesRegion};

use crate::card::LiveCard;
use crate::error::{CmdError, CmdResult};
use crate::state::AppState;

const OPEN_RECORDINGS: usize = 8;
const SD_CACHE_CAP: u64 = 2 * 1024 * 1024 * 1024;

impl From<RecordError> for CmdError {
    fn from(e: RecordError) -> Self {
        CmdError::new(&e.kind, e.detail)
    }
}

/// Recently opened recordings, so each frame request does not re-read the folder.
#[derive(Default)]
pub struct RecordingCache {
    entries: VecDeque<(PathBuf, Arc<RecordingIndex>)>,
}

impl RecordingCache {
    pub fn get(&self, path: &Path) -> Option<Arc<RecordingIndex>> {
        self.entries
            .iter()
            .find(|(p, _)| p == path)
            .map(|(_, r)| Arc::clone(r))
    }

    pub fn put(&mut self, path: PathBuf, rec: Arc<RecordingIndex>) {
        self.entries.retain(|(p, _)| *p != path);
        self.entries.push_front((path, rec));
        self.entries.truncate(OPEN_RECORDINGS);
    }
}

pub fn absolute(path: &str) -> CmdResult<PathBuf> {
    let p = PathBuf::from(path);
    if !p.is_absolute() {
        return Err(CmdError::new("bad_path", "expected an absolute path"));
    }
    Ok(p)
}

/// Open (and remember) the recording at `path`.
pub fn open_cached(cache: &Mutex<RecordingCache>, path: &Path) -> CmdResult<Arc<RecordingIndex>> {
    let rec = Arc::new(open_recording(path)?);
    cache.lock()?.put(path.to_path_buf(), Arc::clone(&rec));
    Ok(rec)
}

pub fn get_or_open(cache: &Mutex<RecordingCache>, path: &Path) -> CmdResult<Arc<RecordingIndex>> {
    if let Some(rec) = cache.lock()?.get(path) {
        return Ok(rec);
    }
    open_cached(cache, path)
}

/// `{ frames, t_ms: number[] | null, width, height, source: "studio"|"timelapse"|"camera",
/// fps_nominal, sensor_material }`.
#[tauri::command]
pub async fn cmd_recording_open(
    state: State<'_, AppState>,
    path: String,
) -> CmdResult<RecordingIndex> {
    let cache = Arc::clone(&state.recordings);
    let path = absolute(&path)?;
    tauri::async_runtime::spawn_blocking(move || Ok((*open_cached(&cache, &path)?).clone()))
        .await
        .map_err(|e| CmdError::other(e.to_string()))?
}

/// Frame `index` as raw bytes in the live stream's `FrameRaw` layout (`ArrayBuffer`).
#[tauri::command]
pub async fn cmd_recording_frame(
    state: State<'_, AppState>,
    path: String,
    index: usize,
) -> CmdResult<Response> {
    let cache = Arc::clone(&state.recordings);
    let path = absolute(&path)?;
    tauri::async_runtime::spawn_blocking(move || {
        let rec = get_or_open(&cache, &path)?;
        let dat = read_recording_frame(&rec, index)?;
        let t = rec.t_ms.as_ref().and_then(|t| t.get(index).copied());
        Ok(Response::new(frame_raw_bytes(&dat, index as u32, t)))
    })
    .await
    .map_err(|e| CmdError::other(e.to_string()))?
}

#[derive(Clone, Serialize)]
struct FetchProgress {
    done: usize,
    total: usize,
}

const CACHED_SERIES: usize = 4;

/// Recently computed series, keyed by recording path, frame count and region set.
#[derive(Default)]
pub struct SeriesCache {
    entries: VecDeque<(String, Arc<Series>)>,
}

/// The series for `regions` over the recording at `path`, computed once per region set.
pub fn series_for(
    recordings: &Mutex<RecordingCache>,
    cache: &Mutex<SeriesCache>,
    path: &Path,
    regions: &[SeriesRegion],
    progress: impl FnMut(usize, usize),
) -> CmdResult<Arc<Series>> {
    let rec = get_or_open(recordings, path)?;
    let key = format!(
        "{}\0{}\0{}",
        path.display(),
        rec.frames,
        serde_json::to_string(regions).unwrap_or_default()
    );
    if let Some((_, s)) = cache.lock()?.entries.iter().find(|(k, _)| *k == key) {
        return Ok(Arc::clone(s));
    }
    let series = Arc::new(compute_series(&rec, regions, progress)?);
    let mut c = cache.lock()?;
    c.entries.push_front((key, Arc::clone(&series)));
    c.entries.truncate(CACHED_SERIES);
    Ok(series)
}

/// `{ t_ms: number[] | null, values: number[][] }` (`values[region][frame]`, °C), with
/// `series-progress {done, total}` events while it computes.
#[tauri::command]
pub async fn cmd_recording_series(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    path: String,
    regions: Vec<SeriesRegion>,
) -> CmdResult<Series> {
    let recordings = Arc::clone(&state.recordings);
    let cache = Arc::clone(&state.series_cache);
    let path = absolute(&path)?;
    tauri::async_runtime::spawn_blocking(move || {
        let series = series_for(&recordings, &cache, &path, &regions, |done, total| {
            let _ = app.emit("series-progress", FetchProgress { done, total });
        })?;
        Ok((*series).clone())
    })
    .await
    .map_err(|e| CmdError::other(e.to_string()))?
}

/// Copy a camera video folder into `app_cache_dir()/sd/<serial>/<folder>/` (with
/// `sd-fetch-progress {done, total}` events) and return the local folder to open.
#[tauri::command]
pub async fn cmd_sd_fetch_folder(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    sd_path: String,
) -> CmdResult<String> {
    let serial = state
        .camera_serial
        .lock()?
        .clone()
        .ok_or_else(CmdError::not_connected)?;
    let camera_root = confine(&app.path().app_cache_dir()?.join("sd"), &serial)?;
    let dest = confine(&camera_root, &sd_path)?;
    let client = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let local = fetch_card_folder(&LiveCard(&client), &sd_path, &dest, |done, total| {
            let _ = app.emit("sd-fetch-progress", FetchProgress { done, total });
        })?;
        let _ = evict_oldest(&camera_root, SD_CACHE_CAP, Some(&local));
        Ok(local.to_string_lossy().into_owned())
    })
    .await
    .map_err(|e| CmdError::other(e.to_string()))?
}
