# Hardware Agent Guide

Applies to `hardware/` (PCB, enclosure CAD, and Bill of Materials).

## Layout

- `hardware/pcb/` — Native KiCad 10 V4.0 zero-solder PCBA project (`diy-thermocam-v4.0.kicad_{sch,pcb,pro}`, `V40.kicad_sym`, `V40.pretty/`), fabrication outputs (`fab/`), and `scripts/` (`build_v4_board.py`, `export_fab.py`) emitting Gerber/Excellon/BOM/CPL/STEP outputs for Seeed Studio Fusion, JLCPCB, and PCBWay. (Legacy V3.0 Eagle files are preserved in git history at tag `3.0.4`.)
- `hardware/enclosure/` — Parametric `build123d` Python CAD (`cad/`), 2D/3D exports (`exports/`), and renders (`renders/`) emitting both 2D laser-cut SVG/DXF and 3D-printable/CNC STEP/STL/3MF. (Legacy V3.0 SketchUp/SVG files are preserved at tag `3.0.4`.)
- `hardware/bom/` — Single-source `bom.csv`, `schema.json`, and `validate_bom.py`. Feeds both KiCad and the website's `/docs/partlist/` page.

## Verification Gates

- **BOM:** `python3 hardware/bom/validate_bom.py` must pass on every BOM edit; supplier URLs are link-checked in CI (`lychee`).
- **KiCad V4.0 PCB:** `kicad-cli sch erc --severity-error --exit-code-violations hardware/pcb/diy-thermocam-v4.0.kicad_sch` and `kicad-cli pcb drc --refill-zones --severity-error --exit-code-violations hardware/pcb/diy-thermocam-v4.0.kicad_pcb` with 0 errors.
- **Enclosure:** Automated clearance, mounting-hole, and Lepton FOV aperture checks (`cd hardware/enclosure/cad && uv run --extra dev --python 3.12 pytest`).
- **Physical prototype gate:** Any new PCB (V4.0) or enclosure revision remains marked `PROTOTYPE-PENDING` until Max manufactures, assembles, and verifies a physical unit.
