"""Printed surface detail for the render props: PCB silkscreen, chip markings and the LiPo label.

Each decal is drawn the way a person sees that face (a "view canvas"), then resampled into the
device frame: Blender maps every textured part with planar UVs over its own XY bounding box
(u along +X, v along +Y), so the PNG written here is that XY map. `view` names the device axes
that point right and down on the canvas, e.g. ("+Y", "-X") for the Teensy's component side.
Layouts follow the vendor photos (PJRC Teensy 4.1, FLIR Breakout V2.0, BuyDisplay ER-TFTM032-3).
The main board needs none: it comes from KiCad with its own silkscreen.
"""

from __future__ import annotations

import random
from collections.abc import Callable
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from .params import V4_PARAMS

FONT_SANS = ("/System/Library/Fonts/Helvetica.ttc", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf")
FONT_MONO = ("/System/Library/Fonts/Menlo.ttc", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf")

# sRGB colours sampled from the vendor photos
TEENSY_GREEN, TEENSY_TRACE = (38, 122, 44), (50, 136, 52)
BREAKOUT_GREEN, BREAKOUT_TRACE = (30, 88, 38), (42, 104, 50)
SILK, GOLD, COPPER = (238, 240, 236), (214, 172, 96), (196, 120, 70)
CHIP, CHIP_INK = (26, 26, 28), (128, 128, 130)
DISPLAY_BLACK, DISPLAY_TRACE = (18, 18, 20), (30, 30, 33)


def _font(candidates: tuple[str, ...], px: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    for path in candidates:
        if Path(path).exists():
            try:
                return ImageFont.truetype(path, px, index=1 if bold and path.endswith(".ttc") else 0)
            except OSError:
                continue
    return ImageFont.load_default(px)


def _text(img: Image.Image, xy: tuple[float, float], text: str, px: int, fill: tuple[int, int, int],
          angle: float = 0.0, mono: bool = False, bold: bool = False) -> None:
    """Draw `text` centred on `xy`, rotated `angle` degrees counter-clockwise."""
    font = _font(FONT_MONO if mono else FONT_SANS, px, bold)
    l, t, r, b = (int(v) for v in font.getbbox(text))
    tile = Image.new("RGBA", (r - l + 4, b - t + 4), (0, 0, 0, 0))
    ImageDraw.Draw(tile).text((2 - l, 2 - t), text, font=font, fill=(*fill, 255))
    if angle:
        tile = tile.rotate(angle, expand=True, resample=Image.Resampling.BICUBIC)
    img.alpha_composite(tile, (round(xy[0] - tile.width / 2), round(xy[1] - tile.height / 2)))


def _traces(d: ImageDraw.ImageDraw, rng: random.Random, s: float, box: tuple[float, float, float, float],
            n: int, fill: tuple[int, int, int], width_mm: float) -> None:
    """Copper routed like a real board: straight runs at 0/90 degrees joined by short 45 degree bends."""
    u0, v0, u1, v1 = box
    dirs = [(1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1)]
    for _ in range(n):
        u, v = rng.uniform(u0, u1), rng.uniform(v0, v1)
        k = rng.choice((0, 2, 4, 6))
        pts = [(u, v)]
        for _ in range(rng.randint(2, 4)):
            du, dv = dirs[k]
            length = rng.uniform(0.6, 1.4) if k % 2 else rng.uniform(1.5, 7.0)  # Short 45 degree bends
            norm = float(du * du + dv * dv) ** 0.5
            u = min(max(u + du / norm * length, u0), u1)
            v = min(max(v + dv / norm * length, v0), v1)
            pts.append((u, v))
            k = (k + rng.choice((-1, 1))) % 8
        d.line([(x * s, y * s) for x, y in pts], fill=fill, width=max(1, round(width_mm * s)), joint="curve")


# ---------------------------------------------------------------------------------------------
# Decal painters: each draws on a canvas of `w_mm` x `h_mm` at `s` px/mm (canvas x right, y down)
# ---------------------------------------------------------------------------------------------

TEENSY_TOP_ROW = ["5V", "G", "3V", "23", "22", "21", "20", "19", "18", "17", "16", "15", "14", "13", "G",
                  "41", "40", "39", "38", "37", "36", "35", "34", "33"]
TEENSY_BOTTOM_ROW = ["G", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "3V",
                     "24", "25", "26", "27", "28", "29", "30", "31", "32"]
TEENSY_PITCH, TEENSY_PIN1_U, TEENSY_ROW_V = 2.54, 1.32, (1.27, 16.51)


def teensy_component_side(img: Image.Image, s: float) -> None:
    """Teensy 4.1 top as in PJRC's photo: USB at the left, the 5V/G/3V row along the top edge."""
    d = ImageDraw.Draw(img)
    rng = random.Random(41)
    w, h = img.width / s, img.height / s
    # Copper under the solder mask, a shade lighter than the board
    _traces(d, rng, s, (2.0, 2.4, w - 12.5, h - 2.4), 60, TEENSY_TRACE, 0.15)
    for _ in range(40):  # Tented vias
        u, v = rng.uniform(3, w - 13), rng.uniform(2.5, h - 2.5)
        r = 0.22 * s
        d.ellipse((u * s - r, v * s - r, u * s + r, v * s + r), fill=TEENSY_TRACE)
    # Pin labels, angled like the real board, just inside each pad row
    px = round(0.95 * s)
    for i, (top, bot) in enumerate(zip(TEENSY_TOP_ROW, TEENSY_BOTTOM_ROW)):
        u = TEENSY_PIN1_U + i * TEENSY_PITCH
        _text(img, ((u + 0.15) * s, 2.85 * s), top, px, SILK, angle=40, bold=True)
        _text(img, ((u + 0.15) * s, (h - 2.85) * s), bot, px, SILK, angle=40, bold=True)
    lw = max(1, round(0.13 * s))
    d.rectangle((13.15 * s, 2.55 * s, 17.45 * s, 8.95 * s), outline=SILK, width=lw)  # Ethernet 2x3
    d.rectangle((6.40 * s, 12.30 * s, 18.95 * s, 14.70 * s), outline=SILK, width=lw)  # USB host 1x5
    d.line((8.85 * s, 12.30 * s, 8.85 * s, 14.70 * s), fill=SILK, width=lw)
    d.rectangle((19.75 * s, 9.35 * s, 22.45 * s, 14.05 * s), outline=SILK, width=lw)  # Bulk capacitor
    # Copper test pads near the USB end
    for u, v in ((2.45, 2.65), (2.45, 15.25)):
        r = 0.42 * s
        d.ellipse((u * s - r, v * s - r, u * s + r, v * s + r), fill=COPPER)


def chip_mcu(img: Image.Image, s: float) -> None:
    """i.MX RT1062 laser marking, reading bottom to top as on the Teensy."""
    d = ImageDraw.Draw(img)
    w, h = img.width / s, img.height / s
    lines = ["MIMXRT1062", "DVJ6A", "1N00X", "CTEW2208H"]
    for i, line in enumerate(lines):
        _text(img, ((w / 2 - 2.6 + i * 1.75) * s, h / 2 * s), line, round(1.25 * s), CHIP_INK, angle=90, mono=True)
    r = 0.35 * s
    d.ellipse((1.2 * s - r, (h - 1.2) * s - r, 1.2 * s + r, (h - 1.2) * s + r), fill=(48, 48, 50))


def chip_small(lines: list[str]) -> Callable[[Image.Image, float], None]:
    def paint(img: Image.Image, s: float) -> None:
        w, h = img.width / s, img.height / s
        px = round(min(0.62, 0.9 * h / (len(lines) + 1)) * s)
        for i, line in enumerate(lines):
            _text(img, (w / 2 * s, (h / 2 + (i - (len(lines) - 1) / 2) * px / s * 1.25) * s), line, px, CHIP_INK, mono=True)
        r = 0.18 * s
        d = ImageDraw.Draw(img)
        d.ellipse((0.5 * s - r, 0.5 * s - r, 0.5 * s + r, 0.5 * s + r), fill=(52, 52, 54))

    return paint


def breakout_lepton_side(img: Image.Image, s: float) -> None:
    """FLIR Breakout V2.0, Lepton side: header pads on the right, socket left of centre."""
    d = ImageDraw.Draw(img)
    rng = random.Random(35)
    w, h = img.width / s, img.height / s
    _traces(d, rng, s, (1.5, 1.5, w - 8.0, h - 1.5), 40, BREAKOUT_TRACE, 0.18)
    for _ in range(55):  # Gold vias, kept off the socket
        u, v = rng.uniform(1.8, w - 8.5), rng.uniform(1.8, h - 1.8)
        if abs(u - 10.0) < 6.8 and abs(v - h / 2) < 6.8:
            continue
        r = rng.choice((0.28, 0.32, 0.45)) * s
        d.ellipse((u * s - r, v * s - r, u * s + r, v * s + r), fill=GOLD)
        d.ellipse((u * s - r * 0.45, v * s - r * 0.45, u * s + r * 0.45, v * s + r * 0.45), fill=(120, 96, 52))
    for u, v in ((4.0, 4.2), (8.2, 3.6), (4.5, 24.8), (15.8, 6.0), (17.0, 22.5)):  # Square test pads
        r = 0.55 * s
        d.rectangle((u * s - r, v * s - r, u * s + r, v * s + r), fill=GOLD)
    _text(img, (1.2 * s, h / 2 * s), "250-0577-00 REV 2", round(0.95 * s), SILK, angle=90, bold=True)
    _text(img, (10.0 * s, (h / 2 + 7.6) * s), "U1", round(1.0 * s), SILK, bold=True)
    d.rectangle((3.2 * s, (h - 6.2) * s, 5.0 * s, (h - 3.4) * s), fill=SILK)  # Label patch


def lipo_label(img: Image.Image, s: float) -> None:
    w, h = img.width / s, img.height / s
    ink = (34, 34, 36)
    _text(img, (w / 2 * s, (h / 2 - 2.2) * s), "LP503562", round(2.6 * s), ink, mono=True)
    _text(img, (w / 2 * s, (h / 2 + 2.2) * s), "3.7V 1200mAh", round(2.6 * s), ink, mono=True)


def display_pcb_front(img: Image.Image, s: float) -> None:
    """ER-TFTM032-3 PCB seen from the front: black mask, gold mounting rings, header pads."""
    d = ImageDraw.Draw(img)
    rng = random.Random(32)
    w, h = img.width / s, img.height / s
    p = V4_PARAMS  # Canvas x = device X - PCB left edge, canvas y = PCB top edge - device Y
    x0, y1 = p.display_pcb_center_x - p.display_pcb_w / 2.0, p.display_pcb_center_y + p.display_pcb_h / 2.0
    holes = [(hx - x0, y1 - hy) for hx, hy in p.mounting_holes]
    _traces(d, rng, s, (6.0, 3.0, w - 3.0, h - 3.0), 40, DISPLAY_TRACE, 0.25)
    for hx, hy in holes:
        for r, col in ((2.5, GOLD), (1.55, (10, 10, 12))):
            d.ellipse(((hx - r) * s, (hy - r) * s, (hx + r) * s, (hy + r) * s), fill=col)
    for i in range(20):  # 2x20 header pads along the left edge
        for col_x in (1.33, 3.87):
            y = h / 2 + (i - 9.5) * 2.54
            r = 0.75 * s
            shape = d.rectangle if i == 0 and col_x == 1.33 else d.ellipse
            shape((col_x * s - r, y * s - r, col_x * s + r, y * s + r), fill=GOLD)
    _text(img, ((w - 14) * s, (h - 2.0) * s), "ER-TFTM032-3", round(1.6 * s), SILK, bold=True)


PAINTERS: dict[str, tuple[Callable[[Image.Image, float], None], tuple[int, int, int], float]] = {
    # key: (painter, background, px per mm)
    "teensy_component_side": (teensy_component_side, TEENSY_GREEN, 40.0),
    "chip_mcu": (chip_mcu, CHIP, 60.0),
    "chip_flash": (chip_small(["25Q64JV", "IQ 2228"]), CHIP, 90.0),
    "chip_bootloader": (chip_small(["MKL02", "Z32VFG4"]), CHIP, 90.0),
    "chip_phy": (chip_small(["DP83825I", "TI 2AT"]), CHIP, 90.0),
    "breakout_lepton_side": (breakout_lepton_side, BREAKOUT_GREEN, 30.0),
    "lipo_label": (lipo_label, (190, 193, 198), 20.0),
    "display_pcb_front": (display_pcb_front, DISPLAY_BLACK, 15.0),
}


# ---------------------------------------------------------------------------------------------
# View canvas -> device XY map
# ---------------------------------------------------------------------------------------------

def _axis_coord(axis: str, X: np.ndarray, Y: np.ndarray, bb: tuple[float, float, float, float]) -> np.ndarray:
    x0, y0, x1, y1 = bb
    return {"+X": X - x0, "-X": x1 - X, "+Y": Y - y0, "-Y": y1 - Y}[axis]


def _extent(axis: str, bb: tuple[float, float, float, float]) -> float:
    x0, y0, x1, y1 = bb
    return (x1 - x0) if axis.endswith("X") else (y1 - y0)


def render_decal(key: str, bbox_xy: tuple[float, float, float, float], view: tuple[str, str], out: Path,
                 painter: Callable[[Image.Image, float], None] | None = None,
                 background: tuple[int, int, int] | None = None, px_per_mm: float | None = None) -> Path:
    """Paint decal `key` for a part whose device-frame XY bounding box is `bbox_xy` and write the XY map."""
    if painter is None:
        painter, background, px_per_mm = PAINTERS[key]
    assert background is not None and px_per_mm is not None
    right, down = view
    s = px_per_mm
    cw, ch = _extent(right, bbox_xy), _extent(down, bbox_xy)
    canvas = Image.new("RGBA", (max(2, round(cw * s)), max(2, round(ch * s))), (*background, 255))
    painter(canvas, s)
    src = np.asarray(canvas.convert("RGB"))
    x0, y0, x1, y1 = bbox_xy
    W, H = max(2, round((x1 - x0) * s)), max(2, round((y1 - y0) * s))
    cols, rows = np.meshgrid(np.arange(W), np.arange(H))
    X = x0 + (cols + 0.5) / s
    Y = y1 - (rows + 0.5) / s  # Image row 0 is the top, i.e. the largest Y
    cx = np.clip((_axis_coord(right, X, Y, bbox_xy) * s).astype(int), 0, src.shape[1] - 1)
    cy = np.clip((_axis_coord(down, X, Y, bbox_xy) * s).astype(int), 0, src.shape[0] - 1)
    Image.fromarray(src[cy, cx]).save(out)
    return out
