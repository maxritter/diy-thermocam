//! Card pictures for the Captures library, rendered in Rust on a worker thread and cached
//! on disk under `app_cache_dir()/thumbs/` by capture identity and palette.

use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Arc;

use tauri::ipc::Response;
use tauri::{Manager, State};

use thermocam_core::disk_cache::{evict_oldest, DiskCache};
use thermocam_core::picture::{render_png, PictureFrame, RenderSpec};
use thermocam_core::sidecar::CaptureRef;

use crate::error::{CmdError, CmdResult};
use crate::pictures::{identify_capture, read_identified};
use crate::state::{resolve_palette, AppState};

const THUMB_SCALE: u32 = 2;
const THUMB_CACHE_CAP: u64 = 256 * 1024 * 1024;
const RENDER_VERSION: &str = "thumb-v1";
static PUTS: AtomicU32 = AtomicU32::new(0);

/// PNG bytes (`ArrayBuffer`, 320 × 240) of a capture's first frame in `palette_id`.
#[tauri::command]
pub async fn cmd_thumbnail(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    capture: CaptureRef,
    palette_id: String,
) -> CmdResult<Response> {
    let palette =
        resolve_palette(&state, &palette_id).map_err(|e| CmdError::new("bad_palette", e))?;
    let cache = DiskCache::new(app.path().app_cache_dir()?.join("thumbs"));
    let client = Arc::clone(&state.client);
    let serial = Arc::clone(&state.camera_serial);
    tauri::async_runtime::spawn_blocking(move || {
        // Identify from metadata first; the frame is read only on a cache miss.
        let frame = identify_capture(&client, &serial, &capture)?;
        let mut parts: Vec<&str> = frame.identity.iter().map(String::as_str).collect();
        parts.push(&palette_id);
        parts.push(RENDER_VERSION);
        let key = DiskCache::key(&parts);
        let (png, hit) = cache.get_or_insert_with(&key, || {
            let dat = read_identified(&client, &frame)?;
            let spec = RenderSpec {
                palette_id: palette_id.clone(),
                range: Default::default(),
                material: None,
                sensor_material: None,
                overlays: Default::default(),
                scale: THUMB_SCALE,
            };
            render_png(&PictureFrame::from_dat(&dat), &spec, &palette)
                .map(|(_, _, png)| png)
                .map_err(|e| CmdError::other(e.to_string()))
        })?;
        if !hit && PUTS.fetch_add(1, Ordering::Relaxed).is_multiple_of(32) {
            let _ = evict_oldest(cache.root(), THUMB_CACHE_CAP, None);
        }
        Ok(Response::new(png))
    })
    .await
    .map_err(|e| CmdError::other(e.to_string()))?
}
