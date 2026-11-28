# Hardware and fabrication

Board, schematic, BOM and enclosure files are in [`hardware/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware), under [CC BY-NC-SA 4.0](https://github.com/maxritter/diy-thermocam/blob/master/hardware/LICENSE).

- Build and change them for non-commercial use.
- Selling boards, kits or cameras needs permission.

## V4 hardware

> **V4 is experimental.** Its board and shell are not yet tested on a real camera. A V3 can run the new firmware and Studio today.

![Exploded render of DIY-Thermocam V4: front shell, display, board with modules, battery and back shell](/images/v4/exploded.jpg)

![KiCad render of the V4 board, module side, with the Teensy, Lepton and battery sockets](/images/v4/pcb-modules.png)

## Schematic

[diy-thermocam-v4.0-schematic.pdf](https://github.com/maxritter/diy-thermocam/blob/master/hardware/pcb/fab/diy-thermocam-v4.0-schematic.pdf)

## Teensy 4.1 pin map

| Pin | Function |
| --- | --- |
| `D2` | Capture push button (active high) |
| `D3` | FLIR Lepton VSync interrupt |
| `D5` | Resistive touch screen interrupt |
| `D9` | Touch screen Chip Select (`SPI0`) |
| `D10` | Display Data/Command (`SPI0`) |
| `D11` | `SPI0` MOSI (Display & Touch) |
| `D12` | `SPI0` MISO (Touch) |
| `D13` | `SPI0` SCK (Display & Touch) |
| `D18` | `I2C` SDA (Lepton CCI) |
| `D19` | `I2C` SCL (Lepton CCI) |
| `D21` | TFT LCD Chip Select (`SPI0`) |
| `D22` | LCD backlight enable |
| `D23` | Battery voltage divider ADC |
| `D26` | `SPI1` MOSI (Lepton VoSPI) |
| `D27` | `SPI1` SCK (Lepton VoSPI) |
| `D38` | FLIR Lepton Chip Select (`SPI1`) |
| `D39` | `SPI1` MISO (Lepton VoSPI) |
| `A16` | USB power sense divider ADC |

## PCB V4.0

One board, fully assembled by the fab. You plug in the Teensy 4.1, the FLIR Lepton on its Breakout Board V2.0 (via the right-angle 2×10 female header), the display and the LiPo; no soldering.

- **Size:** 89.4 × 68.4 mm, 1.6 mm FR-4, 2 layers, 1 oz copper, lead-free HASL or ENIG.
- **Carries:** USB-C, TI `BQ24074` charger, TI `TPS61022` boost, reverse-polarity protection, `XC6206` RTC supply, SMD switches, spring pins for Teensy USB/VBAT/PROG, and the sockets for the Teensy, Lepton Breakout V2.0 and display.

### Ordering the assembled board

1. Check the interactive [bill of materials](/docs/partlist/).
2. Download your fab's folder from [`hardware/pcb/fab/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/pcb/fab): `jlcpcb/`, `seeed/` or `pcbway/`.
3. Place one assembly order at JLCPCB, Seeed Studio Fusion or PCBWay.
4. Follow the order settings and checks in the [PCB README](https://github.com/maxritter/diy-thermocam/tree/master/hardware/pcb#order-the-board).

At JLCPCB:

- Choose *Panel by Customer* (1×1, no edge rails) and *Tooling holes: Added by Customer*.
- Every part comes from JLCPCB's own stock.

<details>
<summary>What each fab folder holds</summary>

- `diy-thermocam-v4.0-panel-gerbers.zip` (JLCPCB) or `diy-thermocam-v4.0-gerbers.zip` (Seeed, PCBWay), plus `diy-thermocam-v4.0-bom.csv` and `diy-thermocam-v4.0-cpl.csv`.

The JLCPCB Gerber puts the board in a 107.4 × 86.4 mm break-off frame. JLCPCB assembles both sides only on panels of at least 70 × 70 mm.

</details>

## Shell and case

Two enclosures for the same board stack, from one [build123d](https://github.com/gumyr/build123d) model in [`hardware/enclosure/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/enclosure).

| Printed shell | Laser-cut acrylic case |
| --- | --- |
| ![The printed shell from the back, lens cap open](/images/v4/hero-back.jpg) | ![The laser-cut acrylic case from the front](/images/v4/laser-hero.jpg) |
| ![Every part of the printed build laid out](/images/v4/kit.jpg) | ![Every part of the acrylic build laid out](/images/v4/laser-kit.jpg) |

- **Printed shell:** sliding lens cap, capture button, switch slider, captive 1/4"-20 tripod nut.
- **Acrylic case:** six plates of 3 mm black cast acrylic, plus button and switch keys.
- **Tripod:** the thread is centered on the bottom; USB-C sits 23 mm to its left.
- A tripod plate up to 30 mm wide, centered on the screw, leaves the cable free.

The [build guide](/building-instructions/) shows every step. The [parts list](/docs/partlist/) has the files for each service.

### Printing the shell

Each part comes as STEP, STL and 3MF, plus a print plate for one camera.

| | Print service (recommended) | At home |
| --- | --- | --- |
| Material | MJF PA12, dyed black | PETG |
| Settings | | 0.4 mm nozzle, 0.2 mm layers, 3–4 walls |

- For a service, unzip `diy-thermocam-v4.0-shell-print-files.zip`.
- Upload every file in its `step/` folder, each once.
- At home, print in the plate file's orientation.
- `back_shell`, `lens_cap` and `battery_cradle` need supports from the build plate only.
- The other parts need no supports.

### Cutting the acrylic case

- Cut the plates and keys, plus a spare of each, from one sheet (`SVG` or `DXF`).
- Order cast (GS) acrylic. Ask the service to confirm 2.8–3.2 mm before cutting.
- The parts take 213.1 × 195.6 mm: Formulor's P2 sheet (384 × 384 mm).
- Blue RGB(0, 0, 255) lines cut; red RGB(255, 0, 0) lines engrave the optional logo.
- Those are Formulor's colors. Other services may use others.

The case also needs small printed parts: the battery cradle, 2 battery spacers, the tripod mount, 2 display spacers and the microSD chute.

- Unzip `diy-thermocam-v4.0-acrylic-files.zip`. It also holds the laser sheet.
- Upload every file in its `step/` folder. `ORDER.txt` gives the quantities.

### Battery floor and lid

Both builds need two bare FR4 boards: a 0.6 mm floor and a 0.4 mm lid.

- Gerbers: `hardware/enclosure/exports/fr4/`
- Order from a PCB maker with no copper and black solder mask.

The shell takes ergonomic ideas from the community [3.0b enclosure](https://github.com/helmarw/DIY-Thermocam/tree/master/Enclosure/3.0b) by helmarw.
