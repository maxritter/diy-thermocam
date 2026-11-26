use std::fs;
use std::path::Path;

use thermocam_core::paths::{confine, ensure_within, PathError};

fn root() -> (tempfile::TempDir, std::path::PathBuf) {
    let dir = tempfile::tempdir().unwrap();
    let canon = fs::canonicalize(dir.path()).unwrap();
    (dir, canon)
}

#[test]
fn confine_accepts_relative_names_inside_the_root() {
    let (_dir, root) = root();
    let ok: &[(&str, &str)] = &[
        ("/x.DAT", "x.DAT"),
        ("x.DAT", "x.DAT"),
        ("a/b.DAT", "a/b.DAT"),
        ("/20261126101500/000000.DAT", "20261126101500/000000.DAT"),
        ("a/./b.DAT", "a/b.DAT"),
    ];
    for (input, expected) in ok {
        let got = confine(&root, input).unwrap_or_else(|e| panic!("{input:?} refused: {e}"));
        assert_eq!(got, root.join(expected), "input {input:?}");
    }
}

#[test]
fn confine_refuses_everything_that_escapes_or_is_not_relative() {
    let (_dir, root) = root();
    let bad = [
        "../../etc/passwd",
        "a/../../b",
        "a/..",
        "..",
        "C:\\x",
        "C:x",
        "c:/x",
        "\\\\server\\x",
        "\\x",
        "a\\b",
        "x\0y",
        "",
        "/",
        "//server/x",
        "./",
    ];
    for input in bad {
        let err = confine(&root, input).expect_err(&format!("{input:?} must be refused"));
        assert_eq!(err.kind(), "bad_path", "{input:?}");
    }
}

#[cfg(unix)]
#[test]
fn confine_refuses_a_symlink_pointing_outside_the_root() {
    let (_dir, root) = root();
    let outside = tempfile::tempdir().unwrap();
    fs::write(outside.path().join("secret.DAT"), b"x").unwrap();
    std::os::unix::fs::symlink(outside.path(), root.join("link")).unwrap();
    std::os::unix::fs::symlink(outside.path().join("secret.DAT"), root.join("file.DAT")).unwrap();

    assert!(matches!(
        confine(&root, "link/secret.DAT"),
        Err(PathError::Escapes(_))
    ));
    assert!(matches!(
        confine(&root, "file.DAT"),
        Err(PathError::Escapes(_))
    ));
    // A symlink that stays inside the root is fine.
    fs::create_dir(root.join("inner")).unwrap();
    std::os::unix::fs::symlink(root.join("inner"), root.join("alias")).unwrap();
    assert_eq!(
        confine(&root, "alias/y.DAT").unwrap(),
        root.join("inner").join("y.DAT")
    );
}

#[test]
fn confine_works_when_the_root_does_not_exist_yet() {
    let (_dir, base) = root();
    let root = base.join("edits").join("0011223344556677");
    assert_eq!(
        confine(&root, "/20261125101500.thermo.json").unwrap(),
        root.join("20261125101500.thermo.json")
    );
    assert!(confine(&root, "../other/x").is_err());
}

#[test]
fn card_paths_are_normalised_and_never_climb() {
    use thermocam_core::paths::card_path;
    assert_eq!(card_path("/x.DAT").unwrap(), "/x.DAT");
    assert_eq!(card_path("a//b/").unwrap(), "/a/b");
    for bad in ["/../x", "a/../b", "", "/", "C:\\x", "x\0y"] {
        assert!(card_path(bad).is_err(), "{bad:?}");
    }
}

#[test]
fn ensure_within_checks_absolute_paths_against_the_root() {
    let (_dir, root) = root();
    fs::write(root.join("a.DAT"), b"x").unwrap();
    assert_eq!(
        ensure_within(&root, &root.join("a.DAT")).unwrap(),
        root.join("a.DAT")
    );
    let outside = tempfile::tempdir().unwrap();
    fs::write(outside.path().join("b.DAT"), b"x").unwrap();
    assert!(ensure_within(&root, &outside.path().join("b.DAT")).is_err());
    assert!(ensure_within(&root, &root.join("..").join("x")).is_err());
    assert!(ensure_within(&root, Path::new("relative.DAT")).is_err());
    // The root itself is not a target.
    assert!(ensure_within(&root, &root).is_err());
}
