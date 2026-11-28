# DIY-Thermocam V4.0 PCB

The main board of the camera. A PCB manufacturer makes it and solders all parts on. You then plug in the Teensy 4.1, the FLIR Lepton on its Breakout Board V2.0, the display and the battery.

| Display side (top) | Module side (bottom) |
| --- | --- |
| ![Top side of the V4.0 main board with the display socket](fab/diy-thermocam-v4.0-top.png) | ![Bottom side of the V4.0 main board with the Teensy, Lepton and battery connectors](fab/diy-thermocam-v4.0-bottom.png) |

## Order the board

Choose a manufacturer and upload the three files from that manufacturer's folder in [`fab/`](fab/):

- the Gerber zip, `diy-thermocam-v4.0-bom.csv` and `diy-thermocam-v4.0-cpl.csv`

| Manufacturer | Folder | Good to know |
| --- | --- | --- |
| **[JLCPCB](https://jlcpcb.com/)** (recommended) | [`fab/jlcpcb/`](fab/jlcpcb/) | The Gerber zip is `diy-thermocam-v4.0-panel-gerbers.zip`: the board sits in its own routed break-off frame (see below). Every part comes from JLCPCB's own stock. The boost converter `U2` (`C2071784`) runs low at times; if the order shows a shortfall, pre-order it in the parts manager. Assembly starts at 2 boards. |
| [Seeed Fusion](https://www.seeedstudio.com/fusion_pcb.html) | [`fab/seeed/`](fab/seeed/) | Upload `diy-thermocam-v4.0-gerbers.zip`. Seeed buys the parts; the BOM links each one on LCSC. Also upload both assembly drawings. Takes about 4 weeks. |
| [PCBWay](https://www.pcbway.com/pcb-assembly.html) | [`fab/pcbway/`](fab/pcbway/) | Upload `diy-thermocam-v4.0-gerbers.zip`. PCBWay buys the parts; the notes column lists each LCSC number. Answer "no" to Chinese substitutes. |

**Board options:** 2 layers, 89.4 × 68.4 mm, 1.6 mm FR-4, 1 oz copper, lead-free HASL or ENIG, any solder mask colour.

**Assembly:** no soldering iron needed. Order double-sided assembly (Standard PCBA at JLCPCB), because the board has a part on its top side: all SMD parts and the Teensy and Lepton sockets go on the bottom side; only the display socket `J1` (2×20, through-hole) sits on the top side.

**Panel at JLCPCB.** Double-sided assembly needs a board of at least 70 × 70 mm. The JLCPCB Gerber already holds the board in a 107.4 × 86.4 mm routed frame with small break-off tabs, three tooling holes and six fiducials. The tabs stay clear of the USB-C connector, both switches and the tripod fin. In the order form:

- choose *Delivery Format: Panel by Customer*, 1 design, 1 × 1, and add no edge rails: the frame is in the file;
- in the assembly options set *Tooling holes: Added by Customer*, because the frame already has them;
- keep the CPL file as it is: its coordinates match the board inside the frame;
- turn on production-file confirmation, so you approve the placement before assembly starts;
- add the remark *"Seat the through-hole sockets (J1, J2, J5, J6) flush and square before soldering."*

Standard PCBA adds a setup fee, a stencil and a hand-soldering charge for the through-hole parts; the quote shows them before you pay. The boards arrive in their frames. Snap each one out by bending the frame, never the board, and never lever against the USB-C connector or the switches. File the small tab stubs flush.

**Before you confirm the production files,** compare the bottom placement with [`fab/diy-thermocam-v4.0-assembly-bottom.pdf`](fab/diy-thermocam-v4.0-assembly-bottom.pdf). The pin-1 marks of `U2`, `U3`, `U4`, `D1`, `D2`, `Q1` and `Q2` must line up. Rotations of bottom-side parts are the most common assembly mistake.

**Do not substitute** these parts; their heights, footprints or timing are part of the design:

- the spring pins `P1`–`P4`, the sockets `J1`, `J2`, `J5` and `J6`, the USB-C connector `J3` and the inductor `L1`.

## What you plug in

- **Teensy 4.1 with pins** into `J5`/`J6`, as it comes. Leave its five inner pads empty: spring pins `P3` (`VBAT`, clock supply) and `P4` (`Program`, PROG button) press on them from below. If pins are soldered there, cut them flush underneath first; never force the Teensy down.
- **FLIR Lepton 3.5** (or 3.1R or 2.5) snapped into a **FLIR Lepton Breakout Board V2.0 (`250-0577-00`, Rev R200)**, fitted onto a **Sullins `PPPC102LJBN-RC`** right-angle 2×10 female header (3.15 mm tails; do not substitute shorter-tail headers). Check the breakout label on arrival: older Rev R120 boards had D1 fitted backwards and cannot power from `J2` pin 2. Mount the breakout on four M2×18 screws with 12 mm sleeves plus three 0.3 mm M2 nylon washers (`003.03.010`) on each sleeve (= 12.9 mm spacing) and 1.2 mm ISO 4035 M2 thin nuts (`002.09.012`), so the right-angle header's bends clear `J2` even at worst-case socket height (+0.24 mm) while its legs seat 3.8 mm deep into `J2`.
- **BuyDisplay `ER-TFTM032-3`** into `J1`, from the display side.
- **LiPo battery** (Adafruit 258, or a `505060` cell; JST-PH) into `J4`. Check that its `+` wire matches the `+` mark first.

The full parts list with shops is in [`hardware/bom/`](../bom/README.md).

## What is on the board

| Function | How it works |
| --- | --- |
| USB-C | Charges the battery and carries USB data. Two spring pins (`P1`, `P2`) press on the USB pads under the Teensy, so the Teensy's own micro-USB port stays unused. Never plug a cable into it while the board is powered. |
| Charging | TI BQ24074 powers the camera over USB while charging the battery (about 0.45 A charge current via `R10 = 2.0 kΩ`, strictly under Adafruit 258's 500 mA limit; red `CHG` LED). Its USB input limit is 1.3 A nominal (up to 1.45 A worst case), so use a charger or port rated 1.5 A or more; a 500 mA hub port can cut out. |
| 5 V supply | TI TPS61022 boost converter. The slide switch `SW1` turns the camera off; the battery still charges while it is off. |
| Battery protection | `Q1`/`Q2` block a battery plugged in the wrong way round; `F1` is a 2 A resettable fuse. |
| Clock | An XC6206 regulator keeps the Teensy's clock running from the battery through spring pin `P3`, so no coin cell is needed. |
| Buttons | `SW2` capture button at the board edge; `SW3` PROG button behind a pinhole for firmware recovery, wired to the Teensy through spring pin `P4`. |
| Touch | Resistive touch works as delivered. A capacitive-touch display needs a soldering iron: cut `JP1`/`JP2` between pads 1–2, bridge pads 2–3, and close `JP3`. |

## First power-up

1. Main board only, battery connected, `SW1` on: 5.0 V (4.9–5.1 V) between `VIN` and `GND` on the Teensy socket. `SW1` off: 0 V.
2. Plug in USB-C: the red `CHG` LED lights while the battery charges.
3. With the Teensy plugged in, connect USB-C to a computer: the Teensy shows up as a USB device. This confirms `P1`/`P2` touch its USB pads.
4. Press PROG through the pinhole: Teensy Loader shows the Teensy in bootloader mode. This confirms `P4`.
5. Set the clock, switch off with `SW1` for a minute and on again: the time kept running. This confirms `P3`.
6. Plug in the FLIR Lepton Breakout Board V2.0 (with the Lepton snapped in) via the right-angle 2×10 female header into `J2`, switch on: the live thermal image appears.

## Change the design

The KiCad 10 project is [`diy-thermocam-v4.0`](diy-thermocam-v4.0.kicad_sch) with its symbol and footprint libraries ([`V40.kicad_sym`](V40.kicad_sym), [`V40.pretty/`](V40.pretty/), [`V40.3dshapes/`](V40.3dshapes/)). Part numbers live in [`hardware/bom/bom.csv`](../bom/bom.csv).

After a change, check the board and rebuild the order files:

```bash
kicad-cli sch erc --severity-error --exit-code-violations hardware/pcb/diy-thermocam-v4.0.kicad_sch
kicad-cli pcb drc --refill-zones --schematic-parity --severity-error --exit-code-violations hardware/pcb/diy-thermocam-v4.0.kicad_pcb
python3 hardware/pcb/scripts/export_fab.py
```
