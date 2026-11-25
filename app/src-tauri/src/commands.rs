//! Typed Tauri v2 commands, managed studio state, raw binary frame channel, and `thermo://` protocol.

use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
use tauri::ipc::{Channel, InvokeResponseBody};
use tauri::{Emitter, Manager, State};

use thermocam_core::dat::{SequenceManifest, ThermalDat, ThermoSidecar};
use thermocam_core::export::{
    encode_gif, encode_h264_mp4, encode_jpeg, encode_png, encode_radiometric_tiff_16,
    encode_temperature_csv, generate_html_report, generate_markdown_report, render_export_canvas,
    ExportOptions, ExportResolution,
};
use thermocam_core::flasher::{
    flash_firmware, parse_teensy41_hex, trigger_legacy_134_baud, FirmwareImage, RealHalfKayDevice,
};
use thermocam_core::imaging::DisplayPipeline;
use thermocam_core::palettes::{
    builtin_palettes, find_palette, parse_custom_palette_png, parse_custom_palette_text, Palette,
};
use thermocam_core::protocol::codec::{FrameRaw, HelloResp, StreamCtrl};
use thermocam_core::protocol::messages::{
    encode_enter_bootloader_req, DeviceConfig, Diagnostics, FsEntry, FsInfoResp, FsListResp,
    PerfTelemetry, RadParams,
};
use thermocam_core::protocol::transport::{
    list_ports, ProtocolClient, SerialPortInfo, SerialTransport, SimulatorTransport,
};
use thermocam_core::protocol::MsgType;
use thermocam_core::radiometry::{RadiometryParams, TempUnit, MATERIAL_PRESETS};
use thermocam_core::roi::{evaluate_roi, Roi, RoiStats};

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
    fn request(&mut self, mt: MsgType, payload: &[u8]) -> Result<Vec<u8>, String> {
        let timeout = Duration::from_millis(2000);
        let pkt = match self {
            Self::Serial(c) => c.request(mt, payload, timeout).map_err(|e| e.to_string())?,
            Self::Simulator(c) => c.request(mt, payload, timeout).map_err(|e| e.to_string())?,
        };
        Ok(pkt.payload)
    }
    fn poll_packets(&mut self) -> Result<Vec<thermocam_core::protocol::Packet>, String> {
        match self {
            Self::Serial(c) => c
                .poll_packets(Duration::from_millis(30))
                .map_err(|e| e.to_string()),
            Self::Simulator(c) => c
                .poll_packets(Duration::from_millis(30))
                .map_err(|e| e.to_string()),
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

pub struct AppState {
    client: Arc<Mutex<Option<ActiveTransport>>>,
    streaming: Arc<AtomicBool>,
    latest_frame: Arc<Mutex<Option<FrameRaw>>>,
    recorded_frames: Arc<Mutex<Vec<FrameRaw>>>,
    recording: Arc<AtomicBool>,
    custom_palettes: Arc<Mutex<HashMap<String, Palette>>>,
    sd_cache: Arc<Mutex<HashMap<String, Vec<u8>>>>,
}

impl Default for AppState {
    fn default() -> Self {
        Self {
            client: Arc::new(Mutex::new(None)),
            streaming: Arc::new(AtomicBool::new(false)),
            latest_frame: Arc::new(Mutex::new(None)),
            recorded_frames: Arc::new(Mutex::new(Vec::new())),
            recording: Arc::new(AtomicBool::new(false)),
            custom_palettes: Arc::new(Mutex::new(HashMap::new())),
            sd_cache: Arc::new(Mutex::new(HashMap::new())),
        }
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
fn cmd_connect(state: State<'_, AppState>, port: String) -> Result<HelloResp, String> {
    state.streaming.store(false, Ordering::SeqCst);
    let mut active = if port.starts_with("sim://") {
        ActiveTransport::Simulator(Box::new(ProtocolClient::new(SimulatorTransport::new())))
    } else {
        let serial = SerialTransport::open(&port).map_err(|e| e.to_string())?;
        ActiveTransport::Serial(Box::new(ProtocolClient::new(serial)))
    };
    let hello = active.hello()?;
    *state.client.lock().map_err(|e| e.to_string())? = Some(active);
    Ok(hello)
}

#[tauri::command]
fn cmd_disconnect(state: State<'_, AppState>) -> Result<(), String> {
    state.streaming.store(false, Ordering::SeqCst);
    let mut guard = state.client.lock().map_err(|e| e.to_string())?;
    if let Some(mut c) = guard.take() {
        let _ = c.request(MsgType::GoodbyeReq, &[]);
    }
    Ok(())
}

/// Stream raw `u16` frames + 64-byte LE metadata directly over Tauri v2 `Channel<InvokeResponseBody::Raw>`.
#[tauri::command]
fn cmd_start_stream(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    channel: Channel<InvokeResponseBody>,
) -> Result<(), String> {
    {
        let mut guard = state.client.lock().map_err(|e| e.to_string())?;
        let client = guard.as_mut().ok_or("not connected")?;
        client.set_stream(StreamCtrl {
            mode: 1,
            keep_lcd_on: false,
            include_temp_points: true,
            perf_interval_s: 1,
            max_fps_x10: 0,
        })?;
    }
    state.streaming.store(true, Ordering::SeqCst);
    let client_arc = Arc::clone(&state.client);
    let streaming_arc = Arc::clone(&state.streaming);
    let latest_arc = Arc::clone(&state.latest_frame);
    let rec_flag = Arc::clone(&state.recording);
    let rec_buf = Arc::clone(&state.recorded_frames);

    thread::spawn(move || {
        while streaming_arc.load(Ordering::SeqCst) {
            let tick = Instant::now();
            let pkts = {
                let mut guard = match client_arc.lock() {
                    Ok(g) => g,
                    Err(_) => break,
                };
                match guard.as_mut() {
                    Some(c) => c.poll_packets().unwrap_or_default(),
                    None => break,
                }
            };
            for pkt in pkts {
                if pkt.msg_type().ok() == Some(MsgType::FrameRaw) {
                    if let Ok(fr) = FrameRaw::from_bytes(&pkt.payload) {
                        let raw_bytes = fr.to_bytes();
                        if let Ok(mut latest) = latest_arc.lock() {
                            *latest = Some(fr.clone());
                        }
                        if rec_flag.load(Ordering::SeqCst) {
                            if let Ok(mut buf) = rec_buf.lock() {
                                buf.push(fr);
                            }
                        }
                        if channel.send(InvokeResponseBody::Raw(raw_bytes)).is_err() {
                            streaming_arc.store(false, Ordering::SeqCst);
                            break;
                        }
                    }
                } else if let Ok(Some(ev)) =
                    thermocam_core::protocol::messages::AsyncEvent::from_packet(&pkt)
                {
                    let _ = app.emit("camera-event", ev);
                }
            }
            let elapsed = tick.elapsed();
            if elapsed < Duration::from_millis(100) {
                thread::sleep(Duration::from_millis(100) - elapsed);
            }
        }
    });
    Ok(())
}

#[tauri::command]
fn cmd_stop_stream(state: State<'_, AppState>) {
    state.streaming.store(false, Ordering::SeqCst);
}

#[tauri::command]
fn cmd_get_config(state: State<'_, AppState>) -> Result<DeviceConfig, String> {
    let mut guard = state.client.lock().map_err(|e| e.to_string())?;
    let payload = guard
        .as_mut()
        .ok_or("not connected")?
        .request(MsgType::GetConfigReq, &[])?;
    DeviceConfig::from_bytes(&payload).map_err(|e| e.to_string())
}

#[tauri::command]
fn cmd_set_config(
    state: State<'_, AppState>,
    config: DeviceConfig,
    persist: bool,
) -> Result<DeviceConfig, String> {
    let mut guard = state.client.lock().map_err(|e| e.to_string())?;
    let payload = guard
        .as_mut()
        .ok_or("not connected")?
        .request(MsgType::SetConfigReq, &config.to_set_req_bytes(persist))?;
    DeviceConfig::from_bytes(&payload).map_err(|e| e.to_string())
}

#[tauri::command]
fn cmd_trigger_ffc(state: State<'_, AppState>, force: bool) -> Result<(), String> {
    let mut guard = state.client.lock().map_err(|e| e.to_string())?;
    let _ = guard
        .as_mut()
        .ok_or("not connected")?
        .request(MsgType::TriggerFfcReq, &[u8::from(force)])?;
    Ok(())
}

#[tauri::command]
fn cmd_get_diagnostics(state: State<'_, AppState>) -> Result<Diagnostics, String> {
    let mut guard = state.client.lock().map_err(|e| e.to_string())?;
    let payload = guard
        .as_mut()
        .ok_or("not connected")?
        .request(MsgType::GetDiagnosticsReq, &[])?;
    Diagnostics::from_bytes(&payload).map_err(|e| e.to_string())
}

#[tauri::command]
fn cmd_get_perf(state: State<'_, AppState>, reset: bool) -> Result<PerfTelemetry, String> {
    let mut guard = state.client.lock().map_err(|e| e.to_string())?;
    let payload = guard
        .as_mut()
        .ok_or("not connected")?
        .request(MsgType::GetPerfReq, &[u8::from(reset)])?;
    PerfTelemetry::from_bytes(&payload).map_err(|e| e.to_string())
}

#[tauri::command]
fn cmd_get_rad_params(state: State<'_, AppState>) -> Result<RadParams, String> {
    let mut guard = state.client.lock().map_err(|e| e.to_string())?;
    let payload = guard
        .as_mut()
        .ok_or("not connected")?
        .request(MsgType::GetRadParamsReq, &[])?;
    RadParams::from_bytes(&payload).map_err(|e| e.to_string())
}

#[tauri::command]
fn cmd_set_rad_params(state: State<'_, AppState>, params: RadParams) -> Result<RadParams, String> {
    let mut guard = state.client.lock().map_err(|e| e.to_string())?;
    let payload = guard
        .as_mut()
        .ok_or("not connected")?
        .request(MsgType::SetRadParamsReq, &params.to_bytes())?;
    RadParams::from_bytes(&payload).map_err(|e| e.to_string())
}

#[derive(Serialize)]
struct SdListing {
    info: FsInfoResp,
    entries: Vec<FsEntry>,
}

#[tauri::command]
fn cmd_sd_list(state: State<'_, AppState>, path: String) -> Result<SdListing, String> {
    let mut guard = state.client.lock().map_err(|e| e.to_string())?;
    let client = guard.as_mut().ok_or("not connected")?;
    let info_b = client.request(MsgType::FsInfoReq, &[])?;
    let info = FsInfoResp::from_bytes(&info_b).map_err(|e| e.to_string())?;
    let list_req = FsListResp::encode_req(0, 64, &path);
    let list_b = client.request(MsgType::FsListReq, &list_req)?;
    let list = FsListResp::from_bytes(&list_b).map_err(|e| e.to_string())?;
    Ok(SdListing {
        info,
        entries: list.entries,
    })
}

#[tauri::command]
fn cmd_sd_read_dat(state: State<'_, AppState>, path: String) -> Result<ThermalDat, String> {
    let bytes = {
        let mut guard = state.client.lock().map_err(|e| e.to_string())?;
        guard.as_mut().ok_or("not connected")?.read_file(&path)?
    };
    if let Ok(mut cache) = state.sd_cache.lock() {
        cache.insert(path, bytes.clone());
    }
    ThermalDat::from_bytes(&bytes).map_err(|e| e.to_string())
}

#[tauri::command]
fn cmd_sd_delete(state: State<'_, AppState>, path: String) -> Result<(), String> {
    {
        let mut guard = state.client.lock().map_err(|e| e.to_string())?;
        guard.as_mut().ok_or("not connected")?.delete_file(&path)?;
    }
    if let Ok(mut cache) = state.sd_cache.lock() {
        cache.remove(&path);
    }
    Ok(())
}

#[tauri::command]
fn cmd_sd_download(
    state: State<'_, AppState>,
    path: String,
    output_path: Option<String>,
) -> Result<String, String> {
    let bytes = {
        let mut guard = state.client.lock().map_err(|e| e.to_string())?;
        guard.as_mut().ok_or("not connected")?.read_file(&path)?
    };
    let file_name = path.trim_start_matches('/').to_string();
    let target = match output_path {
        Some(p) => PathBuf::from(p),
        None => rfd::FileDialog::new()
            .set_file_name(&file_name)
            .save_file()
            .ok_or("cancelled")?,
    };
    fs::write(&target, bytes).map_err(|e| e.to_string())?;
    Ok(target.to_string_lossy().into_owned())
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
fn cmd_load_dat_file(path: Option<String>) -> Result<(String, ThermalDat, Option<ThermoSidecar>), String> {
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
fn cmd_export_media(state: State<'_, AppState>, req: ExportRequest) -> Result<String, String> {
    let (pixels, w, h) = match (req.pixels, req.width, req.height) {
        (Some(px), Some(w), Some(h)) => (px, w, h),
        _ => {
            let guard = state.latest_frame.lock().map_err(|e| e.to_string())?;
            let fr = guard.as_ref().ok_or("no frame available")?;
            (fr.pixels.clone(), fr.meta.width, fr.meta.height)
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
        None => rfd::FileDialog::new()
            .set_file_name(format!("thermocam_capture.{ext}"))
            .save_file()
            .ok_or("cancelled")?,
    };

    match ext.as_str() {
        "png" | "jpg" | "jpeg" => {
            let (ow, oh, rgb) = render_export_canvas(
                &pixels, w, h, req.min_raw, req.max_raw, &palette, req.rad, &req.pipeline,
                &req.rois, &opts,
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
            let bytes = encode_radiometric_tiff_16(&pixels, w, h, req.rad)
                .map_err(|e| e.to_string())?;
            fs::write(&target, bytes).map_err(|e| e.to_string())?;
        }
        "csv" => {
            let csv = encode_temperature_csv(&pixels, w, h, req.rad, req.unit);
            fs::write(&target, csv).map_err(|e| e.to_string())?;
        }
        "html" => {
            let html = generate_html_report(
                req.title.as_deref().unwrap_or("DIY-Thermocam Inspection Report"),
                req.notes.as_deref().unwrap_or(""),
                &pixels, w, h, req.min_raw, req.max_raw, &palette, req.rad, &req.pipeline,
                &req.rois, &opts,
            )
            .map_err(|e| e.to_string())?;
            fs::write(&target, html).map_err(|e| e.to_string())?;
        }
        "md" => {
            let md = generate_markdown_report(
                req.title.as_deref().unwrap_or("DIY-Thermocam Inspection Report"),
                req.notes.as_deref().unwrap_or(""),
                "capture.png",
                &pixels, w, h, &palette, req.rad, &req.rois, req.unit,
            );
            fs::write(&target, md).map_err(|e| e.to_string())?;
        }
        "mp4" | "gif" => {
            let recorded = state.recorded_frames.lock().map_err(|e| e.to_string())?;
            let seq: Vec<FrameRaw> = if recorded.is_empty() {
                let mut tmp = Vec::new();
                if let Some(fr) = state.latest_frame.lock().map_err(|e| e.to_string())?.clone() {
                    tmp.push(fr);
                }
                tmp
            } else {
                recorded.clone()
            };
            let mut rgb_frames = Vec::with_capacity(seq.len());
            let mut dims = (0u32, 0u32);
            for fr in &seq {
                let (ow, oh, rgb) = render_export_canvas(
                    &fr.pixels, fr.meta.width, fr.meta.height, req.min_raw, req.max_raw,
                    &palette, req.rad, &req.pipeline, &req.rois, &opts,
                );
                dims = (ow, oh);
                rgb_frames.push(rgb);
            }
            let bytes = if ext == "mp4" {
                encode_h264_mp4(dims.0, dims.1, &rgb_frames, 9).map_err(|e| e.to_string())?
            } else {
                encode_gif(dims.0 as u16, dims.1 as u16, &rgb_frames, 9.0)
                    .map_err(|e| e.to_string())?
            };
            fs::write(&target, bytes).map_err(|e| e.to_string())?;
        }
        "dat" => {
            let fr = state
                .latest_frame
                .lock()
                .map_err(|e| e.to_string())?
                .clone()
                .ok_or("no frame")?;
            let dat = ThermalDat::from_frame(&fr, palette.firmware_id.unwrap_or(10), 0);
            fs::write(&target, dat.to_bytes()).map_err(|e| e.to_string())?;
        }
        "seq" => {
            let recorded = state.recorded_frames.lock().map_err(|e| e.to_string())?;
            fs::create_dir_all(&target).map_err(|e| e.to_string())?;
            let mut names = Vec::with_capacity(recorded.len());
            for (i, fr) in recorded.iter().enumerate() {
                let fname = format!("{i:06}.DAT");
                let dat = ThermalDat::from_frame(fr, palette.firmware_id.unwrap_or(10), 0);
                fs::write(target.join(&fname), dat.to_bytes()).map_err(|e| e.to_string())?;
                names.push(fname);
            }
            let manifest = SequenceManifest {
                schema_version: 1,
                created_utc: "2026-11-25T00:00:00Z".into(),
                sensor_w: w,
                sensor_h: h,
                fps: 8.78,
                frame_count: names.len(),
                frames: names,
                palette_id: req.palette_id,
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
}

#[tauri::command]
fn cmd_toggle_recording(state: State<'_, AppState>, active: bool) -> Result<usize, String> {
    if active {
        state.recorded_frames.lock().map_err(|e| e.to_string())?.clear();
    }
    state.recording.store(active, Ordering::SeqCst);
    Ok(state.recorded_frames.lock().map_err(|e| e.to_string())?.len())
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
fn cmd_flash_firmware(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    hex_text: String,
    expected_sha256: Option<String>,
    legacy_port: Option<String>,
) -> Result<String, String> {
    let img =
        parse_teensy41_hex(&hex_text, expected_sha256.as_deref()).map_err(|e| e.to_string())?;
    let _ = app.emit("flash-progress", serde_json::json!({ "stage": "verified", "sha256": img.sha256_hex }));

    // Trigger bootloader via Protocol v2 if connected, else optional 134-baud legacy CDC
    if let Ok(mut guard) = state.client.lock() {
        if let Some(c) = guard.as_mut() {
            let _ = encode_enter_bootloader_req(1);
            let _ = c.request(MsgType::EnterBootloaderReq, &thermocam_core::protocol::BOOTLOADER_MAGIC.to_le_bytes());
        }
    }
    if let Some(p) = legacy_port {
        let _ = trigger_legacy_134_baud(&p);
    }

    let mut hid = RealHalfKayDevice::open().map_err(|e| e.to_string())?;
    flash_firmware(&mut hid, &img, |done, total| {
        let _ = app.emit("flash-progress", serde_json::json!({ "stage": "flashing", "done": done, "total": total }));
    })
    .map_err(|e| e.to_string())?;
    Ok(img.sha256_hex)
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
                                    &dat.pixels, dat.width, dat.height, dat.min_raw, dat.max_raw,
                                    &pal, RadiometryParams::default(), &DisplayPipeline::default(),
                                    &[], &opts,
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
            cmd_toggle_recording,
            cmd_evaluate_rois,
            cmd_flash_firmware,
        ])
}
