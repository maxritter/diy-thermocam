# DIY-Thermocam Enclosures (`build123d`)

> **Prototype Gate:** All parametric enclosures in `exports/3d-v3.0/`, `exports/3d-v4.0/`, and `exports/laser/` are `PROTOTYPE-PENDING` until physically built and verified by Max. Proven V3.0 acrylic files stay archived at git tag [`3.0.4`](https://github.com/maxritter/diy-thermocam/tree/3.0.4/enclosure).

## Enclosure Variants

Every variant builds from one parametric Python [`build123d`](https://build123d.readthedocs.io/) project in [`cad/`](cad/). Dimensions parse directly from [`hardware/pcb/diy-thermocam-v4.0.kicad_pcb`](../pcb/diy-thermocam-v4.0.kicad_pcb) (`89.4 × 68.4 mm`, corner holes, Lepton optical center, Teensy 4.1, and BuyDisplay `ER-TFTM032-3`).

| Variant | Target PCB | Hardware | Highlights | Exports |
| --- | --- | --- | --- | --- |
| **V4.0 3D / CNC Shell** (`PROTOTYPE-PENDING`) | V4.0 PCBA (`hardware/pcb/`) | `4× M2.5×6` front + `4× M2.5×8` back CSK | Guided `capture_button` plunger, captive `slide_switch_cap`, insulating `battery_cradle` + `battery_lid` for `505060` LiPo, USB-C boot recess, `1/4"-20` blind-roofed tripod pocket (max stud `≤ 8.0 mm`), dovetail `lens_cap`, `13.4→20.2 mm` cone clearing Lepton 3.5 and 3.1R (`119° DFOV`) | [`exports/3d-v4.0/`](exports/3d-v4.0/) |
| **V3.0 3D / CNC Shell** (`PROTOTYPE-PENDING`) | V3.0 PCB (tag `3.0.4`) | `4× M2.5×6` front + `4× M2.5×8` back CSK | `39.20 mm` standoff stack, `13.4→20.2 mm` cone clearing Lepton 2.5, 3.5, and 3.1R, dovetail `lens_cap`, micro-USB recess, NKK switch pocket, LED bore, `1/4"-20` blind-roofed tripod pocket | [`exports/3d-v3.0/`](exports/3d-v3.0/) |
| **3 mm Laser-Cut Acrylic** (`PROTOTYPE-PENDING`) | V3.0 or V4.0 PCB | Screwdriver + 3 mm laser cutter | 6 interlocking finger-joint plates (`+0.20 mm` slot tolerance for `2.8–3.2 mm` cast acrylic), widened `23.8 mm` Lepton aperture clearing Lepton 3.5 and 3.1R, V4 laser-cut actuator keys, separate `#CC0000` `CUT` and `#0066FF` `ENGRAVE` layers | [`exports/laser/`](exports/laser/) |

## Visual Previews

- **V4.0 3D / CNC Shell:** [`renders/v4.0-3d-enclosure-hero.png`](renders/v4.0-3d-enclosure-hero.png) · [`renders/v4.0-3d-enclosure-orthographic.png`](renders/v4.0-3d-enclosure-orthographic.png)
- **V4.0 Assembly Steps:** [`renders/v4-assembly-overview.png`](renders/v4-assembly-overview.png)
- **V3.0 3D / CNC Shell:** [`renders/v3.0-3d-enclosure-hero.png`](renders/v3.0-3d-enclosure-hero.png) · [`renders/v3.0-3d-enclosure-orthographic.png`](renders/v3.0-3d-enclosure-orthographic.png)
- **Laser-Cut Acrylic:** [`renders/v3.0-laser-enclosure-preview.png`](renders/v3.0-laser-enclosure-preview.png) · [`renders/v4.0-laser-enclosure-preview.png`](renders/v4.0-laser-enclosure-preview.png)

## Home 3D Printing (Support-Free FDM)

Every `.stl`, `.step`, and `.3mf` file is pre-oriented flat on `Z = 0` for support-free printing. For a single build plate with all parts laid out, load:

- V4.0: [`exports/3d-v4.0/diy-thermocam-v4.0-print-plate.3mf`](exports/3d-v4.0/diy-thermocam-v4.0-print-plate.3mf)
- V3.0: [`exports/3d-v3.0/diy-thermocam-v3.0-print-plate.3mf`](exports/3d-v3.0/diy-thermocam-v3.0-print-plate.3mf)

| Setting | Recommendation | Notes |
| --- | --- | --- |
| **Material** | PETG (recommended), matte PLA, or ASA | PETG handles warm LiPo heat and gives smooth dovetail sliding. |
| **Nozzle & layer** | `0.40 mm` nozzle, `0.20 mm` layer height | Use `0.16 mm` on `lens_cap` and `capture_button` for smoother slides. |
| **Walls** | 3–4 perimeters (`≥ 1.2 mm`; CAD wall is `2.60 mm`) | Keeps M2.5 bosses and the tripod pocket solid. |
| **Infill** | 20–25% gyroid | Rigid without extra weight. |
| **Supports** | Off | All chamfers, dovetails, chutes, and battery trays print support-free. |
| **Orientation** | Keep exported orientation (`Z = 0`) | Shells print face-down; `lens_cap`, `capture_button`, and battery trays print flat. |

## Fabrication Bureaus (PCBWay, JLCPCB 3D, Seeed Studio Fusion)

All parts keep `≥ 2.0 mm` minimum wall thickness and export watertight ISO-10303-21 `.step` files ready for quoting.

Upload these 7 `.step` files from `exports/3d-v4.0/` (or the 3 files from `exports/3d-v3.0/`):
- `diy-thermocam-v4.0-front_shell.step`
- `diy-thermocam-v4.0-back_shell.step`
- `diy-thermocam-v4.0-lens_cap.step`
- `diy-thermocam-v4.0-capture_button.step` *(V4.0 only)*
- `diy-thermocam-v4.0-slide_switch_cap.step` *(V4.0 only)*
- `diy-thermocam-v4.0-battery_cradle.step` *(V4.0 only)*
- `diy-thermocam-v4.0-battery_lid.step` *(V4.0 only)*

Recommended materials:
- **MJF PA12 Nylon (Dyed Black)** or **SLS PA12** — durable, matte, heat-resistant, exact `0.25 mm` dovetail fit.
- **SLA Tough Black Resin** — smooth finish (lightly sand dovetail edges if snug).
- **CNC 6061-T6 Aluminum (Bead-Blasted + Black Anodize)** — upload the `.step` files directly.

Assembly needs only a Phillips screwdriver:
1. Drop a metal `1/4"-20` hex nut into the blind-roofed rear tripod pocket (max tripod stud `≤ 8.0 mm`).
2. Seat `capture_button` and `slide_switch_cap` into the shell slots.
3. Stack the `M2.5×20`, `M2.5×11`, and `M2.5×5` standoffs with `4× 1.8 mm` nylon spacer washers supporting `battery_cradle` and `battery_lid` around the `505060` LiPo.
4. Close with `4× M2.5×6` front and `4× M2.5×8` back ISO 10642 countersunk screws.

## Laser Cutting (3 mm Cast Acrylic)

- **V3.0:** [`exports/laser/diy-thermocam-v3.0-laser-sheet.svg`](exports/laser/diy-thermocam-v3.0-laser-sheet.svg) / [`.dxf`](exports/laser/diy-thermocam-v3.0-laser-sheet.dxf)
- **V4.0:** [`exports/laser/diy-thermocam-v4.0-laser-sheet.svg`](exports/laser/diy-thermocam-v4.0-laser-sheet.svg) / [`.dxf`](exports/laser/diy-thermocam-v4.0-laser-sheet.dxf)
- **Material:** `3.0 mm` cast acrylic (`Acryl GS`, ~225 × 205 mm sheet). Slots include `+0.20 mm` tolerance for `2.8–3.2 mm` stock.
- **Kerf:** Default `0.10 mm` (`laser_kerf` in `cad/src/thermocam_case/params.py`).
- **Layers (1:1 mm):** Red `#CC0000` (`CUT`) for through-cuts; blue `#0066FF` (`ENGRAVE`) for optional exterior branding.

## Rebuilding and Testing Locally (`uv` + Python 3.12)

```bash
cd hardware/enclosure/cad
uv sync --extra dev --python 3.12

# Regenerate SVG, DXF, STEP, STL, 3MF exports and PNG renders
uv run python -m thermocam_case.export

# Run the 15 automated geometry, wall-thickness, interference, and FOV tests
uv run pytest
```

`cad/tests/test_geometry.py` checks:
- **KiCad PCB parity:** Verifies outline, corner holes, Lepton holes, and V4.0 control coordinates (`J3`, `SW1`, `SW2`, `SW3`, `D2`) to `≤ 0.01 mm`.
- **Solid validity & wall thickness:** Confirms closed B-Rep solids and `≥ 2.0 mm` walls everywhere.
- **Zero interference (`0.00 mm³`):** Checks pairwise OCCT intersections across shells, actuators, swollen `5.5 mm` LiPo, THT pin tails, Display SMDs, `7.8 mm` tripod stud, and laser plates.
- **Optical FOV clearance:** Confirms both Lepton 3.5 (`71° DFOV`) and Lepton 3.1R (`119° DFOV`) clear the aperture and open lens cap without clipping.
- **Artifact integrity:** Confirms every exported file exists, is non-empty, and has a valid header.
