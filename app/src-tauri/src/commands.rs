//! Camera, stream, SD card and firmware commands (the original Studio command set).

use std::fs;
use std::path::PathBuf;
use std::sync::atomic::Ordering;
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

use serde::Serialize;
use tauri::ipc::{Channel, InvokeResponseBody};
use tauri::{Emitter, State};

use crate::card::LiveCard;
use crate::recorder::{on_camera_lost, record_frame, RecorderHandles};
use crate::state::{
    release_client, spawn_keepalive, stop_stream_and_join, ActiveTransport, AppState,
    KEEPALIVE_INTERVAL,
};
use thermocam_core::card::{list_card_dir, read_card_file};

use thermocam_core::dat::{ThermalDat, ThermoSidecar};
use thermocam_core::flasher::{
    parse_teensy41_hex, run_flash_update, trigger_legacy_134_baud, FirmwareImage, FlashFailure,
    FlashFailureKind, FlashUpdateOptions, RealHalfKayDevice,
};
use thermocam_core::palettes::{
    builtin_palettes, parse_custom_palette_png, parse_custom_palette_text, Palette,
};
use thermocam_core::protocol::codec::{FrameRaw, HelloResp, StreamCtrl};
use thermocam_core::protocol::messages::{
    AsyncEvent, DeviceConfig, Diagnostics, FsEntry, FsInfoResp, PerfTelemetry, RadParams,
};
use thermocam_core::protocol::stream::{
    Disconnected, StreamStep, StreamWatch, CAMERA_DISCONNECTED_EVENT,
};
use thermocam_core::protocol::transport::{
    list_ports, ProtocolClient, SerialPortInfo, SerialTransport, SimulatorTransport,
};
use thermocam_core::protocol::{ConnectError, ConnectErrorKind, MsgType};

#[tauri::command]
pub fn cmd_list_ports() -> Vec<SerialPortInfo> {
    list_ports()
}

#[tauri::command]
pub fn cmd_get_palettes(state: State<'_, AppState>) -> Vec<Palette> {
    let mut list = builtin_palettes();
    if let Ok(custom) = state.custom_palettes.lock() {
        list.extend(custom.values().cloned());
    }
    list
}

