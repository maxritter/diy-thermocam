//! Typed Tauri v2 commands, managed studio state, raw binary frame channel, and `thermo://` protocol.

use std::collections::{HashMap, VecDeque};
use std::fs::{self, File};
use std::io::BufWriter;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
use tauri::ipc::{Channel, InvokeResponseBody};
use tauri::{Emitter, Manager, State};

use thermocam_core::dat::{format_utc_now_iso8601, SequenceManifest, ThermalDat, ThermoSidecar};
use thermocam_core::export::{
    derive_sequence_fps, encode_gif_streaming, encode_h264_mp4_streaming, encode_jpeg, encode_png,
    encode_radiometric_tiff_16, encode_temperature_csv, generate_html_report,
    generate_markdown_report, render_export_canvas, render_export_canvas_with_scratch,
    ExportOptions, ExportResolution,
};
use thermocam_core::flasher::{
    flash_firmware, parse_teensy41_hex, trigger_legacy_134_baud, FirmwareImage, RealHalfKayDevice,
};
use thermocam_core::imaging::{DisplayPipeline, RenderScratch};
use thermocam_core::palettes::{
    builtin_palettes, find_palette, parse_custom_palette_png, parse_custom_palette_text, Palette,
};
use thermocam_core::protocol::codec::{FrameRaw, FrameRawMeta, HelloResp, StreamCtrl};
use thermocam_core::protocol::messages::{
    AsyncEvent, DeviceConfig, Diagnostics, FsEntry, FsInfoResp, PerfTelemetry, RadParams,
};
use thermocam_core::protocol::transport::{
    list_ports, ProtocolClient, SerialPortInfo, SerialTransport, SimulatorTransport,
};
use thermocam_core::protocol::MsgType;
use thermocam_core::radiometry::{RadiometryParams, TempUnit, MATERIAL_PRESETS};
use thermocam_core::roi::{evaluate_roi, Roi, RoiStats};

const MAX_RECORDED_FRAMES: usize = 6_000;
const MAX_SD_CACHE_ENTRIES: usize = 64;

enum ActiveTransport {
    Serial(Box<ProtocolClient<SerialTransport>>),
    Simulator(Box<ProtocolClient<SimulatorTransport>>),
}

impl ActiveTransport {
    fn hello(&mut self) -> Result<HelloResp, String> {
        match self {
            Self::Serial(c) => c.hello().map_err(|e| e.to_string()),
            Self::Simulator(c) => c.hello().map_err(|e| e.to_string()),
        }
    }
    fn set_stream(&mut self, ctrl: StreamCtrl) -> Result<StreamCtrl, String> {
        match self {
            Self::Serial(c) => c.set_stream(ctrl).map_err(|e| e.to_string()),
            Self::Simulator(c) => c.set_stream(ctrl).map_err(|e| e.to_string()),
        }
    }
    fn request_timeout(
        &mut self,
        mt: MsgType,
        payload: &[u8],
        timeout: Duration,
    ) -> Result<Vec<u8>, String> {
        let pkt = match self {
            Self::Serial(c) => c.request(mt, payload, timeout).map_err(|e| e.to_string())?,
            Self::Simulator(c) => c.request(mt, payload, timeout).map_err(|e| e.to_string())?,
        };
        Ok(pkt.payload)
    }
    fn request(&mut self, mt: MsgType, payload: &[u8]) -> Result<Vec<u8>, String> {
        self.request_timeout(mt, payload, Duration::from_millis(2000))
    }
    fn send_ping_nowait(&mut self, host_time_us: u64) -> Result<(), String> {
        match self {
            Self::Serial(c) => c.send_ping_nowait(host_time_us).map_err(|e| e.to_string()),
            Self::Simulator(c) => c.send_ping_nowait(host_time_us).map_err(|e| e.to_string()),
        }
    }
    fn poll_packets(
        &mut self,
        timeout: Duration,
    ) -> Result<Vec<thermocam_core::protocol::Packet>, String> {
        match self {
            Self::Serial(c) => c.poll_packets(timeout).map_err(|e| e.to_string()),
            Self::Simulator(c) => c.poll_packets(timeout).map_err(|e| e.to_string()),
        }
    }
    fn list_dir_all(&mut self, path: &str) -> Result<Vec<FsEntry>, String> {
        match self {
            Self::Serial(c) => c.list_dir_all(path).map_err(|e| e.to_string()),
            Self::Simulator(c) => c.list_dir_all(path).map_err(|e| e.to_string()),
        }
    }
    fn read_file(&mut self, path: &str) -> Result<Vec<u8>, String> {
        match self {
            Self::Serial(c) => c.read_file_bytes(path).map_err(|e| e.to_string()),
            Self::Simulator(c) => c.read_file_bytes(path).map_err(|e| e.to_string()),
        }
    }
    fn delete_file(&mut self, path: &str) -> Result<(), String> {
        match self {
            Self::Serial(c) => c.delete_file(path).map_err(|e| e.to_string()),
            Self::Simulator(c) => c.delete_file(path).map_err(|e| e.to_string()),
        }
    }
}

#[derive(Default)]
struct BoundedSdCache {
    map: HashMap<String, Vec<u8>>,
    order: VecDeque<String>,
}

impl BoundedSdCache {
    fn insert(&mut self, key: String, val: Vec<u8>) {
        if let Some(existing) = self.map.get_mut(&key) {
            *existing = val;
            return;
        }
        while self.map.len() >= MAX_SD_CACHE_ENTRIES {
            if let Some(oldest) = self.order.pop_front() {
                self.map.remove(&oldest);
            } else {
                break;
            }
        }
        self.order.push_back(key.clone());
        self.map.insert(key, val);
    }

