//! Render 8 rich 160x120 simulated radiometric scenarios through Desktop Studio's real
//! 1920x1080 Full-HD export pipeline (EdgePreserve bilateral + Lanczos-3 + halo-clamped USM,
//! firmware/scientific palettes, and crisp ROI + colorbar HUD).
//!
//! Run with:
//!   cargo run --release -p thermocam-core --example render_showcase_1080p -- ../website/public/images/thermal/showcase

use std::env;
use std::fs;
use std::path::PathBuf;

use thermocam_core::export::{encode_png, render_export_canvas, ExportOptions, ExportResolution};
use thermocam_core::imaging::{DisplayPipeline, SpatialFilter, UpscaleMethod};
use thermocam_core::palettes::find_palette;
use thermocam_core::radiometry::{RadiometryParams, TempUnit};
use thermocam_core::roi::{IsothermConfig, Roi, RoiGeometry};

const W: usize = 160;
const H: usize = 120;

fn smooth_box(x: f32, y: f32, x0: f32, y0: f32, x1: f32, y1: f32, edge: f32) -> f32 {
    let dx = (x0 - x).max(x - x1).max(0.0);
    let dy = (y0 - y).max(y - y1).max(0.0);
    let d = dx.hypot(dy);
    (-(d * d) / (edge * edge)).exp()
}

fn dist_seg(px: f32, py: f32, ax: f32, ay: f32, bx: f32, by: f32) -> f32 {
    let (vx, vy) = (bx - ax, by - ay);
    let (wx, wy) = (px - ax, py - ay);
    let len2 = vx * vx + vy * vy;
    let t = if len2 > 1e-6 {
        ((wx * vx + wy * vy) / len2).clamp(0.0, 1.0)
    } else {
        0.0
    };
    (px - (ax + t * vx)).hypot(py - (ay + t * vy))
}

fn grain(fx: f32, fy: f32, amp: f32) -> f32 {
    ((fx * 12.9898 + fy * 78.233).sin() * 43_758.547).fract() * amp - amp * 0.5
}

fn build_pixels(f: impl Fn(f32, f32) -> f32) -> (Vec<u16>, u16, u16) {
    let mut pixels = vec![0u16; W * H];
    let mut mn = u16::MAX;
    let mut mx = 0u16;
    for y in 0..H {
        for x in 0..W {
            let tc = f(x as f32, y as f32);
            let raw = ((tc + 273.15) * 100.0).round().clamp(1000.0, 65000.0) as u16;
            pixels[y * W + x] = raw;
            if raw < mn {
                mn = raw;
            }
            if raw > mx {
                mx = raw;
            }
        }
    }
    (pixels, mn, mx)
}

fn spot(id: &str, label: &str, x: u16, y: u16) -> Roi {
    Roi {
        id: id.into(),
        label: label.into(),
        geometry: RoiGeometry::Spot {
            x,
            y,
            kernel: 3,
            show_raw: false,
        },
        emissivity_override: None,
        reflected_temp_override_c: None,
        visible: true,
    }
}

fn rbox(id: &str, label: &str, x: u16, y: u16, w: u16, h: u16) -> Roi {
    Roi {
        id: id.into(),
        label: label.into(),
        geometry: RoiGeometry::Box { x, y, w, h },
        emissivity_override: None,
        reflected_temp_override_c: None,
        visible: true,
    }
}

fn delta(id: &str, label: &str, x0: u16, y0: u16, x1: u16, y1: u16) -> Roi {
    Roi {
        id: id.into(),
        label: label.into(),
        geometry: RoiGeometry::Delta { x0, y0, x1, y1 },
        emissivity_override: None,
        reflected_temp_override_c: None,
        visible: true,
    }
}

struct Scenario {
    filename: &'static str,
    palette: &'static str,
    pixels: Vec<u16>,
    min_raw: u16,
    max_raw: u16,
    rois: Vec<Roi>,
}

