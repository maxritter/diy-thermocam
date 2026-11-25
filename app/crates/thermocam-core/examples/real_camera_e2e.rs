//! Real DIY-Thermocam V3 Hardware End-to-End Proof (`thermocam-core` Protocol v2 ↔ Firmware 4.0.0).
//!
//! Run with:
//!   cargo run --release -p thermocam-core --example real_camera_e2e -- /dev/cu.usbmodem93126801 /tmp/real-v3-e2e

use std::env;
use std::fs;
use std::io::Cursor;
use std::path::PathBuf;
use std::time::{Duration, Instant};

use thermocam_core::dat::ThermalDat;
use thermocam_core::export::{
    encode_h264_mp4, encode_png, encode_radiometric_tiff_16, render_export_canvas, ExportOptions,
    ExportResolution,
};
use thermocam_core::imaging::{DisplayPipeline, SpatialFilter, UpscaleMethod};
use thermocam_core::palettes::find_palette;
use thermocam_core::protocol::codec::{FrameRaw, StreamCtrl};
use thermocam_core::protocol::crc::crc32_ieee;
use thermocam_core::protocol::messages::{
    AsyncEvent, FsInfoResp, FsListResp, PerfTelemetry, RadParams,
};
use thermocam_core::protocol::transport::{list_ports, ProtocolClient, SerialTransport};
use thermocam_core::protocol::{capabilities, MsgType};
use thermocam_core::radiometry::{raw_to_object_c, RadiometryParams, TempUnit};
use thermocam_core::roi::{IsothermConfig, Roi, RoiGeometry};

fn resolve_port(arg: Option<String>) -> Result<String, String> {
    if let Some(p) = arg {
        return Ok(p);
    }
    if let Ok(p) = env::var("THERMOCAM_PORT") {
        if !p.is_empty() {
            return Ok(p);
        }
    }
    for p in list_ports() {
        if p.is_thermocam && !p.is_simulator {
            return Ok(p.name);
        }
    }
    Err("no DIY-Thermocam serial port detected (pass port path as arg 1)".into())
}

