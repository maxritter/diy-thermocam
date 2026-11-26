use std::fs;
use std::path::{Path, PathBuf};

use thermocam_core::captures::{
    check_writable, default_captures_dir, list_captures, unique_path, write_snapshot,
    CaptureErrorKind, LocalStamp,
};
use thermocam_core::dat::{ThermalDat, LEPTON3_BIG_SIZE};
use thermocam_core::protocol::transport::generate_demo_frame;

fn stamp() -> LocalStamp {
    LocalStamp {
        year: 2026,
        month: 11,
        day: 26,
        hour: 10,
        minute: 20,
        second: 14,
    }
}

#[test]
fn snapshot_is_a_v4_dat_named_after_the_local_time() {
    let dir = tempfile::tempdir().unwrap();
    let frame = generate_demo_frame(7, 160, 120);
    let path = write_snapshot(dir.path(), &frame, stamp()).unwrap();
    assert_eq!(path.file_name().unwrap(), "20261126102014.DAT");
    let bytes = fs::read(&path).unwrap();
    assert_eq!(bytes.len(), LEPTON3_BIG_SIZE);
    assert_eq!(bytes.len(), 38_805);
    let dat = ThermalDat::from_bytes(&bytes).unwrap();
    assert_eq!(dat.pixels, frame.pixels);
    assert_eq!(dat.min_raw, frame.meta.min_raw);
    assert_eq!(dat.max_raw, frame.meta.max_raw);
}

#[test]
fn snapshots_in_the_same_second_get_numbered_suffixes() {
    let dir = tempfile::tempdir().unwrap();
    let frame = generate_demo_frame(1, 160, 120);
    let a = write_snapshot(dir.path(), &frame, stamp()).unwrap();
    let b = write_snapshot(dir.path(), &frame, stamp()).unwrap();
    let c = write_snapshot(dir.path(), &frame, stamp()).unwrap();
    assert_eq!(a.file_name().unwrap(), "20261126102014.DAT");
    assert_eq!(b.file_name().unwrap(), "20261126102014-2.DAT");
    assert_eq!(c.file_name().unwrap(), "20261126102014-3.DAT");
    // Folders (recordings) share the same numbering rule.
    fs::create_dir(dir.path().join("20261126102014")).unwrap();
    assert_eq!(
        unique_path(dir.path(), "20261126102014", None)
            .file_name()
            .unwrap(),
        "20261126102014-2"
    );
}

#[test]
fn the_snapshot_folder_is_created_on_first_use() {
    let dir = tempfile::tempdir().unwrap();
    let target = dir.path().join("Pictures").join("Thermocam");
    let frame = generate_demo_frame(1, 160, 120);
    let path = write_snapshot(&target, &frame, stamp()).unwrap();
    assert!(path.starts_with(&target));
}

#[cfg(unix)]
#[test]
fn a_read_only_folder_is_reported_as_read_only() {
    use std::os::unix::fs::PermissionsExt;
    let dir = tempfile::tempdir().unwrap();
    let ro = dir.path().join("ro");
    fs::create_dir(&ro).unwrap();
    fs::set_permissions(&ro, fs::Permissions::from_mode(0o555)).unwrap();
    // Root ignores directory permissions (a container running as root): nothing to prove there.
    let probe = ro.join(".probe");
    if fs::write(&probe, b"x").is_ok() {
        let _ = fs::remove_file(&probe);
        fs::set_permissions(&ro, fs::Permissions::from_mode(0o755)).unwrap();
        eprintln!("skipped: this user can write into a read-only folder (running as root)");
        return;
    }
    let frame = generate_demo_frame(1, 160, 120);
    let err = write_snapshot(&ro, &frame, stamp()).unwrap_err();
    let check = check_writable(&ro).unwrap_err();
    fs::set_permissions(&ro, fs::Permissions::from_mode(0o755)).unwrap();
    assert_eq!(err.kind, CaptureErrorKind::ReadOnly);
    assert_eq!(err.kind.as_str(), "read_only");
    assert_eq!(check.kind, CaptureErrorKind::ReadOnly);
    assert!(
        fs::read_dir(&ro).unwrap().next().is_none(),
        "nothing written"
    );
}

#[test]
fn check_writable_accepts_a_normal_folder_and_leaves_nothing_behind() {
    let dir = tempfile::tempdir().unwrap();
    check_writable(dir.path()).unwrap();
    assert!(fs::read_dir(dir.path()).unwrap().next().is_none());
}

#[test]
fn default_folder_is_pictures_thermocam_with_a_home_fallback() {
    let pics = Path::new("/Users/someone/Pictures");
    let home = Path::new("/home/someone");
    assert_eq!(
        default_captures_dir(Some(pics), Some(home)),
        Some(PathBuf::from("/Users/someone/Pictures/Thermocam"))
    );
    assert_eq!(
        default_captures_dir(None, Some(home)),
        Some(PathBuf::from("/home/someone/Pictures/Thermocam"))
    );
    assert_eq!(default_captures_dir(None, None), None);
}

#[test]
fn the_captures_folder_lists_snapshots_recordings_and_other_files() {
    let dir = tempfile::tempdir().unwrap();
    let root = dir.path();
    let frame = generate_demo_frame(1, 160, 120);
    let snap = write_snapshot(root, &frame, stamp()).unwrap();
    // PNG on snapshot, edits and a set-aside edits file belong to the capture, not the list.
    fs::write(root.join("20261126102014.png"), b"png").unwrap();
    fs::write(root.join("20261126102014.thermo.json"), b"{}").unwrap();
    fs::write(root.join("20261126102014.thermo.json.bad"), b"x").unwrap();
    fs::write(root.join(".DS_Store"), b"x").unwrap();
    let rec = root.join("20261126103000");
    fs::create_dir(&rec).unwrap();
    for i in 0..3 {
        fs::write(rec.join(format!("{i:06}.DAT")), vec![0u8; 100]).unwrap();
    }
    fs::write(rec.join("recording.json"), b"{}").unwrap();
    fs::create_dir(root.join("empty folder")).unwrap();
    fs::write(root.join("notes.txt"), b"hello").unwrap();

    let mut list = list_captures(root).unwrap();
    list.sort_by(|a, b| a.name.cmp(&b.name));
    let summary: Vec<_> = list
        .iter()
        .map(|c| (c.name.as_str(), c.kind.as_str(), c.frames, c.size))
        .collect();
    assert_eq!(
        summary,
        vec![
            ("20261126102014.DAT", "snapshot", None, 38_805),
            ("20261126103000", "recording", Some(3), 300),
            ("empty folder", "other", None, 0),
            ("notes.txt", "other", None, 5),
        ]
    );
    assert_eq!(list[0].path, snap.to_string_lossy());
    assert!(list.iter().all(|c| c.mtime_ms > 0));
}

#[test]
fn listing_a_missing_folder_is_an_error() {
    let dir = tempfile::tempdir().unwrap();
    assert!(list_captures(&dir.path().join("nope")).is_err());
}

#[test]
fn local_stamp_formats_as_fourteen_digits() {
    assert_eq!(stamp().to_name(), "20261126102014");
    let early = LocalStamp {
        year: 2027,
        month: 1,
        day: 2,
        hour: 3,
        minute: 4,
        second: 5,
    };
    assert_eq!(early.to_name(), "20270102030405");
}
