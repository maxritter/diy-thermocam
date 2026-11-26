//! Edits to a capture (`.thermo.json`, schema 2): palette, colour range, material, regions and
//! notes. The capture itself is never rewritten. Schema 1 files load and are upgraded.

use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};

use crate::atomic_file::write_atomic;
use crate::paths::{confine, PathError};
use crate::radiometry::Material;
use crate::roi::{Roi, RoiGeometry};

pub const SIDECAR_SCHEMA_VERSION: u32 = 2;
pub const SIDECAR_EXTENSION: &str = "thermo.json";

/// Colour scale limits: `mode` is `"auto"` or `"manual"`; limits are object temperatures in °C.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct ColorRange {
    #[serde(default = "auto_mode")]
    pub mode: String,
    #[serde(default)]
    pub min_c: Option<f32>,
    #[serde(default)]
    pub max_c: Option<f32>,
}

fn auto_mode() -> String {
    "auto".into()
}

impl Default for ColorRange {
    fn default() -> Self {
        Self {
            mode: auto_mode(),
            min_c: None,
            max_c: None,
        }
    }
}

/// A measuring region drawn on the image, with its own material when it has one.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Region {
    pub id: String,
    #[serde(default)]
    pub name: String,
    pub geometry: RoiGeometry,
    #[serde(default)]
    pub material: Option<Material>,
    #[serde(default = "yes")]
    pub visible: bool,
}

fn yes() -> bool {
    true
}

