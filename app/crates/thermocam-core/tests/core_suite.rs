use std::io::Cursor;
use std::time::Instant;

use thermocam_core::dat::{
    DatTempPoint, ThermalDat, ThermoSidecar, LEPTON2_BIG_SIZE, LEPTON2_SMALL_SIZE,
    LEPTON3_BIG_SIZE, LEPTON3_SMALL_SIZE,
};
use thermocam_core::export::{
    encode_gif, encode_h264_mp4, encode_jpeg, encode_png, encode_radiometric_tiff_16,
    encode_temperature_csv, generate_html_report, generate_markdown_report, render_export_canvas,
    ExportOptions, ExportResolution,
};
use thermocam_core::flasher::{
    flash_firmware, parse_teensy41_hex, MockHalfKayDevice, TEENSY41_REPORT_SIZE,
};
use thermocam_core::imaging::{DisplayPipeline, SpatialFilter, UpscaleMethod};
use thermocam_core::palettes::{
    builtin_palettes, find_palette, parse_custom_palette_text, FIRMWARE_PALETTES,
};
use thermocam_core::protocol::codec::{
    encode_goodbye_req, encode_ping_req, FrameRaw, HelloReq, StreamCtrl,
};
use thermocam_core::protocol::crc::{crc32_ieee, crc8_smbus};
use thermocam_core::protocol::messages::{
    encode_enter_bootloader_req, encode_generic_error_resp, RadParams,
};
use thermocam_core::protocol::parser::StreamParser;
use thermocam_core::protocol::transport::{
    generate_demo_frame, ProtocolClient, SimulatorTransport,
};
use thermocam_core::protocol::{MsgType, StatusCode};
use thermocam_core::radiometry::{
    celsius_to_centikelvin, centikelvin_to_celsius, compensate_emissivity_c, emissivity_to_q3_13,
    q3_13_to_emissivity, raw_to_object_c, RadiometryParams, TempUnit, MATERIAL_PRESETS,
};
use thermocam_core::roi::{
    compute_histogram_256, compute_temp_grid, evaluate_roi, roi_contrast_range,
    sample_line_profile, AlarmAction, AlarmCondition, IsothermConfig, IsothermMode, Roi,
    RoiGeometry, TempSwitchRule,
};

fn parse_hex(s: &str) -> Vec<u8> {
    s.split_whitespace()
        .map(|tok| u8::from_str_radix(tok, 16).expect("valid hex"))
        .collect()
}

#[test]
fn protocol_v2_all_golden_vectors_and_resync() {
    assert_eq!(crc8_smbus(b"123456789"), 0xF4);
    assert_eq!(crc32_ieee(b"123456789"), 0xCBF4_3926);

    // 1. HELLO_REQ
    let v1 = parse_hex(
        "54 43 32 00 02 01 00 A4 01 00 00 00 08 00 00 00 \
         02 00 00 00 01 00 00 00 86 4F 91 47",
    );
    let hello = HelloReq {
        host_proto_major: 2,
        host_proto_minor: 0,
        host_features: 1,
    };
    assert_eq!(hello.encode_packet(1).unwrap(), v1);

    // 2. PING_REQ
    let v2 = parse_hex(
        "54 43 32 00 02 03 00 41 02 00 00 00 08 00 00 00 \
         40 42 0F 00 00 00 00 00 9F F9 EC D6",
    );
    assert_eq!(encode_ping_req(2, 1_000_000).unwrap(), v2);

    // 3. ENTER_BOOTLOADER_REQ
    let v3 = parse_hex(
        "54 43 32 00 02 F0 00 9C 2A 00 00 00 04 00 00 00 \
         AD 10 07 B0 0F D1 8E 1C",
    );
    assert_eq!(encode_enter_bootloader_req(42).unwrap(), v3);

    // 4. SET_RAD_PARAMS_REQ
    let v4 = parse_hex(
        "54 43 32 00 02 1B 00 3E 07 00 00 00 12 00 00 00 \
         66 1E 4B 73 00 20 4B 73 00 20 4B 73 00 00 4B 73 \
         01 00 AD 8B 28 A2",
    );
    let rp = RadParams {
        scene_emissivity: 7782,
        t_bkg_ck: 29515,
        tau_window: 8192,
        t_window_ck: 29515,
        tau_atm: 8192,
        t_atm_ck: 29515,
        refl_window: 0,
        t_refl_ck: 29515,
        flags: 1,
    };
    assert_eq!(rp.encode_set_packet(7).unwrap(), v4);

    // 5. GENERIC_ERROR_RESP
    let v5 = parse_hex(
        "54 43 32 00 02 FF 02 65 63 00 02 00 00 00 00 00 \
         1B 41 40 96",
    );
    assert_eq!(
        encode_generic_error_resp(99, StatusCode::ErrUnknownMsg as u16, "").unwrap(),
        v5
    );

    // 6. GOODBYE_REQ
    let v6 = parse_hex(
        "54 43 32 00 02 02 00 41 64 00 00 00 00 00 00 00 \
         F0 C0 F2 48",
    );
    assert_eq!(encode_goodbye_req(100).unwrap(), v6);

    // 7. 38-byte Resync stream (byte-by-byte AND chunked)
    let resync = parse_hex(
        "AA BB 54 43 32 00 02 01 00 FF 00 00 00 00 00 00 \
         01 00 54 43 32 00 02 02 00 41 64 00 00 00 00 00 \
         00 00 F0 C0 F2 48",
    );
    assert_eq!(resync.len(), 38);
    let mut parser = StreamParser::new();
    let mut decoded = Vec::new();
    for &b in &resync {
        decoded.extend(parser.feed(&[b]));
    }
    assert_eq!(parser.stats().usb_rx_errors, 1);
    assert_eq!(decoded.len(), 1);
    let pkt = decoded.pop().unwrap().unwrap();
    assert_eq!(pkt.msg_type().unwrap(), MsgType::GoodbyeReq);
    assert_eq!(pkt.header.seq, 100);
}

