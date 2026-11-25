//! Radiometric temperature conversion, Stefan-Boltzmann emissivity & reflected temperature inversion,
//! Lepton CCI Q3.13 / centi-Kelvin encoding, and built-in material emissivity presets.

use serde::{Deserialize, Serialize};

pub const KELVIN_OFFSET: f64 = 273.15;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "lowercase")]
pub enum TempUnit {
    #[default]
    Celsius,
    Fahrenheit,
    Kelvin,
}

impl TempUnit {
    #[must_use]
    pub fn symbol(self) -> &'static str {
        match self {
            Self::Celsius => "°C",
            Self::Fahrenheit => "°F",
            Self::Kelvin => "K",
        }
    }

    #[must_use]
    pub fn from_celsius(self, c: f32) -> f32 {
        match self {
            Self::Celsius => c,
            Self::Fahrenheit => c * 9.0 / 5.0 + 32.0,
            Self::Kelvin => c + 273.15,
        }
    }

    #[must_use]
    pub fn to_celsius(self, v: f32) -> f32 {
        match self {
            Self::Celsius => v,
            Self::Fahrenheit => (v - 32.0) * 5.0 / 9.0,
            Self::Kelvin => v - 273.15,
        }
    }

    #[must_use]
    pub fn format(self, temp_c: f32, decimals: usize) -> String {
        format!(
            "{:.*} {}",
            decimals,
            self.from_celsius(temp_c),
            self.symbol()
        )
    }
}

/// Global/per-ROI radiometric compensation parameters.
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct RadiometryParams {
    pub cal_offset: f32,
    pub cal_slope: f32,
    /// Target surface emissivity `ε ∈ [0.05, 1.0]`.
    pub emissivity: f32,
    /// Reflected ambient/background temperature `T_refl` in °C.
    pub reflected_temp_c: f32,
}

impl Default for RadiometryParams {
    fn default() -> Self {
        Self {
            cal_offset: -273.15,
            cal_slope: 0.01,
            emissivity: 1.0,
            reflected_temp_c: 22.0,
        }
    }
}

/// Convert raw Lepton T-Linear count to apparent blackbody temperature in °C.
#[inline]
#[must_use]
pub fn raw_to_apparent_c(raw: u16, cal_offset: f32, cal_slope: f32) -> f32 {
    f32::from(raw) * cal_slope + cal_offset
}

/// Convert apparent temperature in °C to raw Lepton count.
#[inline]
#[must_use]
pub fn apparent_c_to_raw(temp_c: f32, cal_offset: f32, cal_slope: f32) -> u16 {
    if cal_slope <= 0.0 {
        return 0;
    }
    ((temp_c - cal_offset) / cal_slope)
        .round()
        .clamp(0.0, 65_535.0) as u16
}

/// Stefan-Boltzmann emissivity and reflected-temperature inversion:
///
/// $$T_{\text{obj}} = \left(\frac{T_{\text{app}}^4 - (1 - \varepsilon) T_{\text{refl}}^4}{\varepsilon}\right)^{1/4}$$
///
/// Temperatures in °C; returns corrected object temperature in °C.
#[must_use]
pub fn compensate_emissivity_c(apparent_c: f32, emissivity: f32, reflected_temp_c: f32) -> f32 {
    let eps = f64::from(emissivity.clamp(0.05, 1.0));
    if (eps - 1.0).abs() < 1e-6 {
        return apparent_c;
    }
    let t_app_k = (f64::from(apparent_c) + KELVIN_OFFSET).max(1.0);
    let t_refl_k = (f64::from(reflected_temp_c) + KELVIN_OFFSET).max(1.0);
    let rad_obj = (t_app_k.powi(4) - (1.0 - eps) * t_refl_k.powi(4)) / eps;
    if rad_obj <= 1.0 {
        return (1.0 - KELVIN_OFFSET) as f32;
    }
    (rad_obj.powf(0.25) - KELVIN_OFFSET) as f32
}

