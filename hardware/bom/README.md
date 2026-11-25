# DIY-Thermocam Bill of Materials (`hardware/bom/`)

`bom.csv` is the single machine-readable parts list for DIY-Thermocam. It feeds KiCad exports, CI checks, and the interactive `/docs/partlist/` configurator.

## Files

- [`bom.csv`](bom.csv) — Parts list with revision filters (`all`, `v3.0`, `v4.0`), 31 verified V4.0 LCSC SKUs, supplier URLs, and reference USD pricing.
- [`schema.json`](schema.json) — JSON Schema (Draft 2020-12) defining required columns, enums, and constraints.
- [`validate_bom.py`](validate_bom.py) — Python 3 validator for schema compliance, cost rollups, and live HTTP link checks.

## Hardware Revisions and Estimated Cost (USD)

Both revisions share the core plug-in modules: Teensy 4.1, FLIR Lepton 3.5 (or wide-angle 3.1R) with Breakout V2.0, BuyDisplay 3.2" `ER-TFTM032-3`, microSD card, enclosure, and M2/M2.5 fasteners.

| Revision | Status | Summary | Required Items | Est. Total (with Lepton 3.5) | Est. Total (excl. Lepton 3.5) |
| :--- | :--- | :--- | :---: | :---: | :---: |
| **V3.0** | **Proven & buildable** | Through-hole PCB (tag `3.0.4`). Uses `TP4056 + DW01A` charger ([#103](https://github.com/maxritter/diy-thermocam/issues/103)), Pololu `U3V16F5` boost, wired switches, CR1220 coin cell, and Teensy `VUSB–VIN` pad cut. | 28 | **$366.51** | **$194.51** |
| **V4.0** | **`PROTOTYPE-PENDING`** | Zero-solder PCBA (`hardware/pcb/`). Adds USB-C, TI `BQ24074RGTR` charger, TI `TPS61022RWUR` 5 V boost, `AO3401A`/`AO3400A` polarity protection, `XC6206P332MR-G` RTC LDO, SMD controls, USB pogo pins, and `505060` LiPo. | 43 | **$333.06** | **$161.06** |

*Totals exclude shipping, PCBA extended-part feeder fees (~$3 per extended line at JLCPCB), and optional sensors (Lepton 3.1R at $189; Lepton 2.5 at $139, EOL notice Sept 2027). Retail hardware packs add ~$15–$25 over exact single-unit quantities.*

## How to Order

### 1. Turnkey PCBA (V4.0 Zero-Solder, `PROTOTYPE-PENDING`)

All 31 LCSC SKUs in `bom.csv` match [`hardware/pcb/fab/diy-thermocam-v4.0-bom-jlcpcb.csv`](../pcb/fab/diy-thermocam-v4.0-bom-jlcpcb.csv) 1:1:

- **JLCPCB / JLC PCBA:** Upload `diy-thermocam-v4.0-gerbers.zip`, `diy-thermocam-v4.0-bom-jlcpcb.csv`, and `diy-thermocam-v4.0-cpl-jlcpcb.csv`. Sixteen SMD parts are **Basic** (no feeder fee); fifteen are **Extended**.
- **Seeed Studio Fusion / PCBWay:** Upload `diy-thermocam-v4.0-bom-seeed.csv` (or `pcbway.csv`) and `diy-thermocam-v4.0-cpl-seeed.csv`.
- **Battery (V4.0 3D cradle):** Use a UL/IEC 62133 certified protected `505060` (`≤5.0 mm` thick) 3.7 V 2000 mAh LiPo with Adafruit/SparkFun JST-PH polarity. Verify `+`/`-` against `J4` silkscreen.
- **Fasteners:** `4× M2.5×6` front and `4× M2.5×8` back ISO 10642 CSK screws, `M2.5×5`/`11`/`20` standoffs, `4× 1.8 mm` nylon spacer washers, and a metal `1/4"-20` hex nut (max tripod stud `≤ 8.0 mm`).

### 2. Hand-Soldered Baseline (Proven V3.0)

- **Bare PCB:** Order 5 boards (`89.4 × 68.4 mm`, 2-layer, `1.6 mm` FR4, `1 oz` copper) using the V3.0 Gerbers archived at git tag [`3.0.4`](https://github.com/maxritter/diy-thermocam/tree/3.0.4/pcb).
- **Sensors:** Order FLIR Lepton 3.5 (`500-0771-01`) or Lepton 3.1R (`500-0758-03`) plus Breakout Board V2.0 (`250-0577-00`) from GroupGets, Mouser, or DigiKey. Do not buy a PureThermal USB board — it lacks the 2×10 SPI header.
- **Display:** Order `ER-TFTM032-3` from BuyDisplay configured as **4-Wire SPI, 5V, 3.2" Resistive Touch, Pin Header, No Font Chip**.

## V3 → V4 First Prototype Order List (Minimum Vendors)

Already own a V3 camera? Reuse your FLIR Lepton 3.5, Breakout Board V2.0, BuyDisplay `ER-TFTM032-3`, and Teensy 4.1 (make sure Teensy's inner 1×5 `VBAT`/`PROG` header is soldered for `J7`). You need only **two orders** to build and test both the V4.0 3D shell and the V4.0 laser acrylic enclosure.

### Order 1 — PCBA + 3D Shell + 2D Laser Acrylic

- **Single-vendor option (PCBWay):** Upload all three packages below in one checkout (PCBA, MJF PA12 3D print, and 3 mm cast-acrylic laser cutting).
- **Split-fab option:** Order PCBA and MJF/SLS 3D print from **JLCPCB / JLC3DP** (or **Seeed Studio Fusion**), and 3 mm acrylic from **Formulor**, **Ponoko**, or a local laser shop.

| Package | Files to Upload | Manufacturing Specs |
| :--- | :--- | :--- |
| **V4.0 Zero-Solder PCBA** | `hardware/pcb/fab/diy-thermocam-v4.0-gerbers.zip`<br>`hardware/pcb/fab/diy-thermocam-v4.0-bom-{jlcpcb,pcbway,seeed}.csv`<br>`hardware/pcb/fab/diy-thermocam-v4.0-cpl-{jlcpcb,seeed}.csv` | 2-layer, `89.4 × 68.4 mm`, `1.6 mm` FR4, `1 oz` copper, ENIG or Lead-Free HASL, Matte Green or Black soldermask. Populate all 31 LCSC SKUs on Bottom (`J2`/`J5`/`J6`/`J7`); include Top `J1` or hand-solder `J1` to keep PCBA single-sided. |
| **V4.0 3D Enclosure (7 parts)** | All 7 `.step` files in `hardware/enclosure/exports/3d-v4.0/`:<br>`front_shell`, `back_shell`, `lens_cap`, `capture_button`, `slide_switch_cap`, `battery_cradle`, `battery_lid` | **MJF PA12 Nylon (Dyed Black)** or **SLS PA12** (optional orange accent for cap, button, and switch). Also prints support-free in PETG via `diy-thermocam-v4.0-print-plate.3mf`. |
| **V4.0 2D Laser Acrylic** | `hardware/enclosure/exports/laser/diy-thermocam-v4.0-laser-sheet.svg` (or `.dxf`) | **3.0 mm cast acrylic** (`Acryl GS`, ~225 × 205 mm). Red `#CC0000` (`CUT`) = through-cut; blue `#0066FF` (`ENGRAVE`) = optional exterior branding (skip for $0). |

### Order 2 — Fasteners, Slim `505060` LiPo, and USB-C Cable

One consolidated order covers both the 3D shell and the 2D acrylic enclosure:

- **Battery (1×):** UL/IEC 62133 certified protected `505060` (`5.0 × 50 × 60 mm`, `≤5.0 mm`) 3.7 V 2000 mAh LiPo with Adafruit/SparkFun JST-PH 2-pin plug *(verify `+`/`-` against `J4` silkscreen; 5.0 mm alt: Adafruit ID 258, 1200 mAh)*.
- **Cable (1×):** USB-C data + power cable (`≥ 0.5 m`).
- **M2.5 Enclosure Stack (3D & 2D):**
  - `4× M2.5 × 20 mm` F–F brass standoffs (rear)
  - `4× M2.5 × 11 mm` M–F brass standoffs (mid / LCD mate)
  - `4× M2.5 × 5 mm` M–F brass standoffs (front bezel)
  - `4× 1.8 mm` M2.5 nylon spacer washers (supports `battery_cradle` above PCB THT tails)
  - `4× M2.5 × 6 mm` DIN 965 / ISO 10642 countersunk screws (front)
  - `4× M2.5 × 8 mm` DIN 965 / ISO 10642 countersunk screws (back)
- **M2 Lepton Mounting (reuse from V3 or order fresh):** `4× M2 × 18 mm` DIN 84 screws, `4× M2 × 12 mm` spacers, `4× M2` nylon nuts.
- **Tripod Mount Hardware:**
  - **3D shell:** `1× 1/4"-20 UNC` metal hex nut (blind-roofed pocket; max tripod stud `≤ 8.0 mm`).
  - **2D laser acrylic:** `1× 1/4"-20 UNC` tee nut, `3× M2 × 8 mm` DIN 965 screws, `3× M2` nuts and washers.
- **Teensy Inner Header (if missing on your V3 Teensy 4.1):** `1× 1×5` 2.54 mm male pin header for `VBAT`/`PROG` (`J7`).

## Validation

```bash
python3 hardware/bom/validate_bom.py
python3 hardware/bom/validate_bom.py --check-links
```
