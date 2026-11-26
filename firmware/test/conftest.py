from __future__ import annotations

import json
import os
import platform
import time
from collections.abc import Callable, Iterator
from pathlib import Path

import pytest

import thermocam as tc

# Setting command -> Config field, used to restore the camera's own settings after the run
SETTINGS = {
    tc.CMD_SET_COLORSCHEME: "color_scheme",
    tc.CMD_SET_TEXTCOLOR: "text_color",
    tc.CMD_SET_TEMPFORMAT: "temp_format",
    tc.CMD_SET_SHOWSPOT: "spot",
    tc.CMD_SET_SHOWCOLORBAR: "colorbar",
    tc.CMD_SET_SHOWMINMAX: "min_max",
    tc.CMD_SET_FILTERTYPE: "filter_type",
    tc.CMD_SET_ROTATION: "rotation",
    tc.CMD_SET_LIMITS: "limits_adjustable",
}


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption("--port", default=os.environ.get("THERMOCAM_PORT"), help="serial port (default: first Teensy found)")
    parser.addoption("--manual", action="store_true", help="also run the tests that ask you to press the button or touch the screen")
    parser.addoption("--save-metrics", type=Path, help="write the measured performance to this JSON file")
    parser.addoption("--baseline", type=Path, help="fail when performance is clearly worse than in this JSON file")


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line("markers", "manual: needs a person at the camera (run with --manual)")


def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]) -> None:
    if config.getoption("--manual"):
        return
    skip = pytest.mark.skip(reason="needs a person at the camera; run with --manual")
    for item in items:
        if "manual" in item.keywords:
            item.add_marker(skip)


@pytest.fixture(scope="module")
def cam(pytestconfig: pytest.Config, metrics: Metrics) -> Iterator[tc.Thermocam]:
    port: str | None = pytestconfig.getoption("--port") or tc.find_port()
    if port is None:
        pytest.exit("No DIY-Thermocam found. Connect it, turn it on, or pass --port.", returncode=2)
    cam = tc.Thermocam(port)
    cam.start()
    metrics.fw_version = cam.get_fw_version()
    original = cam.get_config()
    yield cam
    try:
        for cmd, field in SETTINGS.items():
            cam.ack(cmd, getattr(original, field))
        assert cam.get_config() == original, "camera settings were not restored"
    finally:
        cam.end()
        cam.close()


@pytest.fixture
def fresh(cam: tc.Thermocam) -> tc.Thermocam:
    """The session camera with clean buffers and a current config."""
    cam.ser.reset_input_buffer()
    try:
        cam.config = cam.get_config()
        in_sync = cam.config.lepton_version in (tc.LEPTON_2_5, tc.LEPTON_3_5)
    except tc.ProtocolError:
        in_sync = False
    if not in_sync:
        # The camera handles one queued byte per frame; after a rejected payload it can be busy
        # answering leftover bytes for a minute. Later results would only repeat this failure.
        pytest.exit("Camera out of sync after the previous test. Fix the first failure, wait a minute, rerun.", returncode=1)
    return cam


class Metrics:
    def __init__(self, baseline: dict[str, float] | None):
        self.values: dict[str, float] = {}
        self.baseline = baseline
        self.fw_version: int = 0

    def record(self, name: str, value: float, higher_is_better: bool, tolerance: float = 0.15) -> None:
        """Store a measurement and, with --baseline, fail if it is more than `tolerance` worse."""
        self.values[name] = round(value, 3)
        if self.baseline is None or name not in self.baseline:
            return
        base = self.baseline[name]
        if higher_is_better:
            assert value >= base * (1 - tolerance), f"{name}: {value:.3f}, baseline {base:.3f}"
        else:
            assert value <= base * (1 + tolerance) + 0.002, f"{name}: {value:.3f}, baseline {base:.3f}"


@pytest.fixture(scope="session")
def metrics(pytestconfig: pytest.Config) -> Iterator[Metrics]:
    path: Path | None = pytestconfig.getoption("--baseline")
    baseline = json.loads(path.read_text())["metrics"] if path else None
    m = Metrics(baseline)
    yield m
    out: Path | None = pytestconfig.getoption("--save-metrics")
    if out:
        out.write_text(
            json.dumps(
                {
                    "date": time.strftime("%Y-%m-%d %H:%M"),
                    "firmware_version": m.fw_version,
                    "host": platform.platform(),
                    "metrics": m.values,
                },
                indent=2,
            )
            + "\n"
        )


@pytest.fixture
def ask(request: pytest.FixtureRequest) -> Callable[[str], None]:
    """Print an instruction to the person running --manual tests."""
    capman = request.config.pluginmanager.getplugin("capturemanager")

    def _ask(text: str) -> None:
        with capman.global_and_fixture_disabled():
            print(f"\n>>> {text}", flush=True)

    return _ask
