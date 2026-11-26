use std::fs;
use std::path::{Path, PathBuf};

use thermocam_core::captures::{
    check_writable, default_captures_dir, unique_path, write_snapshot, CaptureErrorKind, LocalStamp,
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
