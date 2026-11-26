"""Order files for the two bare FR4 boards of the battery stack (cradle floor and lid).

They are too thin to print (MJF minimum wall 1.0 mm), so a PCB maker cuts them from FR4 with black
solder mask and no copper. For each board this writes a Gerber zip (via a generated KiCad board and
kicad-cli), a 1:1 DXF outline and a STEP model.
"""

from __future__ import annotations

import os
import subprocess
import tempfile
import zipfile
from pathlib import Path

from build123d import Axis, ExportDXF, GeomType, Location, Solid, Unit, export_step

KICAD_CLI = os.environ.get("KICAD_CLI", "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli")
GERBER_LAYERS = "F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts"

BOARD_HEADER = """(kicad_pcb
\t(version 20240108)
\t(generator "thermocam_case")
\t(generator_version "1")
\t(general
\t\t(thickness {t})
\t\t(legacy_teardrops no)
\t)
\t(paper "A4")
\t(layers
\t\t(0 "F.Cu" signal)
\t\t(31 "B.Cu" signal)
\t\t(36 "B.SilkS" user "B.Silkscreen")
\t\t(37 "F.SilkS" user "F.Silkscreen")
\t\t(38 "B.Mask" user)
\t\t(39 "F.Mask" user)
\t\t(44 "Edge.Cuts" user)
\t)
\t(setup
\t\t(pad_to_mask_clearance 0)
\t)
\t(net 0 "")
"""


def _top_face(solid: Solid):
    return solid.faces().sort_by(Axis.Z)[-1]


def _xy(v) -> str:
    """Camera frame (Y up) to KiCad (Y down)."""
    return f"{v.X:.4f} {-v.Y:.4f}"


def _edge_cuts(face) -> list[str]:
    out = []
    for e in face.edges():
        stroke = '(stroke (width 0.05) (type default)) (layer "Edge.Cuts")'
        if e.geom_type == GeomType.LINE:
            out.append(f"\t(gr_line (start {_xy(e.position_at(0))}) (end {_xy(e.position_at(1))}) {stroke})")
        elif e.geom_type == GeomType.CIRCLE:
            if e.is_closed:
                c, r = e.arc_center, e.radius
                out.append(f"\t(gr_circle (center {_xy(c)}) (end {c.X + r:.4f} {-c.Y:.4f}) {stroke} (fill none))")
            else:
                out.append(f"\t(gr_arc (start {_xy(e.position_at(0))}) (mid {_xy(e.position_at(0.5))}) "
                           f"(end {_xy(e.position_at(1))}) {stroke})")
        else:
            raise ValueError(f"FR4 outline edge {e.geom_type} is neither a line nor an arc")
    return out


def kicad_board(solid: Solid, thickness: float, label: str) -> str:
    face = _top_face(solid)
    c = face.center()
    text = (f'\t(gr_text "{label}" (at {c.X:.3f} {-c.Y:.3f} 0) (layer "F.SilkS")\n'
            '\t\t(effects (font (size 1.5 1.5) (thickness 0.25)))\n\t)')
    return BOARD_HEADER.format(t=thickness) + "\n".join([*_edge_cuts(face), text]) + "\n)\n"


def export_fr4_board(solid: Solid, thickness: float, name: str, label: str, out_dir: Path, prefix: str) -> list[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    stem = out_dir / f"{prefix}-{name}"
    step, dxf, gerbers = Path(f"{stem}.step"), Path(f"{stem}.dxf"), Path(f"{stem}-gerbers.zip")

    export_step(solid, str(step))
    face = _top_face(solid)
    exporter = ExportDXF(unit=Unit.MM, line_weight=0.1)
    exporter.add_shape(face.moved(Location((0.0, 0.0, -face.center().Z))).edges())
    exporter.write(str(dxf))

    with tempfile.TemporaryDirectory() as tmp:
        board = Path(tmp) / f"{prefix}-{name}.kicad_pcb"
        board.write_text(kicad_board(solid, thickness, label))
        gdir = Path(tmp) / "gerbers"
        gdir.mkdir()
        subprocess.run([KICAD_CLI, "pcb", "export", "gerbers", "--layers", GERBER_LAYERS, "--no-protel-ext",
                        "-o", str(gdir), str(board)], check=True, capture_output=True)
        with zipfile.ZipFile(gerbers, "w", compression=zipfile.ZIP_DEFLATED) as zf:
            for f in sorted(gdir.iterdir()):
                if f.suffix == ".gbrjob":
                    continue  # Job file carries the export time; the layers are enough for the fab
                info = zipfile.ZipInfo(f.name, date_time=(2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                zf.writestr(info, f.read_bytes())
    return [step, dxf, gerbers]


__all__ = ["export_fr4_board", "kicad_board"]
