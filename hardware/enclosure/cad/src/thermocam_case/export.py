"""One-command build & export CLI for DIY-Thermocam parametric enclosures.

Usage:
    cd hardware/enclosure/cad
    uv run python -m thermocam_case.export
"""

from __future__ import annotations

import argparse
from pathlib import Path
import time

from build123d import Compound, export_step

from .keepout import build_keepouts
from .laser_case import build_laser_enclosure, export_laser_sheet
from .params import PRESETS, get_preset
from .printable_case import build_printable_enclosure, export_printable_enclosure
from .render import (
    render_laser_preview,
    render_orthographic_sheet,
    render_printable_multiview,
    render_v4_assembly_steps,
)


def default_enclosure_root() -> Path:
    """Resolve `hardware/enclosure/` from `hardware/enclosure/cad/src/thermocam_case/`."""
    return Path(__file__).resolve().parents[3]


def export_all(
    enclosure_root: Path | None = None,
    variants: tuple[str, ...] = ("v3_0", "v4_0"),
    *,
    skip_renders: bool = False,
) -> list[Path]:
    """Generate all laser SVG/DXF, 3D STEP/STL/3MF, and preview PNG renders."""
    root = enclosure_root or default_enclosure_root()
    exports_dir = root / "exports"
    renders_dir = root / "renders"
    generated: list[Path] = []

    for variant in variants:
        t0 = time.perf_counter()
        p = get_preset(variant)
        ver = variant.replace("_", ".")
        print(f"[thermocam_case] Building {p.version_tag} ({variant})...")

        keepouts = build_keepouts(p)

        # 1. Improved 6-plate 3 mm laser-cut acrylic enclosure (SVG + DXF + assembled STEP)
        laser = build_laser_enclosure(p)
        laser_dir = exports_dir / "laser"
        svg_path = laser_dir / f"diy-thermocam-{ver}-laser-sheet.svg"
        dxf_path = laser_dir / f"diy-thermocam-{ver}-laser-sheet.dxf"
        laser_step = laser_dir / f"diy-thermocam-{ver}-laser-assembly.step"
        export_laser_sheet(laser, svg_path, dxf_path)
        export_step(Compound(children=list(laser.plates_3d.values())), str(laser_step))
        generated.extend([svg_path, dxf_path, laser_step])

        # 2. 2-piece 3D-printable / CNC shell + lens cap (+ V4 capture button plunger)
        printable = build_printable_enclosure(p)
        shell_dir = exports_dir / f"3d-{ver}"
        shell_files = export_printable_enclosure(printable, shell_dir)
        generated.extend(shell_files)

        # 3. Studio preview PNG renders
        if not skip_renders:
            hero_path = renders_dir / f"{ver}-3d-enclosure-hero.png"
            ortho_path = renders_dir / f"{ver}-3d-enclosure-orthographic.png"
            laser_png = renders_dir / f"{ver}-laser-enclosure-preview.png"

            render_printable_multiview(printable, keepouts, hero_path)
            render_orthographic_sheet(printable, keepouts, ortho_path)
            render_laser_preview(laser, keepouts, laser_png)
            generated.extend([hero_path, ortho_path, laser_png])

            if variant == "v4_0":
                step_pngs = render_v4_assembly_steps(printable, keepouts, renders_dir)
                generated.extend(step_pngs)

        dt = time.perf_counter() - t0
        print(f"[thermocam_case] Finished {variant} in {dt:.1f}s")

    print(f"[thermocam_case] Exported {len(generated)} files under {root}")
    return generated


def main() -> None:
    parser = argparse.ArgumentParser(description="Export DIY-Thermocam parametric enclosure CAD")
    parser.add_argument(
        "--variants",
        nargs="+",
        default=list(PRESETS.keys()),
        choices=list(PRESETS.keys()),
        help="Hardware variants to build (default: v3_0 v4_0)",
    )
    parser.add_argument(
        "--skip-renders",
        action="store_true",
        help="Skip PNG preview rendering (export CAD files only)",
    )
    args = parser.parse_args()
    export_all(variants=tuple(args.variants), skip_renders=args.skip_renders)


if __name__ == "__main__":
    main()
