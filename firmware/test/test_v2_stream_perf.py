"""Protocol v2.0 push streaming, concurrent control latency, perf telemetry & VoSPI integrity tests."""

from __future__ import annotations

import statistics
import time
from pathlib import Path

import pytest

import thermocam_v2 as v2

FIXTURES_DIR = Path(__file__).resolve().parent / "fixtures"


@pytest.fixture(scope="module")
def cam_v2() -> v2.ThermocamV2:
    port = v2.find_port()
    if port is None:
        pytest.exit("No DIY-Thermocam found on USB.", returncode=2)
    cam = v2.ThermocamV2(port)
    cam.hello(host_features=0)
    yield cam
    try:
        cam.stream_ctrl(0)
        cam.goodbye()
    finally:
        cam.close()


def test_v2_raw_push_stream_and_concurrent_commands(cam_v2: v2.ThermocamV2) -> None:
    cam_v2.get_perf(reset_counters=True)
    cam_v2.frames.clear()
    cam_v2.events.clear()
    cam_v2.record_traffic = True
    cam_v2.raw_log.clear()

    resp = cam_v2.stream_ctrl(mode=1, keep_lcd_on=0, include_pts=1, perf_interval_s=1)
    assert resp.status == v2.OK

    frames: list[v2.FrameRaw] = []
    cmd_rtts: list[float] = []
    t_start = time.monotonic()
    while len(frames) < 25 and (time.monotonic() - t_start) < 6.0:
        pkt = cam_v2.read_packet(timeout=0.5)
        if pkt is None:
            continue
        if pkt.msg_type == v2.MSG_FRAME_RAW:
            frames.append(v2.parse_frame_raw(pkt.payload))
            if len(frames) % 5 == 0:
                rtt_ms, _ = cam_v2.ping()
                cmd_rtts.append(rtt_ms)
        elif 0xC0 <= pkt.msg_type <= 0xC5:
            cam_v2.events.append(pkt)

    cam_v2.stream_ctrl(mode=0)
    cam_v2.record_traffic = False

    assert len(frames) >= 20, f"expected >=20 raw frames, got {len(frames)}"
    dt_s = (frames[-1].timestamp_us - frames[0].timestamp_us) / 1e6
    fps = (len(frames) - 1) / max(0.001, dt_s)
    med_rtt = statistics.median(cmd_rtts)
    perf = cam_v2.get_perf()

    print(f"\n[v2] RAW push stream: {fps:.2f} fps | concurrent PING median: {med_rtt:.2f} ms")
    print(f"[v2] Perf: {perf}")

    assert fps >= 7.5, f"RAW stream FPS too low: {fps:.2f}"
    assert med_rtt < 5.0, f"concurrent PING latency too high: {med_rtt:.2f} ms"
    assert perf["vospi_resyncs"] == 0, f"VoSPI resyncs detected: {perf['vospi_resyncs']}"
    assert all(f.min_raw < f.max_raw for f in frames)
    # Check the last 6 frames (avoids a single held frame right after a preceding FFC shutter)
    assert len({bytes(f.pixels) for f in frames[-6:]}) == 6, "duplicate raw frames detected"

    # Persist real captured Protocol v2 traffic fixture for Tauri desktop app tests
    FIXTURES_DIR.mkdir(parents=True, exist_ok=True)
    raw_packets = [b for b in cam_v2.raw_log if len(b) >= 20][:6]
    (FIXTURES_DIR / "sample_v2_stream.bin").write_bytes(b"".join(raw_packets))


def test_v2_rendered_stream_mode(cam_v2: v2.ThermocamV2) -> None:
    resp = cam_v2.stream_ctrl(mode=2, keep_lcd_on=0, include_pts=0, perf_interval_s=0)
    assert resp.status == v2.OK

    rendered_count = 0
    t0 = time.monotonic()
    while rendered_count < 8 and (time.monotonic() - t0) < 3.5:
        pkt = cam_v2.read_packet(timeout=0.5)
        if pkt is not None and pkt.msg_type == v2.MSG_FRAME_RENDERED:
            assert len(pkt.payload) == 16 + 320 * 240 * 2
            rendered_count += 1

    cam_v2.stream_ctrl(mode=0)
    assert rendered_count >= 5, f"expected >=5 rendered frames, got {rendered_count}"