#[test]
fn simulator_handshake_stream_and_sd_access() {
    let mut client = ProtocolClient::new(SimulatorTransport::new());
    let hello = client.hello().unwrap();
    assert_eq!(hello.sensor_w, 160);
    assert_eq!(hello.sensor_h, 120);
    assert_eq!(hello.hw_version, 3);

    client
        .set_stream(StreamCtrl {
            mode: 1,
            keep_lcd_on: false,
            include_temp_points: false,
            perf_interval_s: 0,
            max_fps_x10: 0,
        })
        .unwrap();

    let pkts = client
        .poll_packets(std::time::Duration::from_millis(50))
        .unwrap();
    let raw_pkt = pkts
        .into_iter()
        .find(|p| p.msg_type().ok() == Some(MsgType::FrameRaw))
        .expect("expected FrameRaw");
    let frame = FrameRaw::from_bytes(&raw_pkt.payload).unwrap();
    assert_eq!(frame.pixels.len(), 19_200);

    let dat_bytes = client.read_file_bytes("/20261125101500.DAT").unwrap();
    assert_eq!(dat_bytes.len(), LEPTON3_BIG_SIZE);
    let dat = ThermalDat::from_bytes(&dat_bytes).unwrap();
    assert_eq!(dat.width, 160);

    client.delete_file("/20261125102845.DAT").unwrap();
    assert!(client.read_file_bytes("/20261125102845.DAT").is_err());
}

#[test]
fn firmware_palettes_parity_and_custom_import() {
    assert_eq!(FIRMWARE_PALETTES.len(), 19);
    let expected_lengths = [
        ("arctic", 240),
        ("blackHot", 224),
        ("blueRed", 192),
        ("doubleRainbow", 256),
        ("ironblack", 256),
        ("lava", 240),
        ("rainbow", 256),
        ("whiteHot", 224),
        ("yellow", 224),
    ];
    for (key, len) in expected_lengths {
        let p = find_palette(key).unwrap();
        assert_eq!(p.rgb.len(), len, "palette {key} length mismatch");
    }
    // Verify exact first RGB triple of arctic from colorschemes.cpp line 23: 15, 16, 146
    assert_eq!(find_palette("arctic").unwrap().rgb[0], [15, 16, 146]);
    // All 19 firmware + 5 perceptual = 24
    assert_eq!(builtin_palettes().len(), 24);
    for extra in ["inferno", "viridis", "magma", "plasma", "turbo"] {
        assert_eq!(find_palette(extra).unwrap().rgb.len(), 256);
    }
    let custom = parse_custom_palette_text("TestRamp", "#000000\n#ff8000\n#ffffff").unwrap();
    assert_eq!(custom.rgb.len(), 256);
    assert_eq!(custom.rgb[0], [0, 0, 0]);
    assert_eq!(custom.rgb[255], [255, 255, 255]);
}

