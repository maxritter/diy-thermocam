"""USB serial client for the DIY-Thermocam V3 protocol (include/connection.h)."""

from __future__ import annotations

import struct
import time
from array import array
from dataclasses import dataclass

import serial
import serial.tools.list_ports

CMD_START = 0x64
CMD_END = 0xC8
CMD_INVALID = 0x00
CMD_GET_RAWLIMITS = 0x6E
CMD_GET_RAWDATA = 0x6F
CMD_GET_CONFIGDATA = 0x70
CMD_GET_CALIBDATA = 0x72
CMD_GET_SPOTTEMP = 0x73
CMD_GET_TEMPPOINTS = 0x75
CMD_SET_SHUTTERRUN = 0x78
CMD_SET_SHUTTERMODE = 0x79
CMD_SET_FILTERTYPE = 0x7A
CMD_GET_BATTERYSTATUS = 0x7C
CMD_GET_DIAGNOSTIC = 0x7F
CMD_GET_FWVERSION = 0x81
CMD_SET_LIMITS = 0x82
CMD_SET_TEXTCOLOR = 0x83
CMD_SET_COLORSCHEME = 0x84
CMD_SET_TEMPFORMAT = 0x85
CMD_SET_SHOWSPOT = 0x86
CMD_SET_SHOWCOLORBAR = 0x87
CMD_SET_SHOWMINMAX = 0x88
CMD_SET_TEMPPOINTS = 0x89
CMD_GET_HWVERSION = 0x8A
CMD_SET_ROTATION = 0x8B
CMD_FRAME_RAW = 0x96
CMD_FRAME_COLOR = 0x97
CMD_FRAME_DISPLAY = 0x98
CMD_FRAME_SAVE = 0x99

FRAME_CAPTURE_THERMAL = 0xB4
FRAME_CAPTURE_VIDEO = 0xB6
FRAME_NORMAL = 0xB7

LEPTON_2_5 = 0
LEPTON_3_5 = 1

TEENSY_USB_VID = 0x16C0


class ProtocolError(AssertionError):
    pass


@dataclass(frozen=True)
class Config:
    lepton_version: int
    rotation: int
    color_scheme: int
    temp_format: int
    spot: int
    colorbar: int
    min_max: int
    text_color: int
    filter_type: int
    limits_adjustable: int

    @classmethod
    def parse(cls, data: bytes) -> Config:
        return cls(*data)


@dataclass
class RawFrame:
    pixels: array[int]  # uint16, 160x120 (Lepton 3.5) or 80x60 (Lepton 2.5)
    width: int
    height: int
    min_raw: int
    max_raw: int
    spot_temp: float
    cal_offset: float
    cal_slope: float

    def to_celsius(self, raw: float) -> float:
        return raw * self.cal_slope + self.cal_offset


def find_port() -> str | None:
    """The first Teensy serial port, or None."""
    for p in serial.tools.list_ports.comports():
        if p.vid == TEENSY_USB_VID:
            return p.device
    return None


def _u16_be(data: bytes) -> array[int]:
    values = array("H", data)
    values.byteswap()  # firmware sends MSB first; array uses native (little) endian
    return values