    fn get(&self, key: &str) -> Option<&Vec<u8>> {
        self.map.get(key)
    }

    fn remove(&mut self, key: &str) {
        self.map.remove(key);
        self.order.retain(|k| k != key);
    }
}

pub struct AppState {
    client: Arc<Mutex<Option<ActiveTransport>>>,
    streaming: Arc<AtomicBool>,
    stream_gen: Arc<AtomicU64>,
    stream_lifecycle: Arc<Mutex<()>>,
    stream_thread: Arc<Mutex<Option<thread::JoinHandle<()>>>>,
    latest_frame: Arc<Mutex<Option<Arc<FrameRaw>>>>,
    recorded_frames: Arc<Mutex<Vec<Arc<FrameRaw>>>>,
    recording: Arc<AtomicBool>,
    custom_palettes: Arc<Mutex<HashMap<String, Palette>>>,
    sd_cache: Arc<Mutex<BoundedSdCache>>,
}

impl Default for AppState {
    fn default() -> Self {
        Self {
            client: Arc::new(Mutex::new(None)),
            streaming: Arc::new(AtomicBool::new(false)),
            stream_gen: Arc::new(AtomicU64::new(0)),
            stream_lifecycle: Arc::new(Mutex::new(())),
            stream_thread: Arc::new(Mutex::new(None)),
            latest_frame: Arc::new(Mutex::new(None)),
            recorded_frames: Arc::new(Mutex::new(Vec::new())),
            recording: Arc::new(AtomicBool::new(false)),
            custom_palettes: Arc::new(Mutex::new(HashMap::new())),
            sd_cache: Arc::new(Mutex::new(BoundedSdCache::default())),
        }
    }
}

fn stop_stream_and_join(
    streaming: &AtomicBool,
    stream_gen: &AtomicU64,
    stream_thread: &Mutex<Option<thread::JoinHandle<()>>>,
) {
    stream_gen.fetch_add(1, Ordering::SeqCst);
    streaming.store(false, Ordering::SeqCst);
    let handle = stream_thread.lock().ok().and_then(|mut g| g.take());
    if let Some(h) = handle {
        let _ = h.join();
    }
}

fn resolve_palette(state: &AppState, id: &str) -> Result<Palette, String> {
    if let Ok(p) = find_palette(id) {
        return Ok(p);
    }
    state
        .custom_palettes
        .lock()
        .map_err(|e| e.to_string())?
        .get(id)
        .cloned()
        .ok_or_else(|| format!("palette '{id}' not found"))
}

#[tauri::command]
fn cmd_list_ports() -> Vec<SerialPortInfo> {
    list_ports()
}

#[tauri::command]
fn cmd_get_palettes(state: State<'_, AppState>) -> Vec<Palette> {
    let mut list = builtin_palettes();
    if let Ok(custom) = state.custom_palettes.lock() {
        list.extend(custom.values().cloned());
    }
    list
}

#[tauri::command]
fn cmd_get_material_presets() -> Vec<serde_json::Value> {
    MATERIAL_PRESETS
        .iter()
        .filter_map(|m| serde_json::to_value(m).ok())
        .collect()
}

#[tauri::command]
fn cmd_import_custom_palette(
    state: State<'_, AppState>,
    name: String,
    data: String,
    is_png_base64: bool,
) -> Result<Palette, String> {
    let pal = if is_png_base64 {
        use base64::Engine as _;
        let bytes = base64::engine::general_purpose::STANDARD
            .decode(data.as_bytes())
            .map_err(|e| e.to_string())?;
        parse_custom_palette_png(&name, &bytes).map_err(|e| e.to_string())?
    } else {
        parse_custom_palette_text(&name, &data).map_err(|e| e.to_string())?
    };
    state
        .custom_palettes
        .lock()
        .map_err(|e| e.to_string())?
        .insert(pal.id.clone(), pal.clone());
    Ok(pal)
}

