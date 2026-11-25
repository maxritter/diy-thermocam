# DIY-Thermocam — Parametric Enclosures (`build123d`)

> **Prototype Gate (`PROTOTYPE-PENDING`):** Both the redesigned **V3.0** and **V4.0** parametric enclosures (`exports/3d-v3.0/`, `exports/3d-v4.0/`, and `exports/laser/`) are marked **`PROTOTYPE-PENDING`** until a physical unit is printed/cut, assembled, and verified by Max. The original proven SketchUp/SVG V3.0 acrylic files remain preserved in git history at tag [`3.0.4`](https://github.com/maxritter/diy-thermocam/tree/3.0.4/enclosure).

---

## Enclosure Variants

All new enclosures are generated from a single parametric Python [`build123d`](https://build123d.readthedocs.io/) project in [`cad/`](cad/), anchored directly to the exact mechanical coordinates parsed from [`hardware/pcb/diy-thermocam-v4.0.kicad_pcb`](../pcb/diy-thermocam-v4.0.kicad_pcb) (`89.4 × 68.4 mm`, mounting holes, Lepton optical axis, Teensy 4.1, BuyDisplay `ER-TFTM032-3`).

| Variant | Target Hardware | Assembly Tools | Key Features | Export Directory |
|---|---|---|---|---|
| **V4.0 3D / CNC Shell** *(Recommended for V4.0 zero-solder builds, `PROTOTYPE-PENDING`)* | V4.0 Zero-Solder Factory PCBA (`hardware/pcb/`) | **Screwdriver only** (`4× M2.5×6` front + `4× M2.5×8` back ISO 10642 CSK) | Support-free guided `capture_button` plunger (`Z=19.30` stem, `Y=69.10` zero-preload rest, `Y=68.95` housing hard-stop) + flat-bed captive `slide_switch_cap` (`X=89.10` fork clearing `SW1` body, `±1.65 mm` clear, housing stops), flat-bed `battery_cradle` (`1.80 mm` THT pin relief > `1.60 mm` tails, `Ø6.2 mm` SW5 bores) + `battery_lid` shielding slim `505060` 2000 mAh LiPo (`≤5.5 mm` swell) from Display SMDs, full-depth `14.2×8.0 mm` (`R=1.0 mm`) USB-C boot counterbore (`Z≥22.6` front notch), back-shell `1/4"-20` metal hex-nut pocket w/ `3.0 mm` solid blind roof (zero `89.4×64.9` Display crush, **max tripod stud ≤ 8.0 mm**), support-free microSD chute (`Y=61.2`), top-open dovetail `lens_cap` w/ inboard cantilever floor detents, **13.4→20.2 mm wide-angle cone clearing BOTH Lepton 3.5 (71° DFOV) AND Lepton 3.1R (95° HFOV / 119° DFOV)**, debossed concentric thermal-lens emblem | [`exports/3d-v4.0/`](exports/3d-v4.0/) |
| **V3.0 3D / CNC Shell** *(`PROTOTYPE-PENDING`)* | Proven V3.0 PCB (tag `3.0.4`) | **Screwdriver only** (`4× M2.5×6` front + `4× M2.5×8` back CSK) | Exact `39.20 mm` standoff stack, **13.4→20.2 mm cone clearing Lepton 2.5/3.5 (71° DFOV) AND Lepton 3.1R (95° HFOV / 119° DFOV)**, top-open dovetail `lens_cap` w/ cantilever detents, full-depth micro-USB boot opening, support-free microSD chute, PJRC `(66.90, 43.08)` Prog pinhole, NKK `CWT12AAS1` `19.3×6.9 mm` cutout w/ `1.60 mm` relieved latch pocket, `Ø5.2 mm` LED bore, back-shell `1/4"-20` blind-roofed tripod pocket | [`exports/3d-v3.0/`](exports/3d-v3.0/) |
| **Improved 3 mm Laser-Cut Acrylic** *(V3.0 & V4.0, `PROTOTYPE-PENDING`)* | V3.0 or V4.0 PCB | Screwdriver + 3 mm laser cutter | 6 interlocking finger-joint plates (+ V4 `capture_key` & `switch_key` laser-cut actuators!) at exact `39.20 mm` standoff depth (`0.20 mm` slot tolerance for `2.8–3.2 mm` cast acrylic), **widened `23.8 mm` Lepton aperture clearing BOTH Lepton 3.5 (71° DFOV) AND Lepton 3.1R (95° HFOV / 119° DFOV) even on `3.2 mm` stock**, full-size USB boot cutout, rotated tripod flange (`≥9.6 mm` web from V4 USB-C), microSD slot, Prog pinhole, `Ø5.2 mm` V3 LED, separate `#CC0000` `CUT` + `#0066FF` `ENGRAVE` layers (concentric thermal-lens logo) | [`exports/laser/`](exports/laser/) |

---

## Visual Previews

- **V4.0 Zero-Solder 3D/CNC Enclosure:** [`renders/v4.0-3d-enclosure-hero.png`](renders/v4.0-3d-enclosure-hero.png) · [`renders/v4.0-3d-enclosure-orthographic.png`](renders/v4.0-3d-enclosure-orthographic.png)
- **V4.0 Screwdriver-Only Step-by-Step Assembly:** [`renders/v4-assembly-overview.png`](renders/v4-assembly-overview.png) (`v4-assembly-step-1-board-modules.png` → `step-4-close-screws.png`)
- **V3.0 3D/CNC Enclosure:** [`renders/v3.0-3d-enclosure-hero.png`](renders/v3.0-3d-enclosure-hero.png) · [`renders/v3.0-3d-enclosure-orthographic.png`](renders/v3.0-3d-enclosure-orthographic.png)
- **Improved 3 mm Laser-Cut Acrylic:** [`renders/v3.0-laser-enclosure-preview.png`](renders/v3.0-laser-enclosure-preview.png) · [`renders/v4.0-laser-enclosure-preview.png`](renders/v4.0-laser-enclosure-preview.png)

---

## Home 3D Printing Guide (FDM — 100% Support-Free)

Every individual `.stl`, `.step`, and `.3mf` part under `exports/3d-v3.0/` and `exports/3d-v4.0/` is **pre-oriented flat on `Z = 0`** for support-free printing. For a single-click build plate containing all parts arranged side by side, load:

- V4.0: [`exports/3d-v4.0/diy-thermocam-v4.0-print-plate.3mf`](exports/3d-v4.0/diy-thermocam-v4.0-print-plate.3mf) (`front_shell`, `back_shell`, `lens_cap`, `capture_button`, `slide_switch_cap`, `battery_cradle`, `battery_lid`)
- V3.0: [`exports/3d-v3.0/diy-thermocam-v3.0-print-plate.3mf`](exports/3d-v3.0/diy-thermocam-v3.0-print-plate.3mf) (`front_shell`, `back_shell`, `lens_cap`)

### Recommended Slicer Settings

| Setting | Recommendation | Notes |
|---|---|---|
| **Material** | **PETG** (recommended) or Matte PLA / ASA | PETG resists warm ambient/LiPo heat and gives smooth dovetail lens-cap sliding; PLA prints sharpest if kept out of hot cars. |
| **Nozzle / Layer Height** | `0.40 mm` nozzle, **`0.20 mm` layer height** | Use `0.16 mm` for `lens_cap` and `capture_button` if you want extra-smooth slide feel. |
| **Walls / Perimeters** | **3–4 walls** (`≥ 1.2 mm` shells; CAD wall is `2.60 mm`) | Ensures solid bosses around M2.5 holes and the `1/4"-20` tripod pocket. |
| **Infill** | **20–25% Gyroid** | Rigid without excess weight. |
| **Supports** | **None (`OFF`)** | Outer chamfers, display bezel, 45° dovetail track, 45° capture-button transition, 45° microSD chute, and flat-bed `battery_cradle` / `battery_lid` are 100% support-free. |
| **Orientation** | Keep exported orientation (flat on `Z = 0`) | `front_shell` prints Display-face down; `back_shell` prints Lepton-face down; `lens_cap` prints inner-face down (grip ribs up); `capture_button` prints cap-top down; `battery_cradle` and `battery_lid` print floor-down. |
| **Color Accent (Optional)** | Dark matte grey/charcoal shells + bright orange `lens_cap`, `capture_button` & `slide_switch_cap` | High-visibility thermal-camera accent and easy open/closed shutter glance. |

---

## Ordering from Fabrication Bureaus (Seeed Studio Fusion 3D/CNC, JLCPCB 3D, PCBWay)

All parts maintain **`≥ 2.0 mm` minimum wall thickness** (`2.60 mm` nominal walls, `2.10 mm` under the recessed lens-cap track) and export watertight **ISO-10303-21 STEP** + fine **STL** files ready for instant quoting:

### Seeed Studio Fusion 3D / CNC *(Pairs into 1 shipment with V4 PCBA)*

1. Open [Seeed Studio Fusion 3D Printing](https://www.seeedstudio.com/rapid-prototyping-service.html) (or CNC Machining).
2. Upload the individual **`.step`** files from `exports/3d-v4.0/` (or `exports/3d-v3.0/`):
   - `diy-thermocam-v4.0-front_shell.step`
   - `diy-thermocam-v4.0-back_shell.step`
   - `diy-thermocam-v4.0-lens_cap.step`
   - `diy-thermocam-v4.0-capture_button.step` *(V4.0 only)*
   - `diy-thermocam-v4.0-slide_switch_cap.step` *(V4.0 only)*
   - `diy-thermocam-v4.0-battery_cradle.step` *(V4.0 only — flat insulating floor shielding LiPo from PCB THT tails)*
   - `diy-thermocam-v4.0-battery_lid.step` *(V4.0 only — flat insulating roof shielding LiPo from Display backside SMDs)*
3. Recommended materials:
   - **Best overall value:** **MJF PA12 Nylon (Dyed Black)** or **SLS PA12** — durable, matte, heat-resistant, exact `0.25 mm` dovetail fit.
   - **Budget smooth finish:** **SLA 8000 / Tough Black Resin** (note: resin can be slightly tighter on dovetail tracks; lightly sand the cap edges if snug).
   - **Premium metal:** **CNC 6061-T6 Aluminum (Bead-Blasted + Black Anodize)** — upload the `.step` files directly (top-open dovetail track slides smoothly in rigid metal!).
4. **100% Screwdriver-only assembly (zero soldering, zero heat-set iron):** Slide one standard metal `1/4"-20` UNC hex nut into `back_shell`'s blind-roofed bottom tripod pocket (**max tripod stud length `≤ 8.0 mm`**, `0.000 mm³` roof contact, zero PCB/Display contact), drop in `capture_button` and `slide_switch_cap`, seat the `M2.5×20` + `M2.5×11` + `M2.5×5` brass standoffs with `4× 1.8 mm` nylon spacer washers supporting `battery_cradle` + `battery_lid` around the slim UL/IEC 62133 `505060` 2000 mAh LiPo (`≤5.0 mm` nominal / `5.5 mm` swell), and fasten with `4× M2.5×6` front + `4× M2.5×8` back ISO 10642 countersunk screws.

### JLCPCB 3D Printing & PCBWay

- **JLCPCB 3D:** Upload the individual `.step` or `.stl` files (units: `mm`). Select **MJF PA12-HP Nylon (Black)** or **SLS PA12** for the best mechanical tolerance on the flush dovetail shutter and V4 capture plunger.
- **PCBWay 3D / CNC:** Upload the `.step` files directly (`mm`); MJF PA12 Nylon or 6061 Aluminum CNC both pass DFM without modification.

---

## Laser Cutting Guide (3 mm Cast Acrylic)

- Files:
  - **V3.0 (Complete & recommended for laser acrylic):** [`exports/laser/diy-thermocam-v3.0-laser-sheet.svg`](exports/laser/diy-thermocam-v3.0-laser-sheet.svg) / [`.dxf`](exports/laser/diy-thermocam-v3.0-laser-sheet.dxf)
  - **V4.0 (Reference flat sheet; use [`exports/3d-v4.0/`](exports/3d-v4.0/) for V4.0 zero-solder builds with captive switch/button actuators):** [`exports/laser/diy-thermocam-v4.0-laser-sheet.svg`](exports/laser/diy-thermocam-v4.0-laser-sheet.svg) / [`.dxf`](exports/laser/diy-thermocam-v4.0-laser-sheet.dxf)
- Material: **3.0 mm cast acrylic** (`Acryl GS`, e.g., satin grey or matte black; sheet footprint ~225 × 205 mm). Face slots include a `+0.20 mm` (`3.20 mm`) stock-thickness tolerance matching the original V3.0 acrylic design so `2.8–3.2 mm` cast sheets assemble cleanly at the exact `39.20 mm` standoff cavity depth.
- Kerf compensation: Built with **`0.10 mm` default kerf** (`laser_kerf` in `cad/src/thermocam_case/params.py`). If your laser cutter has a wider kerf (e.g., `0.15 mm`), adjust `laser_kerf` and re-export.
- Units & Layers: **1:1 millimeters** — red `#CC0000` `CUT` / `cut` hairline through-cut layer + separate optional blue `#0066FF` `ENGRAVE` / `engrave` layer (Version 1 concentric thermal-lens mark + wordmark; toggle off for zero engraving cost).

---

## Rebuilding & Verifying Locally (`uv` + Python 3.12)

```bash
cd hardware/enclosure/cad
uv sync --extra dev --python 3.12

# Regenerate all SVG, DXF, STEP, STL, 3MF exports and PNG renders
uv run python -m thermocam_case.export

# Run the automated geometry, wall-thickness, interference, and 71 deg DFOV test suite
uv run pytest
```

### Automated Verification Checks (`cad/tests/test_geometry.py`)

- **KiCad PCB parity:** Parses `hardware/pcb/diy-thermocam-v4.0.kicad_pcb` and verifies board outline (`89.4 × 68.4 mm`), 4× M2.5 mounting holes, 4× Lepton breakout holes, and locked V4.0 PCBA coordinates (`J3`, `SW1`, `SW2`, `SW3`, `D2`) to `≤ 0.01 mm`.
- **Watertight B-Rep validity:** Verifies `is_valid`, positive volume, and closed shells on every solid across V3.0 and V4.0 (laser + 3D).
- **Minimum wall thickness:** Confirms `≥ 2.0 mm` across perimeter walls, front/back faces, and beneath the recessed lens-cap dovetail track.
- **Zero interference (`0.00 mm³`) & mechanical safety gates:** Checks pairwise OCCT boolean intersections between `front_shell`, `back_shell`, `lens_cap` (open & closed), V4 `capture_button` (rest + drop-in), V4 `slide_switch_cap` (rest, `±1.5 mm` throw, drop-in), `battery_cradle` & `battery_lid` vs real `1.55 mm` PCB THT pin tails, swollen `5.5 mm` `505060` LiPo, and Display backside SMDs, real `89.4 × 64.9 mm` `ER-TFTM032-3` Display PCB, and a `7.8 mm` tripod stud — plus all 15 plate pairs of the 6-plate laser enclosure.
- **Optical FOV clearance:** Verifies the full **56°×42° / 71° DFOV** Lepton cone across **both** optical centers `(42.75, 51.00)` and `(44.50, 51.50)` clears the widened aperture and open slide lens cap with **zero clipping**, while the closed lens cap fully covers the optical path.
- **Artifact integrity:** Verifies every exported `.step`, `.stl`, `.3mf`, `.svg`, `.dxf`, and `.png` file exists, is non-empty, and has a valid file format header.
