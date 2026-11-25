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
    let reader =
        mp4::Mp4Reader::read_header(Cursor::new(&mp4_bytes), mp4_bytes.len() as u64).unwrap();
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
    use std::time::Duration;
    use thermocam_core::flasher::{
        flash_firmware_with_options, wait_for_device, FlashError, FlashOptions,
    };

    // Valid Intel HEX record at Teensy 4.1 flash base 0x6000_0000 with 'FCFB' FlexSPI boot header
    let hex = ":0200000460009A\n:100000004643464200000156000000000000000088\n:00000001FF\n";
    let img = parse_teensy41_hex(hex, None).unwrap();
    assert_eq!(img.total_bytes, 16);
    assert_eq!(img.block_count, 1);

    // SHA-256 verification pass & mismatch
    assert!(parse_teensy41_hex(hex, Some(&img.sha256_hex)).is_ok());
    assert!(matches!(
        parse_teensy41_hex(hex, Some("deadbeef")),
        Err(FlashError::ChecksumMismatch { .. })
    ));

    // Reject non-Teensy 4.1 address below 0x6000_0000
    let wrong_board = ":020000040000FA\n:100000004643464200000156000000000000000088\n:00000001FF\n";
    assert!(matches!(
        parse_teensy41_hex(wrong_board, None),
        Err(FlashError::AddressOutOfBounds(0))
    ));

    // Reject truncated Intel HEX missing EOF record
    let truncated = ":0200000460009A\n:100000004643464200000156000000000000000088\n";
    assert!(matches!(
        parse_teensy41_hex(truncated, None),
        Err(FlashError::TruncatedHex)
    ));

    // Reject bad FlexSPI boot header (missing 'FCFB')
    let bad_hdr = ":0200000460009A\n:100000000102030405060708090A0B0C0D0E0F1068\n:00000001FF\n";
    assert!(matches!(
        parse_teensy41_hex(bad_hdr, None),
        Err(FlashError::InvalidBootHeader)
    ));

    // Verify enumeration polling succeeds once HalfKay appears
    let mut polls = 0usize;
    let found = wait_for_device(Duration::from_millis(200), Duration::from_millis(5), || {
        polls += 1;
        if polls < 3 {
            Err(FlashError::DeviceNotFound)
        } else {
            Ok("halfkay-ready")
        }
    })
    .unwrap();
    assert_eq!(found, "halfkay-ready");
    assert_eq!(polls, 3);

    // Verify per-block retry recovers from transient erase busy errors on block 0 and reboot
    let mut mock = MockHalfKayDevice::default();
    mock.fail_block_attempts.insert(0, 3);
    mock.fail_block_attempts.insert(1, 2);
    let mut progress = Vec::new();
    let fast_opts = FlashOptions {
        max_attempts: 4,
        first_block_retry_delay: Duration::from_millis(1),
        block_retry_delay: Duration::from_millis(1),
        first_block_extra_attempts: 4,
    };
    flash_firmware_with_options(&mut mock, &img, fast_opts, |done, total| {
        progress.push((done, total))
    })
    .unwrap();
    assert_eq!(progress, vec![(1, 1)]);
    // 1 data block + 1 reboot packet = 2 packets of 1088 bytes
    assert_eq!(mock.packets.len(), 2);
    assert_eq!(mock.packets[0].len(), TEENSY41_REPORT_SIZE);
    assert_eq!(&mock.packets[0][0..3], &[0, 0, 0]);
    // Header bytes [3..64] padded with 0x00 per Teensy 4 HalfKay spec
    assert!(mock.packets[0][3..64].iter().all(|&b| b == 0x00));
    assert_eq!(&mock.packets[0][64..68], b"FCFB");
    // Unwritten flash bytes padded with 0xFF
    assert!(mock.packets[0][80..1088].iter().all(|&b| b == 0xFF));
    assert_eq!(&mock.packets[1][0..3], &[0xFF, 0xFF, 0xFF]);

    // Also exercise default flash_firmware convenience wrapper
    let mut mock2 = MockHalfKayDevice::default();
    flash_firmware(&mut mock2, &img, |_, _| {}).unwrap();
    assert_eq!(mock2.packets.len(), 2);
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

    assert_eq!(
        ExportResolution::Qhd1440p.dimensions(160, 120),
        (2560, 1440)
    );
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

