"""Physical inputs. Run with --manual -s and follow the prompts."""

import time
from collections.abc import Callable

import pytest

import thermocam as tc

pytestmark = pytest.mark.manual


def wait_for_frame_status(cam: tc.Thermocam, wanted: int, seconds: float) -> bool:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        status = cam.request_frame(tc.CMD_FRAME_RAW)
        if status == wanted:
            return True
        if status == tc.FRAME_NORMAL:
            cam._read_raw(color=False)  # noqa: SLF001  drain the frame data
    return False


def test_button_short_press_asks_pc_to_save_image(fresh: tc.Thermocam, ask: Callable[[str], None]) -> None:
    ask("Press the camera's push button briefly (under 1 second).")
    assert wait_for_frame_status(fresh, tc.FRAME_CAPTURE_THERMAL, 20)


def test_button_long_press_asks_pc_to_record_video(fresh: tc.Thermocam, ask: Callable[[str], None]) -> None:
    ask("Hold the camera's push button for 2 seconds, then release it.")
    assert wait_for_frame_status(fresh, tc.FRAME_CAPTURE_VIDEO, 20)


def test_touch_ends_the_pc_session(fresh: tc.Thermocam, ask: Callable[[str], None]) -> None:
    ask("Touch the camera's screen once.")
    assert fresh.wait_for_byte(tc.CMD_END, 20), "the touch was not detected"
    time.sleep(1.5)
    fresh.start()
