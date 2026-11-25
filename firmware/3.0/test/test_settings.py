"""Every setting the PC can change is applied, reported back and range-checked."""

import time

import pytest

import thermocam as tc

# command, Config field, valid values, first invalid value
SETTERS = [
    (tc.CMD_SET_COLORSCHEME, "color_scheme", range(19), 19),
    (tc.CMD_SET_TEXTCOLOR, "text_color", range(5), 5),
    (tc.CMD_SET_TEMPFORMAT, "temp_format", range(2), 2),
    (tc.CMD_SET_SHOWSPOT, "spot", range(2), 2),
    (tc.CMD_SET_SHOWCOLORBAR, "colorbar", range(2), 2),
    (tc.CMD_SET_SHOWMINMAX, "min_max", range(4), 4),
    (tc.CMD_SET_FILTERTYPE, "filter_type", range(3), 3),
    (tc.CMD_SET_ROTATION, "rotation", range(2), 2),
]


@pytest.mark.parametrize(("cmd", "field", "values", "invalid"), SETTERS, ids=[s[1] for s in SETTERS])
def test_setting_round_trip(fresh: tc.Thermocam, cmd: int, field: str, values: range, invalid: int) -> None:
    for value in values:
        assert fresh.ack(cmd, value) == cmd, f"{field}={value} not acknowledged"
        assert getattr(fresh.get_config(), field) == value
    before = fresh.get_config()
    assert fresh.ack(cmd, invalid) == tc.CMD_INVALID
    assert fresh.get_config() == before, "an invalid value changed the settings"


def test_setting_value_arriving_late_is_accepted(fresh: tc.Thermocam) -> None:
    """The PC may send the value in a separate USB packet (fixed in PR #123)."""
    assert fresh.config is not None
    fresh.send(tc.CMD_SET_SHOWSPOT)
    time.sleep(0.05)
    fresh.send(fresh.config.spot)
    assert fresh.read(1)[0] == tc.CMD_SET_SHOWSPOT


def test_limits_lock_and_release(fresh: tc.Thermocam) -> None:
    assert fresh.ack(tc.CMD_SET_LIMITS, 0) == tc.CMD_SET_LIMITS
    assert fresh.get_config().limits_adjustable == 0
    assert fresh.ack(tc.CMD_SET_LIMITS, 1) == tc.CMD_SET_LIMITS
    assert fresh.get_config().limits_adjustable == 1


def test_shutter_mode_accepts_both_modes(fresh: tc.Thermocam) -> None:
    # The mode is not reported back; the camera default is automatic (1), so end there.
    for mode in (0, 1):
        assert fresh.ack(tc.CMD_SET_SHUTTERMODE, mode) == tc.CMD_SET_SHUTTERMODE
    assert fresh.ack(tc.CMD_SET_SHUTTERMODE, 2) == tc.CMD_INVALID
