# DIY-Thermocam Bill of Materials (`hardware/bom/`)

`bom.csv` is the single machine-readable source of truth for DIY-Thermocam hardware. It feeds KiCad fabrication exports, CI validation, and the documentation website (`/docs/partlist/`).

## Files

- [`bom.csv`](bom.csv) — Verified 2026 parts list with revision filtering (`hw_rev`: `all`, `v3.0`, `v4.0`), all 31 live-verified V4.0 LCSC PCBA SKUs, verified supplier URLs, and reference USD pricing.
- [`schema.json`](schema.json) — JSON Schema (Draft 2020-12) defining required columns, enums, patterns, and numeric constraints.
- [`validate_bom.py`](validate_bom.py) — Pure Python 3 validator for schema compliance, cost rollups, and live HTTP link checking.

## Hardware Revisions & Estimated Cost (2026 USD)

Both revisions share the core plug-in modules (Teensy 4.1, FLIR Lepton 3.5 or optional wide-angle 3.1R + Breakout V2.0, BuyDisplay 3.2" `ER-TFTM032-3` resistive touch, microSD, enclosure, and ISO/DIN fasteners).

| Revision | Status | Description | Required Line Items | Est. Total (with Lepton 3.5) | Est. Total (excl. Lepton 3.5) |
| :--- | :--- | :--- | :---: | :---: | :---: |
| **V3.0** | **Proven & buildable** | 1:1 verified KiCad 10 port of the proven V3.0 through-hole PCB. Uses a `TP4056 + DW01A` charger module workaround ([#103](https://github.com/maxritter/diy-thermocam/issues/103)), Pololu `U3V16F5` boost, panel switches, CR1220 coin cell, and Teensy `VUSB-VIN` pad cut. | 28 | **$366.51** | **$194.51** |
| **V4.0** | **`PROTOTYPE-PENDING`** | Zero-solder factory PCBA with board-edge USB-C, TI `BQ24074RGTR` dynamic power-path charger (`4.4V VSYS`), TI `TPS61022RWUR` 5V boost, `AO3401A`/`AO3400A` active LiPo reverse-polarity + 2A PTC fuse, `XC6206P332MR-G` 3.3V RTC LDO (no coin cell), SMD `SW1`/`SW2`/`SW3`, `P1`/`P2` USB D+/D- pogo pins, factory-soldered sockets, and slim `≤5.0 mm` UL/IEC 62133 `505060` 2000 mAh LiPo. | 43 | **$333.06** | **$161.06** |

*Notes: Totals exclude shipping, PCBA setup/extended-part feeder fees (~$3 per extended line item at JLCPCB), and optional sensors (FLIR Lepton 3.1R 95° wide-angle at $189; Lepton 2.5 budget fallback with Sept 2027 EOL notice at $139). Pack items (M2.5 assortment, V3.0 header strips, hook-up wire, heatshrink) reflect retail pack prices; sourcing exact single-unit quantities lowers non-sensor cost by ~$15–$25.*

## How to Order

### 1. Turnkey PCBA (V4.0 Zero-Solder, `PROTOTYPE-PENDING`)
All 31 LCSC part numbers in `bom.csv` (`hw_rev == "v4.0"` with `lcsc_part`) match [`hardware/pcb/kicad-v4.0/fab/diy-thermocam-v4.0-bom-jlcpcb.csv`](../pcb/kicad-v4.0/fab/diy-thermocam-v4.0-bom-jlcpcb.csv) 1:1:
- **JLCPCB / JLC PCBA**: Upload `diy-thermocam-v4.0-gerbers.zip`, `diy-thermocam-v4.0-bom-jlcpcb.csv`, and `diy-thermocam-v4.0-cpl-jlcpcb.csv`. Sixteen SMD parts use **Basic** parts (no feeder fee, including `XC6206P332MR-G` `C5446`, `AO3401A`/`AO3400A`, `R23 3.0k` `C4211`, and all passives/LED/PROG switch); fifteen components (ICs, USB-C, JST-PH, switches, pogo pins, and THT sockets) are **Extended**.
- **Seeed Studio Fusion / PCBWay**: Upload `diy-thermocam-v4.0-bom-seeed.csv` (or `pcbway.csv`) and `diy-thermocam-v4.0-cpl-seeed.csv`.
- **Battery (V4.0 3D enclosure cradle)**: Use a UL/IEC 62133 certified protected **`505060` (5.0 × 50 × 60 mm, ≤5.0 mm nominal)** 3.7 V 2000 mAh LiPo wired to standard Adafruit/SparkFun JST-PH polarity (verify `+`/`-` against `J4` silkscreen).
- **Fasteners**: `4× M2.5×6` front + `4× M2.5×8` back DIN 965 / ISO 10642 countersunk screws, `4× M2.5×5`, `4× M2.5×11`, `4× M2.5×20` standoffs, `4× 1.8 mm` nylon spacer washers (V4.0 battery cradle), and a standard metal `1/4"-20` hex nut (max tripod stud ≤ 8.0 mm).

### 2. DIY Hand-Soldering (Proven V3.0 Baseline)
- **Bare PCB**: Order 5 boards (89.4×68.4 mm, 2-layer, 1.6 mm FR4, 1 oz copper) from JLCPCB, Seeed Fusion, or PCBWay (~$2/board before shipping) using `hardware/pcb/kicad-v3.0/fab/diy-thermocam-v3.0-gerbers.zip`.
- **Sensors**: Order FLIR Lepton 3.5 (`500-0771-01`) or optional wide-angle Lepton 3.1R (`500-0758-03`) + Lepton Breakout Board V2.0 (`250-0577-00`) from GroupGets, Mouser, or DigiKey. Do **not** buy a PureThermal USB UVC board — it lacks the 2×10 SPI header.
- **Display**: Order `ER-TFTM032-3` from BuyDisplay configured as **4-Wire SPI, 5V Power Supply, 3.2" Resistive Touch (XPT2046), Pin Header, No Font Chip**.

## V3 → V4 First Prototype Order List (Minimum Vendors)

If you **already own a V3 camera** and will reuse your **FLIR Lepton 3.5**, **Lepton Breakout Board V2.0**, **BuyDisplay `ER-TFTM032-3` 3.2" TFT**, and **Teensy 4.1** (make sure the inner 1×5 `VBAT`/`PROG` header is populated for `J7`), you only need **two orders** to manufacture and test **both** the V4.0 3D shell and the V4.0 2D laser acrylic enclosure:

### Order 1 — Combined PCBA + 3D Print + 2D Laser Acrylic

- **Single-vendor option (PCBWay):** Upload all three packages below in one checkout (PCBA + MJF PA12 3D Print + 3 mm cast-acrylic laser cutting).
- **Split-fab option:** Order PCBA + MJF/SLS 3D Print from **JLCPCB / JLC3DP** (or **Seeed Studio Fusion**), and 3 mm cast-acrylic laser cutting from **Formulor** / **Ponoko** / your local laser shop.

| Package | Exact Files to Upload | Manufacturing Specs |
| :--- | :--- | :--- |
| **V4.0 Zero-Solder PCBA** | `hardware/pcb/kicad-v4.0/fab/diy-thermocam-v4.0-gerbers.zip`<br>`diy-thermocam-v4.0-bom-{jlcpcb,pcbway,seeed}.csv`<br>`diy-thermocam-v4.0-cpl-{jlcpcb,seeed}.csv` | 2-layer, `89.4 × 68.4 mm`, `1.6 mm` FR-4, `1 oz` copper, ENIG or Lead-Free HASL, Matte Green or Black soldermask. Populate all 31 LCSC SKUs (**Bottom-side SMD + THT sockets** `J2`/`J5`/`J6`/`J7`; include Top-side `J1` Display socket or hand-solder `J1` to keep assembly single-sided). |
| **V4.0 3D Enclosure (7 parts)** | All 7 `.step` files in `hardware/enclosure/exports/3d-v4.0/`:<br>`front_shell`, `back_shell`, `lens_cap`, `capture_button`, `slide_switch_cap`, `battery_cradle`, `battery_lid` | **MJF PA12 Nylon (Dyed Black)** or **SLS PA12** recommended (optional bright orange accent for `lens_cap`, `capture_button`, `slide_switch_cap`). All parts are also 100% support-free for home FDM PETG printing via `diy-thermocam-v4.0-print-plate.3mf`. |
| **V4.0 2D Laser Acrylic** | `hardware/enclosure/exports/laser/diy-thermocam-v4.0-laser-sheet.svg` (or `.dxf`) | **3.0 mm cast acrylic** (`Acryl GS`, e.g. matte black or satin grey; ~225 × 205 mm sheet). Red `#CC0000` `CUT` layer = through-cut; Blue `#0066FF` `ENGRAVE` layer = optional exterior back-plate branding (toggle off for $0). |

### Order 2 — Fasteners, Slim `505060` LiPo & USB-C Cable

Place one consolidated order (hardware/electronics distributor or local hardware + battery supplier) covering **both** the 3D shell and the 2D acrylic test fit:

- **Battery (1×):** UL/IEC 62133 certified protected slim **`505060` (5.0 × 50 × 60 mm, ≤5.0 mm nominal)** 3.7 V 2000 mAh LiPo with standard Adafruit/SparkFun JST-PH 2-pin plug *(verify `+`/`-` against `J4` silkscreen; 5.0 mm drop-in alt: Adafruit ID 258, 1200 mAh)*.
- **Cable (1×):** Standard USB-C data + power cable (≥ 0.5 m).
- **M2.5 Enclosure Stack (used by both 3D & 2D):**
  - `4× M2.5 × 20 mm` F–F brass standoffs (rear compartment)
  - `4× M2.5 × 11 mm` M–F brass standoffs (mid compartment / LCD mate)
  - `4× M2.5 × 5 mm` M–F brass standoffs (front bezel)
  - `4× 1.8 mm` thick M2.5 nylon spacer washers (supports `battery_cradle` above PCB THT tails)
  - `4× M2.5 × 6 mm` DIN 965 / ISO 10642 countersunk screws (front face)
  - `4× M2.5 × 8 mm` DIN 965 / ISO 10642 countersunk screws (back face)
- **M2 Lepton Mounting (reuse from V3 or order fresh):** `4× M2 × 18 mm` DIN 84 screws, `4× M2 × 12 mm` spacers, `4× M2` nylon nuts.
- **Tripod Mount Hardware (both enclosures):**
  - **For 3D shell:** `1× 1/4"-20 UNC` standard metal hex nut (drops into blind-roofed back-shell pocket; max tripod stud `≤ 8.0 mm`).
  - **For 2D laser acrylic:** `1× 1/4"-20 UNC` tee nut + `3× M2 × 8 mm` DIN 965 screws, `3× M2` nuts & washers.
- **Teensy Inner Header (only if your V3 Teensy 4.1 lacks it):** `1× 1×5` 2.54 mm male pin header for Teensy inner `VBAT`/`PROG` pads (`J7`).

## Validation

Run schema validation and cost rollups locally or in CI:

```bash
python3 hardware/bom/validate_bom.py
```

Run live HTTP link verification:

```bash
python3 hardware/bom/validate_bom.py --check-links
```
