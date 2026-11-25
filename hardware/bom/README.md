# DIY-Thermocam Bill of Materials (`hardware/bom/`)

`bom.csv` is the single machine-readable source of truth for DIY-Thermocam hardware. It feeds KiCad fabrication exports, CI validation, and the documentation website (`/docs/partlist/`).

## Files

- [`bom.csv`](bom.csv) — Verified 2026 parts list with revision filtering (`hw_rev`: `all`, `v3.0`, `v3.1`), LCSC/Seeed PCBA metadata, verified supplier URLs, and reference USD pricing.
- [`schema.json`](schema.json) — JSON Schema (Draft 2020-12) defining required columns, enums, patterns, and numeric constraints.
- [`validate_bom.py`](validate_bom.py) — Pure Python 3 validator for schema compliance, cost rollups, and live HTTP link checking.

## Hardware Revisions & Estimated Cost (2026 USD)

Both revisions share the same core modules (Teensy 4.1, FLIR Lepton 3.5 + Breakout V2.0, BuyDisplay 3.2" `ER-TFTM032-3` resistive touch, Pololu `U3V16F5` 5V boost, protected 2000 mAh LiPo, enclosure, and ISO/DIN fasteners).

| Revision | Description | Required Line Items | Est. Total (with Lepton 3.5) | Est. Total (excl. Lepton 3.5) |
| :--- | :--- | :---: | :---: | :---: |
| **V3.0** | 1:1 verified KiCad port of the proven V3.0 through-hole PCB. Uses a `TP4056 + DW01A` charger module workaround for the obsolete TP4057 ([#103](https://github.com/maxritter/diy-thermocam/issues/103)) and requires cutting the Teensy 4.1 `VUSB-VIN` pad. | 28 | **$366.51** | **$194.51** |
| **V3.1** | Turnkey PCBA revision with board-edge USB-C, TI `BQ24075RGTR` dynamic power-path charger, `USBLC6-2SC6` ESD protection, JST-PH battery header, D+/D- tap (no Teensy pad cut), and 0603/0805 SMD passives. | 37 | **$364.46** | **$192.46** |

*Notes: Totals exclude shipping, PCBA setup/extended-part feeder fees (~$3 per extended line item at JLCPCB), and the optional Lepton 2.5 budget sensor (EOL notice: last buy Sept 2027). Pack items (header strips, M2.5 assortment, hook-up wire, heatshrink) reflect retail pack prices; buying exact single-unit quantities lowers non-sensor cost by ~$20–$30.*

## How to Order

### 1. Turnkey PCBA (Recommended for V3.1)
Filter `bom.csv` for `hw_rev == "v3.1"` and `category in {"pcb_smd", "pcb_tht"}`:
- **JLCPCB**: Upload the KiCad Gerber, BOM (using the `lcsc_part` column), and Pick-and-Place CPL files. Ten SMD passives/LEDs/ESD use **Basic** parts (no feeder fee); only `BQ24075RGTR` (`C15464`), `TYPE-C-31-M-12` (`C165948`), `S2B-PH-K-S` (`C157932`), and `MY-1220-03-R` (`C964819`) are **Extended**.
- **Seeed Studio Fusion / PCBWay**: Upload the standard KiCad BOM with `mpn` and `lcsc_part`. Seeed Fusion's Shenzhen OPL matches these MPNs and LCSC part numbers directly for turnkey assembly.
- Plug-in modules (Teensy 4.1, Lepton Breakout V2.0, BuyDisplay 3.2" TFT, Pololu `U3V16F5`) and panel-mount switch/button remain hand-installed via 2.54 mm headers so expensive modules can be salvaged or upgraded.

### 2. DIY Hand-Soldering (V3.0 or Bare V3.1 Board)
- **Bare PCB**: Order 5 boards (89.4×68.4 mm, 2-layer, 1.6 mm FR4, 1 oz copper) from JLCPCB, Seeed Fusion, or PCBWay (~$2/board before shipping).
- **Sensors**: Order FLIR Lepton 3.5 (`500-0771-01`) + Lepton Breakout Board V2.0 (`250-0577-00`) from GroupGets or Mouser. Do **not** buy a PureThermal USB UVC board—it lacks the 2×10 SPI header.
- **Display**: Order `ER-TFTM032-3` from BuyDisplay configured as **4-Wire SPI, 5V Power Supply, 3.2" Resistive Touch (XPT2046), Pin Header, No Font Chip**.
- **Connectors, Switches, Passives**: Order from DigiKey/Mouser (THT parts) and Adafruit/SparkFun/Pololu (Teensy 4.1, Pololu `U3V16F5`, protected 2000 mAh LiPo, cables, headers).

## Validation

Run schema validation and cost rollups locally or in CI:

```bash
python3 hardware/bom/validate_bom.py
```

Run live HTTP link verification (flags real 404/soft-404/broken links as errors while distinguishing bot-protected distributor WAFs such as DigiKey, Mouser, GroupGets, BuyDisplay, and TinyTronics):

```bash
python3 hardware/bom/validate_bom.py --check-links
```
