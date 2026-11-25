"""Throughput and latency. With --baseline they fail when clearly worse than the saved run."""

import statistics
import time

import thermocam as tc
from conftest import Metrics


def rate(fn, n: int) -> float:  # noqa: ANN001
    fn()  # first call may wait for a frame already in progress
    t = time.monotonic()
    for _ in range(n):
        fn()
    return n / (time.monotonic() - t)


def test_raw_frame_rate(fresh: tc.Thermocam, metrics: Metrics) -> None:
    fps = rate(fresh.raw_frame, 30)
    metrics.record("raw_fps", fps, higher_is_better=True, tolerance=0.1)
    assert fps > 5, f"{fps:.1f} raw frames per second"


def test_color_frame_rate(fresh: tc.Thermocam, metrics: Metrics) -> None:
    fps = rate(fresh.color_frame, 20)
    metrics.record("color_fps", fps, higher_is_better=True, tolerance=0.1)


def test_screen_frame_rate(fresh: tc.Thermocam, metrics: Metrics) -> None:
    """Filter, scaling, coloring and overlays on the camera, plus 150 kB over USB."""
    fps = rate(fresh.display_frame, 15)
    metrics.record("display_fps", fps, higher_is_better=True, tolerance=0.1)
    assert fps > 2, f"{fps:.1f} screen frames per second"


def test_command_latency(fresh: tc.Thermocam, metrics: Metrics) -> None:
    times = []
    for _ in range(20):
        t = time.monotonic()
        fresh.get_config()
        times.append(time.monotonic() - t)
    metrics.record("command_latency_s", statistics.median(times), higher_is_better=False, tolerance=0.5)
