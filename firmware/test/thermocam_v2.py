"""USB Protocol v2.0 client for DIY-Thermocam V3 (matches docs/protocol-v2.md verbatim)."""

from __future__ import annotations

import struct
import time
import zlib
from array import array
from dataclasses import dataclass

import serial
import serial.tools.list_ports

MAGIC = b"TC2\x00"
PROTO_MAJOR = 2
PROTO_MINOR = 0
HEADER_SIZE = 16
TRAILER_SIZE = 4
MAX_PAYLOAD_LEN = 163840
BOOTLOADER_MAGIC = 0xB00710AD
TEENSY_USB_VID = 0x16C0

FLAG_ACK_REQ = 0x01
FLAG_ERROR = 0x02
FLAG_MORE_CHUNKS = 0x04
FLAG_COMPRESSED = 0x08

# Status codes
OK = 0x0000
ERR_BAD_CRC = 0x0001
ERR_UNKNOWN_MSG = 0x0002
ERR_INVALID_ARG = 0x0003
ERR_BUSY = 0x0004
ERR_IO = 0x0005
ERR_NOT_FOUND = 0x0006
ERR_FORBIDDEN = 0x0007
ERR_UNSUPPORTED = 0x0008
ERR_BAD_MAGIC = 0x0009
ERR_NO_SD = 0x000A

# Message IDs
MSG_HELLO_REQ = 0x01
MSG_GOODBYE_REQ = 0x02
MSG_PING_REQ = 0x03
MSG_STREAM_CTRL_REQ = 0x10
MSG_GET_CONFIG_REQ = 0x11
MSG_SET_CONFIG_REQ = 0x12
MSG_SET_TIME_REQ = 0x13
MSG_TRIGGER_FFC_REQ = 0x14
MSG_GET_TEMP_POINTS_REQ = 0x15
MSG_SET_TEMP_POINTS_REQ = 0x16
MSG_SAVE_CAPTURE_REQ = 0x17
MSG_GET_DIAGNOSTICS_REQ = 0x18
MSG_GET_PERF_REQ = 0x19
MSG_GET_RAD_PARAMS_REQ = 0x1A
MSG_SET_RAD_PARAMS_REQ = 0x1B

MSG_HELLO_RESP = 0x41
MSG_GOODBYE_RESP = 0x42
MSG_PONG_RESP = 0x43
MSG_STREAM_CTRL_RESP = 0x50
MSG_GET_CONFIG_RESP = 0x51
MSG_SET_CONFIG_RESP = 0x52
MSG_SET_TIME_RESP = 0x53
MSG_TRIGGER_FFC_RESP = 0x54
MSG_GET_TEMP_POINTS_RESP = 0x55
MSG_SET_TEMP_POINTS_RESP = 0x56
MSG_SAVE_CAPTURE_RESP = 0x57
MSG_GET_DIAGNOSTICS_RESP = 0x58
MSG_GET_PERF_RESP = 0x59
MSG_GET_RAD_PARAMS_RESP = 0x5A
MSG_SET_RAD_PARAMS_RESP = 0x5B

MSG_FRAME_RAW = 0x80
MSG_FRAME_RENDERED = 0x81

MSG_EVT_BUTTON = 0xC0
MSG_EVT_TOUCH = 0xC1
MSG_EVT_BATTERY = 0xC2
MSG_EVT_FFC = 0xC3
MSG_EVT_SESSION_EXIT = 0xC4
MSG_EVT_PERF = 0xC5

MSG_FS_INFO_REQ = 0xE0
MSG_FS_LIST_REQ = 0xE1
MSG_FS_STAT_REQ = 0xE2
MSG_FS_READ_REQ = 0xE3
MSG_FS_DELETE_REQ = 0xE4
MSG_FS_ABORT_REQ = 0xE5

MSG_FS_INFO_RESP = 0xE8
MSG_FS_LIST_RESP = 0xE9
MSG_FS_STAT_RESP = 0xEA
MSG_FS_READ_DATA = 0xEB
MSG_FS_DELETE_RESP = 0xEC
MSG_FS_ABORT_RESP = 0xED

MSG_ENTER_BOOTLOADER_REQ = 0xF0
MSG_ENTER_BOOTLOADER_RESP = 0xF1
MSG_GENERIC_ERROR_RESP = 0xFF


def crc8_smbus(data: bytes) -> int:
    c = 0
    for b in data:
        c ^= b
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if (c & 0x80) else (c << 1) & 0xFF
    return c


def header_crc8(hdr16: bytes | bytearray) -> int:
    return crc8_smbus(bytes(hdr16[:7]) + bytes(hdr16[8:16]))


def encode_packet(msg_type: int, seq: int, payload: bytes = b"", flags: int = 0, status: int = 0) -> bytes:
    hdr = bytearray(MAGIC + struct.pack("<BBBBHHI", PROTO_MAJOR, msg_type, flags, 0, seq, status, len(payload)))
    hdr[7] = header_crc8(hdr)
    body = bytes(hdr) + payload
    return body + struct.pack("<I", zlib.crc32(body) & 0xFFFFFFFF)