#[test]
fn dat_all_four_sizes_and_legacy_192_points_roundtrip() {
    for (w, h, with_pts, expected_len) in [
        (80u16, 60u16, false, LEPTON2_SMALL_SIZE),
        (80u16, 60u16, true, LEPTON2_BIG_SIZE),
        (160u16, 120u16, false, LEPTON3_SMALL_SIZE),
        (160u16, 120u16, true, LEPTON3_BIG_SIZE),
    ] {
        let frame = generate_demo_frame(1, w, h);
        let mut dat = ThermalDat::from_frame(&frame, 10, 0);
        dat.has_temp_points_block = with_pts;
        if with_pts {
            dat.temp_points = vec![DatTempPoint {
                index: 10 + 20 * 160 + 1,
                x: if w == 80 { 5 } else { 10 },
                y: if w == 80 { 10 } else { 20 },
                raw: 30_500,
            }];
        }
        let bytes = dat.to_bytes();
        assert_eq!(bytes.len(), expected_len);
        let parsed = ThermalDat::from_bytes(&bytes).unwrap();
        assert_eq!(parsed.pixels, dat.pixels);
        assert_eq!(parsed.min_raw, dat.min_raw);
        assert_eq!(parsed.max_raw, dat.max_raw);
        assert!((parsed.cal_slope - 0.01).abs() < 1e-6);
        if with_pts {
            assert_eq!(parsed.temp_points.len(), 1);
            assert_eq!(parsed.temp_points[0].raw, 30_500);
        }
    }

    // Legacy 192-grid point heuristic (isolated non-zero at slot 17 -> x=10, y=10)
    let frame = generate_demo_frame(2, 160, 120);
    let mut bytes = ThermalDat::from_frame(&frame, 10, 0).to_bytes();
    let pts_off = 19_200 * 2 + 21;
    bytes[pts_off..].fill(0);
    let legacy_val = 31_234u16.to_be_bytes();
    bytes[pts_off + 17 * 2] = legacy_val[0];
    bytes[pts_off + 17 * 2 + 1] = legacy_val[1];
    let parsed = ThermalDat::from_bytes(&bytes).unwrap();
    assert_eq!(parsed.temp_points.len(), 1);
    assert_eq!(parsed.temp_points[0].x, 10);
    assert_eq!(parsed.temp_points[0].y, 10);
    assert_eq!(parsed.temp_points[0].raw, 31_234);

    let sc = ThermoSidecar::default();
    let json = serde_json::to_string(&sc).unwrap();
    let back: ThermoSidecar = serde_json::from_str(&json).unwrap();
    assert_eq!(back.palette_id, "ironblack");
}

