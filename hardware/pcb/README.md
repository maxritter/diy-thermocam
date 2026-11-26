# DIY-Thermocam V4.0 PCB

The main board of the camera. A PCB manufacturer makes it and solders all parts on; you then plug in the Teensy 4.1, the FLIR Lepton breakout board, the display and the battery.

| Display side (top) | Module side (bottom) |
| --- | --- |
| ![Top side of the V4.0 PCB with the display socket](fab/diy-thermocam-v4.0-top.png) | ![Bottom side of the V4.0 PCB with the Teensy, Lepton and battery connectors](fab/diy-thermocam-v4.0-bottom.png) |

## Order the board

Choose a manufacturer and upload the three files from its folder in [`fab/`](fab/):

| Manufacturer | Files | Good to know |
| --- | --- | --- |
| **[JLCPCB](https://jlcpcb.com/)** (recommended) | [`fab/jlcpcb/`](fab/jlcpcb/): `…-gerbers.zip`, `…-bom.csv`, `…-cpl.csv` | Every part is in JLCPCB's own stock, but only 39 of the boost converter `U2` were left on 26 November 2026; if it runs out, pre-order it in JLCPCB's parts manager. Assembly starts at 2 boards. |
| [Seeed Fusion](https://www.seeedstudio.com/fusion_pcb.html) | [`fab/seeed/`](fab/seeed/): `…-gerbers.zip`, `…-bom.csv`, `…-cpl.csv` | Seeed buys the parts; the BOM links each one on LCSC. Also upload both assembly drawings. Takes about 4 weeks. |
| [PCBWay](https://www.pcbway.com/pcb-assembly.html) | [`fab/pcbway/`](fab/pcbway/): `…-gerbers.zip`, `…-bom.csv`, `…-cpl.csv` | PCBWay buys the parts; the notes column lists each LCSC number. Answer "no" to Chinese substitutes. |

**Board options:** 2 layers, 89.4 × 68.4 mm, 1.6 mm FR-4, 1 oz copper, lead-free HASL or ENIG, any solder mask colour.

**Assembly:** order double-sided assembly so the manufacturer fits every part and you need no soldering iron. All SMD parts and the Teensy and Lepton sockets go on the bottom side; only the display socket `J1` (2×20, through-hole) sits on the top side.

**Before you pay,** compare the manufacturer's placement preview with [`fab/diy-thermocam-v4.0-assembly-bottom.pdf`](fab/diy-thermocam-v4.0-assembly-bottom.pdf). The pin-1 marks of `U2`, `U3`, `U4`, `D1`, `D2`, `Q1` and `Q2` must line up. Rotations of bottom-side parts are the most common assembly mistake.

**Do not substitute** the spring pins `P1`–`P4`, the sockets (`J1`, `J2`, `J5`, `J6`), the USB-C connector `J3` or the inductor `L1`. Their heights and footprints are part of the design.

## What you plug in

- **Teensy 4.1 with pins** into `J5`/`J6`, as it comes. Leave its five inner pads empty: spring pins `P3` (`VBAT`, clock supply) and `P4` (`Program`, PROG button) press on them from below. If pins are soldered there, cut them flush underneath first; never force the Teensy down.
- **FLIR Lepton 3.5** (or 3.1R) on the **Lepton Breakout Board V2.0** into `J2`.
- **BuyDisplay `ER-TFTM032-3`** into `J1`, from the display side.
- **LiPo battery** (`505060`, JST-PH) into `J4`. Check that its `+` wire matches the `+` mark first.

The full parts list with shops is in [`hardware/bom/`](../bom/README.md).

## What is on the board

| Function | How it works |
| --- | --- |
| USB-C | Charges the battery and carries USB data. Two spring pins (`P1`, `P2`) press on the USB pads under the Teensy, so the Teensy's own micro-USB port stays unused. Never plug a cable into it while the board is powered. |
| Charging | TI BQ24074 powers the camera over USB while charging the battery (0.89 A charge current, red `CHG` LED). It draws up to 1.3 A from USB, so use a charger or port rated 1.5 A or more; a 500 mA hub port can cut out. |
| 5 V supply | TI TPS61022 boost converter. The slide switch `SW1` turns the camera off; the battery still charges while it is off. |
| Battery protection | `Q1`/`Q2` block a battery plugged in the wrong way round; `F1` is a 2 A resettable fuse. |
| Clock | An XC6206 regulator keeps the Teensy's clock running from the battery through spring pin `P3`, so no coin cell is needed. |
| Buttons | `SW2` capture button at the board edge; `SW3` PROG button behind a pinhole for firmware recovery, wired to the Teensy through spring pin `P4`. |
| Touch | Resistive touch works as delivered. A capacitive-touch display needs a soldering iron: cut `JP1`/`JP2` between pads 1–2, bridge pads 2–3, and close `JP3`. |

## First power-up

1. Board only, battery connected, `SW1` on: 5.1 V between `VIN` and `GND` on the Teensy socket. `SW1` off: 0 V.
2. Plug in USB-C: the red `CHG` LED lights while the battery charges.
3. With the Teensy plugged in, connect USB-C to a computer: the Teensy shows up as a USB device. This confirms `P1`/`P2` touch its USB pads.
4. Press PROG through the pinhole: Teensy Loader shows the Teensy in bootloader mode. This confirms `P4`.
5. Set the clock, switch off with `SW1` for a minute and on again: the time kept running. This confirms `P3`.

## Change the design

The KiCad 10 project is [`diy-thermocam-v4.0.kicad_sch`](diy-thermocam-v4.0.kicad_sch) and [`diy-thermocam-v4.0.kicad_pcb`](diy-thermocam-v4.0.kicad_pcb), with its own symbol and footprint libraries ([`V40.kicad_sym`](V40.kicad_sym), [`V40.pretty/`](V40.pretty/)). Part numbers live in [`hardware/bom/bom.csv`](../bom/bom.csv).

After a change, check it and rebuild the order files:

```bash
kicad-cli sch erc --severity-error --exit-code-violations hardware/pcb/diy-thermocam-v4.0.kicad_sch
kicad-cli pcb drc --refill-zones --schematic-parity --severity-error --exit-code-violations hardware/pcb/diy-thermocam-v4.0.kicad_pcb
python3 hardware/pcb/scripts/export_fab.py
```
