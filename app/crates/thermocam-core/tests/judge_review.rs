//! Judge tests (independent review, 2026-11-26-studio-ui-redesign): file names a person can
//! really have in the Captures folder, and edits that belong to one capture only.

use std::fs;

use thermocam_core::captures::{list_captures, LocalCaptureKind};
use thermocam_core::recording::is_dat_name;
use thermocam_core::sidecar::{sidecar_path, CaptureRef};

/// A wrong implementation slices the name at `len - ext.len()` bytes and panics when that byte
/// falls inside a multi-byte character (Japanese, emoji, "€"). The listing must survive any
/// valid file name and still list the snapshot beside it.
#[test]
fn the_captures_folder_lists_beside_files_with_non_ascii_names() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path();
    fs::write(dir.join("20261126101500.DAT"), [0u8; 16]).unwrap();
    // 20 bytes: the byte 12 from the end (".thermo.json") lies inside a 3-byte character.
    fs::write(dir.join("サーモ画像1.png"), b"png").unwrap();
    // 6 bytes: the byte 4 from the end (".dat", ".png") lies inside a character.
    fs::write(dir.join("画像"), b"x").unwrap();
    let out = std::panic::catch_unwind(|| list_captures(dir));
    let list = out
        .expect("listing the Captures folder panicked on a non-ASCII file name")
        .unwrap();
    assert!(list
        .iter()
        .any(|c| c.name == "20261126101500.DAT" && c.kind == LocalCaptureKind::Snapshot));
    assert!(list
        .iter()
        .any(|c| c.name == "画像" && c.kind == LocalCaptureKind::Other));
}

/// Frame names from a recording folder or the camera's card go through `is_dat_name`;
/// a name that is not a `.DAT` must be answered `false`, never a panic.
#[test]
fn is_dat_name_answers_for_any_name() {
    let r = std::panic::catch_unwind(|| {
        (
            is_dat_name("000001.DAT"),
            is_dat_name("画像.dat"),
            is_dat_name("画像"),
            is_dat_name("\u{FFFD}\u{FFFD}"),
        )
    });
    assert_eq!(
        r.expect("is_dat_name panicked on a non-ASCII name"),
        (true, true, false, false)
    );
}

/// A snapshot and a recording started in the same second are named `X.DAT` and `X/` (the
/// recorder and the snapshot writer number collisions only among their own kind; alarms with
/// Snapshot and Record fire on the same crossing). Each capture's edits must be its own, so
/// Reset to original on one never wipes the other's edits.
#[test]
fn a_snapshot_and_a_recording_of_the_same_second_keep_separate_edits() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path();
    let snap = dir.join("20261126101500.DAT");
    let rec = dir.join("20261126101500");
    fs::write(&snap, [0u8; 16]).unwrap();
    fs::create_dir(&rec).unwrap();
    let edits = tmp.path().join("edits");
    let a = sidecar_path(
        &CaptureRef::Local {
            path: snap.to_string_lossy().into_owned(),
        },
        &edits,
    )
    .unwrap();
    let b = sidecar_path(
        &CaptureRef::Local {
            path: rec.to_string_lossy().into_owned(),
        },
        &edits,
    )
    .unwrap();
    assert_ne!(
        a,
        b,
        "both captures would share the edits file {}",
        a.display()
    );
}

/// The same holds on the camera's card, where a photo and a video folder can share a stem.
#[test]
fn camera_captures_with_the_same_stem_keep_separate_edits() {
    let tmp = tempfile::tempdir().unwrap();
    let a = sidecar_path(
        &CaptureRef::Camera {
            serial: "V4-1".into(),
            sd_path: "/20261126101500.DAT".into(),
        },
        tmp.path(),
    )
    .unwrap();
    let b = sidecar_path(
        &CaptureRef::Camera {
            serial: "V4-1".into(),
            sd_path: "/20261126101500".into(),
        },
        tmp.path(),
    )
    .unwrap();
    assert_ne!(a, b, "both camera captures would share {}", a.display());
}

/// One folder Studio may not read (another app's library inside a chosen Pictures folder, a
/// folder with no read permission) must not hide every capture: the snapshot beside it is still
/// listed. A wrong implementation propagates the inner `read_dir` error with `?`.
#[cfg(unix)]
#[test]
fn an_unreadable_folder_does_not_hide_the_other_captures() {
    use std::os::unix::fs::PermissionsExt;
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path();
    fs::write(dir.join("20261126101500.DAT"), [0u8; 16]).unwrap();
    let locked = dir.join("Photos Library.photoslibrary");
    fs::create_dir(&locked).unwrap();
    fs::set_permissions(&locked, fs::Permissions::from_mode(0o000)).unwrap();
    let res = list_captures(dir);
    fs::set_permissions(&locked, fs::Permissions::from_mode(0o755)).unwrap();
    let list = res.expect("one unreadable folder made the whole listing fail");
    assert!(list.iter().any(|c| c.name == "20261126101500.DAT"));
}
