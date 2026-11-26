use std::fs;
use std::io;
use std::path::Path;
use std::sync::Mutex;

use thermocam_core::dat::{ThermalDat, LEPTON2_BIG_SIZE};
use thermocam_core::protocol::codec::FrameRaw;
use thermocam_core::protocol::transport::{
    demo_video_frame, generate_demo_frame, ProtocolClient, SimulatorTransport, DEMO_VIDEO_FOLDER,
};
use thermocam_core::radiometry::Material;
use thermocam_core::recording::{
    fetch_card_folder, frame_raw_bytes, open_recording, read_manifest, read_recording_frame,
    FrameSink, PushOutcome, RecordingOptions, RecordingSource, RecordingWriter, StdSink,
    StopReason, MIN_FREE_TO_CONTINUE, MIN_FREE_TO_START,
};

const PLENTY: u64 = 100 * 1024 * 1024 * 1024;

fn small_frame() -> FrameRaw {
    generate_demo_frame(3, 80, 60)
}

fn plenty() -> Box<dyn FnMut() -> Option<u64> + Send> {
    Box::new(|| Some(PLENTY))
}

fn dat_files(dir: &Path) -> Vec<String> {
    let mut names: Vec<String> = fs::read_dir(dir)
        .unwrap()
        .map(|e| e.unwrap().file_name().to_string_lossy().into_owned())
        .filter(|n| n.ends_with(".DAT"))
        .collect();
    names.sort();
    names
}

#[test]
fn ten_thousand_frames_become_ten_thousand_files_and_one_manifest() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("20261126102014");
    let mut w = RecordingWriter::start(
        dir.clone(),
        RecordingOptions::studio(None),
        StdSink,
        plenty(),
    )
    .unwrap();
    let mut frame = small_frame();
    for i in 0..100u32 {
        frame.meta.frame_seq = i;
        assert_eq!(w.push(&frame, i * 114), PushOutcome::Written);
    }
    let after_first_hundred = w.heap_bytes();
    for i in 100..10_000u32 {
        frame.meta.frame_seq = i;
        frame.pixels[0] = i as u16;
        assert_eq!(w.push(&frame, i * 114), PushOutcome::Written);
    }
    // The writer keeps 4 bytes of timing per frame, never the frames themselves.
    let grown = w.heap_bytes() - after_first_hundred;
    assert!(
        grown <= 8 * 9_900,
        "writer memory grew by {grown} bytes over 9,900 frames"
    );
    let info = w.finish(StopReason::User);
    assert_eq!(info.frames, 10_000);
    assert_eq!(info.stopped, StopReason::User);
    assert_eq!(info.duration_ms, 9_999 * 114);

    let names = dat_files(&dir);
    assert_eq!(names.len(), 10_000);
    assert_eq!(names[0], "000000.DAT");
    assert_eq!(names[9_999], "009999.DAT");
    assert_eq!(
        fs::metadata(dir.join("004321.DAT")).unwrap().len() as usize,
        LEPTON2_BIG_SIZE
    );
    let m = read_manifest(&dir).unwrap().unwrap();
    assert_eq!(m.schema_version, 1);
    assert_eq!(m.source, RecordingSource::Studio);
    assert_eq!(m.frames, 10_000);
    assert_eq!(m.t_ms.len(), 10_000);
    assert_eq!(m.t_ms[1], 114);
    assert!((m.fps_nominal - 1000.0 / 114.0).abs() < 0.01);
}

/// Fails to write one frame file, like a full or read-only disk.
struct FailAt {
    name: &'static str,
}

impl FrameSink for FailAt {
    fn write_frame(&mut self, path: &Path, bytes: &[u8]) -> io::Result<()> {
        if path.file_name().is_some_and(|n| n == self.name) {
            return Err(io::Error::new(
                io::ErrorKind::StorageFull,
                "No space left on device",
            ));
        }
        StdSink.write_frame(path, bytes)
    }
    fn write_manifest(&mut self, path: &Path, bytes: &[u8]) -> io::Result<()> {
        StdSink.write_manifest(path, bytes)
    }
}

