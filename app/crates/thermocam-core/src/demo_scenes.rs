//! The demo camera's picture: eight real sample captures from the website, played in turn.
//! Each is turned into °C with its own calibration, re-encoded in the simulator's scale
//! (centi-kelvin, `cal_offset -273.15`, `cal_slope 0.01`) and given a little sensor noise.

use std::sync::OnceLock;

use crate::dat::ThermalDat;
use crate::protocol::codec::{FrameRaw, FrameRawMeta};
use crate::radiometry::raw_to_apparent_c;

/// How long each capture shows: about ten seconds at the demo's 9 fps.
pub const FRAMES_PER_SCENE: u32 = 90;

const SCENES: [(&str, &[u8]); 8] = [
    ("HOUSE", include_bytes!("../assets/demo/HOUSE.DAT")),
    ("SOLAR", include_bytes!("../assets/demo/SOLAR.DAT")),
    ("PCB", include_bytes!("../assets/demo/PCB.DAT")),
    ("BREAKER", include_bytes!("../assets/demo/BREAKER.DAT")),
    ("PRINTER", include_bytes!("../assets/demo/PRINTER.DAT")),
    ("PAN", include_bytes!("../assets/demo/PAN.DAT")),
    ("BEEHIVE", include_bytes!("../assets/demo/BEEHIVE.DAT")),
    ("FRIDGE", include_bytes!("../assets/demo/FRIDGE.DAT")),
];

const W: u16 = 160;
const H: u16 = 120;

/// The captures in the order they play.
#[must_use]
pub fn scene_names() -> Vec<&'static str> {
    SCENES.iter().map(|(name, _)| *name).collect()
}

/// Each capture in °C, row by row, 160 × 120.
fn scenes() -> &'static [Vec<f32>] {
    static CELSIUS: OnceLock<Vec<Vec<f32>>> = OnceLock::new();
    CELSIUS.get_or_init(|| {
        SCENES
            .iter()
            .map(|(name, bytes)| {
                let dat = ThermalDat::from_bytes(bytes)
                    .unwrap_or_else(|e| panic!("demo capture {name} does not parse: {e}"));
                assert_eq!((dat.width, dat.height), (W, H), "demo capture {name}");
                dat.pixels
                    .iter()
                    .map(|&raw| raw_to_apparent_c(raw, dat.cal_offset, dat.cal_slope))
                    .collect()
            })
            .collect()
    })
}

/// The demo camera's frame `frame_seq`.
#[must_use]
pub fn demo_scene_frame(frame_seq: u32) -> FrameRaw {
    let all = scenes();
    let scene = &all[(frame_seq / FRAMES_PER_SCENE) as usize % all.len()];
    let mut pixels = Vec::with_capacity(scene.len());
    let (mut min_raw, mut max_raw) = (u16::MAX, 0u16);
    let (mut min_i, mut max_i) = (0usize, 0usize);
    for (i, &c) in scene.iter().enumerate() {
        // About 50 mK of sensor noise, the same hash the simulated scene used.
        let x = (i % usize::from(W)) as u32;
        let y = (i / usize::from(W)) as u32;
        let hash = (x.wrapping_mul(73_856_093)
            ^ y.wrapping_mul(19_349_663)
            ^ frame_seq.wrapping_mul(83_492_791))
            & 0xFF;
        let noisy = c + ((hash as f32 / 255.0) - 0.5) * 0.14;
        let raw = ((noisy + 273.15) / 0.01).round().clamp(1000.0, 65000.0) as u16;
        if raw < min_raw {
            min_raw = raw;
            min_i = i;
        }
        if raw > max_raw {
            max_raw = raw;
            max_i = i;
        }
        pixels.push(raw);
    }
    let xy = |i: usize| ((i % usize::from(W)) as u16, (i / usize::from(W)) as u16);
    let (min_x, min_y) = xy(min_i);
    let (max_x, max_y) = xy(max_i);
    let center = pixels[usize::from(H / 2) * usize::from(W) + usize::from(W / 2)];
    FrameRaw {
        meta: FrameRawMeta {
            frame_seq,
            timestamp_us: u64::from(frame_seq) * 113_895,
            rtc_epoch: 1_764_066_000 + frame_seq / 9,
            dropped_frames: 0,
            width: W,
            height: H,
            pixel_format: 0,
            gain_mode: 0,
            ffc_state: 0,
            flags: 0x02,
            min_raw,
            max_raw,
            min_x,
            min_y,
            max_x,
            max_y,
            spot_temp_c: f32::from(center) * 0.01 - 273.15,
            cal_offset: -273.15,
            cal_slope: 0.01,
            fpa_temp_c: 31.4,
            bat_pct: 87,
            usb_power: true,
            temp_point_count: 0,
        },
        pixels,
        temp_points: Vec::new(),
    }
}
