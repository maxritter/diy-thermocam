"""One-command build and export for the DIY-Thermocam V4.0 enclosures.

Usage:
    cd hardware/enclosure/cad
    uv run thermocam-case-export          # CAD files (STEP / STL / 3MF / SVG / DXF)
    uv run thermocam-case-render          # Blender renders (see render.py)
"""

from __future__ import annotations

import time
import zipfile
from pathlib import Path

from build123d import Compound, Location, Mesher, Rotation, Solid, export_step, export_stl

from .accessories import _as_solid
from .fr4 import export_fr4_board
from .laser_case import LASER_PRINT_PARTS, LaserEnclosure, build_laser_enclosure, export_laser_sheet
from .params import V4_PARAMS, EnclosureParams
from .printable_case import (
    FR4_PARTS,
    PRINT_PARTS,
    PrintableEnclosure,
    build_printable_enclosure,
    export_printable_enclosure,
    orient_for_printing,
)

LASER_ONLY_PARTS = ("tripod_mount", "sd_chute", "display_spacer")

# Print services take one part per object and cap an order (JLC3DP: 10 objects per upload), so the
# service files hold every unique part once.
SHELL_SERVICE_PARTS = ("front_shell", "back_shell", "lens_cap", "capture_button", "slide_switch_cap",
                       "battery_cradle")
LASER_SERVICE_PARTS = {"battery_cradle": 1, "tripod_mount": 1, "sd_chute": 1, "display_spacer": 2}
ORANGE_PARTS = ("lens_cap", "capture_button", "slide_switch_cap")
FR4_NOTE = ("The battery floor and lid are bare FR4 boards from a PCB maker, not printed parts: "
            "see exports/fr4/.")


def default_enclosure_root() -> Path:
    """Resolve `hardware/enclosure/` from `hardware/enclosure/cad/src/thermocam_case/`."""
    return Path(__file__).resolve().parents[3]


def service_files(root: Path, p: EnclosureParams = V4_PARAMS) -> dict[str, Path]:
    pre = p.file_prefix
    shell_dir, laser_dir = root / "exports" / "3d-printed", root / "exports" / "laser-cut"
    return {
        "shell_plate": shell_dir / f"{pre}-service-plate.3mf",
        "shell_zip": shell_dir / f"{pre}-shell-print-files.zip",
        "laser_plate": laser_dir / f"{pre}-laser-service-plate.3mf",
        "laser_zip": laser_dir / f"{pre}-acrylic-files.zip",
    }


def expected_exports(root: Path, p: EnclosureParams = V4_PARAMS) -> list[Path]:
    """Every file `export_all` writes, for tests and docs."""
    pre = p.file_prefix
    shell_dir, laser_dir = root / "exports" / "3d-printed", root / "exports" / "laser-cut"
    files = [shell_dir / f"{pre}-{part}.{ext}" for part in PRINT_PARTS for ext in ("step", "stl", "3mf")]
    files += [root / "exports" / "fr4" / f"{pre}-{part}{suffix}" for part in FR4_PARTS
              for suffix in (".step", ".dxf", "-gerbers.zip")]
    files += [shell_dir / f"{pre}-assembly.step", shell_dir / f"{pre}-print-plate.3mf"]
    files += [laser_dir / f"{pre}-laser-sheet.{ext}" for ext in ("svg", "dxf")]
    files += [laser_dir / f"{pre}-{part}.{ext}" for part in LASER_ONLY_PARTS for ext in ("step", "stl", "3mf")]
    files += [laser_dir / f"{pre}-laser-assembly.step", laser_dir / f"{pre}-laser-print-plate.3mf"]
    files += list(service_files(root, p).values())
    return files


def _named(solid: Solid, name: str) -> Solid:
    solid.label = name  # build123d writes the label as the 3MF object name (what a service's splitter shows)
    return solid


def _write_service_plate(parts: list[tuple[str, Solid]], path: Path) -> None:
    """One named object per part, laid out in rows on a 200 mm wide plate."""
    mesher = Mesher()
    cursor_x, cursor_y, row_h = 0.0, 0.0, 0.0
    for name, solid in parts:
        bb = solid.bounding_box()
        if cursor_x > 0 and cursor_x + bb.size.X > 200.0:
            cursor_x, cursor_y, row_h = 0.0, cursor_y + row_h + 8.0, 0.0
        mesher.add_shape(_named(_as_solid(Location((cursor_x, cursor_y, 0.0)) * solid), name), part_number=name)
        cursor_x += bb.size.X + 8.0
        row_h = max(row_h, bb.size.Y)
    mesher.write(str(path))