#[test]
fn radiometry_emissivity_roi_and_tempswitch() {
    // At epsilon = 1.0, apparent == object
    assert!((compensate_emissivity_c(50.0, 1.0, 22.0) - 50.0).abs() < 1e-4);
    // Low-emissivity hot surface appears colder than true temperature when T_obj > T_refl
    let t_true = compensate_emissivity_c(50.0, 0.5, 22.0);
    assert!(t_true > 70.0 && t_true < 85.0, "got {t_true}");

    assert_eq!(emissivity_to_q3_13(0.95), 7782);
    assert!((q3_13_to_emissivity(7782) - 0.95).abs() < 0.001);
    assert_eq!(celsius_to_centikelvin(22.0), 29515);
    assert!((centikelvin_to_celsius(29515) - 22.0).abs() < 0.01);
    assert!(MATERIAL_PRESETS.len() >= 15);

    let frame = generate_demo_frame(3, 160, 120);
    let rad = RadiometryParams::default();
    let box_roi = Roi {
        id: "r1".into(),
        label: "MCU".into(),
        geometry: RoiGeometry::Box {
            x: 45,
            y: 45,
            w: 30,
            h: 25,
        },
        emissivity_override: Some(0.91),
        reflected_temp_override_c: None,
        visible: true,
    };
    let stats = evaluate_roi(&box_roi, &frame.pixels, 160, 120, rad);
    assert_eq!(stats.pixel_count, 750);
    assert!((stats.effective_emissivity - 0.91).abs() < 1e-6);
    assert!(stats.max_c > stats.min_c);

    let (lo, hi) = roi_contrast_range(&box_roi, &frame.pixels, 160, 120, 0.05).unwrap();
    assert!(hi > lo);

    let line = sample_line_profile(&frame.pixels, 160, 120, 10, 60, 150, 60, rad);
    assert_eq!(line.len(), 141);

    let grid = compute_temp_grid(&frame.pixels, 160, 120, 4, 3, rad);
    assert_eq!(grid.len(), 3);
    assert_eq!(grid[0].len(), 4);

    let hist = compute_histogram_256(&frame.pixels, frame.meta.min_raw, frame.meta.max_raw);
    assert_eq!(hist.iter().sum::<u32>(), 19_200);

    let mut rule = TempSwitchRule {
        id: "a1".into(),
        name: "Overtemp".into(),
        enabled: true,
        target_roi_id: None,
        condition: AlarmCondition::Above,
        threshold_c: 60.0,
        cooldown_ms: 5000,
        action: AlarmAction::SaveDat,
        last_triggered_ms: None,
    };
    assert!(!rule.evaluate(55.0, 1000));
    assert!(rule.evaluate(62.0, 2000));
    assert!(!rule.evaluate(65.0, 4000)); // still in cooldown
    assert!(rule.evaluate(65.0, 7500)); // cooldown elapsed
    // Circle, Polygon, and 2-point Delta ROIs
    let circle = Roi {
        id: "c1".into(),
        label: "VRM".into(),
        geometry: RoiGeometry::Circle {
            cx: 108.0,
            cy: 41.0,
            radius: 10.0,
        },
        emissivity_override: None,
        reflected_temp_override_c: None,
        visible: true,
    };
    assert!(evaluate_roi(&circle, &frame.pixels, 160, 120, rad).pixel_count > 250);

    let poly = Roi {
        id: "p1".into(),
        label: "Heatsink".into(),
        geometry: RoiGeometry::Polygon {
            points: vec![[14.0, 24.0], [32.0, 24.0], [32.0, 90.0], [14.0, 90.0]],
        },
        emissivity_override: Some(0.25),
        reflected_temp_override_c: Some(24.0),
        visible: true,
    };
    let pstats = evaluate_roi(&poly, &frame.pixels, 160, 120, rad);
    assert!(pstats.pixel_count > 900);
    assert!((pstats.effective_emissivity - 0.25).abs() < 1e-6);

    let delta = Roi {
        id: "d1".into(),
        label: "ΔT".into(),
        geometry: RoiGeometry::Delta {
            x0: 22,
            y0: 55,
            x1: 61,
            y1: 58,
        },
        emissivity_override: None,
        reflected_temp_override_c: None,
        visible: true,
    };
    let dstats = evaluate_roi(&delta, &frame.pixels, 160, 120, rad);
    assert!(dstats.delta_c.unwrap() > 30.0);

    let _ = raw_to_object_c(30000, rad);
}

