use std::cell::Cell;
use std::fs;

use thermocam_core::captures::trash_targets;
use thermocam_core::disk_cache::{evict_oldest, DiskCache};
use thermocam_core::imaging::render_palette_rgb;
use thermocam_core::palettes::find_palette;
use thermocam_core::picture::{render_png, PictureFrame, RenderSpec};
use thermocam_core::protocol::transport::generate_demo_frame;
use thermocam_core::radiometry::raw_to_apparent_c;

fn decode(png: &[u8]) -> image::RgbImage {
    image::load_from_memory_with_format(png, image::ImageFormat::Png)
        .unwrap()
        .to_rgb8()
}

fn hottest(pixels: &[u16], width: u16) -> (u32, u32) {
    let (idx, _) = pixels.iter().enumerate().max_by_key(|(_, v)| **v).unwrap();
    (
        (idx % usize::from(width)) as u32,
        (idx / usize::from(width)) as u32,
    )
}

#[test]
fn ironblack_at_1x_is_160_by_120_and_the_hottest_pixel_is_the_last_colour() {
    let frame = generate_demo_frame(5, 160, 120);
    let palette = find_palette("ironblack").unwrap();
    let spec: RenderSpec = serde_json::from_str(r#"{"palette_id":"ironblack"}"#).unwrap();
    let pf = PictureFrame::from_frame(&frame);
    let (w, h, png) = render_png(&pf, &spec, &palette).unwrap();
    assert_eq!((w, h), (160, 120));
    let img = decode(&png);
    assert_eq!(img.dimensions(), (160, 120));
    let (x, y) = hottest(&frame.pixels, 160);
    assert_eq!(img.get_pixel(x, y).0, *palette.rgb.last().unwrap());
    // And the coldest pixel is the first colour.
    let (idx, _) = frame
        .pixels
        .iter()
        .enumerate()
        .min_by_key(|(_, v)| **v)
        .unwrap();
    assert_eq!(
        img.get_pixel((idx % 160) as u32, (idx / 160) as u32).0,
        palette.rgb[0]
    );
}

#[test]
fn a_manual_range_clips_to_the_palette_ends() {
    let frame = generate_demo_frame(5, 160, 120);
    let palette = find_palette("ironblack").unwrap();
    let min_c = raw_to_apparent_c(frame.meta.min_raw, -273.15, 0.01);
    let spec: RenderSpec = serde_json::from_value(serde_json::json!({
        "palette_id": "ironblack",
        "range": {"mode": "manual", "min_c": min_c + 1.0, "max_c": min_c + 2.0},
        "material": {"emissivity": 1.0, "reflected_c": 22.0},
    }))
    .unwrap();
    let (_, _, png) = render_png(&PictureFrame::from_frame(&frame), &spec, &palette).unwrap();
    let img = decode(&png);
    let (x, y) = hottest(&frame.pixels, 160);
    assert_eq!(img.get_pixel(x, y).0, *palette.rgb.last().unwrap());
}

#[test]
fn larger_scales_and_overlays_render_at_the_requested_size() {
    let frame = generate_demo_frame(5, 160, 120);
    let palette = find_palette("rainbow").unwrap();
    let spec: RenderSpec = serde_json::from_value(serde_json::json!({
        "palette_id": "rainbow",
        "scale": 4,
        "overlays": {
            "regions": [{"id": "r1", "name": "MCU", "geometry": {"kind": "box", "x": 50, "y": 46, "w": 24, "h": 18}}],
            "colorbar": true
        }
    }))
    .unwrap();
    let (w, h, png) = render_png(&PictureFrame::from_frame(&frame), &spec, &palette).unwrap();
    assert_eq!((w, h), (640, 480));
    assert_eq!(decode(&png).dimensions(), (640, 480));
}

#[test]
fn palette_mapping_is_exact_per_pixel() {
    let palette = find_palette("grayscale").unwrap();
    let rgb = render_palette_rgb(&[100, 200, 300, 400], 100, 400, &palette);
    assert_eq!(&rgb[0..3], &palette.rgb[0]);
    assert_eq!(&rgb[9..12], palette.rgb.last().unwrap());
}

#[test]
fn a_thumbnail_requested_twice_comes_from_the_cache_the_second_time() {
    let dir = tempfile::tempdir().unwrap();
    let cache = DiskCache::new(dir.path().join("thumbs"));
    let key = DiskCache::key(&["/Captures/a.DAT", "1764065700", "38805", "ironblack"]);
    let renders = Cell::new(0);
    let render = || -> Result<Vec<u8>, std::io::Error> {
        renders.set(renders.get() + 1);
        Ok(vec![1, 2, 3])
    };
    let (first, hit1) = cache.get_or_insert_with(&key, render).unwrap();
    let (second, hit2) = cache.get_or_insert_with(&key, render).unwrap();
    assert_eq!(renders.get(), 1);
    assert!(!hit1);
    assert!(hit2);
    assert_eq!(first, second);
    // A different palette is a different picture.
    let other = DiskCache::key(&["/Captures/a.DAT", "1764065700", "38805", "rainbow"]);
    assert_ne!(key, other);
    let (_, hit3) = cache.get_or_insert_with(&other, render).unwrap();
    assert!(!hit3);
    assert_eq!(renders.get(), 2);
}

#[test]
fn eviction_removes_the_oldest_entries_first() {
    let dir = tempfile::tempdir().unwrap();
    let root = dir.path();
    for (i, name) in ["a", "b", "c"].iter().enumerate() {
        let p = root.join(name);
        fs::create_dir(&p).unwrap();
        fs::write(p.join("x"), vec![0u8; 1000]).unwrap();
        let t =
            std::time::SystemTime::UNIX_EPOCH + std::time::Duration::from_secs(1_000 + i as u64);
        fs::File::options()
            .write(true)
            .open(p.join("x"))
            .unwrap()
            .set_modified(t)
            .unwrap();
    }
    evict_oldest(root, 2_000, Some(&root.join("a"))).unwrap();
    assert!(root.join("a").exists(), "kept entry stays");
    assert!(!root.join("b").exists(), "oldest other entry goes");
    assert!(root.join("c").exists());
}

#[test]
fn trash_refuses_paths_outside_the_captures_folder() {
    let captures = tempfile::tempdir().unwrap();
    let inside = captures.path().join("20261126102014.DAT");
    fs::write(&inside, b"x").unwrap();
    let elsewhere = tempfile::tempdir().unwrap();
    let outside = elsewhere.path().join("keep.DAT");
    fs::write(&outside, b"x").unwrap();

    let results = trash_targets(
        captures.path(),
        &[
            inside.to_string_lossy().into_owned(),
            outside.to_string_lossy().into_owned(),
            format!("{}/../keep.DAT", captures.path().display()),
            captures.path().to_string_lossy().into_owned(),
        ],
    );
    assert!(results[0].is_ok());
    for r in &results[1..] {
        assert_eq!(r.as_ref().unwrap_err().kind(), "bad_path");
    }
    assert!(outside.exists());
}
