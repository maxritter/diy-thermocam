use thermocam_core::dat::ThermalDat;
use thermocam_core::demo_scenes::{demo_scene_frame, scene_names, FRAMES_PER_SCENE};
use thermocam_core::radiometry::raw_to_apparent_c;

fn celsius(raw: u16) -> f32 {
    raw_to_apparent_c(raw, -273.15, 0.01)
}

#[test]
fn eight_real_captures_play_in_turn() {
    assert_eq!(
        scene_names(),
        ["HOUSE", "SOLAR", "PCB", "BREAKER", "PRINTER", "PAN", "BEEHIVE", "FRIDGE"]
    );
}

#[test]
fn a_frame_carries_its_capture_temperatures_with_a_little_noise() {
    let dat = ThermalDat::from_bytes(include_bytes!("../assets/demo/HOUSE.DAT")).unwrap();
    let frame = demo_scene_frame(0);
    assert_eq!((frame.meta.width, frame.meta.height), (160, 120));
    assert_eq!(frame.pixels.len(), 160 * 120);
    for i in (0..frame.pixels.len()).step_by(97) {
        let want = raw_to_apparent_c(dat.pixels[i], dat.cal_offset, dat.cal_slope);
        let got = celsius(frame.pixels[i]);
        assert!((got - want).abs() < 0.15, "pixel {i}: {got} vs {want}");
    }
}

#[test]
fn the_next_capture_follows_after_its_frames() {
    let solar = ThermalDat::from_bytes(include_bytes!("../assets/demo/SOLAR.DAT")).unwrap();
    let frame = demo_scene_frame(FRAMES_PER_SCENE);
    let i = 60 * 160 + 80;
    let want = raw_to_apparent_c(solar.pixels[i], solar.cal_offset, solar.cal_slope);
    assert!((celsius(frame.pixels[i]) - want).abs() < 0.15);
    // After the last capture it starts again.
    let again = demo_scene_frame(FRAMES_PER_SCENE * 8);
    let house = demo_scene_frame(0);
    assert!((celsius(again.pixels[i]) - celsius(house.pixels[i])).abs() < 0.3);
}

#[test]
fn hottest_and_coldest_agree_with_the_pixels() {
    for seq in [0, 5, FRAMES_PER_SCENE * 3 + 7] {
        let f = demo_scene_frame(seq);
        let min = *f.pixels.iter().min().unwrap();
        let max = *f.pixels.iter().max().unwrap();
        assert_eq!((f.meta.min_raw, f.meta.max_raw), (min, max));
        let at = |x: u16, y: u16| f.pixels[usize::from(y) * 160 + usize::from(x)];
        assert_eq!(at(f.meta.min_x, f.meta.min_y), min);
        assert_eq!(at(f.meta.max_x, f.meta.max_y), max);
        assert_eq!(f.meta.frame_seq, seq);
    }
}