class Thermocam:
    def __init__(self, port: str, timeout: float = 3.0):
        self.port = port
        self.ser = serial.Serial(port, 115200, timeout=timeout)
        self.config: Config | None = None

    def close(self) -> None:
        self.ser.close()

    # --- low level -------------------------------------------------------

    def send(self, *values: int) -> None:
        self.ser.write(bytes(values))
        self.ser.flush()

    def read(self, n: int) -> bytes:
        data = self.ser.read(n)
        if len(data) != n:
            raise ProtocolError(f"expected {n} bytes, got {len(data)}")
        return data

    def ack(self, cmd: int, *args: int) -> int:
        """Send a command and return the one-byte answer."""
        self.send(cmd, *args)
        return self.read(1)[0]

    # --- session ---------------------------------------------------------

    def start(self, wait: float = 6.0) -> None:
        """Enter serial mode. From live mode the camera shows a message for 1 s first."""
        self.ser.reset_input_buffer()
        self.send(CMD_START)
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline:
            b = self.ser.read(1)
            if b == bytes([CMD_START]):
                self.ser.reset_input_buffer()
                self.config = self.get_config()
                return
        raise ProtocolError("no start acknowledgement")

    def end(self, wait: float = 6.0) -> bool:
        """Leave serial mode; True once the camera confirms it is back in live mode."""
        self.send(CMD_END)
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline:
            if self.ser.read(1) == bytes([CMD_END]):
                return True
        return False

    def wait_for_byte(self, value: int, wait: float) -> bool:
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline:
            if self.ser.read(1) == bytes([value]):
                return True
        return False

    # --- queries ---------------------------------------------------------

    def get_config(self) -> Config:
        self.send(CMD_GET_CONFIGDATA)
        return Config.parse(self.read(10))

    def get_fw_version(self) -> int:
        self.send(CMD_GET_FWVERSION)
        return int.from_bytes(self.read(2), "big")

    def get_hw_version(self) -> int:
        return self.ack(CMD_GET_HWVERSION)

    def get_diagnostic(self) -> int:
        return self.ack(CMD_GET_DIAGNOSTIC)

    def get_battery(self) -> int:
        return struct.unpack("b", bytes([self.ack(CMD_GET_BATTERYSTATUS)]))[0]

    def get_spot_temp(self) -> float:
        self.send(CMD_GET_SPOTTEMP)
        return struct.unpack("<f", self.read(4))[0]

    def get_calibration(self) -> tuple[float, float]:
        self.send(CMD_GET_CALIBDATA)
        return struct.unpack("<ff", self.read(8))

    def get_raw_limits(self) -> tuple[int, int]:
        self.send(CMD_GET_RAWLIMITS)
        return struct.unpack(">HH", self.read(4))

    def get_temp_points(self) -> list[tuple[int, int]]:
        self.send(CMD_GET_TEMPPOINTS)
        data = self.read(384)
        return [struct.unpack(">HH", data[i : i + 4]) for i in range(0, 384, 4)]

    def set_temp_points(self, points: list[tuple[int, int]], pieces: int = 1, gap: float = 0.0) -> int:
        """Send 96 (index, value) points, optionally split into pieces with a gap between."""
        assert len(points) == 96
        payload = b"".join(struct.pack(">HH", i, v) for i, v in points)
        self.send(CMD_SET_TEMPPOINTS)
        step = len(payload) // pieces
        for k in range(pieces):
            time.sleep(gap)
            self.ser.write(payload[k * step : (k + 1) * step])
            self.ser.flush()
        return self.read(1)[0]

    # --- frames ----------------------------------------------------------

    def _raw_shape(self, color: bool) -> tuple[int, int]:
        assert self.config is not None
        if self.config.lepton_version == LEPTON_2_5 and not color:
            return 80, 60
        return 160, 120

    def request_frame(self, cmd: int) -> int:
        """Send a frame command and return its status byte. Only FRAME_NORMAL is followed by data;
        FRAME_CAPTURE_* means the camera's button asked the PC to save an image or a video."""
        self.ser.reset_input_buffer()
        return self.ack(cmd)

    def _expect_normal(self, cmd: int) -> None:
        status = self.request_frame(cmd)
        if status != FRAME_NORMAL:
            raise ProtocolError(f"frame command 0x{cmd:02x} answered 0x{status:02x}")

    def _read_raw(self, color: bool) -> RawFrame:
        w, h = self._raw_shape(color)
        pixels = _u16_be(self.read(w * h * 2))
        mn, mx = struct.unpack(">HH", self.read(4))
        spot, off, slope = struct.unpack("<fff", self.read(12))
        return RawFrame(pixels, w, h, mn, mx, spot, off, slope)

    def raw_frame(self) -> RawFrame:
        self._expect_normal(CMD_FRAME_RAW)
        return self._read_raw(color=False)

    def color_frame(self) -> RawFrame:
        """Pixels are RGB565 after the camera's filter and color scheme."""
        self._expect_normal(CMD_FRAME_COLOR)
        return self._read_raw(color=True)

    def display_frame(self) -> array[int]:
        """The 320x240 RGB565 screen image the camera would show, overlays included."""
        self._expect_normal(CMD_FRAME_DISPLAY)
        return _u16_be(self.read(320 * 240 * 2))
