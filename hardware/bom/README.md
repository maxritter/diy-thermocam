# Bill of materials

[`bom.csv`](bom.csv) lists every part of the DIY-Thermocam V4.0: the parts the PCB manufacturer solders onto the main board, and the modules and accessories you buy yourself. The order files in [`hardware/pcb/fab/`](../pcb/fab/) and the parts list on the website are generated from it, so change part numbers here and nowhere else.

A complete camera costs about **$356** at reference prices with a FLIR Lepton 3.5 and Breakout Board V2.0, or **$184** without the Lepton sensor. Shipping, PCB assembly fees and the optional sensors are not included.

## What you buy yourself

The assembled main board comes from JLCPCB, Seeed Fusion or PCBWay (see [`hardware/pcb/`](../pcb/README.md)). You add:

| Part | Choose |
| --- | --- |
| Teensy 4.1 | The version **with pins**. Leave its five inner pads empty: spring pins on the board touch VBAT and PROGRAM. |
| FLIR Lepton 3.5 (or 3.1R wide-angle) | Snaps into the FLIR Lepton Breakout Board V2.0 (`250-0577-00`). |
| FLIR Lepton Breakout Board V2.0 + right-angle header | Teledyne FLIR `250-0577-00` (Rev R200) and Sullins `PPPC102LJBN-RC` (3.15 mm tail; do not substitute shorter-tail headers or PureThermal USB boards). |
| BuyDisplay `ER-TFTM032-3` | Pin header, 4-wire SPI, 5V supply, 3.2" resistive touch, no font chip. |
| LiPo battery | Adafruit 258 (1200 mAh, protected, 5 × 34 × 62 mm, JST-PH 2-pin). For more runtime, a `505060` 2000 mAh cell (≤ 5.0 mm thick) also fits; confirm its JST-PH plug and polarity with the seller first. Sellers wire this plug both ways round: check `+` against the board's `J4` marking before plugging in. |
| microSD card | 16–32 GB, FAT32. |
| USB-C cable | One that carries data, not charge-only. |
| Enclosure and screws | See [`hardware/enclosure/`](../enclosure/README.md). |

Every row in `bom.csv` has a primary and an alternative shop link.

## Files

- [`bom.csv`](bom.csv): the parts list.
- [`schema.json`](schema.json): required columns and allowed values.
- [`validate_bom.py`](validate_bom.py): checks `bom.csv` against the schema, prints the cost total, and with `--check-links` tests every shop link.

```bash
python3 hardware/bom/validate_bom.py
python3 hardware/bom/validate_bom.py --check-links
```
