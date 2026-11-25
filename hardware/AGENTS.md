# Hardware Agent Guide

Applies to `hardware/` (PCB, enclosure CAD, and Bill of Materials).

## Layout

- `hardware/pcb/eagle-v3.0/` — Original Eagle 9.6.2 `.sch`/`.brd`, `gerber.zip`, and board renders (89.4 × 68.4 mm, 2-layer, 1.6 mm). Keep intact as the verified V3.0 baseline.
- `hardware/pcb/` (Phase 6) — Native KiCad 9/10 S-expression projects (1:1 V3.0 port + V3.1/V4 USB-C & TI `BQ24075` revision) with scripted `kicad-cli` Gerber/Excellon/BOM/CPL/STEP outputs for Seeed Studio Fusion, JLCPCB, and PCBWay.
- `hardware/enclosure/legacy-v3.0/` — Original SketchUp `.skp`/`.skb` and 2D laser-cut `.svg`.
- `hardware/enclosure/` (Phase 6) — Parametric `build123d` Python CAD (`uv`) emitting both 2D laser-cut SVG/DXF and 3D-printable/CNC STEP/STL/3MF.
- `hardware/bom/` — Single-source `bom.csv`, `schema.json`, and `validate_bom.py`. Feeds both KiCad and the website's `/docs/partlist/` page.

## Verification Gates

- **BOM:** `python3 hardware/bom/validate_bom.py` must pass on every BOM edit; supplier URLs are link-checked in CI (`lychee`).
- **KiCad V3.0 1:1 port:** `kicad-cli sch erc` and `kicad-cli pcb drc` with 0 errors, 1:1 netlist parity (31 nets / 21 parts) vs Eagle `<signals>`, and `gerbv` raster XOR diff (≤0.05 mm) vs `eagle-v3.0/gerber.zip`.
- **Enclosure:** Automated clearance, mounting-hole, and Lepton FOV aperture checks against exported `pcb.step`.
- **Physical prototype gate:** Any new PCB (V3.1/V4) or enclosure revision remains marked prototype-pending until Max manufactures, assembles, and verifies a physical unit. Never remove or break the buildable V3.0 default files.
