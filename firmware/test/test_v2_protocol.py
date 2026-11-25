"""Protocol v2.0 handshake, diagnostics, config, temp points & error-handling hardware tests."""

from __future__ import annotations

import re
import struct
import time
from pathlib import Path

import pytest

import thermocam_v2 as v2

FIRMWARE = Path(__file__).resolve().parent.parent


@pytest.fixture(scope="module")
def cam_v2() -> v2.ThermocamV2:
    port = v2.find_port()
    if port is None:
        pytest.exit("No DIY-Thermocam found on USB.", returncode=2)
    cam = v2.ThermocamV2(port)
    cam.hello(host_features=0)
    # Save original DeviceConfig to restore after tests
    orig_cfg = cam.request(v2.MSG_GET_CONFIG_REQ).payload
    yield cam
    try:
        cam.stream_ctrl(0)
        cam.request(v2.MSG_SET_CONFIG_REQ, bytes([0, 0, 0, 0]) + orig_cfg)
        cam.request(v2.MSG_SET_TEMP_POINTS_REQ, struct.pack("<H", 0))
        cam.goodbye()
    finally:
        cam.close()


def test_v2_hello_and_version(cam_v2: v2.ThermocamV2) -> None:
    info = cam_v2.hello(host_features=0)
    src = (FIRMWARE / "src/general/globalvariables.cpp").read_text()
    expected_fw = int(re.search(r"uint16_t fwVersion = (\d+);", src)[1])  # type: ignore[index]
    assert info["proto_major"] == 2 and info["proto_minor"] == 0
    assert info["fw_version"] == expected_fw
    assert info["hw_version"] == 3
    assert info["diagnostic_mask"] == 0xFF
    assert info["sensor_w"] in (80, 160)
    assert info["display_w"] == 320 and info["display_h"] == 240
    assert info["teensy_uid"] != 0
    assert info["capabilities"] & 0x003F == 0x003F  # raw, display, sd, perf, boot, rad


def test_v2_ping_latency(cam_v2: v2.ThermocamV2) -> None:
    rtts = [cam_v2.ping()[0] for _ in range(20)]
    rtts.sort()
    median_ms = rtts[len(rtts) // 2]
    assert median_ms < 5.0, f"median PING RTT too high: {median_ms:.2f} ms"


def test_v2_diagnostics(cam_v2: v2.ThermocamV2) -> None:
    p = cam_v2.request(v2.MSG_GET_DIAGNOSTICS_REQ)
    assert p.msg_type == v2.MSG_GET_DIAGNOSTICS_RESP and p.status == v2.OK and len(p.payload) == 80
    mask, bat, usb, sd_fs, bat_mv, cpu_mhz, mcu_c, sd_tot, sd_free, up_us, dtcm, ocram, fpa_c = struct.unpack(
        "<BbBBHHfIIQIIf", p.payload[:40]
    )
    part = p.payload[40:72].split(b"\x00", 1)[0].decode("ascii", errors="replace")
    assert mask == 0xFF
    assert -1 <= bat <= 100
    assert cpu_mhz == 450
    assert 10.0 < mcu_c < 90.0
    assert sd_fs in (1, 2, 3) and sd_tot > 0
    assert up_us > 0 and dtcm > 20000 and ocram > 50000
    assert "05-070" in part


def test_v2_config_atomic_validation_and_roundtrip(cam_v2: v2.ThermocamV2) -> None:
    orig = bytearray(cam_v2.request(v2.MSG_GET_CONFIG_REQ).payload)
    assert len(orig) == 32 and orig[0] == 1

    # Valid RAM-only update (persist=0)
    mod = bytearray(orig)
    mod[1] = 10  # ironblack
    mod[4] = 2   # box filter
    p = cam_v2.request(v2.MSG_SET_CONFIG_REQ, bytes([0, 0, 0, 0]) + bytes(mod))
    assert p.msg_type == v2.MSG_SET_CONFIG_RESP and p.status == v2.OK
    assert p.payload[1] == 10 and p.payload[4] == 2

    # Invalid color scheme (19) must fail atomically without mutating state
    bad = bytearray(mod)
    bad[1] = 19
    p_bad = cam_v2.request(v2.MSG_SET_CONFIG_REQ, bytes([0, 0, 0, 0]) + bytes(bad))
    assert (p_bad.flags & v2.FLAG_ERROR) and p_bad.status == v2.ERR_INVALID_ARG
    after = cam_v2.request(v2.MSG_GET_CONFIG_REQ).payload
    assert after[1] == 10 and after[4] == 2

    # Restore original
    p_rest = cam_v2.request(v2.MSG_SET_CONFIG_REQ, bytes([0, 0, 0, 0]) + bytes(orig))
    assert p_rest.status == v2.OK


def test_v2_temp_points_and_time_sync(cam_v2: v2.ThermocamV2) -> None:
    # Set 2 sparse temp points
    pts_payload = struct.pack("<HHHHH", 2, 40, 30, 120, 90)
    p = cam_v2.request(v2.MSG_SET_TEMP_POINTS_REQ, pts_payload)
    assert p.status == v2.OK and struct.unpack("<H", p.payload)[0] == 2

    p_get = cam_v2.request(v2.MSG_GET_TEMP_POINTS_REQ)
    assert p_get.status == v2.OK
    cnt = struct.unpack("<H", p_get.payload[:2])[0]
    assert cnt == 2
    x0, y0, r0 = struct.unpack("<HHH", p_get.payload[2:8])
    x1, y1, r1 = struct.unpack("<HHH", p_get.payload[8:14])
    assert (x0, y0) == (40, 30) and (x1, y1) == (120, 90)
    assert r0 > 0 and r1 > 0

    # Clear points
    assert cam_v2.request(v2.MSG_SET_TEMP_POINTS_REQ, struct.pack("<H", 0)).status == v2.OK

    # RTC sync
    now_s = int(time.time())
    p_t = cam_v2.request(v2.MSG_SET_TIME_REQ, struct.pack("<I", now_s))
    assert p_t.status == v2.OK
    got_s = struct.unpack("<I", p_t.payload)[0]
    assert abs(got_s - now_s) <= 2


def test_v2_error_handling_and_bad_bootloader_magic(cam_v2: v2.ThermocamV2) -> None:
    # Unknown msg_type -> GENERIC_ERROR_RESP(ERR_UNKNOWN_MSG)
    p_unk = cam_v2.request(0x3E)
    assert p_unk.msg_type == v2.MSG_GENERIC_ERROR_RESP
    assert (p_unk.flags & v2.FLAG_ERROR) and p_unk.status == v2.ERR_UNKNOWN_MSG

    # Bad CRC-32 -> GENERIC_ERROR_RESP(ERR_BAD_CRC) with echoed seq
    seq = cam_v2.next_seq()
    bad_pkt = bytearray(v2.encode_packet(v2.MSG_PING_REQ, seq, struct.pack("<Q", 1234)))
    bad_pkt[-1] ^= 0xFF
    cam_v2.send_raw(bytes(bad_pkt))
    p_crc = cam_v2.read_packet(timeout=1.5)
    assert p_crc is not None and p_crc.seq == seq and p_crc.status == v2.ERR_BAD_CRC

    # Wrong bootloader magic must return ERR_BAD_MAGIC without rebooting!
    p_boot = cam_v2.request(v2.MSG_ENTER_BOOTLOADER_REQ, struct.pack("<I", 0xDEADBEEF))
    assert (p_boot.flags & v2.FLAG_ERROR) and p_boot.status == v2.ERR_BAD_MAGIC
    # Camera still responsive
    assert cam_v2.ping()[0] < 10.0