fn run_session(
    client: &mut ProtocolClient<SerialTransport>,
    out_dir: &PathBuf,
) -> Result<(), String> {
    // 1. HELLO handshake
    let t_hello = Instant::now();
    let hello = client.hello().map_err(|e| format!("HELLO failed: {e}"))?;
    let hello_ms = t_hello.elapsed().as_secs_f64() * 1000.0;
    println!(
        "[1/6] HELLO OK ({hello_ms:.2} ms): proto={}.{} fw={} ('{}') hw=V{} lepton_type={} sensor={}x{} display={}x{} diag=0x{:02X} caps=0x{:04X} max_chunk={} heartbeat={}ms",
        hello.proto_major,
        hello.proto_minor,
        hello.fw_version,
        hello.fw_build_str,
        hello.hw_version,
        hello.lepton_type,
        hello.sensor_w,
        hello.sensor_h,
        hello.display_w,
        hello.display_h,
        hello.diagnostic_mask,
        hello.capabilities,
        hello.max_chunk_size,
        hello.heartbeat_timeout_ms,
    );
    if hello.sensor_w != 160 || hello.sensor_h != 120 {
        return Err(format!(
            "expected 160x120 Lepton 3.5, got {}x{}",
            hello.sensor_w, hello.sensor_h
        ));
    }
    if hello.diagnostic_mask != 0xFF {
        return Err(format!(
            "expected diagnostic_mask=0xFF, got 0x{:02X}",
            hello.diagnostic_mask
        ));
    }
    let required_caps =
        capabilities::RAW_STREAM | capabilities::SD_FS | capabilities::PERF_TELEMETRY;
    if (hello.capabilities & required_caps) != required_caps {
        return Err(format!(
            "missing required capabilities: got 0x{:04X}",
            hello.capabilities
        ));
    }

    // 2. Start live FRAME_RAW push stream and capture >= 25 frames + concurrent control commands!
    let sc = client
        .set_stream(StreamCtrl {
            mode: 1,
            keep_lcd_on: true,
            include_temp_points: true,
            perf_interval_s: 0,
            max_fps_x10: 0,
        })
        .map_err(|e| format!("STREAM_CTRL start failed: {e}"))?;
    println!(
        "[2/6] STREAM_CTRL started: mode={} keep_lcd_on={} include_temp_points={}",
        sc.mode, sc.keep_lcd_on, sc.include_temp_points
    );

    let mut frames: Vec<FrameRaw> = Vec::with_capacity(40);
    let mut first_frame_t: Option<Instant> = None;
    let mut last_frame_t: Option<Instant> = None;
    let mut last_ping_t = Instant::now();
    let mut ffc_events: Vec<(u8, u16)> = Vec::new();
    let stream_deadline = Instant::now() + Duration::from_secs(12);

    // Phase A: collect 12 clean steady-state frames before triggering FFC so FPS measurement is clean
    while frames.len() < 12 && Instant::now() < stream_deadline {
        if last_ping_t.elapsed() >= Duration::from_millis(1500) {
            let _ = client.ping(last_ping_t.elapsed().as_micros() as u64);
            last_ping_t = Instant::now();
        }
        let pkts = client
            .poll_packets(Duration::from_millis(150))
            .map_err(|e| format!("poll_packets failed: {e}"))?;
        for pkt in pkts {
            if pkt.msg_type().ok() == Some(MsgType::FrameRaw) {
                let fr = FrameRaw::from_bytes(&pkt.payload)
                    .map_err(|e| format!("FrameRaw decode failed: {e}"))?;
                let now = Instant::now();
                if first_frame_t.is_none() {
                    first_frame_t = Some(now);
                }
                last_frame_t = Some(now);
                frames.push(fr);
            } else if let Ok(Some(AsyncEvent::Ffc { state, duration_ms })) =
                AsyncEvent::from_packet(&pkt)
            {
                ffc_events.push((state, duration_ms));
            }
        }
    }

    // Measure pure steady-state stream FPS over first batch (before FFC shutter pause)
    let steady_frames = frames.len();
    let steady_fps = match (first_frame_t, last_frame_t) {
        (Some(t0), Some(t1)) if steady_frames >= 2 => {
            let dt = t1.duration_since(t0).as_secs_f64();
            if dt > 0.0 {
                (steady_frames - 1) as f64 / dt
            } else {
                0.0
            }
        }
        _ => 0.0,
    };

    // 3. Concurrent GET_PERF + GET_RAD_PARAMS + TRIGGER_FFC while streaming is ACTIVE!
    let t_perf = Instant::now();
    let perf_pkt = client
        .request(MsgType::GetPerfReq, &[0], Duration::from_millis(2000))
        .map_err(|e| format!("concurrent GET_PERF failed: {e}"))?;
    let perf_ms = t_perf.elapsed().as_secs_f64() * 1000.0;
    let perf = PerfTelemetry::from_bytes(&perf_pkt.payload)
        .map_err(|e| format!("PerfTelemetry decode failed: {e}"))?;

    let t_rad = Instant::now();
    let rad_pkt = client
        .request(MsgType::GetRadParamsReq, &[], Duration::from_millis(2000))
        .map_err(|e| format!("concurrent GET_RAD_PARAMS failed: {e}"))?;
    let rad_ms = t_rad.elapsed().as_secs_f64() * 1000.0;
    let rad_params = RadParams::from_bytes(&rad_pkt.payload)
        .map_err(|e| format!("RadParams decode failed: {e}"))?;

    let t_ffc = Instant::now();
    let _ffc_pkt = client
        .request(MsgType::TriggerFfcReq, &[1], Duration::from_millis(5000))
        .map_err(|e| format!("concurrent TRIGGER_FFC failed: {e}"))?;
    let ffc_ms = t_ffc.elapsed().as_secs_f64() * 1000.0;
    last_ping_t = Instant::now();

    // Collect post-FFC frames until we have >= 25 total real live frames!
    let mut post_first_t: Option<Instant> = None;
    let mut post_last_t: Option<Instant> = None;
    let mut post_count = 0usize;
    let post_deadline = Instant::now() + Duration::from_secs(10);
    while frames.len() < 26 && Instant::now() < post_deadline {
        if last_ping_t.elapsed() >= Duration::from_millis(1500) {
            let _ = client.ping(last_ping_t.elapsed().as_micros() as u64);
            last_ping_t = Instant::now();
        }
        let pkts = client
            .poll_packets(Duration::from_millis(150))
            .map_err(|e| format!("post-FFC poll_packets failed: {e}"))?;
        for pkt in pkts {
            if pkt.msg_type().ok() == Some(MsgType::FrameRaw) {
                let fr = FrameRaw::from_bytes(&pkt.payload)
                    .map_err(|e| format!("FrameRaw decode failed: {e}"))?;
                let now = Instant::now();
                if post_first_t.is_none() {
                    post_first_t = Some(now);
                }
                post_last_t = Some(now);
                post_count += 1;
                frames.push(fr);
            } else if let Ok(Some(AsyncEvent::Ffc { state, duration_ms })) =
                AsyncEvent::from_packet(&pkt)
            {
                ffc_events.push((state, duration_ms));
            }
        }
    }

    // Stop live stream before SD benchmark
    client
        .set_stream(StreamCtrl {
            mode: 0,
            keep_lcd_on: true,
            include_temp_points: false,
            perf_interval_s: 0,
            max_fps_x10: 0,
        })
        .map_err(|e| format!("STREAM_CTRL stop failed: {e}"))?;

    if frames.len() < 20 {
        return Err(format!("expected >=20 real live frames, got {}", frames.len()));
    }
    let post_fps = match (post_first_t, post_last_t) {
        (Some(t0), Some(t1)) if post_count >= 2 => {
            let dt = t1.duration_since(t0).as_secs_f64();
            if dt > 0.0 {
                (post_count - 1) as f64 / dt
            } else {
                0.0
            }
        }
        _ => 0.0,
    };

    let last_fr = frames.last().expect("non-empty frames");
    let rad_cfg = RadiometryParams {
        cal_offset: last_fr.meta.cal_offset,
        cal_slope: last_fr.meta.cal_slope,
        ..RadiometryParams::default()
    };
    let min_c = raw_to_object_c(last_fr.meta.min_raw, rad_cfg);
    let max_c = raw_to_object_c(last_fr.meta.max_raw, rad_cfg);
    let spot_c = last_fr.meta.spot_temp_c;
    let total_drops = last_fr.meta.dropped_frames;
    let pstats = client.parser_stats();

    println!(
        "[2/6] LIVE STREAM OK: frames={} (pre_ffc_fps={:.2} fps, post_ffc_fps={:.2} fps), dropped={}, parser_rx_err={}, parser_crc_err={}, min={:.2} C max={:.2} C spot={:.2} C fpa={:.2} C",
        frames.len(),
        steady_fps,
        post_fps,
        total_drops,
        pstats.usb_rx_errors,
        pstats.usb_crc_errors,
        min_c,
        max_c,
        spot_c,
        last_fr.meta.fpa_temp_c,
    );
    if total_drops != 0 || pstats.usb_crc_errors != 0 {
        return Err(format!(
            "stream had errors: dropped={total_drops}, crc_err={}",
            pstats.usb_crc_errors
        ));
    }
    if !(min_c > -20.0 && max_c < 200.0 && max_c > min_c && spot_c > -20.0 && spot_c < 200.0) {
        return Err(format!(
            "implausible live temperatures: min={min_c:.2} max={max_c:.2} spot={spot_c:.2}"
        ));
    }

    println!(
        "[3/6] CONCURRENT CTRL OK: GET_PERF ({perf_ms:.2} ms) stream_fps={:.2} loop={}us usb_tx={}us cpu={}MHz mcu={:.1}C resyncs={} | GET_RAD_PARAMS ({rad_ms:.2} ms) eps_q313={} ({:.3}) t_bkg={:.2}C | TRIGGER_FFC ({ffc_ms:.1} ms) events={ffc_events:?}",
        f64::from(perf.stream_fps_x100) / 100.0,
        perf.loop_total_us,
        perf.usb_tx_us,
        perf.cpu_mhz,
        perf.mcu_temp_c,
        perf.vospi_resyncs,
        rad_params.scene_emissivity,
        f64::from(rad_params.scene_emissivity) / 8192.0,
        f64::from(rad_params.t_bkg_ck) * 0.01 - 273.15,
    );

    // 4. Real SD card FS_INFO + FS_LIST + FS_STAT(crc=1) + FS_READ
    let info_pkt = client
        .request(MsgType::FsInfoReq, &[], Duration::from_millis(2000))
        .map_err(|e| format!("FS_INFO failed: {e}"))?;
    let fs_info = FsInfoResp::from_bytes(&info_pkt.payload)
        .map_err(|e| format!("FsInfoResp decode failed: {e}"))?;

    let list_req = FsListResp::encode_req(0, 64, "/");
    let list_pkt = client
        .request(MsgType::FsListReq, &list_req, Duration::from_millis(2000))
        .map_err(|e| format!("FS_LIST failed: {e}"))?;
    let fs_list = FsListResp::from_bytes(&list_pkt.payload)
        .map_err(|e| format!("FsListResp decode failed: {e}"))?;

    // Find a .DAT file on SD (prefer 38,805 B real V3 .DAT)
    let dat_entry = fs_list
        .entries
        .iter()
        .filter(|e| !e.is_dir && e.name.to_ascii_uppercase().ends_with(".DAT"))
        .max_by_key(|e| e.size)
        .ok_or_else(|| {
            format!(
                "no .DAT file found on SD card (root entries: {})",
                fs_list.entries.len()
            )
        })?;
    let sd_path = format!("/{}", dat_entry.name);

    let stat = client
        .fs_stat(&sd_path, true)
        .map_err(|e| format!("FS_STAT failed for {sd_path}: {e}"))?;

    let t_read = Instant::now();
    let sd_bytes = client
        .read_file_bytes(&sd_path)
        .map_err(|e| format!("FS_READ failed for {sd_path}: {e}"))?;
    let read_sec = t_read.elapsed().as_secs_f64();
    let mb_s = (sd_bytes.len() as f64 / (1024.0 * 1024.0)) / read_sec.max(1e-6);
    let local_crc = crc32_ieee(&sd_bytes);
    if stat.crc32_valid && local_crc != stat.crc32 {
        return Err(format!(
            "SD CRC-32 mismatch for {sd_path}: device=0x{:08X}, local=0x{:08X}",
            stat.crc32, local_crc
        ));
    }
    let sd_dat = ThermalDat::from_bytes(&sd_bytes)
        .map_err(|e| format!("downloaded SD .DAT parse failed: {e}"))?;
    println!(
        "[4/6] SD CARD OK: present={} fs_type={} total={}MB free={}MB root_entries={} | downloaded {} ({} B) in {:.2} ms ({:.2} MB/s) CRC32=0x{:08X} (verified={}) parsed={}x{}",
        fs_info.card_present,
        fs_info.fs_type,
        fs_info.total_kb / 1024,
        fs_info.free_kb / 1024,
        fs_info.root_entry_count,
        sd_path,
        sd_bytes.len(),
        read_sec * 1000.0,
        mb_s,
        local_crc,
        stat.crc32_valid,
        sd_dat.width,
        sd_dat.height,
    );

    // 5. Render real 1920x1080 PNG + zero-FFmpeg H.264 MP4 + 16-bit Kelvin TIFF from live V3 frames
    fs::create_dir_all(out_dir).map_err(|e| format!("create_dir_all failed: {e}"))?;
    let palette = find_palette("ironblack").map_err(|e| e.to_string())?;
    let pipeline = DisplayPipeline {
        filter: SpatialFilter::None,
        upscaler: UpscaleMethod::EdgePreserve, // bilateral + Lanczos-3 + Unsharp Mask
        sharpen_amount: 0.55,
        rotate_180: false,
        flip_horizontal: false,
        histogram_eq: false,
        isotherm: IsothermConfig::default(),
    };
    let rois = vec![
        Roi {
            id: "spot_center".into(),
            label: "Center".into(),
            geometry: RoiGeometry::Spot {
                x: last_fr.meta.width / 2,
                y: last_fr.meta.height / 2,
                kernel: 3,
                show_raw: false,
            },
            emissivity_override: None,
            reflected_temp_override_c: None,
            visible: true,
        },
        Roi {
            id: "hotspot".into(),
            label: "Max".into(),
            geometry: RoiGeometry::Spot {
                x: last_fr.meta.max_x,
                y: last_fr.meta.max_y,
                kernel: 1,
                show_raw: false,
            },
            emissivity_override: None,
            reflected_temp_override_c: None,
            visible: true,
        },
    ];
    let opts = ExportOptions {
        resolution: ExportResolution::FullHd1080p,
        show_colorbar: true,
        show_hud: true,
        unit: TempUnit::Celsius,
        jpeg_quality: 94,
    };

    let t_png = Instant::now();
    let (w, h, rgb_1080) = render_export_canvas(
        &last_fr.pixels,
        last_fr.meta.width,
        last_fr.meta.height,
        last_fr.meta.min_raw,
        last_fr.meta.max_raw,
        &palette,
        rad_cfg,
        &pipeline,
        &rois,
        &opts,
    );
    let png_bytes = encode_png(w, h, &rgb_1080).map_err(|e| format!("PNG encode failed: {e}"))?;
    let png_ms = t_png.elapsed().as_secs_f64() * 1000.0;
    let png_path = out_dir.join("real-v3-1080p-capture.png");
    fs::write(&png_path, &png_bytes).map_err(|e| format!("write PNG failed: {e}"))?;

    // 16-bit radiometric Kelvin TIFF
    let tiff_bytes = encode_radiometric_tiff_16(
        &last_fr.pixels,
        last_fr.meta.width,
        last_fr.meta.height,
        rad_cfg,
    )
    .map_err(|e| format!("TIFF encode failed: {e}"))?;
    let mut tiff_dec = tiff::decoder::Decoder::new(Cursor::new(&tiff_bytes))
        .map_err(|e| format!("TIFF verify decode failed: {e}"))?;
    let tiff_dims = tiff_dec
        .dimensions()
        .map_err(|e| format!("TIFF dimensions failed: {e}"))?;
    let tiff_path = out_dir.join("real-v3-kelvin-16bit.tiff");
    fs::write(&tiff_path, &tiff_bytes).map_err(|e| format!("write TIFF failed: {e}"))?;

    // Zero-FFmpeg H.264 MP4 from 12 real consecutive post-FFC live V3 frames (1280x720 or 1920x1080)
    // Use 1920x1080 for full HD proof!
    let mp4_frames_src: Vec<&FrameRaw> = frames.iter().rev().take(12).collect::<Vec<_>>().into_iter().rev().collect();
    let t_mp4 = Instant::now();
    let mut rgb_seq: Vec<Vec<u8>> = Vec::with_capacity(mp4_frames_src.len());
    for fr in &mp4_frames_src {
        let (_, _, rgb) = render_export_canvas(
            &fr.pixels,
            fr.meta.width,
            fr.meta.height,
            fr.meta.min_raw,
            fr.meta.max_raw,
            &palette,
            rad_cfg,
            &pipeline,
            &rois,
            &opts,
        );
        rgb_seq.push(rgb);
    }
    let mp4_bytes =
        encode_h264_mp4(w, h, &rgb_seq, 9).map_err(|e| format!("H.264 MP4 encode failed: {e}"))?;
    let mp4_ms = t_mp4.elapsed().as_secs_f64() * 1000.0;
    let mp4_reader = mp4::Mp4Reader::read_header(Cursor::new(&mp4_bytes), mp4_bytes.len() as u64)
        .map_err(|e| format!("MP4 verify failed: {e}"))?;
    let track = mp4_reader.tracks().get(&1).ok_or("missing MP4 video track 1")?;
    let mp4_path = out_dir.join("real-v3-1080p-live.mp4");
    fs::write(&mp4_path, &mp4_bytes).map_err(|e| format!("write MP4 failed: {e}"))?;

    println!(
        "[5/6] EXPORTS OK: PNG={}x{} ({} B, {:.1} ms) -> {} | MP4={}x{} {} frames ({} B, {:.1} ms) -> {} | TIFF={}x{} ({} B) -> {}",
        w,
        h,
        png_bytes.len(),
        png_ms,
        png_path.display(),
        track.width(),
        track.height(),
        track.sample_count(),
        mp4_bytes.len(),
        mp4_ms,
        mp4_path.display(),
        tiff_dims.0,
        tiff_dims.1,
        tiff_bytes.len(),
        tiff_path.display(),
    );

    Ok(())
}

