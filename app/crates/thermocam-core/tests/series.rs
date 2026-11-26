use std::fs;
use std::path::PathBuf;

use serde::Deserialize;

use thermocam_core::protocol::transport::generate_demo_frame;
use thermocam_core::radiometry::{
    raw_to_apparent_c, raw_to_object_c, reading_c, Calibration, Material, RadiometryParams,
};
use thermocam_core::recording::{
    open_recording, read_recording_frame, write_trimmed_recording, RecordingOptions,
    RecordingWriter, StdSink, StopReason,
};
use thermocam_core::roi::{evaluate_roi, Roi, RoiGeometry};
use thermocam_core::series::{compute_series, series_csv, SeriesRegion, SeriesStat};

#[derive(Deserialize)]
struct Vectors {
    vectors: Vec<Vector>,
}

#[derive(Deserialize)]
struct Vector {
    id: String,
    raw: u16,
    cal_offset: f32,
    cal_slope: f32,
    sensor_material: Option<Material>,
    want: Material,
    expected_c: f32,
    tolerance_c: f32,
}

fn fixture(name: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(name)
}

#[test]
fn shared_reading_vectors_a_to_c_pass() {
    let v: Vectors =
        serde_json::from_str(&fs::read_to_string(fixture("readings.json")).unwrap()).unwrap();
    assert!(v.vectors.len() >= 3);
    for case in v.vectors {
        let got = reading_c(
            case.raw,
            Calibration {
                cal_offset: case.cal_offset,
                cal_slope: case.cal_slope,
            },
            case.sensor_material,
            case.want,
        );
        assert!(
            (got - case.expected_c).abs() <= case.tolerance_c,
            "{}: got {got}, expected {} ± {}",
            case.id,
            case.expected_c,
            case.tolerance_c
        );
    }
}

const CAL: Calibration = Calibration {
    cal_offset: -273.15,
    cal_slope: 0.01,
};

#[test]
fn a_factory_sensor_gives_todays_readings_bit_for_bit() {
    let want = Material::STUDIO_DEFAULT;
    let params = RadiometryParams {
        cal_offset: CAL.cal_offset,
        cal_slope: CAL.cal_slope,
        emissivity: want.emissivity,
        reflected_temp_c: want.reflected_c,
    };
    for raw in (27_000u16..40_000).step_by(37) {
        assert_eq!(
            reading_c(raw, CAL, None, want).to_bits(),
            raw_to_object_c(raw, params).to_bits(),
            "raw {raw}"
        );
        assert_eq!(
            reading_c(raw, CAL, Some(Material::FACTORY), want).to_bits(),
            raw_to_object_c(raw, params).to_bits()
        );
    }
}

#[test]
fn the_same_material_on_sensor_and_reading_passes_through() {
    let m = Material {
        emissivity: 0.9,
        reflected_c: 18.0,
    };
    for raw in [29_000u16, 31_500, 35_000] {
        assert_eq!(
            reading_c(raw, CAL, Some(m), m),
            raw_to_apparent_c(raw, CAL.cal_offset, CAL.cal_slope)
        );
    }
}

fn synthetic_recording(frames: u32) -> (tempfile::TempDir, PathBuf) {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("rec");
    let mut w = RecordingWriter::start(
        dir.clone(),
        RecordingOptions::studio(None),
        StdSink,
        Box::new(|| None),
    )
    .unwrap();
    for i in 0..frames {
        w.push(&generate_demo_frame(i, 160, 120), i * 114);
    }
    w.finish(StopReason::User);
    (tmp, dir)
}

fn regions() -> Vec<SeriesRegion> {
    let image = Material::STUDIO_DEFAULT;
    vec![
        SeriesRegion {
            id: "box".into(),
            geometry: RoiGeometry::Box {
                x: 50,
                y: 46,
                w: 24,
                h: 18,
            },
            material: image,
            stat: SeriesStat::Max,
        },
        SeriesRegion {
            id: "circle".into(),
            geometry: RoiGeometry::Circle {
                cx: 108.0,
                cy: 41.0,
                radius: 7.5,
            },
            material: Material {
                emissivity: 0.8,
                reflected_c: 20.0,
            },
            stat: SeriesStat::Avg,
        },
        SeriesRegion {
            id: "spot".into(),
            geometry: RoiGeometry::Spot {
                x: 80,
                y: 60,
                kernel: 3,
                show_raw: false,
            },
            material: image,
            stat: SeriesStat::Min,
        },
        SeriesRegion {
            id: "delta".into(),
            geometry: RoiGeometry::Delta {
                x0: 61,
                y0: 57,
                x1: 20,
                y1: 100,
            },
            material: image,
            stat: SeriesStat::Max,
        },
    ]
}

