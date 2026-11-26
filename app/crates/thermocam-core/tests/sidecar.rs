use std::fs;
use std::path::PathBuf;

use thermocam_core::radiometry::Material;
use thermocam_core::roi::RoiGeometry;
use thermocam_core::sidecar::{
    load_sidecar, reset_sidecar, save_sidecar, sidecar_path, CaptureRef, ThermoSidecar,
};

fn fixture(name: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(name)
}

#[test]
fn a_frontend_written_v2_sidecar_round_trips() {
    let text = fs::read_to_string(fixture("sidecar-v2.json")).unwrap();
    let sc: ThermoSidecar = serde_json::from_str(&text).unwrap();
    assert_eq!(sc.schema_version, 2);
    assert_eq!(sc.palette_id, "rainbow");
    assert_eq!(sc.range.mode, "manual");
    assert_eq!(sc.range.min_c, Some(21.5));
    assert_eq!(sc.range.max_c, Some(48.25));
    assert_eq!(
        sc.material,
        Material {
            emissivity: 0.94,
            reflected_c: 23.0
        }
    );
    assert_eq!(sc.sensor_material, None);
    assert_eq!(sc.regions.len(), 6);
    assert_eq!(sc.regions[0].name, "MCU");
    assert!(matches!(
        sc.regions[0].geometry,
        RoiGeometry::Box {
            x: 50,
            y: 46,
            w: 24,
            h: 18
        }
    ));
    assert_eq!(
        sc.regions[1].material,
        Some(Material {
            emissivity: 0.8,
            reflected_c: 22.0
        })
    );
    assert!(!sc.regions[2].visible);
    assert_eq!(sc.notes.name, "Board under load");
    assert_eq!(sc.notes.text, "Fan off, 5 minutes after power-on.");

    // Serialising and parsing again gives the same JSON value as the fixture.
    let original: serde_json::Value = serde_json::from_str(&text).unwrap();
    let again: serde_json::Value =
        serde_json::from_str(&serde_json::to_string(&sc).unwrap()).unwrap();
    assert_eq!(again, original);
}

#[test]
fn every_field_after_schema_version_has_a_default() {
    let sc: ThermoSidecar = serde_json::from_str(r#"{"schema_version":2}"#).unwrap();
    assert_eq!(sc.palette_id, "ironblack");
    assert_eq!(sc.range.mode, "auto");
    assert_eq!(sc.material.emissivity, 0.95);
    assert_eq!(sc.material.reflected_c, 22.0);
    assert!(sc.regions.is_empty());
    assert_eq!(sc.notes.text, "");
    let unknown: ThermoSidecar =
        serde_json::from_str(r#"{"schema_version":2,"future_field":[1,2,3]}"#).unwrap();
    assert_eq!(unknown.palette_id, "ironblack");
}

#[test]
fn a_schema_1_file_loads_with_its_values_carried_over() {
    let v1 = r#"{
        "schema_version": 1,
        "palette_id": "grayscale",
        "global_emissivity": 0.9,
        "reflected_temp_c": 25.0,
        "range_mode": "manual",
        "manual_min_c": 20.0,
        "manual_max_c": 40.0,
        "upscaler": "edge_preserve",
        "sharpen_amount": 0.45,
        "notes": "old note",
        "rois": [{
            "id": "a", "label": "Hot spot",
            "geometry": {"kind": "spot", "x": 3, "y": 4, "kernel": 1, "show_raw": false},
            "emissivity_override": 0.7, "reflected_temp_override_c": null, "visible": true
        }],
        "annotations": [],
        "export_preset": "1080p"
    }"#;
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("20261125101500.thermo.json");
    fs::write(&path, v1).unwrap();
    let loaded = load_sidecar(&path);
    assert_eq!(loaded.quarantined, None);
    let sc = loaded.sidecar.expect("v1 sidecar loads");
    assert_eq!(sc.schema_version, 2);
    assert_eq!(sc.palette_id, "grayscale");
    assert_eq!(sc.material.emissivity, 0.9);
    assert_eq!(sc.material.reflected_c, 25.0);
    assert_eq!(sc.range.mode, "manual");
    assert_eq!(sc.range.min_c, Some(20.0));
    assert_eq!(sc.notes.text, "old note");
    assert_eq!(sc.regions.len(), 1);
    assert_eq!(sc.regions[0].name, "Hot spot");
    assert_eq!(
        sc.regions[0].material,
        Some(Material {
            emissivity: 0.7,
            reflected_c: 25.0
        })
    );
    // The old default value still parses too.
    let json = serde_json::to_string(&ThermoSidecar::default()).unwrap();
    let back: ThermoSidecar = serde_json::from_str(&json).unwrap();
    assert_eq!(back.palette_id, "ironblack");
}

#[test]
fn a_damaged_file_is_set_aside_and_reported() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("x.thermo.json");
    fs::write(&path, b"{ this is not json").unwrap();
    let loaded = load_sidecar(&path);
    assert!(loaded.sidecar.is_none());
    assert_eq!(loaded.quarantined.as_deref(), Some("x.thermo.json.bad"));
    assert!(!path.exists());
    assert_eq!(
        fs::read(dir.path().join("x.thermo.json.bad")).unwrap(),
        b"{ this is not json"
    );
    // A wrong field type is damage too.
    fs::write(&path, br#"{"schema_version":2,"regions":"nope"}"#).unwrap();
    let again = load_sidecar(&path);
    assert!(again.sidecar.is_none());
    assert!(again.quarantined.is_some());
}

#[test]
fn a_missing_file_is_simply_no_edits() {
    let dir = tempfile::tempdir().unwrap();
    let loaded = load_sidecar(&dir.path().join("none.thermo.json"));
    assert!(loaded.sidecar.is_none());
    assert!(loaded.quarantined.is_none());
}

#[test]
fn save_is_atomic_and_reset_deletes_the_file() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("deep").join("a.thermo.json");
    let sc = ThermoSidecar {
        palette_id: "rainbow".into(),
        ..Default::default()
    };
    save_sidecar(&path, &sc).unwrap();
    assert_eq!(load_sidecar(&path).sidecar.unwrap().palette_id, "rainbow");
    let leftovers: Vec<_> = fs::read_dir(path.parent().unwrap())
        .unwrap()
        .map(|e| e.unwrap().file_name())
        .collect();
    assert_eq!(leftovers, vec![std::ffi::OsString::from("a.thermo.json")]);
    assert!(reset_sidecar(&path).unwrap());
    assert!(!path.exists());
    assert!(!reset_sidecar(&path).unwrap());
}