fn build_scenarios() -> Vec<Scenario> {
    let mut out = Vec::new();

    // 1. Electronics PCB Power Stage (Inferno)
    {
        let (pixels, min_raw, max_raw) = build_pixels(|fx, fy| {
            let mut t = 22.8 + 0.008 * fx;
            t += 5.5 * smooth_box(fx, fy, 16.0, 14.0, 142.0, 106.0, 2.4);
            t += 7.5 * smooth_box(fx, fy, 68.0, 30.0, 128.0, 88.0, 15.0);
            t += 30.0 * smooth_box(fx, fy, 92.0, 34.0, 108.0, 48.0, 5.2); // QFN buck IC
            for &(ax, ay, bx, by, boost) in &[
                (100.0, 41.0, 122.0, 41.0, 11.0),
                (100.0, 41.0, 100.0, 78.0, 9.5),
                (100.0, 78.0, 62.0, 78.0, 8.5),
                (62.0, 78.0, 62.0, 56.0, 6.5),
            ] {
                let d = dist_seg(fx, fy, ax, ay, bx, by);
                t += boost * (-(d * d) / 7.5).exp();
            }
            t += 18.0 * smooth_box(fx, fy, 114.0, 36.0, 126.0, 46.0, 3.8); // MOSFET 1
            t += 16.5 * smooth_box(fx, fy, 114.0, 64.0, 126.0, 74.0, 3.8); // MOSFET 2
            t += 14.5 * smooth_box(fx, fy, 54.0, 70.0, 70.0, 86.0, 4.8); // Power inductor
                                                                         // Finned aluminum heatsink
            if (22.0..=48.0).contains(&fx) && (26.0..=82.0).contains(&fy) {
                let fin = 0.5 + 0.5 * ((fx - 22.0) * 1.7).sin();
                t -= (4.8 + 2.2 * fin) * smooth_box(fx, fy, 24.0, 28.0, 46.0, 80.0, 2.0);
            }
            t + grain(fx, fy, 0.16)
        });
        out.push(Scenario {
            filename: "pcb-power-stage-1080p.png",
            palette: "inferno",
            pixels,
            min_raw,
            max_raw,
            rois: vec![
                rbox("qfn", "BUCK IC", 88, 30, 24, 22),
                spot("ind", "INDUCTOR", 62, 78),
                spot("sink", "HEATSINK", 35, 54),
            ],
        });
    }

    // 2. Electrical Breaker Panel Fault (Ironblack)
    {
        let (pixels, min_raw, max_raw) = build_pixels(|fx, fy| {
            let mut t = 23.0;
            t += 2.2 * smooth_box(fx, fy, 14.0, 56.0, 146.0, 64.0, 2.0); // DIN rail
            let breaker_temps = [28.4_f32, 29.8, 28.9, 78.5, 34.8, 27.6];
            for (b, &bt) in breaker_temps.iter().enumerate() {
                let bx0 = 20.0 + b as f32 * 20.0;
                let bx1 = bx0 + 16.0;
                let bm = smooth_box(fx, fy, bx0, 30.0, bx1, 90.0, 2.0);
                t += (bt - 23.0) * bm;
                let cx = 0.5 * (bx0 + bx1);
                let dt = dist_seg(fx, fy, cx, 8.0, cx, 30.0);
                let db = dist_seg(fx, fy, cx, 90.0, cx, 114.0);
                let wb = if b == 3 { 24.0 } else { 4.2 };
                t += wb * (-(dt * dt) / 5.0).exp();
                t += (wb * 0.85) * (-(db * db) / 5.0).exp();
            }
            let d_lug = (fx - 88.0).hypot(fy - 37.0);
            t += 15.0 * (-(d_lug * d_lug) / 28.0).exp();
            t + grain(fx, fy, 0.14)
        });
        out.push(Scenario {
            filename: "breaker-panel-fault-1080p.png",
            palette: "ironblack",
            pixels,
            min_raw,
            max_raw,
            rois: vec![
                rbox("b4", "BREAKER #4", 78, 28, 20, 64),
                delta("dt", "FAULT DT", 48, 60, 88, 37),
            ],
        });
    }

    // 3. Window Draft & Radiator Plume (Rainbow)
    {
        let (pixels, min_raw, max_raw) = build_pixels(|fx, fy| {
            let stud = 0.65 * (-(((fx + 10.0) % 34.0 - 17.0).powi(2)) / 3.5).exp();
            let mut t = 21.4 - 0.02 * fy - stud;
            let in_win = smooth_box(fx, fy, 36.0, 14.0, 122.0, 68.0, 2.0);
            t -= 9.8 * in_win;
            let vbar = smooth_box(fx, fy, 77.0, 14.0, 81.0, 68.0, 1.6);
            let hbar = smooth_box(fx, fy, 36.0, 39.0, 122.0, 43.0, 1.6);
            t += 3.4 * vbar.max(hbar);
            let d_draft = dist_seg(fx, fy, 35.0, 68.0, 72.0, 71.0);
            t -= 6.2 * (-(d_draft * d_draft) / 20.0).exp();
            let rad_box = smooth_box(fx, fy, 40.0, 82.0, 118.0, 110.0, 2.8);
            let ribs = 0.74 + 0.26 * ((fx - 40.0) * 1.15).sin();
            t += 22.5 * rad_box * ribs;
            let plume = smooth_box(fx, fy, 44.0, 66.0, 114.0, 84.0, 9.0);
            t += 6.5 * plume * (1.0 - in_win * 0.65);
            t + grain(fx, fy, 0.14)
        });
        out.push(Scenario {
            filename: "window-draft-radiator-1080p.png",
            palette: "rainbow",
            pixels,
            min_raw,
            max_raw,
            rois: vec![
                spot("draft", "COLD DRAFT", 44, 69),
                spot("rad", "RADIATOR", 84, 95),
            ],
        });
    }

    // 4. Underfloor Hydronic Radiant Heating Loops (Lava)
    {
        let pipes: &[(f32, f32, f32, f32, f32)] = &[
            (18.0, 20.0, 138.0, 20.0, 18.0),
            (138.0, 20.0, 138.0, 39.0, 17.2),
            (138.0, 39.0, 24.0, 39.0, 16.4),
            (24.0, 39.0, 24.0, 58.0, 15.6),
            (24.0, 58.0, 138.0, 58.0, 14.8),
            (138.0, 58.0, 138.0, 77.0, 14.0),
            (138.0, 77.0, 24.0, 77.0, 13.2),
            (24.0, 77.0, 24.0, 96.0, 12.4),
            (24.0, 96.0, 142.0, 96.0, 11.6),
        ];
        let (pixels, min_raw, max_raw) = build_pixels(|fx, fy| {
            let tile = if (fx as i32) % 28 == 0 || (fy as i32) % 28 == 0 {
                -0.35
            } else {
                0.0
            };
            let mut pipe_heat = 0.0_f32;
            for &(x0, y0, x1, y1, h) in pipes {
                let d = dist_seg(fx, fy, x0, y0, x1, y1);
                pipe_heat = pipe_heat.max(h * (-(d * d) / 24.0).exp());
            }
            20.1 + tile + pipe_heat + grain(fx, fy, 0.12)
        });
        out.push(Scenario {
            filename: "underfloor-heating-1080p.png",
            palette: "lava",
            pixels,
            min_raw,
            max_raw,
            rois: vec![
                spot("sup", "SUPPLY", 34, 20),
                spot("ret", "RETURN", 122, 96),
                rbox("zone", "FLOOR ZONE", 20, 14, 122, 88),
            ],
        });
    }

    // 5. Solar PV Panel Cell Hotspot (Turbo)
    {
        let (pixels, min_raw, max_raw) = build_pixels(|fx, fy| {
            let mut t = 18.5; // cool sky / roof background
            let panel = smooth_box(fx, fy, 16.0, 14.0, 144.0, 106.0, 2.0);
            t += 16.5 * panel; // healthy cells ~35 C
                               // 6x4 solar cell grid lines (cooler aluminum fingers & busbars)
            if (18.0..=142.0).contains(&fx) && (16.0..=104.0).contains(&fy) {
                let gx = ((fx - 18.0) % 20.67).min(20.67 - ((fx - 18.0) % 20.67));
                let gy = ((fy - 16.0) % 22.0).min(22.0 - ((fy - 16.0) % 22.0));
                if gx < 1.1 || gy < 1.1 {
                    t -= 4.2;
                }
            }
            // Overheated bypassed cell at col=3, row=1 (x=80..100, y=38..60) -> ~74 C
            t += 38.5 * smooth_box(fx, fy, 81.0, 39.0, 99.0, 59.0, 3.0);
            // Adjacent warm neighbor bleed
            t += 6.0 * smooth_box(fx, fy, 74.0, 34.0, 106.0, 64.0, 8.0);
            t + grain(fx, fy, 0.15)
        });
        out.push(Scenario {
            filename: "solar-pv-hotspot-1080p.png",
            palette: "turbo",
            pixels,
            min_raw,
            max_raw,
            rois: vec![
                rbox("cell", "HOT CELL", 78, 36, 24, 26),
                spot("norm", "HEALTHY", 48, 72),
            ],
        });
    }

    // 6. Hydronic Radiator Air Pocket (Plasma — "Before Venting")
    {
        let (pixels, min_raw, max_raw) = build_pixels(|fx, fy| {
            let mut t = 21.0 - 0.015 * fy; // wall behind radiator
            let rad = smooth_box(fx, fy, 20.0, 22.0, 140.0, 98.0, 2.5);
            let ribs = 0.78 + 0.22 * ((fx - 20.0) * 0.92).sin();
            // Trapped air pocket keeps upper-right diagonal cold while bottom-left runs 56 C!
            let diag = (fx - 20.0) * 0.42 - (fy - 24.0) * 0.85;
            let water_fill = 1.0 / (1.0 + (diag / 8.5).exp());
            t += rad * ribs * (4.5 + 32.0 * water_fill);
            // Hot supply pipe on bottom-left (clamped to radiator max so ribs span full dynamic range)
            let d_in = dist_seg(fx, fy, 4.0, 92.0, 21.0, 92.0);
            let pipe_t = 21.0 + 36.0 * (-(d_in * d_in) / 10.0).exp();
            t = t.max(pipe_t);
            t + grain(fx, fy, 0.14)
        });
        out.push(Scenario {
            filename: "radiator-air-pocket-1080p.png",
            palette: "plasma",
            pixels,
            min_raw,
            max_raw,
            rois: vec![
                spot("air", "AIR POCKET", 112, 32),
                spot("water", "HOT FLOW", 38, 86),
                delta("dv", "DELTA T", 104, 48, 46, 76),
            ],
        });
    }

    // 7. Electric Motor & Overheating Bearing (Glowbow)
    {
        let (pixels, min_raw, max_raw) = build_pixels(|fx, fy| {
            let mut t = 22.5;
            // Finned motor body (x=24..92, y=28..92)
            let body = smooth_box(fx, fy, 26.0, 30.0, 90.0, 90.0, 3.0);
            let cooling_fins = 0.80 + 0.20 * ((fy - 30.0) * 1.1).sin();
            t += 22.0 * body * cooling_fins;
            // End-bell bearing housing (x=90..108, y=42..78) — failing bearing at ~86 C!
            t += 44.0 * smooth_box(fx, fy, 92.0, 46.0, 106.0, 74.0, 4.5);
            let d_brg = (fx - 99.0).hypot(fy - 60.0);
            t += 20.0 * (-(d_brg * d_brg) / 36.0).exp();
            // Coupling & output shaft (x=106..144, y=54..66)
            t += 26.0
                * smooth_box(fx, fy, 106.0, 54.0, 142.0, 66.0, 2.5)
                * (1.0 - (fx - 106.0).max(0.0) / 52.0);
            // Terminal box on top of motor
            t += 12.0 * smooth_box(fx, fy, 48.0, 14.0, 76.0, 30.0, 2.2);
            t + grain(fx, fy, 0.16)
        });
        out.push(Scenario {
            filename: "motor-bearing-load-1080p.png",
            palette: "glowbow",
            pixels,
            min_raw,
            max_raw,
            rois: vec![
                spot("brg", "BEARING", 99, 60),
                spot("stator", "STATOR", 58, 60),
            ],
        });
    }

    // 8. Building Facade Thermal Bridges at Dusk (Viridis)
    {
        let (pixels, min_raw, max_raw) = build_pixels(|fx, fy| {
            let mut t = 3.2 - 0.02 * (120.0 - fy); // cold dusk sky above roof
                                                   // House facade wall (x=16..144, y=42..116)
            let wall = smooth_box(fx, fy, 16.0, 42.0, 144.0, 116.0, 2.0);
            // Gable roof triangle (y=12..42)
            let roof_half_w = ((fy - 12.0).max(0.0) / 30.0) * 66.0;
            let roof = if (12.0..=44.0).contains(&fy) && (fx - 80.0).abs() <= roof_half_w {
                1.0
            } else {
                0.0
            };
            t += 3.5 * wall + 2.2 * roof;
            // Warm attic ridge leak under roof
            let d_ridge = dist_seg(fx, fy, 66.0, 24.0, 94.0, 24.0);
            t += 8.8 * (-(d_ridge * d_ridge) / 28.0).exp() * roof;
            // Warm concrete floor-slab thermal bridge horizontal stripe at y=76
            let d_slab = dist_seg(fx, fy, 18.0, 76.0, 142.0, 76.0);
            t += 6.5 * (-(d_slab * d_slab) / 9.0).exp() * wall;
            // Four windows with warm leaky perimeter frames
            for &(wx, wy) in &[(32.0, 48.0), (104.0, 48.0), (32.0, 84.0), (104.0, 84.0)] {
                let frame = smooth_box(fx, fy, wx, wy, wx + 24.0, wy + 22.0, 1.8);
                let glass = smooth_box(fx, fy, wx + 3.0, wy + 3.0, wx + 21.0, wy + 19.0, 1.5);
                t += 8.2 * frame - 3.6 * glass;
            }
            t + grain(fx, fy, 0.14)
        });
        out.push(Scenario {
            filename: "facade-thermal-bridge-1080p.png",
            palette: "viridis",
            pixels,
            min_raw,
            max_raw,
            rois: vec![
                spot("ridge", "ROOF LEAK", 80, 24),
                rbox("slab", "SLAB BRIDGE", 58, 72, 44, 10),
            ],
        });
    }

    out
}