#[test]
fn a_series_over_120_frames_equals_per_frame_evaluate_roi() {
    let (_tmp, dir) = synthetic_recording(120);
    let rec = open_recording(&dir).unwrap();
    let regions = regions();
    let mut progress = Vec::new();
    let series = compute_series(&rec, &regions, |d, t| progress.push((d, t))).unwrap();
    assert_eq!(progress.last(), Some(&(120, 120)));
    assert_eq!(series.t_ms.as_ref().unwrap().len(), 120);
    assert_eq!(series.values.len(), regions.len());

    for frame in [0usize, 1, 59, 119] {
        let dat = read_recording_frame(&rec, frame).unwrap();
        for (ri, region) in regions.iter().enumerate() {
            let global = RadiometryParams {
                cal_offset: dat.cal_offset,
                cal_slope: dat.cal_slope,
                emissivity: region.material.emissivity,
                reflected_temp_c: region.material.reflected_c,
            };
            let roi = Roi {
                id: region.id.clone(),
                label: region.id.clone(),
                geometry: region.geometry.clone(),
                emissivity_override: None,
                reflected_temp_override_c: None,
                visible: true,
            };
            let stats = evaluate_roi(&roi, &dat.pixels, dat.width, dat.height, global);
            let want = match (&region.geometry, region.stat) {
                (RoiGeometry::Delta { .. }, _) => stats.delta_c.unwrap(),
                (_, SeriesStat::Max) => stats.max_c,
                (_, SeriesStat::Min) => stats.min_c,
                (_, SeriesStat::Avg) => stats.mean_c,
            };
            assert_eq!(
                series.values[ri][frame].to_bits(),
                want.to_bits(),
                "{} at frame {frame}",
                region.id
            );
        }
    }
}

#[test]
fn series_csv_for_frames_10_to_19_has_10_rows() {
    let (_tmp, dir) = synthetic_recording(30);
    let rec = open_recording(&dir).unwrap();
    let regions = regions();
    let series = compute_series(&rec, &regions, |_, _| {}).unwrap();
    let ids: Vec<String> = regions.iter().map(|r| r.id.clone()).collect();
    let csv = series_csv(&series, &ids, 10, 19);
    let lines: Vec<&str> = csv.lines().collect();
    assert_eq!(lines[0], "frame,t_ms,box,circle,spot,delta");
    assert_eq!(lines.len(), 11, "header plus 10 rows");
    assert!(lines[1].starts_with("10,1140,"));
    assert!(lines[10].starts_with("19,2166,"));

    // A camera video has no times: the column stays empty.
    let untimed = thermocam_core::series::Series {
        t_ms: None,
        values: vec![vec![1.0, 2.0, 3.0]],
    };
    let csv = series_csv(&untimed, &["a".to_string()], 0, 99);
    assert_eq!(csv, "frame,t_ms,a\n0,,1.00\n1,,2.00\n2,,3.00\n");
}

#[test]
fn region_ids_are_quoted_when_needed() {
    let s = thermocam_core::series::Series {
        t_ms: Some(vec![0]),
        values: vec![vec![1.0]],
    };
    assert_eq!(
        series_csv(&s, &["MCU, top \"hot\"".to_string()], 0, 0),
        "frame,t_ms,\"MCU, top \"\"hot\"\"\"\n0,0,1.00\n"
    );
}

#[test]
fn a_trimmed_dat_export_is_a_recording_of_just_that_range() {
    let (tmp, dir) = synthetic_recording(30);
    let rec = open_recording(&dir).unwrap();
    let dest = tmp.path().join("trimmed");
    write_trimmed_recording(&rec, 10, 19, &dest).unwrap();
    let trimmed = open_recording(&dest).unwrap();
    assert_eq!(trimmed.frames, 10);
    assert_eq!(trimmed.t_ms.as_ref().unwrap()[0], 0);
    assert_eq!(trimmed.t_ms.as_ref().unwrap()[9], 9 * 114);
    assert_eq!(
        read_recording_frame(&trimmed, 0).unwrap().pixels,
        read_recording_frame(&rec, 10).unwrap().pixels
    );
    assert!(write_trimmed_recording(&rec, 20, 40, &tmp.path().join("bad")).is_err());
}