impl Region {
    /// The region as a `roi.rs` ROI measured with `image` unless it has its own material.
    #[must_use]
    pub fn to_roi(&self) -> Roi {
        Roi {
            id: self.id.clone(),
            label: self.name.clone(),
            geometry: self.geometry.clone(),
            emissivity_override: self.material.map(|m| m.emissivity),
            reflected_temp_override_c: self.material.map(|m| m.reflected_c),
            visible: self.visible,
        }
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct Notes {
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub text: String,
}

/// Schema 2 sidecar. Every field after `schema_version` has a default, unknown fields are
/// ignored, and schema 1 fields are carried over when present.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(from = "RawSidecar")]
pub struct ThermoSidecar {
    pub schema_version: u32,
    pub palette_id: String,
    pub range: ColorRange,
    /// The image's material (what Studio corrects every reading to).
    pub material: Material,
    /// The material the camera's sensor held when the capture was taken (`None` = factory).
    pub sensor_material: Option<Material>,
    pub regions: Vec<Region>,
    pub notes: Notes,
}

impl Default for ThermoSidecar {
    fn default() -> Self {
        Self {
            schema_version: SIDECAR_SCHEMA_VERSION,
            palette_id: "ironblack".into(),
            range: ColorRange::default(),
            material: Material::STUDIO_DEFAULT,
            sensor_material: None,
            regions: Vec::new(),
            notes: Notes::default(),
        }
    }
}

#[derive(Deserialize)]
#[serde(untagged)]
enum NotesField {
    Text(String),
    Full(Notes),
}

#[derive(Deserialize, Default)]
#[serde(default)]
struct RawSidecar {
    palette_id: Option<String>,
    range: Option<ColorRange>,
    material: Option<Material>,
    sensor_material: Option<Material>,
    regions: Option<Vec<Region>>,
    notes: Option<NotesField>,
    // Schema 1 fields.
    global_emissivity: Option<f32>,
    reflected_temp_c: Option<f32>,
    range_mode: Option<String>,
    manual_min_c: Option<f32>,
    manual_max_c: Option<f32>,
    rois: Option<Vec<Roi>>,
}

impl From<RawSidecar> for ThermoSidecar {
    fn from(raw: RawSidecar) -> Self {
        let defaults = Self::default();
        let material = raw.material.unwrap_or(Material {
            emissivity: raw
                .global_emissivity
                .unwrap_or(defaults.material.emissivity),
            reflected_c: raw
                .reflected_temp_c
                .unwrap_or(defaults.material.reflected_c),
        });
        let range = raw.range.unwrap_or_else(|| ColorRange {
            mode: raw.range_mode.unwrap_or_else(auto_mode),
            min_c: raw.manual_min_c,
            max_c: raw.manual_max_c,
        });
        let regions = raw.regions.unwrap_or_else(|| {
            raw.rois
                .unwrap_or_default()
                .into_iter()
                .map(|r| Region {
                    material: r.emissivity_override.map(|e| Material {
                        emissivity: e,
                        reflected_c: r.reflected_temp_override_c.unwrap_or(material.reflected_c),
                    }),
                    id: r.id,
                    name: r.label,
                    geometry: r.geometry,
                    visible: r.visible,
                })
                .collect()
        });
        let notes = match raw.notes {
            Some(NotesField::Full(n)) => n,
            Some(NotesField::Text(text)) => Notes {
                name: String::new(),
                text,
            },
            None => Notes::default(),
        };
        Self {
            schema_version: SIDECAR_SCHEMA_VERSION,
            palette_id: raw.palette_id.unwrap_or(defaults.palette_id),
            range,
            material,
            sensor_material: raw.sensor_material,
            regions,
            notes,
        }
    }
}

/// Which capture an edit belongs to, as the frontend names it.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum CaptureRef {
    /// A `.DAT` file or recording folder on this computer.
    Local { path: String },
    /// A file or video folder on the camera's card.
    Camera { serial: String, sd_path: String },
}

/// `<capture>.thermo.json` beside a local capture; `edits_root/<serial>/<sd_path>.thermo.json`
/// for a camera capture, confined to `edits_root`.
pub fn sidecar_path(capture: &CaptureRef, edits_root: &Path) -> Result<PathBuf, PathError> {
    match capture {
        CaptureRef::Local { path } => {
            if path.contains('\0') {
                return Err(PathError::Nul);
            }
            let p = PathBuf::from(path);
            if !p.is_absolute() || p.file_name().is_none() {
                return Err(PathError::NotRelative(path.clone()));
            }
            Ok(p.with_extension(SIDECAR_EXTENSION))
        }
        CaptureRef::Camera { serial, sd_path } => {
            if serial.contains('/') {
                return Err(PathError::NotRelative(serial.clone()));
            }
            let camera_root = confine(edits_root, serial)?;
            let file = confine(&camera_root, sd_path)?;
            Ok(file.with_extension(SIDECAR_EXTENSION))
        }
    }
}

/// Result of opening a capture's edits.
#[derive(Debug, Clone, Default, Serialize)]
pub struct SidecarLoad {
    pub sidecar: Option<ThermoSidecar>,
    /// File name the damaged sidecar was moved to (`<name>.thermo.json.bad`).
    pub quarantined: Option<String>,
}

/// Read a sidecar. A missing file means no edits; a damaged one is renamed to `<file>.bad`.
#[must_use]
pub fn load_sidecar(path: &Path) -> SidecarLoad {
    let parsed = match fs::read(path) {
        Ok(bytes) => serde_json::from_slice::<ThermoSidecar>(&bytes).ok(),
        Err(e) if e.kind() == io::ErrorKind::NotFound => return SidecarLoad::default(),
        Err(_) => None,
    };
    if let Some(sidecar) = parsed {
        return SidecarLoad {
            sidecar: Some(sidecar),
            quarantined: None,
        };
    }
    SidecarLoad {
        sidecar: None,
        quarantined: quarantine(path),
    }
}

fn quarantine(path: &Path) -> Option<String> {
    let mut bad = path.as_os_str().to_os_string();
    bad.push(".bad");
    let bad = PathBuf::from(bad);
    let _ = fs::remove_file(&bad);
    fs::rename(path, &bad).ok()?;
    bad.file_name().map(|n| n.to_string_lossy().into_owned())
}

/// Write the sidecar atomically (temporary file, then rename).
pub fn save_sidecar(path: &Path, sidecar: &ThermoSidecar) -> io::Result<()> {
    let mut sc = sidecar.clone();
    sc.schema_version = SIDECAR_SCHEMA_VERSION;
    let json = serde_json::to_vec_pretty(&sc).map_err(io::Error::other)?;
    write_atomic(path, &json)
}

/// Delete the sidecar (Reset to original). Returns whether a file was removed.
pub fn reset_sidecar(path: &Path) -> io::Result<bool> {
    match fs::remove_file(path) {
        Ok(()) => Ok(true),
        Err(e) if e.kind() == io::ErrorKind::NotFound => Ok(false),
        Err(e) => Err(e),
    }
}
