"""The firmware's color schemes, read from its own sources so tests compare against what was built."""

from __future__ import annotations

import re
from dataclasses import dataclass
from functools import cache
from pathlib import Path

FIRMWARE = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class ColorScheme:
    number: int
    name: str
    rgb: tuple[tuple[int, int, int], ...]  # first `elements` entries are the ones the firmware uses
    elements: int


def rgb565(r: int, g: int, b: int) -> int:
    """Same packing as convertColors() and display_setColor()."""
    return (((r & 248) | g >> 5) << 8) | ((g & 28) << 3 | b >> 3)


@cache
def schemes() -> dict[int, ColorScheme]:
    defines = (FIRMWARE / "include/globaldefines.h").read_text()
    numbers = {m[1]: int(m[2]) for m in re.finditer(r"#define colorScheme_(\w+)\s+(\d+)", defines)}
    numbers.pop("Total", None)

    tables_src = (FIRMWARE / "src/general/colorschemes.cpp").read_text()
    tables = {
        m[1]: [int(v) for v in m[2].replace("\n", " ").split(",") if v.strip()]
        for m in re.finditer(r"const byte colorMap_(\w+)\[\]\s*=\s*\{([^}]*)\}", tables_src)
    }

    select_src = (FIRMWARE / "src/thermal/thermal.cpp").read_text()
    elements = {
        m[1]: int(m[3])
        for m in re.finditer(
            r"case colorScheme_(\w+):\s*colorMap = colorMap_(\w+);\s*colorElements = (\d+);",
            select_src,
        )
    }

    result: dict[int, ColorScheme] = {}
    for name, number in numbers.items():
        v = tables[name]
        rgb = tuple((v[i], v[i + 1], v[i + 2]) for i in range(0, len(v) - 2, 3))
        result[number] = ColorScheme(number, name, rgb, elements[name])
    return result


def colorbar_rows(scheme: ColorScheme) -> dict[int, int]:
    """Screen row -> RGB565 of the color bar, as showColorBar() draws it (hot/cold mode off)."""
    height = (240 - ((240 - (scheme.elements // 2)) // 2)) & 0xFF  # `byte height`
    rows: dict[int, int] = {}
    count = 0
    for i in range(0, scheme.elements - 1):
        if i % 2 == 0:
            rows[height - count] = rgb565(*scheme.rgb[i])
            count += 1
    return rows
