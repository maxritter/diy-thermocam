# Bill of materials

[`bom.csv`](bom.csv) lists every part of the DIY-Thermocam V4.0: the parts the PCB manufacturer solders onto the board, and the modules and accessories you buy yourself. The order files in [`hardware/pcb/fab/`](../pcb/fab/) and the parts list on the website are generated from it, so change part numbers here and nowhere else.

A complete camera costs about **$318** at reference prices with a FLIR Lepton 3.5, or **$146** without it. Shipping, PCB assembly fees and the optional sensors are not included.

## What you buy yourself

The assembled PCB comes from JLCPCB, Seeed Fusion or PCBWay (see [`hardware/pcb/`](../pcb/README.md)). You add:

| Part | Choose |
| --- | --- |
| Teensy 4.1 | The version **with pins**. Leave its five inner pads empty: spring pins on the board touch VBAT and PROGRAM. |
| FLIR Lepton 3.5 (or 3.1R wide-angle) | Plus the Lepton Breakout Board V2.0. A PureThermal board does not fit. |
| BuyDisplay `ER-TFTM032-3` | Pin header, 4-wire SPI, 5V supply, 3.2" resistive touch, no font chip. |
| LiPo battery | 3.7 V, protected, `505060` size (≤ 5.0 mm thick), JST-PH 2-pin. Sellers wire this plug both ways round: check `+` against the board's `J4` marking before plugging in. |
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