#[test]
fn an_io_error_at_frame_37_stops_with_write_error_and_keeps_frames_0_to_36() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("rec");
    let mut w = RecordingWriter::start(
        dir.clone(),
        RecordingOptions::studio(None),
        FailAt { name: "000037.DAT" },
        plenty(),
    )
    .unwrap();
    let frame = small_frame();
    let mut outcome = PushOutcome::Written;
    for i in 0..60u32 {
        outcome = w.push(&frame, i * 100);
        if outcome != PushOutcome::Written {
            assert_eq!(i, 37);
            break;
        }
    }
    assert_eq!(outcome, PushOutcome::Stopped(StopReason::WriteError));
    assert_eq!(
        w.push(&frame, 99_999),
        PushOutcome::Stopped(StopReason::WriteError)
    );
    let info = w.finish(StopReason::User);
    assert_eq!(
        info.stopped,
        StopReason::WriteError,
        "the first reason wins"
    );
    assert!(info.detail.unwrap().contains("No space left"));
    assert_eq!(info.frames, 37);
    let names = dat_files(&dir);
    assert_eq!(names.len(), 37);
    assert_eq!(names.last().unwrap(), "000036.DAT");
    let m = read_manifest(&dir).unwrap().unwrap();
    assert_eq!(m.frames, 37);
    assert_eq!(m.t_ms.len(), 37);
}

#[test]
fn a_disconnect_keeps_every_frame_written_so_far() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("rec");
    let mut w = RecordingWriter::start(
        dir.clone(),
        RecordingOptions::studio(None),
        StdSink,
        plenty(),
    )
    .unwrap();
    let frame = small_frame();
    for i in 0..25u32 {
        w.push(&frame, i * 114);
    }
    let info = w.finish(StopReason::Disconnected);
    assert_eq!(info.frames, 25);
    assert_eq!(info.stopped, StopReason::Disconnected);
    assert_eq!(dat_files(&dir).len(), 25);
    assert_eq!(read_manifest(&dir).unwrap().unwrap().frames, 25);
}

#[test]
fn low_disk_refuses_to_start_and_stops_a_running_recording() {
    let tmp = tempfile::tempdir().unwrap();
    let err = RecordingWriter::start(
        tmp.path().join("a"),
        RecordingOptions::studio(None),
        StdSink,
        Box::new(|| Some(MIN_FREE_TO_START - 1)),
    )
    .err()
    .expect("refused below 500 MB");
    assert_eq!(err.kind, "low_disk");
    assert!(!tmp.path().join("a").exists());

    let free = std::sync::Arc::new(std::sync::atomic::AtomicU64::new(PLENTY));
    let probe = std::sync::Arc::clone(&free);
    let dir = tmp.path().join("b");
    let mut w = RecordingWriter::start(
        dir.clone(),
        RecordingOptions::studio(None),
        StdSink,
        Box::new(move || Some(probe.load(std::sync::atomic::Ordering::SeqCst))),
    )
    .unwrap();
    let frame = small_frame();
    for i in 0..20u32 {
        assert_eq!(w.push(&frame, i * 114), PushOutcome::Written);
    }
    free.store(
        MIN_FREE_TO_CONTINUE - 1,
        std::sync::atomic::Ordering::SeqCst,
    );
    let mut stopped_at = None;
    for i in 20..200u32 {
        if let PushOutcome::Stopped(r) = w.push(&frame, i * 114) {
            assert_eq!(r, StopReason::LowDisk);
            stopped_at = Some(i);
            break;
        }
    }
    let at = stopped_at.expect("stops at 200 MB free");
    let info = w.finish(StopReason::User);
    assert_eq!(info.stopped, StopReason::LowDisk);
    assert_eq!(info.frames as u32, at);
    assert_eq!(read_manifest(&dir).unwrap().unwrap().frames as u32, at);
}

/// Counts manifest writes.
#[derive(Default)]
struct CountManifests {
    writes: usize,
}

impl FrameSink for CountManifests {
    fn write_frame(&mut self, path: &Path, bytes: &[u8]) -> io::Result<()> {
        StdSink.write_frame(path, bytes)
    }
    fn write_manifest(&mut self, path: &Path, bytes: &[u8]) -> io::Result<()> {
        self.writes += 1;
        StdSink.write_manifest(path, bytes)
    }
}

