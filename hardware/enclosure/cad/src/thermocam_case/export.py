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

from build123d import (
    Compound,
    Location,
    Mesher,
    Rotation,
    Solid,
    export_step,
    export_stl,
)

from .accessories import _as_solid, build_battery_floor, build_battery_lid
from .fr4 import export_fr4_board
from .laser_case import (
    LASER_PRINT_PARTS,
    LaserEnclosure,
    build_laser_enclosure,
    export_laser_sheet,
)
from .params import V4_PARAMS, EnclosureParams
from .printable_case import (
    FR4_PARTS,
    PRINT_PARTS,
    PrintableEnclosure,
    build_printable_enclosure,
    export_printable_enclosure,
    orient_for_printing,
    write_print_plate,
)

LASER_ONLY_PARTS = ("tripod_mount", "sd_chute", "display_spacer")

# Print services print one part per file (JLC3DP's Q&A: "each file could only support 1 part"), so
# the upload zips hold every unique part once, one file each; the quantity goes in the file name.
SHELL_SERVICE_PARTS = ("front_shell", "back_shell", "lens_cap", "capture_button", "slide_switch_cap",
                       "battery_cradle", "battery_spacer")
LASER_SERVICE_PARTS = {"battery_cradle": 1, "battery_spacer": 2, "tripod_mount": 1, "sd_chute": 1,
                       "display_spacer": 2}
FR4_NOTE = ("The battery floor and lid are bare FR4 boards from a PCB maker, not printed parts: "
            "see exports/fr4/.")


def default_enclosure_root() -> Path:
    """Resolve `hardware/enclosure/` from `hardware/enclosure/cad/src/thermocam_case/`."""
    return Path(__file__).resolve().parents[3]


def service_files(root: Path, p: EnclosureParams = V4_PARAMS) -> dict[str, Path]:
    pre = p.file_prefix
    shell_dir, laser_dir = root / "exports" / "3d-printed", root / "exports" / "laser-cut"
    return {
        "shell_zip": shell_dir / f"{pre}-shell-print-files.zip",
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


def _order_text(title: str, rows: list[tuple[str, int]], extra: list[str]) -> str:
    width = max(len(r[0]) for r in rows) + 2
    lines = [
        f"DIY-Thermocam V4.0 - {title}",
        "",
        "Upload every file from ONE folder (step/, or stl/ if the service takes no STEP), never both:",
        "each folder holds every part once, one part per file. Set the quantity of each file below.",
        "",
        "Material: MJF PA12 (nylon), dyed black.",
        "",
        f"{'Part'.ljust(width)}Qty",
        *[f"{name.ljust(width)}{qty}" for name, qty in rows],
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


def export_service_files(root: Path, laser_sheet: tuple[Path, Path], p: EnclosureParams = V4_PARAMS) -> list[Path]:
    """Upload zips for both builds: one STEP and one STL per unique part, plus ORDER.txt."""
    out = service_files(root, p)
    pre = p.file_prefix
    written: list[Path] = []

    # The zips pack the single-part files export_all has already written; ORDER.txt carries quantities
    shell_dir, laser_dir = out["shell_zip"].parent, out["laser_zip"].parent

    def formats(name: str, stem: str) -> dict[str, Path | str]:
        src_dir = laser_dir if name in LASER_ONLY_PARTS else shell_dir
        return {f"{fmt}/{pre}-{stem}.{fmt}": src_dir / f"{pre}-{name}.{fmt}" for fmt in ("step", "stl")}

    members: dict[str, Path | str] = {}
    for name in SHELL_SERVICE_PARTS:
        members |= formats(name, name)
    rows = [(name, PRINT_PARTS[name]) for name in SHELL_SERVICE_PARTS]
    members["ORDER.txt"] = _order_text("3D-printed shell: files for a print service", rows, [FR4_NOTE])
    _write_zip(out["shell_zip"], members)

    members = {f"laser-sheet/{src.name}": src for src in laser_sheet}
    rows: list[tuple[str, int]] = []
    for name, qty in LASER_SERVICE_PARTS.items():
        members |= formats(name, name)
        rows.append((name, qty))
    members["ORDER.txt"] = _order_text(
        "laser-cut acrylic case: files for the cutting and print services", rows,
        ["Laser sheet (laser-sheet/): 3 mm black cast acrylic (GS). Please confirm the sheet measures",
         "2.8-3.2 mm before cutting (slots are cut 3.2 mm wide). The parts take 213.1 x 195.6 mm",
         "(Formulor P2). Blue RGB(0,0,255) hairlines are cuts; red RGB(255,0,0) hairlines are the optional",
         "engraving on the back plate (Formulor's colour rules). The file allows for a 0.35 mm kerf. The",
         "four keys (a spare of each) hang on 0.8 mm bridges.", FR4_NOTE])
    _write_zip(out["laser_zip"], members)
    written += [out["shell_zip"], out["laser_zip"]]
    return written


def _ground(s: Solid) -> Solid:
    bb = s.bounding_box()
    return _as_solid(Location((-bb.min.X, -bb.min.Y, -bb.min.Z)) * s)


def orient_laser_parts(laser: LaserEnclosure, shell: PrintableEnclosure) -> dict[str, Solid]:
    """Print-ready laser-case parts, flat on Z=0 (MJF needs no supports)."""
    shell_parts = orient_for_printing(shell)
    return {
        "battery_cradle": shell_parts["battery_cradle"],
        "battery_spacer": shell_parts["battery_spacer"],
        "tripod_mount": _ground(_as_solid(Rotation(0, -90, 0) * laser.printed["tripod_mount"])),
        "sd_chute": _ground(_as_solid(Rotation(-90, 0, 0) * laser.printed["sd_chute"])),
        "display_spacer": _ground(laser.printed["display_spacer_bottom"]),
    }


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
    write_print_plate([laser_parts[n] for n, qty in LASER_PRINT_PARTS.items() for _ in range(qty)], plate)
    laser_step = laser_dir / f"{p.file_prefix}-laser-assembly.step"
    export_step(Compound(children=[*laser.plates_3d.values(), *laser.printed.values()]), str(laser_step))
    generated += [plate, laser_step]

    shell_dir = root / "exports" / "3d-printed"
    generated += export_printable_enclosure(printable, shell_dir)

    # Fresh solids: the assembly STEP above made the enclosure's own ones children of a compound,
    # and build123d then refuses to export them on their own
    fr4_dir = root / "exports" / "fr4"
    for name, solid in (("battery_floor", build_battery_floor(p)), ("battery_lid", build_battery_lid(p))):
        label = f"DIY-Thermocam V4.0 {name.replace('_', ' ')}"
        generated += export_fr4_board(solid, FR4_PARTS[name], name, label, fr4_dir, p.file_prefix)
    generated += export_service_files(root, (svg_path, dxf_path), p)

    print(f"[thermocam_case] Exported {len(generated)} files in {time.perf_counter() - t0:.1f}s under {root}")
    return generated


def main() -> None:
    export_all()


if __name__ == "__main__":
    main()