fn main() {
    let out_dir = env::args()
        .nth(1)
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("../website/public/images/thermal/showcase"));
    fs::create_dir_all(&out_dir).expect("create showcase output directory");

    let pipeline = DisplayPipeline {
        filter: SpatialFilter::None,
        upscaler: UpscaleMethod::EdgePreserve, // bilateral + Lanczos-3 + halo-clamped USM
        sharpen_amount: 0.50,
        rotate_180: false,
        flip_horizontal: false,
        histogram_eq: false,
        isotherm: IsothermConfig::default(),
    };
    let opts = ExportOptions {
        resolution: ExportResolution::FullHd1080p,
        show_colorbar: true,
        show_hud: true,
        unit: TempUnit::Celsius,
        jpeg_quality: 94,
    };
    let rad = RadiometryParams::default();

    for sc in build_scenarios() {
        let palette = find_palette(sc.palette)
            .unwrap_or_else(|_| panic!("palette '{}' not found", sc.palette));
        let (w, h, rgb) = render_export_canvas(
            &sc.pixels, W as u16, H as u16, sc.min_raw, sc.max_raw, &palette, rad, &pipeline,
            &sc.rois, &opts,
        );
        let png = encode_png(w, h, &rgb).expect("PNG encode");
        let path = out_dir.join(sc.filename);
        fs::write(&path, &png).expect("write PNG");
        println!(
            "Wrote {} ({}x{}, {} KB, palette={})",
            path.display(),
            w,
            h,
            png.len() / 1024,
            sc.palette
        );
    }
}
