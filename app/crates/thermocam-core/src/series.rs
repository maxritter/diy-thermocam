//! Region values across a whole recording (the player's Measure chart) and their CSV.

use std::fmt::Write as _;

use serde::{Deserialize, Serialize};

use crate::radiometry::{reading_c, Calibration, Material};
use crate::recording::{read_recording_frame, RecordError, RecordingIndex};
use crate::roi::{evaluate_roi_with, Roi, RoiGeometry};

/// Which statistic a region's row shows (difference regions always give their difference).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum SeriesStat {
    Min,
    #[default]
    Max,
    Avg,
}

/// One region to follow through a recording, with the material its reading uses
/// (its own, or the image's).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SeriesRegion {
    pub id: String,
    pub geometry: RoiGeometry,
    pub material: Material,
    #[serde(default)]
    pub stat: SeriesStat,
}

/// `values[region][frame]` in °C; `t_ms` as in the recording (`None` for camera videos).
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Series {
    pub t_ms: Option<Vec<u32>>,
    pub values: Vec<Vec<f32>>,
}

/// A region's value in one frame through the single-correction reading.
#[must_use]
pub fn region_value(
    region: &SeriesRegion,
    pixels: &[u16],
    width: u16,
    height: u16,
    cal: Calibration,
    sensor_material: Option<Material>,
) -> f32 {
    let roi = Roi {
        id: region.id.clone(),
        label: String::new(),
        geometry: region.geometry.clone(),
        emissivity_override: None,
        reflected_temp_override_c: None,
        visible: true,
    };
    let stats = evaluate_roi_with(
        &roi,
        pixels,
        width,
        height,
        region.material.emissivity,
        |raw| reading_c(raw, cal, sensor_material, region.material),
    );
    if let RoiGeometry::Delta { .. } = region.geometry {
        return stats.delta_c.unwrap_or(0.0);
    }
    match region.stat {
        SeriesStat::Min => stats.min_c,
        SeriesStat::Max => stats.max_c,
        SeriesStat::Avg => stats.mean_c,
    }
}

/// Every region's value in every frame, reading frames from disk one at a time.
/// `progress(done, total)` is called about every 2 % and at the end.
pub fn compute_series<P: FnMut(usize, usize)>(
    rec: &RecordingIndex,
    regions: &[SeriesRegion],
    mut progress: P,
) -> Result<Series, RecordError> {
    let total = rec.frames;
    let mut values = vec![Vec::with_capacity(total); regions.len()];
    let step = (total / 50).max(1);
    for i in 0..total {
        let dat = read_recording_frame(rec, i)?;
        let cal = Calibration {
            cal_offset: dat.cal_offset,
            cal_slope: dat.cal_slope,
        };
        for (r, region) in regions.iter().enumerate() {
            values[r].push(region_value(
                region,
                &dat.pixels,
                dat.width,
                dat.height,
                cal,
                rec.sensor_material,
            ));
        }
        if (i + 1) % step == 0 || i + 1 == total {
            progress(i + 1, total);
        }
    }
    Ok(Series {
        t_ms: rec.t_ms.clone(),
        values,
    })
}

fn csv_field(s: &str) -> String {
    if s.contains([',', '"', '\n', '\r']) {
        format!("\"{}\"", s.replace('"', "\"\""))
    } else {
        s.to_string()
    }
}

/// `frame,t_ms,<region…>` rows for frames `from..=to` (clamped to the series), values in °C
/// with two decimals; `t_ms` is empty when the recording has no times.
#[must_use]
pub fn series_csv(series: &Series, region_ids: &[String], from: usize, to: usize) -> String {
    let frames = series.values.first().map_or(0, Vec::len);
    let mut out = String::from("frame,t_ms");
    for id in region_ids {
        out.push(',');
        out.push_str(&csv_field(id));
    }
    out.push('\n');
    if frames == 0 || from >= frames {
        return out;
    }
    for f in from..=to.min(frames - 1) {
        let _ = write!(out, "{f},");
        if let Some(t) = series.t_ms.as_ref().and_then(|t| t.get(f)) {
            let _ = write!(out, "{t}");
        }
        for column in &series.values {
            let _ = write!(out, ",{:.2}", column[f]);
        }
        out.push('\n');
    }
    out
}