fn main() {
    let mut args = env::args().skip(1);
    let port_arg = args.next();
    let out_dir = args
        .next()
        .map(PathBuf::from)
        .unwrap_or_else(|| env::temp_dir().join("diy-thermocam-real-e2e"));

    let port = match resolve_port(port_arg) {
        Ok(p) => p,
        Err(e) => {
            eprintln!("ERROR: {e}");
            std::process::exit(1);
        }
    };
    println!("Opening real DIY-Thermocam V3 on {port} ...");
    let transport = match SerialTransport::open(&port) {
        Ok(t) => t,
        Err(e) => {
            eprintln!("ERROR: failed to open {port}: {e}");
            std::process::exit(1);
        }
    };
    let mut client = ProtocolClient::new(transport);
    let res = run_session(&mut client, &out_dir);

    // 6. ALWAYS send clean GOODBYE and drop SerialTransport to close the port!
    let goodbye_res = client.goodbye();
    drop(client);
    match &goodbye_res {
        Ok(()) => println!("[6/6] GOODBYE OK & serial port {port} CLOSED cleanly."),
        Err(e) => eprintln!("[6/6] GOODBYE warning ({e}); serial port {port} CLOSED."),
    }

    if let Err(e) = res {
        eprintln!("E2E FAILED: {e}");
        std::process::exit(1);
    }
    if let Err(e) = goodbye_res {
        eprintln!("E2E FAILED on GOODBYE: {e}");
        std::process::exit(1);
    }
}
