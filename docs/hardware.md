# Hardware and Fabrication

The board, schematic, BOM and enclosure files are in [`hardware/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware) under GPL-3.0. Order the two assembled boards from JLCPCB, Seeed Studio Fusion or PCBWay. Print the shell or have the acrylic case laser-cut, yourself or through a service.

## V4 hardware

> **V4 is experimental and still in development.** The V4 board and shell haven't been built and tested on a real camera yet. You're welcome to try them, but there's no guarantee they work. Already have a V3? It can update its firmware and use Studio today.

![Exploded render of DIY-Thermocam V4: front shell, display, board with modules, battery and back shell](/images/v4/exploded.jpg)

![KiCad render of the V4 board, module side, with the Teensy, Lepton and battery sockets](/images/v4/pcb-modules.png)

## Schematic

The full V4.0 schematics are in [`hardware/pcb/fab/diy-thermocam-v4.0-schematic.pdf`](https://github.com/maxritter/diy-thermocam/blob/master/hardware/pcb/fab/diy-thermocam-v4.0-schematic.pdf) (main board) and [`hardware/pcb/fab/diy-thermocam-v4.0-lepton-schematic.pdf`](https://github.com/maxritter/diy-thermocam/blob/master/hardware/pcb/fab/diy-thermocam-v4.0-lepton-schematic.pdf) (Lepton carrier).

## Teensy 4.1 pin map

| Teensy 4.1 Pin | DIY-Thermocam Function |
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

Two boards, both **1.6 mm FR-4**, **2-layer**, 1 oz copper, Lead-Free HASL or ENIG finish:

- **Main board, 89.4 × 68.4 mm:** USB-C, TI `BQ24074` charger, TI `TPS61022` boost, reverse-polarity protection, `XC6206` RTC supply, SMD switches.
- **Lepton carrier, 29.2 × 47.0 mm:** holds the Lepton in a Molex socket and makes its 1.2 V, 2.8 V and 3.0 V supplies and 25 MHz clock, like FLIR's Breakout V2. Its pin header plugs straight into `J2`.
- **Assembly:** no soldering. The fab fits every part on both boards; you plug in the Teensy 4.1, Lepton, display and LiPo.

### Ordering the assembled boards

1. Review the interactive [Bill of Materials](/docs/partlist/).
2. Download the package for your fab from [`hardware/pcb/fab/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/pcb/fab): `jlcpcb/`, `seeed/`, or `pcbway/`. Each holds a Gerber zip, BOM and CPL file for the main board (`diy-thermocam-v4.0-*`) and for the carrier (`diy-thermocam-v4.0-lepton-*`). The JLCPCB Gerbers hold each board in a break-off frame, because JLCPCB assembles both sides only on panels of at least 70 × 70 mm.
3. Place one assembly order per board at **JLCPCB**, **Seeed Studio Fusion**, or **PCBWay**. At JLCPCB, pre-order the carrier's Lepton socket (`C585422`, minimum 9) in the parts manager first; it is usually out of stock.
4. Use the order settings in the [PCB README](https://github.com/maxritter/diy-thermocam/tree/master/hardware/pcb#order-the-boards): *Panel by Customer* at JLCPCB, the remarks, production-file confirmation, and the placement checks before you approve.

## Shell and case

Two enclosures come from one parametric [build123d](https://github.com/gumyr/build123d) model in [`hardware/enclosure/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/enclosure). Both hold the same board stack, so the hardware list and most build steps are shared.

| Printed shell | Laser-cut acrylic case |
| --- | --- |
| ![The printed shell from the back, lens cap open](/images/v4/hero-back.jpg) | ![The laser-cut acrylic case from the front](/images/v4/laser-hero.jpg) |
| ![Every part of the printed build laid out](/images/v4/kit.jpg) | ![Every part of the acrylic build laid out](/images/v4/laser-kit.jpg) |

- **Printed shell:** a sliding lens cap, capture button, switch slider and a captive 1/4"-20 tripod nut. Each part comes as STEP, STL and 3MF, plus a print plate with every part at the quantity one camera needs.
- **Acrylic case:** six interlocking plates of 3 mm black cast acrylic, with the button and switch keys, plus a spare of each, cut from the same sheet (`SVG` or `DXF`). Order the sheet as cast (GS) and ask the service to confirm it measures 2.8–3.2 mm before cutting. The parts take 213.1 × 195.6 mm of sheet, which is Formulor's P2 format (384 × 384 mm). Blue RGB(0, 0, 255) hairlines are cuts and red RGB(255, 0, 0) hairlines the optional logo engraving on the back plate, as Formulor's file rules ask; other services may use other colours. It also needs a few small printed parts: the battery cradle and its two battery spacers, the tripod mount, two display spacers and the microSD chute. To order them from a print service, unzip `diy-thermocam-v4.0-acrylic-files.zip`, which also holds the laser sheet, and upload every file in its `step/` folder; `ORDER.txt` gives the quantities.
- **Battery floor and lid (both builds):** two bare FR4 boards, a 0.6 mm floor in the cradle and a 0.4 mm lid on top. Order them from a PCB maker with the Gerbers in `hardware/enclosure/exports/fr4/`: no copper, black solder mask.
- **Tripod and USB-C:** in both cases the 1/4"-20 tripod thread sits in the middle of the bottom face, and the USB-C port is 23 mm to its left, so a tripod plate up to 30 mm wide, centred on the screw, leaves the cable free.

The [build guide](/building-instructions/) shows every step for both, and the [parts list](/docs/partlist/) has the files each print or laser-cutting service needs.

### Printing the shell

- **From a print service (recommended):** order MJF PA12, dyed black. Unzip `diy-thermocam-v4.0-shell-print-files.zip` and upload every file in its `step/` folder: one part per file, each once, as JLC3DP and most services want. Every wall is at least 1.0 mm thick and every raised or engraved detail at least 0.8 mm, JLC3DP's MJF minimums.
- **At home:** PETG, 0.4 mm nozzle, 0.2 mm layers, 3–4 walls. In the orientation of the plate file, `back_shell`, `lens_cap` and `battery_cradle` need supports touching the build plate only: the back shell's lens-cap track is a 28 × 58 mm bridge, and most of the lens cap's and the cradle's undersides start above the plate. The other parts print without supports.

The parametric shell takes ergonomic ideas from the community [3.0b enclosure](https://github.com/helmarw/DIY-Thermocam/tree/master/Enclosure/3.0b) by helmarw.