#[test]
fn real_v3_dat_and_protocol_v2_stream_fixtures() {
    let manifest_dir = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let fixtures_dir = manifest_dir
        .join("../../..")
        .join("firmware")
        .join("test")
        .join("fixtures");

    // 1. Real DIY-Thermocam V3 `.DAT` capture (38,805 bytes)
    let dat_bytes =
        std::fs::read(fixtures_dir.join("sample_real_v3.DAT")).expect("read sample_real_v3.DAT");
    assert_eq!(dat_bytes.len(), LEPTON3_BIG_SIZE);
    let dat = ThermalDat::from_bytes(&dat_bytes).expect("parse real V3 DAT");
    assert_eq!((dat.width, dat.height), (160, 120));
    assert_eq!(dat.pixels.len(), 19_200);
    assert!(dat.min_raw > 10_000 && dat.max_raw > dat.min_raw);

    let rad = RadiometryParams {
        cal_offset: dat.cal_offset,
        cal_slope: dat.cal_slope,
        emissivity: 0.95,
        reflected_temp_c: 22.0,
    };
    let min_c = raw_to_object_c(dat.min_raw, rad);
    let max_c = raw_to_object_c(dat.max_raw, rad);
    assert!(min_c > -20.0 && min_c < 80.0, "unexpected min_c={min_c}");
    assert!(max_c > min_c && max_c < 150.0, "unexpected max_c={max_c}");
    let reencoded = dat.to_bytes();
    assert_eq!(reencoded.len(), LEPTON3_BIG_SIZE);
    let reparsed = ThermalDat::from_bytes(&reencoded).expect("reparse real V3 DAT");
    assert_eq!(reparsed.pixels, dat.pixels);
    assert_eq!(reparsed.min_raw, dat.min_raw);
    assert_eq!(reparsed.max_raw, dat.max_raw);

    // Render 1080p PNG and 16-bit radiometric Kelvin TIFF directly from the real V3 fixture
    let pal = find_palette("ironblack").unwrap();
    let (w, h, rgb) = render_export_canvas(
        &dat.pixels,
        dat.width,
        dat.height,
        dat.min_raw,
        dat.max_raw,
        &pal,
        rad,
        &DisplayPipeline::default(),
        &[],
        &ExportOptions::default(),
    );
    assert_eq!((w, h), (1920, 1080));
    let png = encode_png(w, h, &rgb).unwrap();
    assert!(png.starts_with(&[0x89, b'P', b'N', b'G']));
    let tiff = encode_radiometric_tiff_16(&dat.pixels, dat.width, dat.height, rad).unwrap();
    assert!(tiff.starts_with(b"II*\0") || tiff.starts_with(b"MM\0*"));

    // 2. Real DIY-Thermocam V3 Protocol v2 binary capture stream (153,992 bytes)
    let stream_bytes = std::fs::read(fixtures_dir.join("sample_v2_stream.bin"))
        .expect("read sample_v2_stream.bin");
    assert_eq!(stream_bytes.len(), 153_992);
    let mut parser = StreamParser::new();
    let mut packets = Vec::new();
    for chunk in stream_bytes.chunks(1024) {
        for res in parser.feed(chunk) {
            packets.push(res.expect("valid v2 packet in real stream fixture"));
        }
    }
    assert_eq!(parser.stats().usb_rx_errors, 0);
    assert_eq!(parser.stats().usb_crc_errors, 0);
    assert_eq!(packets.len(), 6);
    assert_eq!(packets[0].msg_type().unwrap(), MsgType::StreamCtrlReq);
    assert_eq!(packets[1].msg_type().unwrap(), MsgType::StreamCtrlResp);

    let mut prev_seq: Option<u32> = None;
    for pkt in &packets[2..] {
        assert_eq!(pkt.msg_type().unwrap(), MsgType::FrameRaw);
        let frame = FrameRaw::from_bytes(&pkt.payload).expect("decode real FrameRaw");
        assert_eq!((frame.meta.width, frame.meta.height), (160, 120));
        assert_eq!(frame.pixels.len(), 19_200);
        assert_eq!(frame.meta.dropped_frames, 0);
        assert!(frame.meta.max_raw > frame.meta.min_raw);
        assert!(
            frame.meta.spot_temp_c > -20.0 && frame.meta.spot_temp_c < 150.0,
            "unexpected spot_temp_c={}",
            frame.meta.spot_temp_c
        );
        if let Some(prev) = prev_seq {
            assert!(frame.meta.frame_seq > prev, "frame_seq must be monotonic");
        }
        prev_seq = Some(frame.meta.frame_seq);
    }
}

