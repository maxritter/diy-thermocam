"""Protocol v2.0 SD filesystem, capture speed, FFC & Lepton CCI radiometric params tests."""

from __future__ import annotations

import struct
import time
import zlib
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


def test_v2_lepton_cci_rad_params_roundtrip(cam_v2: v2.ThermocamV2) -> None:
    """A4: Verify Lepton CCI 0x4EBC/0x4EBD emissivity & T_bkg roundtrip on real Lepton 3.5."""
    orig = cam_v2.request(v2.MSG_GET_RAD_PARAMS_REQ)
    assert orig.status == v2.OK and len(orig.payload) == 18
    v0 = struct.unpack("<8HBB", orig.payload)
    assert (v0[8] & 0x01) != 0, "RAD params should report supported=1"

    # Set emissivity=0.95 (7782), T_bkg=25.00 C (29815), RAM-only (flags=0)
    req = struct.pack("<8HBB", 7782, 29815, 8192, 29515, 8192, 29515, 0, 29515, 0, 0)
    resp = cam_v2.request(v2.MSG_SET_RAD_PARAMS_REQ, req)
    assert resp.status == v2.OK
    v1 = struct.unpack("<8HBB", resp.payload)
    assert v1[0] == 7782 and v1[1] == 29815

    # Invalid tau_window + refl_window > 8192 must return ERR_INVALID_ARG
    bad = struct.pack("<8HBB", 8192, 29515, 5000, 29515, 8192, 29515, 4000, 29515, 0, 0)
    r_bad = cam_v2.request(v2.MSG_SET_RAD_PARAMS_REQ, bad)
    assert (r_bad.flags & v2.FLAG_ERROR) and r_bad.status == v2.ERR_INVALID_ARG

    # Reset to factory defaults (flags bit 1)
    rst = struct.pack("<8HBB", 0, 0, 0, 0, 0, 0, 0, 0, 0x02, 0)
    r_rst = cam_v2.request(v2.MSG_SET_RAD_PARAMS_REQ, rst)
    assert r_rst.status == v2.OK
    v2_vals = struct.unpack("<8HBB", r_rst.payload)
    assert v2_vals[0] == 8192 and v2_vals[2] == 8192 and v2_vals[4] == 8192 and v2_vals[6] == 0


def test_v2_trigger_ffc(cam_v2: v2.ThermocamV2) -> None:
    cam_v2.events.clear()
    p = cam_v2.request(v2.MSG_TRIGGER_FFC_REQ, bytes([0x01]), timeout=4.0)
    assert p.status == v2.OK and p.msg_type == v2.MSG_TRIGGER_FFC_RESP
    ffc_evts = [e for e in cam_v2.events if e.msg_type == v2.MSG_EVT_FFC]
    assert len(ffc_evts) >= 2


def test_v2_save_capture_and_sd_fs_roundtrip(cam_v2: v2.ThermocamV2) -> None:
    # Root-jail enforcement
    for bad_path in (b"/../foo", b"rel.DAT", b"/a\\b"):
        r = cam_v2.request(v2.MSG_FS_STAT_REQ, struct.pack("<BBH", 0, 0, len(bad_path)) + bad_path)
        assert r.status == v2.ERR_FORBIDDEN
    r_root = cam_v2.request(v2.MSG_FS_DELETE_REQ, struct.pack("<BBH", 0, 0, 1) + b"/")
    assert r_root.status == v2.ERR_FORBIDDEN

    # Save fresh radiometric .DAT capture (override_convert_setting=1, save_bmp=0)
    t0 = time.monotonic()
    cap = cam_v2.request(v2.MSG_SAVE_CAPTURE_REQ, bytes([0x02, 0, 0, 0]), timeout=3.0)
    save_ms = (time.monotonic() - t0) * 1000.0
    assert cap.status == v2.OK and len(cap.payload) == 40
    dat_size, bmp_size = struct.unpack("<II", cap.payload[:8])
    stem = cap.payload[8:40].split(b"\x00", 1)[0].decode("ascii")
    print(f"\n[v2] SAVE_CAPTURE /{stem}.DAT ({dat_size} B) in {save_ms:.1f} ms")
    assert dat_size in (10005, 38805) and bmp_size == 0
    assert save_ms < 120.0, f"DAT save slower than expected: {save_ms:.1f} ms"

    # FS_INFO
    info = cam_v2.request(v2.MSG_FS_INFO_REQ)
    assert info.status == v2.OK and info.payload[0] == 1

    # FS_LIST root
    pb_root = b"/"
    lst = cam_v2.request(v2.MSG_FS_LIST_REQ, struct.pack("<IHH", 0, 64, len(pb_root)) + pb_root)
    assert lst.status == v2.OK and f"{stem}.DAT".encode("ascii") in lst.payload

    # FS_STAT with CRC-32
    path = f"/{stem}.DAT"
    pb = path.encode("ascii")
    st = cam_v2.request(v2.MSG_FS_STAT_REQ, struct.pack("<BBH", 1, 0, len(pb)) + pb)
    assert st.status == v2.OK
    sz, _mtime, fcrc, is_dir, crc_ok = struct.unpack("<IIIBB", st.payload[:14])
    assert sz == dat_size and is_dir == 0 and crc_ok == 1

    # FS_READ full file + verify byte-exact CRC-32 + save real V3 .DAT fixture
    t1 = time.monotonic()
    data = cam_v2.fs_read_file(path)
    read_ms = (time.monotonic() - t1) * 1000.0
    print(f"[v2] FS_READ {path}: {len(data)} B in {read_ms:.1f} ms ({len(data)/1024/max(0.001, read_ms/1000):.0f} KB/s)")
    assert len(data) == dat_size
    assert (zlib.crc32(data) & 0xFFFFFFFF) == fcrc

    FIXTURES_DIR.mkdir(parents=True, exist_ok=True)
    (FIXTURES_DIR / "sample_real_v3.DAT").write_bytes(data)

    # FS_DELETE cleanup
    dl = cam_v2.request(v2.MSG_FS_DELETE_REQ, struct.pack("<BBH", 0, 0, len(pb)) + pb)
    assert dl.status == v2.OK and struct.unpack("<I", dl.payload)[0] == 1
