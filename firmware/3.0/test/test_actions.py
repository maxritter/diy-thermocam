"""Temperature points, shutter and saving an image to the SD card."""

import time

import thermocam as tc
from conftest import Metrics

EMPTY = [(0, 0)] * 96


def test_temperature_points_round_trip(fresh: tc.Thermocam) -> None:
    points = [(0x1234, 0), (0x0101, 0)] + [(0, 0)] * 94
    assert fresh.set_temp_points(points) == tc.CMD_SET_TEMPPOINTS
    fresh.raw_frame()  # the camera fills in the live value of each point every frame
    back = fresh.get_temp_points()
    assert [i for i, _ in back[:3]] == [0x1234, 0x0101, 0]
    assert back[0][1] > 0, "point value was not refreshed from the sensor"
    assert fresh.set_temp_points(EMPTY) == tc.CMD_SET_TEMPPOINTS
    assert fresh.get_temp_points()[0][0] == 0


def test_temperature_points_sent_in_pieces(fresh: tc.Thermocam) -> None:
    """The payload may trail the command byte in later USB packets (rejected before PR #123).
    The camera waits up to 100 ms for it, so these pieces arrive within 75 ms."""
    for _ in range(5):
        assert fresh.set_temp_points(EMPTY, pieces=3, gap=0.025) == tc.CMD_SET_TEMPPOINTS


def test_shutter_calibration(fresh: tc.Thermocam, metrics: Metrics) -> None:
    t = time.monotonic()
    assert fresh.ack(tc.CMD_SET_SHUTTERRUN) == tc.CMD_SET_SHUTTERRUN
    metrics.record("shutter_s", time.monotonic() - t, higher_is_better=False, tolerance=0.5)
    f = fresh.raw_frame()
    assert f.min_raw < f.max_raw


def test_save_image_to_sd_card(fresh: tc.Thermocam, metrics: Metrics) -> None:
    """Writes one real image (.DAT, plus .BMP if enabled on the camera) to the SD card."""
    t = time.monotonic()
    answer = fresh.ack(tc.CMD_FRAME_SAVE)
    assert answer != tc.CMD_INVALID, "camera refused to save: SD card missing or full"
    assert answer == tc.CMD_FRAME_SAVE
    metrics.record("save_image_s", time.monotonic() - t, higher_is_better=False, tolerance=0.5)
    assert fresh.get_diagnostic() & (1 << 2), "SD card reported faulty after saving"
    f = fresh.raw_frame()
    assert f.min_raw < f.max_raw
