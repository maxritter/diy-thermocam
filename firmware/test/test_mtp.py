"""Automated USB MTP responder verification (GetDeviceInfo, OpenSession, GetStorageIDs/Info, GetObjectHandles).

Run while the camera is in USB File Transfer (MTP) mode:
    pytest -v test_mtp.py
"""

from __future__ import annotations

import platform
import struct
import subprocess
import time

import pytest

VID_TEENSY = 0x16C0
PID_SERIAL_MTP = 0x04D5
MTP_RESPONSE_OK = 0x2001

pytestmark = pytest.mark.manual


def _mtp_cmd(ep_out, ep_in, op: int, tid: int, params: tuple[int, ...] = ()) -> tuple[int, bytes]:
    pkt = struct.pack(f"<IHHI{'I' * len(params)}", 12 + 4 * len(params), 1, op, tid, *params)
    ep_out.write(pkt, timeout=2000)
    data = b""
    while True:
        raw = bytes(ep_in.read(16384, timeout=2000))
        _length, ctype, code, _rtid = struct.unpack("<IHHI", raw[:12])
        if ctype == 2:
            data = raw[12:]
        elif ctype == 3:
            return code, data


def test_mtp_storage_and_root_listing() -> None:
    usb_core = pytest.importorskip("usb.core")
    usb_util = pytest.importorskip("usb.util")

    if platform.system() == "Darwin":
        # macOS ptpcamerad exclusively claims bInterfaceClass=6 (Still Image/MTP) on connect
        subprocess.run(["pkill", "-9", "ptpcamerad"], stderr=subprocess.DEVNULL, check=False)
        time.sleep(0.1)

    dev = usb_core.find(idVendor=VID_TEENSY, idProduct=PID_SERIAL_MTP)
    if dev is None:
        pytest.skip("Teensy MTP device (16c0:04d5) not connected")

    cfg = dev.get_active_configuration()
    mtp_intf = next((i for i in cfg if i.bInterfaceClass == 6), None)
    assert mtp_intf is not None, "MTP interface (bInterfaceClass=6) missing from USB descriptor"

    ep_out = usb_util.find_descriptor(
        mtp_intf,
        custom_match=lambda e: usb_util.endpoint_direction(e.bEndpointAddress) == usb_util.ENDPOINT_OUT
        and usb_util.endpoint_type(e.bmAttributes) == usb_util.ENDPOINT_TYPE_BULK,
    )
    ep_in = usb_util.find_descriptor(
        mtp_intf,
        custom_match=lambda e: usb_util.endpoint_direction(e.bEndpointAddress) == usb_util.ENDPOINT_IN
        and usb_util.endpoint_type(e.bmAttributes) == usb_util.ENDPOINT_TYPE_BULK,
    )
    assert ep_out is not None and ep_in is not None, "Missing MTP bulk endpoints"

    usb_util.claim_interface(dev, mtp_intf.bInterfaceNumber)
    try:
        code, _ = _mtp_cmd(ep_out, ep_in, 0x1001, 1)  # GetDeviceInfo
        assert code == MTP_RESPONSE_OK, f"GetDeviceInfo failed: 0x{code:04x}"

        code, _ = _mtp_cmd(ep_out, ep_in, 0x1002, 2, (1,))  # OpenSession
        assert code == MTP_RESPONSE_OK, f"OpenSession failed: 0x{code:04x}"

        code, storage_bytes = _mtp_cmd(ep_out, ep_in, 0x1004, 3)  # GetStorageIDs
        assert code == MTP_RESPONSE_OK, f"GetStorageIDs failed: 0x{code:04x}"
        assert len(storage_bytes) >= 8, "No MTP storage reported (check SD.begin(BUILTIN_SDCARD))"
        count = struct.unpack("<I", storage_bytes[:4])[0]
        assert count >= 1, "GetStorageIDs returned 0 storages"
        sid = struct.unpack("<I", storage_bytes[4:8])[0]

        code, info = _mtp_cmd(ep_out, ep_in, 0x1005, 4, (sid,))  # GetStorageInfo
        assert code == MTP_RESPONSE_OK, f"GetStorageInfo failed: 0x{code:04x}"
        _st, _fs, _ac, max_cap, free_sp, _free_obj = struct.unpack("<HHHQQI", info[:26])
        assert max_cap > 0, "MTP reported 0 capacity (SDClass::cardPreviouslyPresent not set)"
        assert free_sp <= max_cap
        desc_len = info[26]
        desc = info[27 : 27 + desc_len * 2].decode("utf-16-le").rstrip("\x00")
        assert desc == "Thermocam SD", f"unexpected MTP storage name: {desc!r}"

        code, handles = _mtp_cmd(ep_out, ep_in, 0x1007, 5, (sid, 0, 0xFFFFFFFF))  # GetObjectHandles
        assert code == MTP_RESPONSE_OK, f"GetObjectHandles failed: 0x{code:04x}"
        assert len(handles) >= 4
        _mtp_cmd(ep_out, ep_in, 0x1003, 6)  # CloseSession
    finally:
        usb_util.release_interface(dev, mtp_intf.bInterfaceNumber)