#[test]
fn full_hd_1080p_upscaler_png_mp4_gif_tiff_csv_and_reports() {
    let frame = generate_demo_frame(5, 160, 120);
    let palette = find_palette("inferno").unwrap();
    let rad = RadiometryParams {
        emissivity: 0.95,
        reflected_temp_c: 22.0,
        ..RadiometryParams::default()
    };
    let pipeline = DisplayPipeline {
        filter: SpatialFilter::None,
        upscaler: UpscaleMethod::EdgePreserve,
        sharpen_amount: 0.5,
        rotate_180: false,
        flip_horizontal: false,
        histogram_eq: false,
        isotherm: IsothermConfig {
            mode: IsothermMode::Above,
            low_c: 40.0,
            high_c: 62.0,
            color: [0, 230, 160],
        },
    };
    let rois = vec![
        Roi {
            id: "spot1".into(),
            label: "MCU".into(),
            geometry: RoiGeometry::Spot {
                x: 61,
                y: 58,
                kernel: 3,
                show_raw: true,
            },
            emissivity_override: None,
            reflected_temp_override_c: None,
            visible: true,
        },
        Roi {
            id: "line1".into(),
            label: "Profile".into(),
            geometry: RoiGeometry::Line {
                x0: 20,
                y0: 58,
                x1: 140,
                y1: 58,
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

    let t0 = Instant::now();
    let (w, h, rgb) = render_export_canvas(
        &frame.pixels,
        160,
        120,
        frame.meta.min_raw,
        frame.meta.max_raw,
        &palette,
        rad,
        &pipeline,
        &rois,
        &opts,
    );
    let render_ms = t0.elapsed().as_millis();
    assert_eq!((w, h), (1920, 1080));
    assert_eq!(rgb.len(), 1920 * 1080 * 3);

    let png = encode_png(w, h, &rgb).unwrap();
    assert!(png.starts_with(&[0x89, b'P', b'N', b'G']));
    let jpg = encode_jpeg(w, h, &rgb, 92).unwrap();
    assert!(jpg.starts_with(&[0xFF, 0xD8, 0xFF]));

    // Zero-FFmpeg H.264 MP4 at Full HD 1920×1080 (3 frames) + verify via mp4::Mp4Reader
    let t_mp4 = Instant::now();
    let mp4_bytes = encode_h264_mp4(w, h, &[rgb.clone(), rgb.clone(), rgb.clone()], 9).unwrap();
    let mp4_ms = t_mp4.elapsed().as_millis();
    assert!(mp4_bytes.len() > 1000);
    let reader = mp4::Mp4Reader::read_header(
        Cursor::new(&mp4_bytes),
        mp4_bytes.len() as u64,
    )
    .unwrap();
    let track = reader.tracks().get(&1).expect("video track 1");
    assert_eq!(track.width(), 1920);
    assert_eq!(track.height(), 1080);
    assert_eq!(track.sample_count(), 3);
    eprintln!("Benchmark: 1920x1080 EdgePreserve+Unsharp={render_ms}ms, 3-frame H.264 MP4={mp4_ms}ms ({} B)", mp4_bytes.len());

    // Animated GIF (compact)
    let (_, _, small_rgb) = render_export_canvas(
        &frame.pixels,
        160,
        120,
        frame.meta.min_raw,
        frame.meta.max_raw,
        &palette,
        rad,
        &DisplayPipeline {
            upscaler: UpscaleMethod::Lanczos3,
            ..pipeline
        },
        &rois,
        &ExportOptions {
            resolution: ExportResolution::Custom {
                width: 320,
                height: 240,
            },
            ..opts.clone()
        },
    );
    let gif_bytes = encode_gif(320, 240, &[small_rgb.clone(), small_rgb], 9.0).unwrap();
    assert!(gif_bytes.starts_with(b"GIF89a"));

    // 16-bit radiometric Kelvin TIFF
    let tiff_bytes = encode_radiometric_tiff_16(&frame.pixels, 160, 120, rad).unwrap();
    assert!(tiff_bytes.starts_with(b"II*\0") || tiff_bytes.starts_with(b"MM\0*"));

    // CSV & HTML/Markdown reports
    let csv = encode_temperature_csv(&frame.pixels, 160, 120, rad, TempUnit::Celsius);
    assert_eq!(csv.lines().count(), 121);
    let html = generate_html_report(
        "PCB Thermal Audit",
        "MCU steady-state load",
        &frame.pixels,
        160,
        120,
        frame.meta.min_raw,
        frame.meta.max_raw,
        &palette,
        rad,
        &pipeline,
        &rois,
        &ExportOptions {
            resolution: ExportResolution::Hd720p,
            ..opts
        },
    )
    .unwrap();
    assert!(html.contains("data:image/png;base64,"));
    assert!(html.contains("<svg"));
    let md = generate_markdown_report(
        "PCB Thermal Audit",
        "Steady state",
        "thermal.png",
        &frame.pixels,
        160,
        120,
        &palette,
        rad,
        &rois,
        TempUnit::Celsius,
    );
    assert!(md.contains("| MCU |"));
}

#[test]
fn halfkay_hex_parse_sha256_and_mock_hid_flash() {
    // Valid Intel HEX record at Teensy 4.1 flash base 0x6000_0000
    let hex = ":0200000460009A\n:100000000102030405060708090A0B0C0D0E0F1068\n:00000001FF\n";
    let img = parse_teensy41_hex(hex, None).unwrap();
    assert_eq!(img.total_bytes, 16);
    assert_eq!(img.block_count, 1);

    // SHA-256 verification pass & mismatch
    assert!(parse_teensy41_hex(hex, Some(&img.sha256_hex)).is_ok());
    assert!(parse_teensy41_hex(hex, Some("deadbeef")).is_err());

    let mut mock = MockHalfKayDevice::default();
    let mut progress = Vec::new();
    flash_firmware(&mut mock, &img, |done, total| progress.push((done, total))).unwrap();
    assert_eq!(progress, vec![(1, 1)]);
    // 1 data block + 1 reboot packet = 2 packets of 1088 bytes
    assert_eq!(mock.packets.len(), 2);
    assert_eq!(mock.packets[0].len(), TEENSY41_REPORT_SIZE);
    assert_eq!(&mock.packets[0][0..3], &[0, 0, 0]);
    assert_eq!(&mock.packets[0][64..68], &[1, 2, 3, 4]);
    assert_eq!(&mock.packets[1][0..3], &[0xFF, 0xFF, 0xFF]);
}

#[test]
fn circle_polygon_delta_rois_and_uhd_4k_resolutions() {
    let frame = generate_demo_frame(7, 160, 120);
    let rad = RadiometryParams {
        emissivity: 0.95,
        reflected_temp_c: 22.0,
        ..RadiometryParams::default()
    };

    let circle = Roi {
        id: "c1".into(),
        label: "Regulator".into(),
        geometry: RoiGeometry::Circle {
            cx: 108.0,
            cy: 41.0,
            radius: 10.0,
        },
        emissivity_override: Some(0.65),
        reflected_temp_override_c: Some(25.0),
        visible: true,
    };
    let c_stats = evaluate_roi(&circle, &frame.pixels, 160, 120, rad);
    assert!(c_stats.pixel_count > 250);
    assert!((c_stats.effective_emissivity - 0.65).abs() < 1e-6);

    let poly = Roi {
        id: "p1".into(),
        label: "Trace".into(),
        geometry: RoiGeometry::Polygon {
            points: vec![[50.0, 40.0], [110.0, 35.0], [110.0, 65.0], [50.0, 70.0]],
        },
        emissivity_override: None,
        reflected_temp_override_c: None,
        visible: true,
    };
    let p_stats = evaluate_roi(&poly, &frame.pixels, 160, 120, rad);
    assert!(p_stats.pixel_count > 1000);

    let delta = Roi {
        id: "d1".into(),
        label: "MCU-Heatsink".into(),
        geometry: RoiGeometry::Delta {
            x0: 22,
            y0: 60,
            x1: 61,
            y1: 58,
        },
        emissivity_override: None,
        reflected_temp_override_c: None,
        visible: true,
    };
    let d_stats = evaluate_roi(&delta, &frame.pixels, 160, 120, rad);
    assert!(d_stats.delta_c.expect("delta") > 25.0);

    assert_eq!(ExportResolution::Qhd1440p.dimensions(160, 120), (2560, 1440));
    assert_eq!(ExportResolution::Uhd4k.dimensions(160, 120), (3840, 2160));

    // Verify custom 256-px PNG LUT import roundtrip
    let mut lut_rgb = Vec::with_capacity(256 * 3);
    for i in 0..256u16 {
        lut_rgb.push(i as u8);
        lut_rgb.push((i / 2) as u8);
        lut_rgb.push((255 - i) as u8);
    }
    let lut_png = encode_png(256, 1, &lut_rgb).unwrap();
    let parsed_pal =
        thermocam_core::palettes::parse_custom_palette_png("CustomRamp", &lut_png).unwrap();
    assert_eq!(parsed_pal.rgb.len(), 256);
    assert_eq!(parsed_pal.rgb[0], [0, 0, 255]);
    assert_eq!(parsed_pal.rgb[255], [255, 127, 0]);

    // Verify display-only clipped-CDF Histogram Equalization (HEQ / CLAHE)
    let low_contrast: Vec<f32> = (0..256).map(|i| 0.40 + (i as f32 / 255.0) * 0.12).collect();
    let eq = thermocam_core::imaging::apply_clipped_histogram_eq(&low_contrast);
    assert_eq!(eq.len(), 256);
    assert!(eq[255] - eq[0] > 0.70);

    let heq_pipeline = DisplayPipeline {
        histogram_eq: true,
        ..DisplayPipeline::default()
    };
    let heq_rgb = thermocam_core::imaging::render_thermal_rgb(
        &frame.pixels,
        160,
        120,
        320,
        240,
        frame.meta.min_raw,
        frame.meta.max_raw,
        &parsed_pal,
        rad,
        &heq_pipeline,
    );
    assert_eq!(heq_rgb.len(), 320 * 240 * 3);
}