@dataclass
class Packet:
    msg_type: int
    flags: int
    seq: int
    status: int
    payload: bytes
    raw: bytes


@dataclass
class FrameRaw:
    frame_seq: int
    timestamp_us: int
    rtc_epoch: int
    dropped_frames: int
    width: int
    height: int
    pixel_format: int
    gain_mode: int
    ffc_state: int
    flags: int
    min_raw: int
    max_raw: int
    min_x: int
    min_y: int
    max_x: int
    max_y: int
    spot_temp_c: float
    cal_offset: float
    cal_slope: float
    fpa_temp_c: float
    bat_pct: int
    usb_power: int
    temp_points: list[tuple[int, int, int]]
    pixels: array[int]


def parse_frame_raw(payload: bytes) -> FrameRaw:
    (
        frame_seq, ts_us, rtc_epoch, dropped, w, h, pfmt, gain, ffc, flags,
        mn, mx, mnx, mny, mxx, mxy, spot_c, cal_off, cal_slp, fpa_c,
        bat_pct, usb_pwr, pt_cnt, _res,
    ) = struct.unpack("<IQIIHHBBBBHHHHHHffffbBH4s", payload[:64])
    pix_end = 64 + w * h * 2
    pixels = array("H")
    pixels.frombytes(payload[64:pix_end])
    pts = []
    if flags & 0x04:
        off = pix_end
        for _ in range(pt_cnt):
            pts.append(struct.unpack("<HHH", payload[off : off + 6]))
            off += 6
    return FrameRaw(
        frame_seq, ts_us, rtc_epoch, dropped, w, h, pfmt, gain, ffc, flags,
        mn, mx, mnx, mny, mxx, mxy, spot_c, cal_off, cal_slp, fpa_c,
        bat_pct, usb_pwr, pts, pixels,
    )


def find_port() -> str | None:
    for p in serial.tools.list_ports.comports():
        if p.vid == TEENSY_USB_VID:
            return p.device
    return None