#[test]
fn the_manifest_is_rewritten_every_5_seconds_and_at_stop() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("rec");
    let mut w = RecordingWriter::start(
        dir.clone(),
        RecordingOptions::studio(Some(Material {
            emissivity: 0.95,
            reflected_c: 21.0,
        })),
        CountManifests::default(),
        plenty(),
    )
    .unwrap();
    assert_eq!(w.sink().writes, 1, "written at start");
    let frame = small_frame();
    for t in (0..=12_000u32).step_by(500) {
        w.push(&frame, t);
        if t == 4_500 {
            assert_eq!(w.sink().writes, 1);
            // A crash now still leaves a readable manifest.
            assert_eq!(read_manifest(&dir).unwrap().unwrap().frames, 0);
        }
        if t == 5_000 {
            assert_eq!(w.sink().writes, 2);
            assert_eq!(read_manifest(&dir).unwrap().unwrap().frames, 11);
        }
    }
    assert_eq!(w.sink().writes, 3, "at 5 s and 10 s");
    let info = w.finish(StopReason::User);
    assert_eq!(info.frames, 25);
    let m = read_manifest(&dir).unwrap().unwrap();
    assert_eq!(m.frames, 25);
    assert_eq!(
        m.sensor_material,
        Some(Material {
            emissivity: 0.95,
            reflected_c: 21.0
        })
    );
}

// ── Reading recordings back (T11) ───────────────────────────────────────────

fn write_dat(path: &Path, seq: u32) -> FrameRaw {
    let f = generate_demo_frame(seq, 160, 120);
    fs::write(path, ThermalDat::from_frame(&f, 10, 0).to_bytes()).unwrap();
    f
}

#[test]
fn a_folder_without_manifest_is_a_camera_video_with_sorted_frames_and_no_times() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("20261126101500");
    fs::create_dir(&dir).unwrap();
    let f2 = write_dat(&dir.join("000002.DAT"), 2);
    let f0 = write_dat(&dir.join("000000.DAT"), 0);
    write_dat(&dir.join("000001.DAT"), 1);
    fs::write(dir.join("000000.BMP"), b"bitmap").unwrap();

    let rec = open_recording(&dir).unwrap();
    assert_eq!(rec.frames, 3);
    assert_eq!(rec.t_ms, None);
    assert_eq!(rec.source, RecordingSource::Camera);
    assert_eq!((rec.width, rec.height), (160, 120));
    assert_eq!(read_recording_frame(&rec, 0).unwrap().pixels, f0.pixels);
    assert_eq!(read_recording_frame(&rec, 2).unwrap().pixels, f2.pixels);
    assert!(read_recording_frame(&rec, 3).is_err());
}

#[test]
fn a_studio_recording_opens_with_its_times_and_frames_come_back_in_the_stream_layout() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("rec");
    let mut w = RecordingWriter::start(
        dir.clone(),
        RecordingOptions::studio(None),
        StdSink,
        plenty(),
    )
    .unwrap();
    let frames: Vec<FrameRaw> = (0..5).map(|i| generate_demo_frame(i, 160, 120)).collect();
    for (i, f) in frames.iter().enumerate() {
        w.push(f, i as u32 * 120);
    }
    w.finish(StopReason::User);

    let rec = open_recording(&dir).unwrap();
    assert_eq!(rec.frames, 5);
    assert_eq!(rec.t_ms, Some(vec![0, 120, 240, 360, 480]));
    assert_eq!(rec.source, RecordingSource::Studio);

    let dat = read_recording_frame(&rec, 3).unwrap();
    let bytes = frame_raw_bytes(&dat, 3, rec.t_ms.as_ref().map(|t| t[3]));
    let back = FrameRaw::from_bytes(&bytes).unwrap();
    assert_eq!(back.pixels, frames[3].pixels);
    assert_eq!(back.meta.frame_seq, 3);
    assert_eq!(back.meta.timestamp_us, 360_000);
    assert_eq!((back.meta.width, back.meta.height), (160, 120));
    assert_eq!(back.meta.cal_slope, 0.01);
}