#[tauri::command]
pub fn cmd_import_custom_palette(
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
pub async fn cmd_connect(
    state: State<'_, AppState>,
    port: String,
) -> Result<HelloResp, ConnectError> {
    let other = |detail: String| ConnectError {
        kind: ConnectErrorKind::Other,
        detail,
    };
    let client_arc = Arc::clone(&state.client);
    let streaming = Arc::clone(&state.streaming);
    let stream_gen = Arc::clone(&state.stream_gen);
    let stream_lifecycle = Arc::clone(&state.stream_lifecycle);
    let stream_thread = Arc::clone(&state.stream_thread);
    let serial_slot = Arc::clone(&state.camera_serial);
    tauri::async_runtime::spawn_blocking(move || {
        let _lifecycle = stream_lifecycle.lock().map_err(|e| other(e.to_string()))?;
        stop_stream_and_join(&streaming, &stream_gen, &stream_thread);
        release_client(&client_arc);
        let mut active = if port.starts_with("sim://") {
            ActiveTransport::Simulator(Box::new(ProtocolClient::new(SimulatorTransport::new())))
        } else {
            let serial = SerialTransport::open(&port).map_err(|e| ConnectError::from(&e))?;
            ActiveTransport::Serial(Box::new(ProtocolClient::new(serial)))
        };
        let hello = active.hello().map_err(|e| ConnectError::from(&e))?;
        *client_arc.lock().map_err(|e| other(e.to_string()))? = Some(active);
        spawn_keepalive(
            Arc::clone(&client_arc),
            Arc::clone(&streaming),
            Arc::clone(&stream_gen),
            KEEPALIVE_INTERVAL,
        );
        if let Ok(mut s) = serial_slot.lock() {
            *s = Some(format!("{:016x}", hello.teensy_uid));
        }
        Ok(hello)
    })
    .await
    .map_err(|e| other(e.to_string()))?
}

#[tauri::command]
pub async fn cmd_disconnect(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
) -> Result<(), String> {
    on_camera_lost(&app, &state.recording, &state.last_recording);
    let client_arc = Arc::clone(&state.client);
    let streaming = Arc::clone(&state.streaming);
    let stream_gen = Arc::clone(&state.stream_gen);
    let stream_lifecycle = Arc::clone(&state.stream_lifecycle);
    let stream_thread = Arc::clone(&state.stream_thread);
    tauri::async_runtime::spawn_blocking(move || {
        let _lifecycle = stream_lifecycle.lock().map_err(|e| e.to_string())?;
        stop_stream_and_join(&streaming, &stream_gen, &stream_thread);
        release_client(&client_arc);
        Ok::<(), String>(())
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Stream raw `u16` frames + 64-byte LE metadata directly over Tauri v2 `Channel<InvokeResponseBody::Raw>`.
/// Uses a per-stream generation token, joins any prior poll thread, reads continuously with a short
/// 10 ms timeout (sub-5ms delivery), sends non-blocking heartbeats, and emits `camera-disconnected` on unplug.
#[tauri::command]
pub async fn cmd_start_stream(
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
    let recorder = RecorderHandles::from_state(&state);

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
            let mut watch = StreamWatch::default();

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
                // Let a waiting command (a card chunk, a config change) take the lock now.
                thread::yield_now();

                match watch.observe(poll_res) {
                    StreamStep::Packets(pkts) => {
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
                                    // Send raw wire payload directly (zero redundant re-encoding)
                                    if channel.send(InvokeResponseBody::Raw(pkt.payload)).is_err() {
                                        if worker_gen.load(Ordering::SeqCst) == my_gen {
                                            worker_streaming.store(false, Ordering::SeqCst);
                                        }
                                        return;
                                    }
                                    // Then write it to the running recording, if any.
                                    record_frame(&app, &recorder.slot, &recorder.last, &fr_arc);
                                }
                            } else if let Ok(Some(ev)) = AsyncEvent::from_packet(&pkt) {
                                let is_exit = matches!(ev, AsyncEvent::SessionExit { .. });
                                let _ = app.emit("camera-event", ev);
                                if is_exit && worker_gen.load(Ordering::SeqCst) == my_gen {
                                    worker_streaming.store(false, Ordering::SeqCst);
                                    on_camera_lost(&app, &recorder.slot, &recorder.last);
                                    let _ = app.emit(
                                        CAMERA_DISCONNECTED_EVENT,
                                        Disconnected {
                                            reason: "camera exited session".into(),
                                        },
                                    );
                                    return;
                                }
                            }
                        }
                        if !had_pkts && tick.elapsed() < Duration::from_millis(2) {
                            thread::sleep(Duration::from_millis(2));
                        }
                    }
                    StreamStep::Retry => thread::sleep(Duration::from_millis(15)),
                    StreamStep::Disconnected(gone) => {
                        if worker_gen.load(Ordering::SeqCst) == my_gen {
                            if let Ok(mut g) = worker_client.lock() {
                                *g = None;
                            }
                            worker_streaming.store(false, Ordering::SeqCst);
                            on_camera_lost(&app, &recorder.slot, &recorder.last);
                            let _ = app.emit(CAMERA_DISCONNECTED_EVENT, gone);
                        }
                        break;
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
pub async fn cmd_stop_stream(state: State<'_, AppState>) -> Result<(), String> {
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
        spawn_keepalive(client_arc, streaming, stream_gen, KEEPALIVE_INTERVAL);
        Ok(())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
pub async fn cmd_get_config(state: State<'_, AppState>) -> Result<DeviceConfig, String> {
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
pub async fn cmd_set_config(
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
pub async fn cmd_trigger_ffc(state: State<'_, AppState>, force: bool) -> Result<(), String> {
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
pub async fn cmd_get_diagnostics(state: State<'_, AppState>) -> Result<Diagnostics, String> {
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
pub async fn cmd_get_perf(
    state: State<'_, AppState>,
    reset: bool,
) -> Result<PerfTelemetry, String> {
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
pub async fn cmd_get_rad_params(state: State<'_, AppState>) -> Result<RadParams, String> {
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
pub async fn cmd_set_rad_params(
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
pub struct SdListing {
    info: FsInfoResp,
    entries: Vec<FsEntry>,
}

#[tauri::command]
pub async fn cmd_sd_list(state: State<'_, AppState>, path: String) -> Result<SdListing, String> {
    let client_arc = Arc::clone(&state.client);
    tauri::async_runtime::spawn_blocking(move || {
        let info = {
            let mut guard = client_arc.lock().map_err(|e| e.to_string())?;
            let client = guard.as_mut().ok_or("not connected")?;
            let info_b = client.request(MsgType::FsInfoReq, &[])?;
            FsInfoResp::from_bytes(&info_b).map_err(|e| e.to_string())?
        };
        // One page per lock, so the live stream keeps flowing while a big folder lists.
        let entries = list_card_dir(&LiveCard(&client_arc), &path).map_err(|e| e.to_string())?;
        Ok(SdListing { info, entries })
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
pub async fn cmd_sd_read_dat(
    state: State<'_, AppState>,
    path: String,
) -> Result<ThermalDat, String> {
    let client_arc = Arc::clone(&state.client);
    let sd_cache = Arc::clone(&state.sd_cache);
    tauri::async_runtime::spawn_blocking(move || {
        // One 32 KB chunk per lock, so the live stream keeps flowing during the read.
        let bytes = read_card_file(&LiveCard(&client_arc), &path).map_err(|e| e.to_string())?;
        if let Ok(mut cache) = sd_cache.lock() {
            cache.insert(path, bytes.clone());
        }
        ThermalDat::from_bytes(&bytes).map_err(|e| e.to_string())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
pub async fn cmd_sd_delete(state: State<'_, AppState>, path: String) -> Result<(), String> {
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
pub async fn cmd_sd_download(
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
        let bytes = read_card_file(&LiveCard(&client_arc), &path).map_err(|e| e.to_string())?;
        fs::write(&target, bytes).map_err(|e| e.to_string())?;
        Ok(target.to_string_lossy().into_owned())
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
pub fn cmd_verify_hex(
    hex_text: String,
    expected_sha256: Option<String>,
) -> Result<FirmwareImage, String> {
    parse_teensy41_hex(&hex_text, expected_sha256.as_deref()).map_err(|e| e.to_string())
}

/// A firmware file and its text: the one at `path` (Finish update reopens the last chosen
/// file), or one chosen in the system's file dialog.
#[tauri::command]
pub fn cmd_pick_hex_file(path: Option<String>) -> Result<(String, String), String> {
    let file_path = match path {
        Some(p) => {
            let p = PathBuf::from(p);
            let is_hex = p
                .extension()
                .and_then(|e| e.to_str())
                .is_some_and(|e| e.eq_ignore_ascii_case("hex"));
            if !is_hex {
                return Err("not a .hex file".into());
            }
            p
        }
        None => rfd::FileDialog::new()
            .add_filter("Intel HEX Firmware", &["hex", "HEX"])
            .pick_file()
            .ok_or("cancelled")?,
    };
    let text = fs::read_to_string(&file_path).map_err(|e| e.to_string())?;
    Ok((file_path.to_string_lossy().into_owned(), text))
}

#[tauri::command]
pub fn cmd_load_dat_file(
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

#[tauri::command]
pub async fn cmd_pick_directory() -> Result<String, String> {
    let dir = rfd::AsyncFileDialog::new()
        .pick_folder()
        .await
        .ok_or("cancelled")?;
    Ok(dir.path().to_string_lossy().into_owned())
}

#[tauri::command]
pub async fn cmd_flash_firmware(
    app: tauri::AppHandle,
    state: State<'_, AppState>,
    hex_text: String,
    expected_sha256: Option<String>,
    legacy_port: Option<String>,
) -> Result<String, FlashFailure> {
    let client_arc = Arc::clone(&state.client);
    let streaming = Arc::clone(&state.streaming);
    let stream_gen = Arc::clone(&state.stream_gen);
    let stream_lifecycle = Arc::clone(&state.stream_lifecycle);
    let stream_thread = Arc::clone(&state.stream_thread);
    let serial_slot = Arc::clone(&state.camera_serial);
    let failed = |detail: String| FlashFailure {
        kind: FlashFailureKind::NotFound,
        detail,
    };

    tauri::async_runtime::spawn_blocking(move || {
        let _lifecycle = stream_lifecycle.lock().map_err(|e| failed(e.to_string()))?;
        let before_wait = || {
            // Stop the live stream and release the serial port after asking for update mode.
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
            if let Ok(mut s) = serial_slot.lock() {
                *s = None;
            }
            if let Some(p) = &legacy_port {
                if !p.starts_with("sim://") {
                    let _ = trigger_legacy_134_baud(p);
                }
            }
        };
        run_flash_update(
            &hex_text,
            expected_sha256.as_deref(),
            FlashUpdateOptions::default(),
            before_wait,
            RealHalfKayDevice::open,
            |event| {
                let _ = app.emit("flash-progress", event);
            },
        )
    })
    .await
    .map_err(|e| failed(e.to_string()))?
}