#[test]
fn protocol_client_ignores_unsolicited_frame_with_colliding_seq_and_reads_multichunk_fs() {
    use thermocam_core::protocol::transport::MockTransport;
    use thermocam_core::protocol::{flags, Packet};

    let mut mock = MockTransport::default();
    // Simulate camera pushing an unsolicited FrameRaw with cam_seq == 1 BEFORE PongResp with host_seq == 1
    let demo = generate_demo_frame(1, 80, 60);
    mock.push_rx(&Packet::encode(MsgType::FrameRaw, 0, 1, 0, &demo.to_bytes()).unwrap());
    let mut pong_payload = [0u8; 16];
    pong_payload[0..8].copy_from_slice(&42u64.to_le_bytes());
    pong_payload[8..16].copy_from_slice(&999u64.to_le_bytes());
    mock.push_rx(&Packet::encode(MsgType::PongResp, 0, 1, 0, &pong_payload).unwrap());

    // Also pre-queue a 2-chunk FsReadData stream for seq == 2
    let mut c1 = Vec::new();
    c1.extend_from_slice(&0u32.to_le_bytes());
    c1.extend_from_slice(&4u16.to_le_bytes());
    c1.push(0); // eof = false
    c1.push(0);
    c1.extend_from_slice(b"ABCD");
    mock.push_rx(&Packet::encode(MsgType::FsReadData, flags::MORE_CHUNKS, 2, 0, &c1).unwrap());

    let mut c2 = Vec::new();
    c2.extend_from_slice(&4u32.to_le_bytes());
    c2.extend_from_slice(&3u16.to_le_bytes());
    c2.push(1); // eof = true
    c2.push(0);
    c2.extend_from_slice(b"EFG");
    mock.push_rx(&Packet::encode(MsgType::FsReadData, 0, 2, 0, &c2).unwrap());

    let mut client = ProtocolClient::new(mock);
    let pong = client
        .request(
            MsgType::PingReq,
            &42u64.to_le_bytes(),
            std::time::Duration::from_millis(500),
        )
        .expect("should match PongResp, not colliding FrameRaw");
    assert_eq!(pong.msg_type().unwrap(), MsgType::PongResp);

    let file = client
        .read_file_bytes("/test.dat")
        .expect("read 2-chunk file");
    assert_eq!(file, b"ABCDEFG");

    // The unsolicited FrameRaw must still be queued for poll_packets!
    let leftover = client
        .poll_packets(std::time::Duration::from_millis(10))
        .unwrap();
    assert_eq!(leftover.len(), 1);
    assert_eq!(leftover[0].msg_type().unwrap(), MsgType::FrameRaw);
}

