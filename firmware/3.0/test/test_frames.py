"""Sensor data, temperatures and what the camera renders."""

import statistics

import pytest

import thermocam as tc
from colormaps import colorbar_rows, rgb565, schemes


def to_celsius(cam: tc.Thermocam, temp: float) -> float:
    assert cam.config is not None
    return (temp - 32) * 5 / 9 if cam.config.temp_format == 1 else temp


def test_raw_frames_are_live(fresh: tc.Thermocam) -> None:
    frames = [fresh.raw_frame() for _ in range(8)]
    for f in frames:
        assert len(f.pixels) == f.width * f.height
        assert f.min_raw < f.max_raw, "flat frame: sensor not delivering an image"
        assert len(set(f.pixels)) > 50
    assert len({bytes(f.pixels) for f in frames}) == len(frames), "repeated frame: sensor stuck"


def test_frame_limits_match_pixels(fresh: tc.Thermocam) -> None:
    f = fresh.raw_frame()
    inside = sum(f.min_raw <= p <= f.max_raw for p in f.pixels)
    assert inside / len(f.pixels) > 0.95


def test_scene_temperature_is_plausible(fresh: tc.Thermocam) -> None:
    f = fresh.raw_frame()
    median = f.to_celsius(statistics.median(f.pixels))
    assert -20 < median < 80, f"median scene temperature {median:.1f} °C"


def test_calibration_matches_frame(fresh: tc.Thermocam) -> None:
    offset, slope = fresh.get_calibration()
    f = fresh.raw_frame()
    assert (offset, slope) == pytest.approx((f.cal_offset, f.cal_slope))
    assert 0.001 < slope < 0.2


def test_spot_meter_agrees_with_frame_center(fresh: tc.Thermocam) -> None:
    """The spot value comes from the Lepton over I2C, the frame over SPI: they must agree."""
    diffs = []
    for _ in range(3):
        f = fresh.raw_frame()
        cx, cy = f.width // 2, f.height // 2
        center = [f.pixels[(cy + dy) * f.width + cx + dx] for dy in (-1, 0) for dx in (-1, 0)]
        diffs.append(to_celsius(fresh, f.spot_temp) - f.to_celsius(statistics.mean(center)))
    assert abs(statistics.median(diffs)) < 1.5, f"spot vs frame center: {diffs}"
    assert fresh.get_spot_temp() == pytest.approx(fresh.raw_frame().spot_temp, abs=2.0)


def test_color_frame_uses_the_color_scheme(fresh: tc.Thermocam) -> None:
    assert fresh.config is not None
    for number in (fresh.config.color_scheme, 13, 8):  # current, rainbow, grayscale
        assert fresh.ack(tc.CMD_SET_COLORSCHEME, number) == tc.CMD_SET_COLORSCHEME
        scheme = schemes()[number]
        palette = {rgb565(*scheme.rgb[i]) for i in range(scheme.elements)}
        f = fresh.color_frame()
        outside = sum(p not in palette for p in f.pixels)
        assert outside == 0, f"{scheme.name}: {outside} pixels not from the color scheme"
        assert len(set(f.pixels)) > 20, f"{scheme.name}: image has almost no colors"


def test_colorbar_is_drawn_for_every_scheme(fresh: tc.Thermocam) -> None:
    """The screen image must contain each scheme's color bar exactly as showColorBar() draws it."""
    for cmd, value in ((tc.CMD_SET_SHOWCOLORBAR, 1), (tc.CMD_SET_SHOWSPOT, 0), (tc.CMD_SET_SHOWMINMAX, 0)):
        assert fresh.ack(cmd, value) == cmd
    for number, scheme in sorted(schemes().items()):
        assert fresh.ack(tc.CMD_SET_COLORSCHEME, number) == tc.CMD_SET_COLORSCHEME
        screen = fresh.display_frame()
        rows = colorbar_rows(scheme)
        wrong = [
            (y, x)
            for y, color in rows.items()
            for x in range(300, 313)
            if screen[y * 320 + x] != color
        ]
        assert not wrong, f"{scheme.name}: {len(wrong)} color bar pixels wrong, first at (y, x) {wrong[0]}"


def test_screen_image_has_content(fresh: tc.Thermocam) -> None:
    screen = fresh.display_frame()
    assert len(screen) == 320 * 240
    assert len(set(screen)) > 50