class ThermocamV2:
    def __init__(self, port: str, timeout: float = 0.05):
        self.port = port
        self.ser = serial.Serial(port, 115200, timeout=timeout)
        self.ser.dtr = True
        self.ser.rts = False
        self._buf = bytearray()
        self._seq = 1
        self.frames: list[Packet] = []
        self.events: list[Packet] = []
        self.raw_log: list[bytes] = []
        self.record_traffic = False

    def close(self) -> None:
        if self.ser.is_open:
            self.ser.close()

    def next_seq(self) -> int:
        s = self._seq
        self._seq = (self._seq + 1) & 0xFFFF
        if self._seq == 0:
            self._seq = 1
        return s

    def send_raw(self, data: bytes) -> None:
        if self.record_traffic:
            self.raw_log.append(data)
        self.ser.write(data)
        self.ser.flush()

    def _align_to_magic(self) -> bool:
        idx = self._buf.find(MAGIC)
        if idx < 0:
            if len(self._buf) > 3:
                del self._buf[:-3]
            return False
        if idx > 0:
            del self._buf[:idx]
        return len(self._buf) >= HEADER_SIZE

    def _decode_aligned_packet(self, total: int) -> Packet | None:
        raw = bytes(self._buf[:total])
        del self._buf[:total]
        exp_crc = struct.unpack("<I", raw[-4:])[0]
        if (zlib.crc32(raw[:-4]) & 0xFFFFFFFF) != exp_crc:
            return None
        if self.record_traffic:
            self.raw_log.append(raw)
        msg_type, flags = raw[5], raw[6]
        seq, status = struct.unpack("<HH", raw[8:12])
        return Packet(msg_type, flags, seq, status, raw[16:-4], raw)

    def _pop_packet(self) -> Packet | None:
        while self._align_to_magic():
            ver = self._buf[4]
            plen = struct.unpack("<I", self._buf[12:16])[0]
            if ver != PROTO_MAJOR or plen > MAX_PAYLOAD_LEN or self._buf[7] != header_crc8(self._buf[:16]):
                del self._buf[:1]
                continue
            total = HEADER_SIZE + plen + TRAILER_SIZE
            if len(self._buf) < total:
                return None
            pkt = self._decode_aligned_packet(total)
            if pkt is not None:
                return pkt
        return None

    def read_packet(self, timeout: float = 2.0) -> Packet | None:
        deadline = time.monotonic() + timeout
        while True:
            pkt = self._pop_packet()
            if pkt is not None:
                return pkt
            if time.monotonic() >= deadline:
                return None
            waiting = self.ser.in_waiting
            chunk = self.ser.read(waiting if waiting > 0 else 1)
            if chunk:
                self._buf.extend(chunk)

    def request(self, msg_type: int, payload: bytes = b"", timeout: float = 2.5, flags: int = 0) -> Packet:
        seq = self.next_seq()
        self.send_raw(encode_packet(msg_type, seq, payload, flags=flags))
        deadline = time.monotonic() + timeout
        while True:
            rem = max(0.01, deadline - time.monotonic())
            pkt = self.read_packet(timeout=rem)
            if pkt is None:
                raise TimeoutError(f"timeout waiting for response to 0x{msg_type:02x} seq={seq}")
            if pkt.msg_type in (MSG_FRAME_RAW, MSG_FRAME_RENDERED):
                self.frames.append(pkt)
                continue
            if 0xC0 <= pkt.msg_type <= 0xC5:
                self.events.append(pkt)
                continue
            if pkt.seq == seq:
                return pkt

    def hello(self, host_features: int = 0) -> dict:
        self.ser.reset_input_buffer()
        self._buf.clear()
        p = self.request(MSG_HELLO_REQ, struct.pack("<BBHI", PROTO_MAJOR, PROTO_MINOR, 0, host_features))
        assert p.msg_type == MSG_HELLO_RESP and p.status == OK and len(p.payload) == 64
        vals = struct.unpack("<BBH32sBBHHHHBBQIH H", p.payload)
        return {
            "proto_major": vals[0],
            "proto_minor": vals[1],
            "fw_version": vals[2],
            "fw_build_str": vals[3].split(b"\x00", 1)[0].decode("utf-8", errors="replace"),
            "hw_version": vals[4],
            "lepton_type": vals[5],
            "sensor_w": vals[6],
            "sensor_h": vals[7],
            "display_w": vals[8],
            "display_h": vals[9],
            "diagnostic_mask": vals[10],
            "teensy_uid": vals[12],
            "capabilities": vals[13],
            "max_chunk_size": vals[14],
            "heartbeat_timeout_ms": vals[15],
        }

    def goodbye(self) -> None:
        p = self.request(MSG_GOODBYE_REQ)
        assert p.msg_type == MSG_GOODBYE_RESP and p.status == OK

    def ping(self) -> tuple[float, int]:
        t_us = int(time.monotonic() * 1_000_000)
        p = self.request(MSG_PING_REQ, struct.pack("<Q", t_us))
        assert p.msg_type == MSG_PONG_RESP and p.status == OK
        echo_us, dev_us = struct.unpack("<QQ", p.payload)
        assert echo_us == t_us
        return (time.monotonic() * 1_000_000 - t_us) / 1000.0, dev_us

    def stream_ctrl(self, mode: int, keep_lcd_on: int = 0, include_pts: int = 0,
                    perf_interval_s: int = 0, max_fps_x10: int = 0) -> Packet:
        return self.request(
            MSG_STREAM_CTRL_REQ,
            struct.pack("<BBBBHH", mode, keep_lcd_on, include_pts, perf_interval_s, max_fps_x10, 0),
        )

    def get_perf(self, reset_counters: bool = False, select_last_video: bool = False) -> dict:
        flags = (1 if reset_counters else 0) | (2 if select_last_video else 0)
        p = self.request(MSG_GET_PERF_REQ, bytes([flags]))
        assert p.msg_type == MSG_GET_PERF_RESP and p.status == OK and len(p.payload) == 84
        v = struct.unpack("<16I3H4BHfI", p.payload)
        return {
            "acquire_us": v[0], "stats_us": v[1], "filter_us": v[2], "scale_us": v[3],
            "colorize_us": v[4], "overlay_us": v[5], "display_us": v[6], "sd_write_us": v[7],
            "usb_tx_us": v[8], "loop_total_us": v[9], "frame_count": v[10], "frames_dropped": v[11],
            "vospi_resyncs": v[12], "vospi_crc_errors": v[13], "usb_rx_errors": v[14],
            "usb_crc_errors": v[15], "live_fps": v[16] / 100.0, "video_fps": v[17] / 100.0,
            "stream_fps": v[18] / 100.0, "perf_mode": v[19], "flags": v[20],
            "display_spi_mhz": v[21], "lepton_spi_mhz": v[22], "cpu_mhz": v[23],
            "mcu_temp_c": v[24], "frame_period_us": v[25],
        }

    def fs_read_file(self, path: str, chunk_size: int = 8192, timeout: float = 5.0) -> bytes:
        pb = path.encode("utf-8")
        seq = self.next_seq()
        self.send_raw(encode_packet(MSG_FS_READ_REQ, seq, struct.pack("<IIHH", 0, 0, chunk_size, len(pb)) + pb))
        out = bytearray()
        deadline = time.monotonic() + timeout
        while True:
            rem = max(0.05, deadline - time.monotonic())
            pkt = self.read_packet(timeout=rem)
            if pkt is None:
                raise TimeoutError(f"FS_READ timeout for {path}")
            if pkt.msg_type in (MSG_FRAME_RAW, MSG_FRAME_RENDERED):
                self.frames.append(pkt)
                continue
            if 0xC0 <= pkt.msg_type <= 0xC5:
                self.events.append(pkt)
                continue
            if pkt.seq != seq:
                continue
            if pkt.flags & FLAG_ERROR:
                raise RuntimeError(f"FS_READ error status=0x{pkt.status:04x}")
            off, dlen, _, _ = struct.unpack("<IHBB", pkt.payload[:8])
            assert off == len(out), f"chunk offset mismatch {off} != {len(out)}"
            out.extend(pkt.payload[8 : 8 + dlen])
            if not (pkt.flags & FLAG_MORE_CHUNKS):
                return bytes(out)