#[test]
fn streaming_mp4_gif_pagination_temporal_heq_and_utc_metadata() {
    use thermocam_core::dat::{format_utc_iso8601, format_utc_now_iso8601};
    use thermocam_core::export::{
        derive_sequence_fps, encode_gif_streaming, encode_h264_mp4_streaming,
        render_export_canvas_with_scratch,
    };
    use thermocam_core::imaging::{
        apply_clipped_histogram_eq_into, RenderScratch, TemporalHeqState,
    };

    // 1. Multi-page SD FS_LIST pagination (>64 files -> multiple pages to 0xFFFFFFFF)
    let mut sim = SimulatorTransport::new();
    for i in 0..135usize {
        sim.add_sd_file(format!("CAP_{i:04}.DAT"), vec![0u8; 64]);
    }
    let mut client = ProtocolClient::new(sim);
    client.hello().unwrap();
    let entries = client.list_dir_all("/").unwrap();
    assert_eq!(entries.len(), 138); // 3 default + 135 added
    client.send_ping_nowait(123_456).unwrap();
    let pong_pkts = client
        .poll_packets(std::time::Duration::from_millis(50))
        .unwrap();
    assert!(pong_pkts
        .iter()
        .any(|p| p.msg_type().ok() == Some(MsgType::PongResp)));

    // 2. Real ISO-8601 UTC formatting and fractional FPS derivation
    assert_eq!(format_utc_iso8601(0), "1970-01-01T00:00:00Z");
    assert_eq!(format_utc_iso8601(1_764_066_000), "2025-11-25T10:20:00Z");
    let now_iso = format_utc_now_iso8601();
    assert!(now_iso.ends_with('Z') && now_iso.len() == 20);
    assert_ne!(now_iso, "2026-11-25T00:00:00Z");

    let frames: Vec<FrameRaw> = (0..5).map(|i| generate_demo_frame(i, 160, 120)).collect();
    let ts: Vec<u64> = frames.iter().map(|f| f.meta.timestamp_us).collect();
    let fps = derive_sequence_fps(&ts);
    assert!((fps - 8.78).abs() < 0.05, "expected ~8.78 fps, got {fps}");

    // 3. ThermalDat::from_pixels roundtrip (used by .DAT export and time-lapse)
    let rad = RadiometryParams::default();
    let dat_px = ThermalDat::from_pixels(frames[2].pixels.clone(), 160, 120, rad, 10, 0);
    let dat_bytes = dat_px.to_bytes();
    assert_eq!(dat_bytes.len(), LEPTON3_BIG_SIZE);
    let reparsed = ThermalDat::from_bytes(&dat_bytes).unwrap();
    assert_eq!(reparsed.pixels, frames[2].pixels);

    // 4. Temporal HEQ EMA smoothing damps sudden contrast jumps across frames
    let mut heq = TemporalHeqState::new(0.25);
    let frame_a: Vec<f32> = (0..256).map(|i| (i as f32 / 255.0) * 0.3).collect();
    let frame_b: Vec<f32> = (0..256).map(|i| 0.7 + (i as f32 / 255.0) * 0.3).collect();
    let mut out_a = Vec::new();
    let mut out_b_smooth = Vec::new();
    let mut out_b_unsmoothed = Vec::new();
    apply_clipped_histogram_eq_into(&frame_a, &mut out_a, Some(&mut heq));
    apply_clipped_histogram_eq_into(&frame_b, &mut out_b_smooth, Some(&mut heq));
    apply_clipped_histogram_eq_into(&frame_b, &mut out_b_unsmoothed, None);
    let diff_smooth = (out_b_smooth[128] - out_a[128]).abs();
    let diff_raw = (out_b_unsmoothed[128] - out_a[128]).abs();
    assert!(diff_smooth < diff_raw || (out_b_smooth[128] - out_b_unsmoothed[128]).abs() > 0.02);

    // 5. Constant-RAM streaming H.264 MP4 & GIF with RenderScratch reuse
    let pal = find_palette("inferno").unwrap();
    let pipeline = DisplayPipeline {
        histogram_eq: true,
        sharpen_amount: 0.8,
        ..DisplayPipeline::default()
    };
    let opts = ExportOptions {
        resolution: ExportResolution::Custom {
            width: 320,
            height: 240,
        },
        show_colorbar: true,
        show_hud: true,
        unit: TempUnit::Celsius,
        jpeg_quality: 92,
    };
    let mut scratch = RenderScratch::new();
    let mut mp4_buf = Cursor::new(Vec::new());
    encode_h264_mp4_streaming(&mut mp4_buf, 320, 240, frames.len(), fps, |idx, emit| {
        let fr = &frames[idx];
        render_export_canvas_with_scratch(
            &fr.pixels,
            fr.meta.width,
            fr.meta.height,
            fr.meta.min_raw,
            fr.meta.max_raw,
            &pal,
            rad,
            &pipeline,
            &[],
            &opts,
            true,
            &mut scratch,
        );
        emit(&scratch.canvas)
    })
    .unwrap();
    let mp4_bytes = mp4_buf.into_inner();
    let reader =
        mp4::Mp4Reader::read_header(Cursor::new(&mp4_bytes), mp4_bytes.len() as u64).unwrap();
    let track = reader.tracks().get(&1).expect("video track 1");
    assert_eq!(track.sample_count(), frames.len() as u32);
    assert_eq!(track.timescale(), 100_000);

    let mut gif_buf = Vec::new();
    encode_gif_streaming(&mut gif_buf, 320, 240, 2, fps, |idx, emit| {
        let fr = &frames[idx];
        render_export_canvas_with_scratch(
            &fr.pixels,
            fr.meta.width,
            fr.meta.height,
            fr.meta.min_raw,
            fr.meta.max_raw,
            &pal,
            rad,
            &pipeline,
            &[],
            &opts,
            true,
            &mut scratch,
        );
        emit(&scratch.canvas)
    })
    .unwrap();
    assert!(gif_buf.starts_with(b"GIF89a"));
}