/// Convert raw pixel value to compensated object temperature in °C.
#[inline]
#[must_use]
pub fn raw_to_object_c(raw: u16, params: RadiometryParams) -> f32 {
    let app = raw_to_apparent_c(raw, params.cal_offset, params.cal_slope);
    compensate_emissivity_c(app, params.emissivity, params.reflected_temp_c)
}

/// Encode emissivity `[0.01, 1.0]` as Lepton CCI Q3.13 (`8192 = 1.0`).
#[must_use]
pub fn emissivity_to_q3_13(eps: f32) -> u16 {
    (eps.clamp(0.01, 1.0) * 8192.0).round() as u16
}

/// Decode Lepton CCI Q3.13 into float emissivity.
#[must_use]
pub fn q3_13_to_emissivity(raw: u16) -> f32 {
    f32::from(raw) / 8192.0
}

/// Encode °C as Lepton CCI centi-Kelvin (`29515 = 22.00 °C`).
#[must_use]
pub fn celsius_to_centikelvin(temp_c: f32) -> u16 {
    ((f64::from(temp_c) + KELVIN_OFFSET) * 100.0)
        .round()
        .clamp(0.0, 65_535.0) as u16
}

/// Decode Lepton CCI centi-Kelvin to °C.
#[must_use]
pub fn centikelvin_to_celsius(ck: u16) -> f32 {
    (f64::from(ck) / 100.0 - KELVIN_OFFSET) as f32
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct MaterialPreset {
    pub id: &'static str,
    pub name: &'static str,
    pub category: &'static str,
    pub emissivity: f32,
}

/// Built-in engineering/thermography material emissivity library.
pub const MATERIAL_PRESETS: &[MaterialPreset] = &[
    MaterialPreset {
        id: "blackbody",
        name: "Blackbody Reference",
        category: "Reference",
        emissivity: 1.00,
    },
    MaterialPreset {
        id: "skin",
        name: "Human Skin",
        category: "Biological",
        emissivity: 0.98,
    },
    MaterialPreset {
        id: "electrical_tape",
        name: "Black Electrical Tape (3M)",
        category: "Electronics",
        emissivity: 0.96,
    },
    MaterialPreset {
        id: "matte_pcb",
        name: "PCB Solder Mask (FR-4)",
        category: "Electronics",
        emissivity: 0.91,
    },
    MaterialPreset {
        id: "ic_epoxy",
        name: "IC Plastic Package (Epoxy)",
        category: "Electronics",
        emissivity: 0.94,
    },
    MaterialPreset {
        id: "water",
        name: "Water / Ice",
        category: "Building",
        emissivity: 0.96,
    },
    MaterialPreset {
        id: "brick",
        name: "Brick / Concrete / Plaster",
        category: "Building",
        emissivity: 0.93,
    },
    MaterialPreset {
        id: "wood",
        name: "Wood / Paper / Cardboard",
        category: "Building",
        emissivity: 0.90,
    },
    MaterialPreset {
        id: "paint_matte",
        name: "Matte Paint (Any Color)",
        category: "Coatings",
        emissivity: 0.94,
    },
    MaterialPreset {
        id: "anodized_al",
        name: "Anodized Aluminum (Black)",
        category: "Metals",
        emissivity: 0.84,
    },
    MaterialPreset {
        id: "oxidized_steel",
        name: "Oxidized Steel / Iron",
        category: "Metals",
        emissivity: 0.79,
    },
    MaterialPreset {
        id: "oxidized_cu",
        name: "Oxidized Copper",
        category: "Metals",
        emissivity: 0.65,
    },
    MaterialPreset {
        id: "solder_joint",
        name: "Solder Joint (Sn/Ag/Cu)",
        category: "Electronics",
        emissivity: 0.35,
    },
    MaterialPreset {
        id: "brushed_al",
        name: "Brushed Aluminum",
        category: "Metals",
        emissivity: 0.22,
    },
    MaterialPreset {
        id: "polished_cu",
        name: "Polished Copper / Brass",
        category: "Metals",
        emissivity: 0.08,
    },
];