#[test]
fn locations_for_local_and_camera_captures() {
    let dir = tempfile::tempdir().unwrap();
    let edits = fs::canonicalize(dir.path()).unwrap().join("edits");

    let local = CaptureRef::Local {
        path: dir
            .path()
            .join("20261126102014.DAT")
            .to_string_lossy()
            .into_owned(),
    };
    assert_eq!(
        sidecar_path(&local, &edits).unwrap(),
        dir.path().join("20261126102014.thermo.json")
    );
    let relative = CaptureRef::Local {
        path: "relative.DAT".into(),
    };
    assert!(sidecar_path(&relative, &edits).is_err());

    let camera: CaptureRef = serde_json::from_str(
        r#"{"kind":"camera","serial":"0011223344556677","sd_path":"/20261125101500.DAT"}"#,
    )
    .unwrap();
    assert_eq!(
        sidecar_path(&camera, &edits).unwrap(),
        edits
            .join("0011223344556677")
            .join("20261125101500.thermo.json")
    );

    let video = CaptureRef::Camera {
        serial: "0011223344556677".into(),
        sd_path: "/20261126101500".into(),
    };
    assert_eq!(
        sidecar_path(&video, &edits).unwrap(),
        edits
            .join("0011223344556677")
            .join("20261126101500.thermo.json")
    );
}

#[test]
fn camera_paths_that_escape_the_edits_folder_are_refused() {
    let dir = tempfile::tempdir().unwrap();
    let edits = dir.path().join("edits");
    for (serial, sd_path) in [
        ("0011223344556677", "/../../x"),
        ("0011223344556677", "../x.DAT"),
        ("..", "/x.DAT"),
        ("a/b", "/x.DAT"),
        ("", "/x.DAT"),
        ("0011223344556677", "C:\\x.DAT"),
    ] {
        let cap = CaptureRef::Camera {
            serial: serial.into(),
            sd_path: sd_path.into(),
        };
        let err = sidecar_path(&cap, &edits).expect_err(&format!("{serial:?} {sd_path:?}"));
        assert_eq!(err.kind(), "bad_path");
    }
}
