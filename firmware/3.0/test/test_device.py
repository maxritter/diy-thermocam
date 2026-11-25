"""Session, versions and hardware self-test."""

import re
import time

import thermocam as tc
from colormaps import FIRMWARE

DIAG_BITS = {0: "display", 1: "touch screen", 2: "SD card", 3: "battery gauge", 4: "Lepton sensor"}


def test_firmware_version_matches_checkout(fresh: tc.Thermocam) -> None:
    source = (FIRMWARE / "src/general/globalvariables.cpp").read_text()
    expected = int(re.search(r"uint16_t fwVersion = (\d+);", source)[1])  # type: ignore[index]
    assert fresh.get_fw_version() == expected, "the camera runs a different firmware than this checkout"


def test_hardware_version(fresh: tc.Thermocam) -> None:
    assert fresh.get_hw_version() == 2


def test_all_hardware_detected(fresh: tc.Thermocam) -> None:
    diag = fresh.get_diagnostic()
    failed = [name for bit, name in DIAG_BITS.items() if not diag & (1 << bit)]
    assert not failed, f"hardware self-test failed for: {', '.join(failed)} (0x{diag:02x})"


def test_config_values_in_range(fresh: tc.Thermocam) -> None:
    c = fresh.get_config()
    assert c.lepton_version in (tc.LEPTON_2_5, tc.LEPTON_3_5)
    assert c.rotation in (0, 1)
    assert 0 <= c.color_scheme <= 18
    assert c.temp_format in (0, 1)
    assert c.spot in (0, 1) and c.colorbar in (0, 1)
    assert 0 <= c.min_max <= 3
    assert 0 <= c.text_color <= 4
    assert 0 <= c.filter_type <= 2
    assert c.limits_adjustable in (0, 1)


def test_battery_status(fresh: tc.Thermocam) -> None:
    assert -1 <= fresh.get_battery() <= 100


def test_unknown_command_is_rejected(fresh: tc.Thermocam) -> None:
    assert fresh.ack(0x01) == tc.CMD_INVALID
    assert fresh.get_config() is not None  # session still works


def test_session_ends_and_restarts(fresh: tc.Thermocam) -> None:
    assert fresh.end(), "camera did not confirm the end of the session"
    time.sleep(1.5)  # "Connection ended" message on the camera
    fresh.start()
    assert fresh.get_diagnostic() is not None