def _order_text(title: str, rows: list[tuple[str, int, str]], plate_name: str, extra: list[str]) -> str:
    width = max(len(r[0]) for r in rows) + 2
    lines = [
        f"DIY-Thermocam V4.0 - {title}",
        "",
        "Upload the files from ONE folder only (step/ or stl/), never both: each folder holds every part once.",
        f"JLC3DP: you can upload {plate_name} instead; its online splitter turns it into these parts.",
        "",
        "Material: MJF PA12 (nylon), dyed black.",
        "Optional: lens_cap, capture_button and slide_switch_cap in orange." if any(r[0] in ORANGE_PARTS for r in rows) else "",
        "",
        f"{'Part'.ljust(width)}Qty  Note",
        *[f"{name.ljust(width)}{qty:<5}{note}" for name, qty, note in rows],
        "",
        *extra,
    ]
    return "\n".join(line for i, line in enumerate(lines) if line or (i and lines[i - 1])) + "\n"


def _write_zip(path: Path, members: dict[str, Path | str]) -> None:
    """Zip with fixed timestamps so an unchanged design gives the same archive."""
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for arcname, src in members.items():
            info = zipfile.ZipInfo(arcname, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            data = src.read_bytes() if isinstance(src, Path) else src.encode("utf-8")
            zf.writestr(info, data)


def export_service_files(root: Path, shell_parts: dict[str, Solid], laser_parts: dict[str, Solid],
                         laser_sheet: tuple[Path, Path], p: EnclosureParams = V4_PARAMS) -> list[Path]:
    """JLC3DP service plates (one named object per part) and upload zips for both builds."""
    out = service_files(root, p)
    pre = p.file_prefix
    written: list[Path] = []

    # Shell: 6 unique parts, each once
    shell = [(name, shell_parts[name]) for name in SHELL_SERVICE_PARTS]
    _write_service_plate(shell, out["shell_plate"])
    # Acrylic case: the printed parts, the display spacer twice (5 objects)
    laser = [(f"{name}_{i + 1}" if qty > 1 else name, laser_parts[name] if name in laser_parts else shell_parts[name])
             for name, qty in LASER_SERVICE_PARTS.items() for i in range(qty)]
    _write_service_plate(laser, out["laser_plate"])
    written += [out["shell_plate"], out["laser_plate"]]

    # The zips pack the single-part files export_all has already written (quantity in the name if > 1)
    shell_dir, laser_dir = out["shell_zip"].parent, out["laser_zip"].parent

    def formats(name: str, stem: str) -> dict[str, Path | str]:
        src_dir = laser_dir if name in LASER_ONLY_PARTS else shell_dir
        return {f"{fmt}/{pre}-{stem}.{fmt}": src_dir / f"{pre}-{name}.{fmt}" for fmt in ("step", "stl")}

    members: dict[str, Path | str] = {}
    for name, _ in shell:
        members |= formats(name, name)
    rows = [(name, 1, "black, or orange" if name in ORANGE_PARTS else "") for name, _ in shell]
    members["ORDER.txt"] = _order_text("3D-printed shell: files for a print service", rows,
                                       out["shell_plate"].name, [FR4_NOTE])
    _write_zip(out["shell_zip"], members)

    members = {f"laser-sheet/{src.name}": src for src in laser_sheet}
    rows = []
    for name, qty in LASER_SERVICE_PARTS.items():
        stem = f"{name}_x{qty}" if qty > 1 else name
        members |= formats(name, stem)
        rows.append((stem, qty, ""))
    members["ORDER.txt"] = _order_text(
        "laser-cut acrylic case: files for the cutting and print services", rows, out["laser_plate"].name,
        ["Laser sheet (laser-sheet/): 3.0 mm black cast acrylic, 213 x 195 mm.",
         "Red #CC0000 lines are cuts; blue #0066FF is the optional engraving on the back plate.", FR4_NOTE])
    _write_zip(out["laser_zip"], members)
    written += [out["shell_zip"], out["laser_zip"]]
    return written


def _ground(s: Solid) -> Solid:
    bb = s.bounding_box()
    return _as_solid(Location((-bb.min.X, -bb.min.Y, -bb.min.Z)) * s)


def orient_laser_parts(laser: LaserEnclosure, shell: PrintableEnclosure) -> dict[str, Solid]:
    """Print-ready laser-case parts, flat on Z=0 without supports."""
    shell_parts = orient_for_printing(shell)
    return {
        "battery_cradle": shell_parts["battery_cradle"],
        "tripod_mount": _ground(_as_solid(Rotation(0, -90, 0) * laser.printed["tripod_mount"])),
        "sd_chute": _ground(_as_solid(Rotation(-90, 0, 0) * laser.printed["sd_chute"])),
        "display_spacer": _ground(laser.printed["display_spacer_bottom"]),
    }


def _write_plate(parts: dict[str, Solid], counts: dict[str, int], path: Path) -> None:
    """Lay every part out in rows at its per-camera quantity and write one 3MF build plate."""
    mesher = Mesher()
    cursor_x, cursor_y, row_h = 0.0, 0.0, 0.0
    for name, solid in parts.items():
        for _ in range(counts[name]):
            bb = solid.bounding_box()
            if cursor_x > 0 and cursor_x + bb.size.X > 200.0:
                cursor_x, cursor_y, row_h = 0.0, cursor_y + row_h + 8.0, 0.0
            mesher.add_shape(_as_solid(Location((cursor_x, cursor_y, 0.0)) * solid))
            cursor_x += bb.size.X + 8.0
            row_h = max(row_h, bb.size.Y)
    mesher.write(str(path))


def export_all(enclosure_root: Path | None = None, p: EnclosureParams = V4_PARAMS) -> list[Path]:
    """Generate the laser SVG/DXF sheet, its printed parts, and the 3D-printed shell parts."""
    root = enclosure_root or default_enclosure_root()
    t0 = time.perf_counter()
    generated: list[Path] = []

    laser = build_laser_enclosure(p)
    printable = build_printable_enclosure(p)
    laser_dir = root / "exports" / "laser-cut"
    svg_path = laser_dir / f"{p.file_prefix}-laser-sheet.svg"
    dxf_path = laser_dir / f"{p.file_prefix}-laser-sheet.dxf"
    export_laser_sheet(laser, svg_path, dxf_path)
    generated += [svg_path, dxf_path]

    laser_parts = orient_laser_parts(laser, printable)
    for name in LASER_ONLY_PARTS:
        solid = laser_parts[name]
        step, stl, threemf = (laser_dir / f"{p.file_prefix}-{name}.{ext}" for ext in ("step", "stl", "3mf"))
        export_step(solid, str(step))
        export_stl(solid, str(stl), tolerance=0.02, angular_tolerance=0.1)
        m = Mesher()
        m.add_shape(solid)
        m.write(str(threemf))
        generated += [step, stl, threemf]
    plate = laser_dir / f"{p.file_prefix}-laser-print-plate.3mf"
    _write_plate(laser_parts, LASER_PRINT_PARTS, plate)
    laser_step = laser_dir / f"{p.file_prefix}-laser-assembly.step"
    export_step(Compound(children=[*laser.plates_3d.values(), *laser.printed.values()]), str(laser_step))
    generated += [plate, laser_step]

    shell_dir = root / "exports" / "3d-printed"
    generated += export_printable_enclosure(printable, shell_dir)

    fr4_dir = root / "exports" / "fr4"
    for name, solid in (("battery_floor", printable.battery_floor), ("battery_lid", printable.battery_lid)):
        label = f"DIY-Thermocam V4.0 {name.replace('_', ' ')}"
        generated += export_fr4_board(solid, FR4_PARTS[name], name, label, fr4_dir, p.file_prefix)
    generated += export_service_files(root, orient_for_printing(printable), laser_parts, (svg_path, dxf_path), p)

    print(f"[thermocam_case] Exported {len(generated)} files in {time.perf_counter() - t0:.1f}s under {root}")
    return generated


def main() -> None:
    export_all()


if __name__ == "__main__":
    main()
