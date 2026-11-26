//! Pictures rendered from raw frames (Copy image, Save PNG…, PNG on snapshot, card pictures),
//! so they look the same on every system and never read back the screen.

use serde::Deserialize;

use crate::dat::ThermalDat;
use crate::export::{
    encode_png, render_export_canvas, ExportError, ExportOptions, ExportResolution,
};
use crate::imaging::{render_palette_rgb, render_thermal_rgb, DisplayPipeline};
use crate::palettes::Palette;
use crate::protocol::codec::FrameRaw;
use crate::radiometry::{
    apparent_c_to_raw, compensate_emissivity_c, uncompensate_emissivity_c, Material,
    RadiometryParams, TempUnit,
};
use crate::roi::Roi;
use crate::sidecar::{ColorRange, Region};

pub const MAX_PICTURE_SCALE: u32 = 8;

/// What to draw: the frontend's palette, colour range, material and overlays.
#[derive(Debug, Clone, Deserialize)]
pub struct RenderSpec {
    pub palette_id: String,
    #[serde(default)]
    pub range: ColorRange,
    /// The image's material; manual range limits are object temperatures in this material.
    #[serde(default)]
    pub material: Option<Material>,
    /// The material the sensor held for this frame (`None` = factory).
    #[serde(default)]
    pub sensor_material: Option<Material>,
    #[serde(default)]
    pub overlays: Overlays,
    /// Integer upscale of the 160 × 120 sensor image (1 to 8).
    #[serde(default = "one")]
    pub scale: u32,
}

fn one() -> u32 {
    1
}

#[derive(Debug, Clone, Default, Deserialize)]
pub struct Overlays {
    #[serde(default)]
    pub regions: Vec<Region>,
    #[serde(default)]
    pub colorbar: bool,
    #[serde(default)]
    pub hud: bool,
    #[serde(default)]
    pub unit: TempUnit,
}

impl Overlays {
    fn any(&self) -> bool {
        !self.regions.is_empty() || self.colorbar || self.hud
    }
}

/// A raw frame to draw, from the stream or from a `.DAT`.
#[derive(Debug, Clone, Copy)]
pub struct PictureFrame<'a> {
    pub pixels: &'a [u16],
    pub width: u16,
    pub height: u16,
    pub cal_offset: f32,
    pub cal_slope: f32,
}

impl<'a> PictureFrame<'a> {
    #[must_use]
    pub fn from_frame(f: &'a FrameRaw) -> Self {
        Self {
            pixels: &f.pixels,
            width: f.meta.width,
            height: f.meta.height,
            cal_offset: f.meta.cal_offset,
            cal_slope: f.meta.cal_slope,
        }
    }

    #[must_use]
    pub fn from_dat(d: &'a ThermalDat) -> Self {
        Self {
            pixels: &d.pixels,
            width: d.width,
            height: d.height,
            cal_offset: d.cal_offset,
            cal_slope: d.cal_slope,
        }
    }
}

/// Raw count that reads as `object_c` for a surface of `want`, given the sensor's material.
#[must_use]
pub fn raw_for_object_c(
    object_c: f32,
    cal_offset: f32,
    cal_slope: f32,
    sensor: Option<Material>,
    want: Material,
) -> u16 {
    let apparent = uncompensate_emissivity_c(object_c, want.emissivity, want.reflected_c);
    let reported = match sensor {
        Some(s) if !s.is_blackbody() => {
            compensate_emissivity_c(apparent, s.emissivity, s.reflected_c)
        }
        _ => apparent,
    };
    apparent_c_to_raw(reported, cal_offset, cal_slope)
}

/// The raw limits the palette spans: the frame's own min and max, or the manual range.
#[must_use]
pub fn raw_range(frame: &PictureFrame<'_>, spec: &RenderSpec) -> (u16, u16) {
    let (lo, hi) = frame
        .pixels
        .iter()
        .fold((u16::MAX, 0u16), |(lo, hi), &p| (lo.min(p), hi.max(p)));
    let (mut lo, mut hi) = if frame.pixels.is_empty() {
        (0, 1)
    } else {
        (lo, hi)
    };
    if spec.range.mode == "manual" {
        let want = spec.material.unwrap_or(Material::STUDIO_DEFAULT);
        let to_raw = |c: f32| {
            raw_for_object_c(
                c,
                frame.cal_offset,
                frame.cal_slope,
                spec.sensor_material,
                want,
            )
        };
        if let Some(c) = spec.range.min_c {
            lo = to_raw(c);
        }
        if let Some(c) = spec.range.max_c {
            hi = to_raw(c);
        }
    }
    if hi <= lo {
        hi = lo.saturating_add(1);
    }
    (lo, hi)
}

/// Render `frame` as a PNG `(width, height, bytes)`.
pub fn render_png(
    frame: &PictureFrame<'_>,
    spec: &RenderSpec,
    palette: &Palette,
) -> Result<(u32, u32, Vec<u8>), ExportError> {
    let (w, h, rgb) = render_rgb(frame, spec, palette);
    Ok((w, h, encode_png(w, h, &rgb)?))
}

/// Render `frame` as RGB8 `(width, height, pixels)`.
#[must_use]
pub fn render_rgb(
    frame: &PictureFrame<'_>,
    spec: &RenderSpec,
    palette: &Palette,
) -> (u32, u32, Vec<u8>) {
    let scale = spec.scale.clamp(1, MAX_PICTURE_SCALE);
    let (min_raw, max_raw) = raw_range(frame, spec);
    let material = spec.material.unwrap_or(Material::STUDIO_DEFAULT);
    let rad = RadiometryParams {
        cal_offset: frame.cal_offset,
        cal_slope: frame.cal_slope,
        emissivity: material.emissivity,
        reflected_temp_c: material.reflected_c,
    };
    let sw = usize::from(frame.width);
    let sh = usize::from(frame.height);
    let (dw, dh) = (frame.width as u32 * scale, frame.height as u32 * scale);

    if spec.overlays.any() {
        let rois: Vec<Roi> = spec.overlays.regions.iter().map(Region::to_roi).collect();
        let opts = ExportOptions {
            resolution: ExportResolution::Custom {
                width: dw,
                height: dh,
            },
            show_colorbar: spec.overlays.colorbar,
            show_hud: spec.overlays.hud,
            unit: spec.overlays.unit,
            jpeg_quality: 94,
        };
        return render_export_canvas(
            frame.pixels,
            frame.width,
            frame.height,
            min_raw,
            max_raw,
            palette,
            rad,
            &DisplayPipeline::default(),
            &rois,
            &opts,
        );
    }
    if scale == 1 {
        return (
            dw,
            dh,
            render_palette_rgb(frame.pixels, min_raw, max_raw, palette),
        );
    }
    let rgb = render_thermal_rgb(
        frame.pixels,
        sw,
        sh,
        dw as usize,
        dh as usize,
        min_raw,
        max_raw,
        palette,
        rad,
        &DisplayPipeline::default(),
    );
    (dw, dh, rgb)
}