#[test]
fn a_manifest_naming_a_frame_outside_the_folder_is_refused() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("seq");
    fs::create_dir(&dir).unwrap();
    write_dat(&dir.join("000000.DAT"), 0);
    write_dat(&tmp.path().join("x.DAT"), 9);
    let manifest = serde_json::json!({
        "schema_version": 1, "created_utc": "2026-11-26T10:00:00Z",
        "sensor_w": 160, "sensor_h": 120, "fps": 8.78, "frame_count": 2,
        "frames": ["000000.DAT", "../x.DAT"], "palette_id": "ironblack"
    });
    fs::write(dir.join("manifest.json"), manifest.to_string()).unwrap();
    let err = open_recording(&dir).unwrap_err();
    assert_eq!(err.kind, "bad_path");

    // The same file with safe names opens (Studio's older "seq" export).
    let ok = serde_json::json!({
        "schema_version": 1, "created_utc": "2026-11-26T10:00:00Z",
        "sensor_w": 160, "sensor_h": 120, "fps": 8.78, "frame_count": 1,
        "frames": ["000000.DAT"], "palette_id": "ironblack"
    });
    fs::write(dir.join("manifest.json"), ok.to_string()).unwrap();
    assert_eq!(open_recording(&dir).unwrap().frames, 1);
}

#[test]
fn frame_59_of_the_seeded_camera_video_reads_back_equal_to_the_simulators_frame() {
    let mut client = ProtocolClient::new(SimulatorTransport::new());
    client.hello().unwrap();
    let card = Mutex::new(client);
    let tmp = tempfile::tempdir().unwrap();
    let dest = tmp
        .path()
        .join("sd")
        .join("0011223344556677")
        .join(DEMO_VIDEO_FOLDER);

    let mut progress = Vec::new();
    let local = fetch_card_folder(
        &card,
        &format!("/{DEMO_VIDEO_FOLDER}"),
        &dest,
        |done, total| {
            progress.push((done, total));
        },
    )
    .unwrap();
    assert_eq!(local, dest);
    assert_eq!(progress.first(), Some(&(0, 60)));
    assert_eq!(progress.last(), Some(&(60, 60)));

    let rec = open_recording(&dest).unwrap();
    assert_eq!(rec.frames, 60);
    assert_eq!(rec.t_ms, None);
    assert_eq!(rec.source, RecordingSource::Camera);
    let frame59 = read_recording_frame(&rec, 59).unwrap();
    assert_eq!(frame59.pixels, demo_video_frame(59).pixels);

    // A second fetch finds every frame already copied.
    let mut again = Vec::new();
    fetch_card_folder(&card, &format!("/{DEMO_VIDEO_FOLDER}"), &dest, |d, t| {
        again.push((d, t))
    })
    .unwrap();
    assert_eq!(again.last(), Some(&(60, 60)));
}

#[test]
fn a_card_folder_with_an_unsafe_entry_name_is_refused() {
    let mut sim = SimulatorTransport::new();
    sim.add_sd_file("VID/000000.DAT".into(), vec![0u8; 16]);
    let mut client = ProtocolClient::new(sim);
    client.hello().unwrap();
    let card = Mutex::new(client);
    let tmp = tempfile::tempdir().unwrap();
    // The folder path itself may not climb out of the cache.
    let err = fetch_card_folder(&card, "/../VID", &tmp.path().join("x"), |_, _| {}).unwrap_err();
    assert_eq!(err.kind, "bad_path");
}

#[test]
fn a_time_lapse_keeps_one_frame_per_interval() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("lapse");
    let mut w = RecordingWriter::start(
        dir.clone(),
        RecordingOptions::timelapse(1_000, None),
        StdSink,
        plenty(),
    )
    .unwrap();
    let frame = small_frame();
    let mut written = 0;
    for t in (0..=3_000u32).step_by(100) {
        if w.push(&frame, t) == PushOutcome::Written {
            written += 1;
        }
    }
    assert_eq!(written, 4);
    w.finish(StopReason::User);
    let m = read_manifest(&dir).unwrap().unwrap();
    assert_eq!(m.source, RecordingSource::Timelapse);
    assert_eq!(m.t_ms, vec![0, 1_000, 2_000, 3_000]);
    assert!((m.fps_nominal - 1.0).abs() < 1e-6);
    // The JSON uses the Plan's field names.
    let raw: serde_json::Value =
        serde_json::from_str(&fs::read_to_string(dir.join("recording.json")).unwrap()).unwrap();
    assert_eq!(raw["source"], "timelapse");
    for key in [
        "schema_version",
        "frames",
        "fps_nominal",
        "t_ms",
        "sensor_material",
    ] {
        assert!(raw.get(key).is_some(), "{key}");
    }
}