#[tauri::command]
async fn cmd_connect(state: State<'_, AppState>, port: String) -> Result<HelloResp, String> {
    let client_arc = Arc::clone(&state.client);
    let streaming = Arc::clone(&state.streaming);
    let stream_gen = Arc::clone(&state.stream_gen);
    let stream_lifecycle = Arc::clone(&state.stream_lifecycle);
    let stream_thread = Arc::clone(&state.stream_thread);
    tauri::async_runtime::spawn_blocking(move || {
        let _lifecycle = stream_lifecycle.lock().map_err(|e| e.to_string())?;
        stop_stream_and_join(&streaming, &stream_gen, &stream_thread);
        let mut active = if port.starts_with("sim://") {
            ActiveTransport::Simulator(Box::new(ProtocolClient::new(SimulatorTransport::new())))
        } else {
            let serial = SerialTransport::open(&port).map_err(|e| e.to_string())?;
            ActiveTransport::Serial(Box::new(ProtocolClient::new(serial)))
        };
        let hello = active.hello()?;
        *client_arc.lock().map_err(|e| e.to_string())? = Some(active);
        Ok(hello)
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_disconnect(state: State<'_, AppState>) -> Result<(), String> {
    let client_arc = Arc::clone(&state.client);
    let streaming = Arc::clone(&state.streaming);
    let stream_gen = Arc::clone(&state.stream_gen);
    let stream_lifecycle = Arc::clone(&state.stream_lifecycle);
    let stream_thread = Arc::clone(&state.stream_thread);
    tauri::async_runtime::spawn_blocking(move || {
        let _lifecycle = stream_lifecycle.lock().map_err(|e| e.to_string())?;
        stop_stream_and_join(&streaming, &stream_gen, &stream_thread);
        let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
        if let Some(mut c) = guard.take() {
            let _ = c.request_timeout(MsgType::GoodbyeReq, &[], Duration::from_millis(500));
        }
        Ok(())
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Stream raw `u16` frames + 64-byte LE metadata directly over Tauri v2 `Channel<InvokeResponseBody::Raw>`.
/// Uses a per-stream generation token, joins any prior poll thread, reads continuously with a short
/// 10 ms timeout (sub-5ms delivery), sends non-blocking heartbeats, and emits `camera-disconnected` on unplug.
#[tauri::command]
async fn cmd_start_stream(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    channel: Channel<InvokeResponseBody>,
) -> Result<(), String> {
    let client_arc = Arc::clone(&state.client);
    let streaming_arc = Arc::clone(&state.streaming);
    let stream_gen_arc = Arc::clone(&state.stream_gen);
    let stream_lifecycle = Arc::clone(&state.stream_lifecycle);
    let stream_thread_arc = Arc::clone(&state.stream_thread);
    let latest_arc = Arc::clone(&state.latest_frame);
    let rec_flag = Arc::clone(&state.recording);
    let rec_buf = Arc::clone(&state.recorded_frames);

    tauri::async_runtime::spawn_blocking(move || {
        let _lifecycle = stream_lifecycle.lock().map_err(|e| e.to_string())?;
        stop_stream_and_join(&streaming_arc, &stream_gen_arc, &stream_thread_arc);
        {
            let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
            let client = guard.as_mut().ok_or("not connected")?;
            client.set_stream(StreamCtrl {
                mode: 1,
                keep_lcd_on: true,
                include_temp_points: true,
                perf_interval_s: 1,
                max_fps_x10: 0,
            })?;
        }
        let my_gen = stream_gen_arc.fetch_add(1, Ordering::SeqCst) + 1;
        streaming_arc.store(true, Ordering::SeqCst);

        let worker_client = Arc::clone(&client_arc);
        let worker_streaming = Arc::clone(&streaming_arc);
        let worker_gen = Arc::clone(&stream_gen_arc);
        let handle = thread::spawn(move || {
            let started = Instant::now();
            let mut last_ping = Instant::now();
            let mut consecutive_io_errors: u32 = 0;

            while worker_gen.load(Ordering::SeqCst) == my_gen
                && worker_streaming.load(Ordering::SeqCst)
            {
                let tick = Instant::now();
                let poll_res = {
                    let mut guard = match worker_client.lock() {
                        Ok(g) => g,
                        Err(_) => break,
                    };
                    match guard.as_mut() {
                        Some(c) => {
                            let mut ping_err = None;
                            if last_ping.elapsed() >= Duration::from_secs(2) {
                                let us = started.elapsed().as_micros() as u64;
                                if let Err(e) = c.send_ping_nowait(us) {
                                    ping_err = Some(e);
                                }
                                last_ping = Instant::now();
                            }
                            if let Some(e) = ping_err {
                                Err(e)
                            } else {
                                c.poll_packets(Duration::from_millis(10))
                            }
                        }
                        None => break,
                    }
                };

                match poll_res {
                    Ok(pkts) => {
                        consecutive_io_errors = 0;
                        let had_pkts = !pkts.is_empty();
                        for pkt in pkts {
                            if worker_gen.load(Ordering::SeqCst) != my_gen {
                                break;
                            }
                            if pkt.msg_type().ok() == Some(MsgType::FrameRaw) {
                                if let Ok(fr) = FrameRaw::from_bytes(&pkt.payload) {
                                    let fr_arc = Arc::new(fr);
                                    if let Ok(mut latest) = latest_arc.lock() {
                                        *latest = Some(Arc::clone(&fr_arc));
                                    }
                                    if rec_flag.load(Ordering::SeqCst) {
                                        if let Ok(mut buf) = rec_buf.lock() {
                                            if buf.len() < MAX_RECORDED_FRAMES {
                                                buf.push(Arc::clone(&fr_arc));
                                            }
                                        }
                                    }
                                    // Send raw wire payload directly (zero redundant re-encoding)
                                    if channel.send(InvokeResponseBody::Raw(pkt.payload)).is_err() {
                                        if worker_gen.load(Ordering::SeqCst) == my_gen {
                                            worker_streaming.store(false, Ordering::SeqCst);
                                        }
                                        return;
                                    }
                                }
                            } else if let Ok(Some(ev)) = AsyncEvent::from_packet(&pkt) {
                                let is_exit = matches!(ev, AsyncEvent::SessionExit { .. });
                                let _ = app.emit("camera-event", ev);
                                if is_exit && worker_gen.load(Ordering::SeqCst) == my_gen {
                                    worker_streaming.store(false, Ordering::SeqCst);
                                    let _ = app.emit(
                                        "camera-disconnected",
                                        serde_json::json!({ "reason": "camera exited session" }),
                                    );
                                    return;
                                }
                            }
                        }
                        if !had_pkts && tick.elapsed() < Duration::from_millis(2) {
                            thread::sleep(Duration::from_millis(2));
                        }
                    }
                    Err(err) => {
                        consecutive_io_errors += 1;
                        if consecutive_io_errors >= 5 {
                            if worker_gen.load(Ordering::SeqCst) == my_gen {
                                if let Ok(mut g) = worker_client.lock() {
                                    *g = None;
                                }
                                worker_streaming.store(false, Ordering::SeqCst);
                                let _ = app.emit(
                                    "camera-disconnected",
                                    serde_json::json!({ "reason": err }),
                                );
                            }
                            break;
                        }
                        thread::sleep(Duration::from_millis(15));
                    }
                }
            }
        });
        if let Ok(mut slot) = stream_thread_arc.lock() {
            *slot = Some(handle);
        }
        Ok(())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_stop_stream(state: State<'_, AppState>) -> Result<(), String> {
    let client_arc = Arc::clone(&state.client);
    let streaming = Arc::clone(&state.streaming);
    let stream_gen = Arc::clone(&state.stream_gen);
    let stream_lifecycle = Arc::clone(&state.stream_lifecycle);
    let stream_thread = Arc::clone(&state.stream_thread);
    tauri::async_runtime::spawn_blocking(move || {
        let _lifecycle = stream_lifecycle.lock().map_err(|e| e.to_string())?;
        stop_stream_and_join(&streaming, &stream_gen, &stream_thread);
        if let Ok(mut guard) = client_arc.lock() {
            if let Some(c) = guard.as_mut() {
                let _ = c.set_stream(StreamCtrl {
                    mode: 0,
                    keep_lcd_on: true,
                    include_temp_points: false,
                    perf_interval_s: 0,
                    max_fps_x10: 0,
                });
            }
        }
        Ok(())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_get_config(state: State<'_, AppState>) -> Result<DeviceConfig, String> {
    let client_arc = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
        let payload = guard
            .as_mut()
            .ok_or("not connected")?
            .request(MsgType::GetConfigReq, &[])?;
        DeviceConfig::from_bytes(&payload).map_err(|e| e.to_string())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_set_config(
    state: State<'_, AppState>,
    config: DeviceConfig,
    persist: bool,
) -> Result<DeviceConfig, String> {
    let client_arc = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
        let payload = guard
            .as_mut()
            .ok_or("not connected")?
            .request(MsgType::SetConfigReq, &config.to_set_req_bytes(persist))?;
        DeviceConfig::from_bytes(&payload).map_err(|e| e.to_string())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_trigger_ffc(state: State<'_, AppState>, force: bool) -> Result<(), String> {
    let client_arc = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
        let _ = guard.as_mut().ok_or("not connected")?.request_timeout(
            MsgType::TriggerFfcReq,
            &[u8::from(force)],
            Duration::from_millis(4000),
        )?;
        Ok(())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_get_diagnostics(state: State<'_, AppState>) -> Result<Diagnostics, String> {
    let client_arc = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
        let payload = guard
            .as_mut()
            .ok_or("not connected")?
            .request(MsgType::GetDiagnosticsReq, &[])?;
        Diagnostics::from_bytes(&payload).map_err(|e| e.to_string())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_get_perf(state: State<'_, AppState>, reset: bool) -> Result<PerfTelemetry, String> {
    let client_arc = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
        let payload = guard
            .as_mut()
            .ok_or("not connected")?
            .request(MsgType::GetPerfReq, &[u8::from(reset)])?;
        PerfTelemetry::from_bytes(&payload).map_err(|e| e.to_string())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_get_rad_params(state: State<'_, AppState>) -> Result<RadParams, String> {
    let client_arc = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
        let payload = guard
            .as_mut()
            .ok_or("not connected")?
            .request(MsgType::GetRadParamsReq, &[])?;
        RadParams::from_bytes(&payload).map_err(|e| e.to_string())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_set_rad_params(
    state: State<'_, AppState>,
    params: RadParams,
) -> Result<RadParams, String> {
    let client_arc = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
        let payload = guard
            .as_mut()
            .ok_or("not connected")?
            .request(MsgType::SetRadParamsReq, &params.to_bytes())?;
        RadParams::from_bytes(&payload).map_err(|e| e.to_string())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[derive(Serialize)]
struct SdListing {
    info: FsInfoResp,
    entries: Vec<FsEntry>,
}

#[tauri::command]
async fn cmd_sd_list(state: State<'_, AppState>, path: String) -> Result<SdListing, String> {
    let client_arc = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
        let client = guard.as_mut().ok_or("not connected")?;
        let info_b = client.request(MsgType::FsInfoReq, &[])?;
        let info = FsInfoResp::from_bytes(&info_b).map_err(|e| e.to_string())?;
        let entries = client.list_dir_all(&path)?;
        Ok(SdListing { info, entries })
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_sd_read_dat(state: State<'_, AppState>, path: String) -> Result<ThermalDat, String> {
    let client_arc = Arc::clone(&state.client);
    let sd_cache = Arc::clone(&state.sd_cache);
    tauri::async_runtime::spawn_blocking(move || {
        let bytes = {
            let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
            guard.as_mut().ok_or("not connected")?.read_file(&path)?
        };
        if let Ok(mut cache) = sd_cache.lock() {
            cache.insert(path, bytes.clone());
        }
        ThermalDat::from_bytes(&bytes).map_err(|e| e.to_string())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_sd_delete(state: State<'_, AppState>, path: String) -> Result<(), String> {
    let client_arc = Arc::clone(&state.client);
    let sd_cache = Arc::clone(&state.sd_cache);
    tauri::async_runtime::spawn_blocking(move || {
        {
            let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
            guard.as_mut().ok_or("not connected")?.delete_file(&path)?;
        }
        if let Ok(mut cache) = sd_cache.lock() {
            cache.remove(&path);
        }
        Ok(())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_sd_download(
    state: State<'_, AppState>,
    path: String,
    output_path: Option<String>,
) -> Result<String, String> {
    let file_name = path.trim_start_matches('/').to_string();
    let target = match output_path {
        Some(p) => PathBuf::from(p),
        None => rfd::AsyncFileDialog::new()
            .set_file_name(&file_name)
            .save_file()
            .await
            .ok_or("cancelled")?
            .path()
            .to_path_buf(),
    };
    let client_arc = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let bytes = {
            let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
            guard.as_mut().ok_or("not connected")?.read_file(&path)?
        };
        fs::write(&target, bytes).map_err(|e| e.to_string())?;
        Ok(target.to_string_lossy().into_owned())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
fn cmd_save_sidecar(dat_path: String, sidecar: ThermoSidecar) -> Result<String, String> {
    let path = PathBuf::from(dat_path).with_extension("thermo.json");
    let json = serde_json::to_string_pretty(&sidecar).map_err(|e| e.to_string())?;
    fs::write(&path, json).map_err(|e| e.to_string())?;
    Ok(path.to_string_lossy().into_owned())
}

#[tauri::command]
fn cmd_verify_hex(
    hex_text: String,
    expected_sha256: Option<String>,
) -> Result<FirmwareImage, String> {
    parse_teensy41_hex(&hex_text, expected_sha256.as_deref()).map_err(|e| e.to_string())
}

#[tauri::command]
fn cmd_pick_hex_file() -> Result<(String, String), String> {
    let file_path = rfd::FileDialog::new()
        .add_filter("Intel HEX Firmware", &["hex", "HEX"])
        .pick_file()
        .ok_or("cancelled")?;
    let text = fs::read_to_string(&file_path).map_err(|e| e.to_string())?;
    Ok((file_path.to_string_lossy().into_owned(), text))
}

#[tauri::command]
fn cmd_load_dat_file(
    path: Option<String>,
) -> Result<(String, ThermalDat, Option<ThermoSidecar>), String> {
    let file_path = match path {
        Some(p) => PathBuf::from(p),
        None => rfd::FileDialog::new()
            .add_filter("Thermal DAT", &["DAT", "dat"])
            .pick_file()
            .ok_or("cancelled")?,
    };
    let bytes = fs::read(&file_path).map_err(|e| e.to_string())?;
    let dat = ThermalDat::from_bytes(&bytes).map_err(|e| e.to_string())?;
    let sidecar_path = file_path.with_extension("thermo.json");
    let sidecar = fs::read_to_string(&sidecar_path)
        .ok()
        .and_then(|s| serde_json::from_str(&s).ok());
    Ok((file_path.to_string_lossy().into_owned(), dat, sidecar))
}

#[derive(Deserialize)]
pub struct ExportRequest {
    pub format: String,
    pub output_path: Option<String>,
    pub palette_id: String,
    pub min_raw: u16,
    pub max_raw: u16,
    pub rad: RadiometryParams,
    pub pipeline: DisplayPipeline,
    pub rois: Vec<Roi>,
    pub resolution: ExportResolution,
    pub show_colorbar: bool,
    pub show_hud: bool,
    pub unit: TempUnit,
    pub title: Option<String>,
    pub notes: Option<String>,
    pub pixels: Option<Vec<u16>>,
    pub width: Option<u16>,
    pub height: Option<u16>,
}

#[tauri::command]
async fn cmd_export_media(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    req: ExportRequest,
) -> Result<String, String> {
    let has_req_pixels = req.pixels.is_some() && req.width.is_some() && req.height.is_some();
    let (pixels, w, h, latest_snapshot) = match (req.pixels, req.width, req.height) {
        (Some(px), Some(w), Some(h)) => {
            let expected = usize::from(w) * usize::from(h);
            if expected == 0 || px.len() != expected {
                return Err(format!(
                    "pixel buffer length {} does not match {w}x{h} ({expected})",
                    px.len()
                ));
            }
            (px, w, h, None)
        }
        _ => {
            let guard = state.latest_frame.lock().map_err(|e| e.to_string())?;
            let fr = guard.as_ref().ok_or("no frame available")?;
            (
                fr.pixels.clone(),
                fr.meta.width,
                fr.meta.height,
                Some(Arc::clone(fr)),
            )
        }
    };
    let palette = resolve_palette(&state, &req.palette_id)?;
    let opts = ExportOptions {
        resolution: req.resolution,
        show_colorbar: req.show_colorbar,
        show_hud: req.show_hud,
        unit: req.unit,
        jpeg_quality: 94,
    };

    let ext = req.format.to_lowercase();
    let target = match req.output_path {
        Some(p) => PathBuf::from(p),
        None => {
            if ext == "seq" {
                rfd::AsyncFileDialog::new()
                    .pick_folder()
                    .await
                    .ok_or("cancelled")?
                    .path()
                    .to_path_buf()
            } else {
                rfd::AsyncFileDialog::new()
                    .set_file_name(format!("thermocam_capture.{ext}"))
                    .save_file()
                    .await
                    .ok_or("cancelled")?
                    .path()
                    .to_path_buf()
            }
        }
    };

    // Snapshot lightweight Arc<FrameRaw> handles and release recorded_frames lock immediately!
    let seq_snapshot: Vec<Arc<FrameRaw>> = if matches!(ext.as_str(), "mp4" | "gif" | "seq") {
        let recorded = state.recorded_frames.lock().map_err(|e| e.to_string())?;
        if recorded.is_empty() {
            drop(recorded);
            if let Some(fr) = state
                .latest_frame
                .lock()
                .map_err(|e| e.to_string())?
                .clone()
            {
                vec![fr]
            } else {
                Vec::new()
            }
        } else {
            recorded.clone()
        }
    } else {
        Vec::new()
    };

    let palette_id = req.palette_id;
    let min_raw = req.min_raw;
    let max_raw = req.max_raw;
    let rad = req.rad;
    let pipeline = req.pipeline;
    let rois = req.rois;
    let title = req.title;
    let notes = req.notes;
    let unit = req.unit;

    tauri::async_runtime::spawn_blocking(move || {
        match ext.as_str() {
            "png" | "jpg" | "jpeg" => {
                let (ow, oh, rgb) = render_export_canvas(
                    &pixels, w, h, min_raw, max_raw, &palette, rad, &pipeline, &rois, &opts,
                );
                let bytes = if ext == "png" {
                    encode_png(ow, oh, &rgb)
                } else {
                    encode_jpeg(ow, oh, &rgb, opts.jpeg_quality)
                }
                .map_err(|e| e.to_string())?;
                fs::write(&target, bytes).map_err(|e| e.to_string())?;
            }
            "tiff" | "tif" => {
                let bytes = encode_radiometric_tiff_16(&pixels, w, h, rad)
                    .map_err(|e| e.to_string())?;
                fs::write(&target, bytes).map_err(|e| e.to_string())?;
            }
            "csv" => {
                let csv = encode_temperature_csv(&pixels, w, h, rad, unit);
                fs::write(&target, csv).map_err(|e| e.to_string())?;
            }
            "html" => {
                let html = generate_html_report(
                    title.as_deref().unwrap_or("DIY-Thermocam Inspection Report"),
                    notes.as_deref().unwrap_or(""),
                    &pixels, w, h, min_raw, max_raw, &palette, rad, &pipeline, &rois, &opts,
                )
                .map_err(|e| e.to_string())?;
                fs::write(&target, html).map_err(|e| e.to_string())?;
            }
            "md" => {
                let md = generate_markdown_report(
                    title.as_deref().unwrap_or("DIY-Thermocam Inspection Report"),
                    notes.as_deref().unwrap_or(""),
                    "capture.png",
                    &pixels, w, h, &palette, rad, &rois, unit,
                );
                fs::write(&target, md).map_err(|e| e.to_string())?;
            }
            "mp4" | "gif" => {
                if seq_snapshot.is_empty() {
                    return Err("no frames available for video export".into());
                }
                let first = &seq_snapshot[0];
                let (out_w, out_h) = opts.resolution.dimensions(first.meta.width, first.meta.height);
                let timestamps: Vec<u64> = seq_snapshot.iter().map(|f| f.meta.timestamp_us).collect();
                let fps = derive_sequence_fps(&timestamps);
                let total = seq_snapshot.len();
                let mut scratch = RenderScratch::new();
                let file = File::create(&target).map_err(|e| e.to_string())?;
                let writer = BufWriter::new(file);

                if ext == "mp4" {
                    encode_h264_mp4_streaming(writer, out_w, out_h, total, fps, |idx, emit| {
                        let fr = &seq_snapshot[idx];
                        render_export_canvas_with_scratch(
                            &fr.pixels,
                            fr.meta.width,
                            fr.meta.height,
                            min_raw,
                            max_raw,
                            &palette,
                            rad,
                            &pipeline,
                            &rois,
                            &opts,
                            true,
                            &mut scratch,
                        );
                        emit(&scratch.canvas)?;
                        let _ = app.emit(
                            "export-progress",
                            serde_json::json!({ "stage": "encoding", "done": idx + 1, "total": total }),
                        );
                        Ok(())
                    })
                    .map_err(|e| e.to_string())?;
                } else {
                    encode_gif_streaming(
                        writer,
                        out_w as u16,
                        out_h as u16,
                        total,
                        fps,
                        |idx, emit| {
                            let fr = &seq_snapshot[idx];
                            render_export_canvas_with_scratch(
                                &fr.pixels,
                                fr.meta.width,
                                fr.meta.height,
                                min_raw,
                                max_raw,
                                &palette,
                                rad,
                                &pipeline,
                                &rois,
                                &opts,
                                true,
                                &mut scratch,
                            );
                            emit(&scratch.canvas)?;
                            let _ = app.emit(
                                "export-progress",
                                serde_json::json!({ "stage": "encoding", "done": idx + 1, "total": total }),
                            );
                            Ok(())
                        },
                    )
                    .map_err(|e| e.to_string())?;
                }
            }
            "dat" => {
                let scheme = palette.firmware_id.unwrap_or(10);
                let dat = if has_req_pixels {
                    ThermalDat::from_pixels(pixels, w, h, rad, scheme, 0)
                } else if let Some(fr) = latest_snapshot {
                    ThermalDat::from_frame(&fr, scheme, 0)
                } else {
                    ThermalDat::from_pixels(pixels, w, h, rad, scheme, 0)
                };
                fs::write(&target, dat.to_bytes()).map_err(|e| e.to_string())?;
            }
            "seq" => {
                fs::create_dir_all(&target).map_err(|e| e.to_string())?;
                let mut names = Vec::with_capacity(seq_snapshot.len());
                let scheme = palette.firmware_id.unwrap_or(10);
                for (i, fr) in seq_snapshot.iter().enumerate() {
                    let fname = format!("{i:06}.DAT");
                    let dat = ThermalDat::from_frame(fr, scheme, 0);
                    fs::write(target.join(&fname), dat.to_bytes()).map_err(|e| e.to_string())?;
                    names.push(fname);
                }
                let timestamps: Vec<u64> = seq_snapshot.iter().map(|f| f.meta.timestamp_us).collect();
                let fps = derive_sequence_fps(&timestamps) as f32;
                let manifest = SequenceManifest {
                    schema_version: 1,
                    created_utc: format_utc_now_iso8601(),
                    sensor_w: w,
                    sensor_h: h,
                    fps,
                    frame_count: names.len(),
                    frames: names,
                    palette_id,
                };
                fs::write(
                    target.join("manifest.json"),
                    serde_json::to_string_pretty(&manifest).map_err(|e| e.to_string())?,
                )
                .map_err(|e| e.to_string())?;
            }
            other => return Err(format!("unsupported export format: {other}")),
        }

        Ok(target.to_string_lossy().into_owned())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn cmd_pick_directory() -> Result<String, String> {
    let dir = rfd::AsyncFileDialog::new()
        .pick_folder()
        .await
        .ok_or("cancelled")?;
    Ok(dir.path().to_string_lossy().into_owned())
}

#[derive(Serialize)]
struct TimelapseCaptureResult {
    recorded_count: usize,
    saved_path: Option<String>,
}

#[allow(clippy::too_many_arguments)]
#[tauri::command]
async fn cmd_capture_timelapse_frame(
    state: State<'_, AppState>,
    output_dir: Option<String>,
    palette_id: String,
    index: usize,
    pixels: Vec<u16>,
    width: u16,
    height: u16,
    rad: RadiometryParams,
) -> Result<TimelapseCaptureResult, String> {
    let expected = usize::from(width) * usize::from(height);
    if expected == 0 || pixels.len() != expected {
        return Err(format!(
            "timelapse pixel buffer length {} does not match {width}x{height} ({expected})",
            pixels.len()
        ));
    }
    let palette = resolve_palette(&state, &palette_id)?;
    let rec_buf = Arc::clone(&state.recorded_frames);
    let latest_arc = Arc::clone(&state.latest_frame);
    tauri::async_runtime::spawn_blocking(move || {
        let scheme = palette.firmware_id.unwrap_or(10);
        let dat = ThermalDat::from_pixels(pixels.clone(), width, height, rad, scheme, 0);
        let saved_path = if let Some(dir) = output_dir {
            let dir_path = PathBuf::from(dir);
            fs::create_dir_all(&dir_path).map_err(|e| e.to_string())?;
            let file_path = dir_path.join(format!("timelapse_{index:06}.DAT"));
            fs::write(&file_path, dat.to_bytes()).map_err(|e| e.to_string())?;
            Some(file_path.to_string_lossy().into_owned())
        } else {
            None
        };

        let now_us = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map_or(0, |d| d.as_micros() as u64);
        let live_meta = latest_arc
            .lock()
            .ok()
            .and_then(|g| g.as_ref().map(|fr| fr.meta.clone()));
        let frame_arc = Arc::new(FrameRaw {
            meta: FrameRawMeta {
                frame_seq: index as u32,
                timestamp_us: now_us,
                rtc_epoch: (now_us / 1_000_000) as u32,
                dropped_frames: 0,
                width,
                height,
                pixel_format: 0,
                gain_mode: live_meta.as_ref().map_or(0, |m| m.gain_mode),
                ffc_state: live_meta.as_ref().map_or(0, |m| m.ffc_state),
                flags: live_meta.as_ref().map_or(0, |m| m.flags),
                min_raw: dat.min_raw,
                max_raw: dat.max_raw,
                min_x: live_meta.as_ref().map_or(0, |m| m.min_x),
                min_y: live_meta.as_ref().map_or(0, |m| m.min_y),
                max_x: live_meta.as_ref().map_or(0, |m| m.max_x),
                max_y: live_meta.as_ref().map_or(0, |m| m.max_y),
                spot_temp_c: dat.spot_temp,
                cal_offset: rad.cal_offset,
                cal_slope: rad.cal_slope,
                fpa_temp_c: live_meta.as_ref().map_or(30.0, |m| m.fpa_temp_c),
                bat_pct: live_meta.as_ref().map_or(100, |m| m.bat_pct),
                usb_power: live_meta.as_ref().is_none_or(|m| m.usb_power),
                temp_point_count: 0,
            },
            pixels,
            temp_points: Vec::new(),
        });

        let recorded_count = {
            let mut buf = rec_buf.lock().map_err(|e| e.to_string())?;
            if buf.len() < MAX_RECORDED_FRAMES {
                buf.push(frame_arc);
            }
            buf.len()
        };
        Ok(TimelapseCaptureResult {
            recorded_count,
            saved_path,
        })
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
fn cmd_toggle_recording(state: State<'_, AppState>, active: bool) -> Result<usize, String> {
    if active {
        state
            .recorded_frames
            .lock()
            .map_err(|e| e.to_string())?
            .clear();
    }
    state.recording.store(active, Ordering::SeqCst);
    Ok(state
        .recorded_frames
        .lock()
        .map_err(|e| e.to_string())?
        .len())
}

#[tauri::command]
fn cmd_evaluate_rois(
    pixels: Vec<u16>,
    width: u16,
    height: u16,
    rad: RadiometryParams,
    rois: Vec<Roi>,
) -> Vec<RoiStats> {
    rois.iter()
        .map(|r| evaluate_roi(r, &pixels, width, height, rad))
        .collect()
}

#[tauri::command]
async fn cmd_flash_firmware(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    hex_text: String,
    expected_sha256: Option<String>,
    legacy_port: Option<String>,
) -> Result<String, String> {
    let client_arc = Arc::clone(&state.client);
    let streaming = Arc::clone(&state.streaming);
    let stream_gen = Arc::clone(&state.stream_gen);
    let stream_lifecycle = Arc::clone(&state.stream_lifecycle);
    let stream_thread = Arc::clone(&state.stream_thread);

    tauri::async_runtime::spawn_blocking(move || {
        let _lifecycle = stream_lifecycle.lock().map_err(|e| e.to_string())?;
        let img =
            parse_teensy41_hex(&hex_text, expected_sha256.as_deref()).map_err(|e| e.to_string())?;
        let _ = app.emit(
            "flash-progress",
            serde_json::json!({ "stage": "verified", "sha256": img.sha256_hex }),
        );

        // Stop live stream & cleanly release serial port after triggering bootloader
        stop_stream_and_join(&streaming, &stream_gen, &stream_thread);
        if let Ok(mut guard) = client_arc.lock() {
            if let Some(mut c) = guard.take() {
                let _ = c.request_timeout(
                    MsgType::EnterBootloaderReq,
                    &thermocam_core::protocol::BOOTLOADER_MAGIC.to_le_bytes(),
                    Duration::from_millis(500),
                );
                drop(c);
            }
        }
        if let Some(p) = legacy_port {
            if !p.starts_with("sim://") {
                let _ = trigger_legacy_134_baud(&p);
            }
        }

        let _ = app.emit(
            "flash-progress",
            serde_json::json!({ "stage": "waiting_bootloader" }),
        );
        let mut hid = RealHalfKayDevice::open_with_timeout(
            Duration::from_secs(10),
            Duration::from_millis(100),
        )
        .map_err(|e| e.to_string())?;

        flash_firmware(&mut hid, &img, |done, total| {
            let _ = app.emit(
                "flash-progress",
                serde_json::json!({ "stage": "flashing", "done": done, "total": total }),
            );
        })
        .map_err(|e| e.to_string())?;
        let _ = app.emit(
            "flash-progress",
            serde_json::json!({ "stage": "rebooted", "sha256": img.sha256_hex }),
        );
        Ok(img.sha256_hex)
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Build the configured Tauri v2 application with `thermo://` custom protocol for SD thumbnails/media.
pub fn build_app() -> tauri::Builder<tauri::Wry> {
    tauri::Builder::default()
        .manage(AppState::default())
        .register_uri_scheme_protocol("thermo", |ctx, req| {
            let path = req.uri().path().to_string();
            let app = ctx.app_handle();
            if let Some(state) = app.try_state::<AppState>() {
                if let Ok(cache) = state.sd_cache.lock() {
                    if let Some(dat_bytes) = cache.get(&path) {
                        if let Ok(dat) = ThermalDat::from_bytes(dat_bytes) {
                            if let Ok(pal) = find_palette("ironblack") {
                                let opts = ExportOptions {
                                    resolution: ExportResolution::Custom {
                                        width: 320,
                                        height: 240,
                                    },
                                    show_colorbar: false,
                                    show_hud: false,
                                    ..ExportOptions::default()
                                };
                                let (w, h, rgb) = render_export_canvas(
                                    &dat.pixels,
                                    dat.width,
                                    dat.height,
                                    dat.min_raw,
                                    dat.max_raw,
                                    &pal,
                                    RadiometryParams::default(),
                                    &DisplayPipeline::default(),
                                    &[],
                                    &opts,
                                );
                                if let Ok(png) = encode_png(w, h, &rgb) {
                                    return tauri::http::Response::builder()
                                        .status(200)
                                        .header("Content-Type", "image/png")
                                        .body(png)
                                        .unwrap_or_default();
                                }
                            }
                        }
                    }
                }
            }
            tauri::http::Response::builder()
                .status(404)
                .body(Vec::new())
                .unwrap_or_default()
        })
        .invoke_handler(tauri::generate_handler![
            cmd_list_ports,
            cmd_get_palettes,
            cmd_get_material_presets,
            cmd_import_custom_palette,
            cmd_connect,
            cmd_disconnect,
            cmd_start_stream,
            cmd_stop_stream,
            cmd_get_config,
            cmd_set_config,
            cmd_trigger_ffc,
            cmd_get_diagnostics,
            cmd_get_perf,
            cmd_get_rad_params,
            cmd_set_rad_params,
            cmd_sd_list,
            cmd_sd_read_dat,
            cmd_sd_delete,
            cmd_sd_download,
            cmd_load_dat_file,
            cmd_save_sidecar,
            cmd_verify_hex,
            cmd_pick_hex_file,
            cmd_export_media,
            cmd_pick_directory,
            cmd_capture_timelapse_frame,
            cmd_toggle_recording,
            cmd_evaluate_rois,
            cmd_flash_firmware,
        ])
}
