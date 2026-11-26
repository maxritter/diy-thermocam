# Hardware and Fabrication

The board, schematic, BOM and enclosure files are in [`hardware/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware) under GPL-3.0. Order the assembled board from JLCPCB, Seeed Studio Fusion or PCBWay. Print the shell or have the acrylic case laser-cut, yourself or through a service.

## V4 hardware

> **V4 is experimental and still in development.** The V4 board and shell haven't been built and tested on a real camera yet. You're welcome to try them, but there's no guarantee they work. Already have a V3? It can update its firmware and use Studio today.

![Exploded render of DIY-Thermocam V4: front shell, display, board with modules, battery and back shell](/images/v4/exploded.jpg)

![KiCad render of the V4 board, module side, with the Teensy, Lepton and battery sockets](/images/v4/pcb-modules.png)

## Schematic

The full V4.0 schematic is in [`hardware/pcb/fab/diy-thermocam-v4.0-schematic.pdf`](https://github.com/maxritter/diy-thermocam/blob/master/hardware/pcb/fab/diy-thermocam-v4.0-schematic.pdf).

## Teensy 4.1 pin map

| Teensy 4.1 Pin | DIY-Thermocam Function |
| --- | --- |
| `D2` | Capture push button (active low) |
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

Board outline: **89.4 × 68.4 mm**, **1.6 mm FR-4**, **2-layer**, 1 oz copper, Lead-Free HASL or ENIG finish.

- **Power, USB and controls:** USB-C, TI `BQ24074` charger, TI `TPS61022` boost, reverse-polarity protection, `XC6206` RTC supply, SMD switches.
- **Assembly:** no soldering. The fab fits every part; you plug in the Teensy 4.1, Lepton, display and `505060` LiPo.

### Ordering the assembled board

1. Review the interactive [Bill of Materials](/docs/partlist/).
2. Download the package for your fab from [`hardware/pcb/fab/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/pcb/fab): `jlcpcb/`, `seeed/`, or `pcbway/`, each with a Gerber zip, BOM, and CPL file.
3. Upload the package to **JLCPCB**, **Seeed Studio Fusion**, or **PCBWay**.

## Shell and case

Two enclosures come from one parametric [build123d](https://github.com/gumyr/build123d) model in [`hardware/enclosure/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/enclosure). Both hold the same board stack, so the hardware list and most build steps are shared.

| Printed shell | Laser-cut acrylic case |
| --- | --- |
| ![The printed shell from the back, lens cap open](/images/v4/hero-back.jpg) | ![The laser-cut acrylic case from the front](/images/v4/laser-hero.jpg) |
| ![Every part of the printed build laid out](/images/v4/kit.jpg) | ![Every part of the acrylic build laid out](/images/v4/laser-kit.jpg) |

- **Printed shell:** a sliding lens cap, capture button, switch slider and a captive 1/4"-20 tripod nut. Each part comes as STEP, STL and 3MF, plus a print plate with every part at the quantity one camera needs.
- **Acrylic case:** six interlocking plates of 3 mm black cast acrylic, with the button and switch keys cut from the same sheet (`SVG` or `DXF`, 213 × 195 mm). It also needs a few small printed parts: the battery cradle, its lid and spacers, the tripod mount, two display spacers and the microSD chute.

The [build guide](/building-instructions/) shows every step for both, and the [parts list](/docs/partlist/) has the files each print or laser-cutting service needs.

### Printing the shell

- **From a print service (recommended):** order MJF PA12, dyed black. At JLC3DP, upload the one service-plate file; it holds each part once, with the four cradle spacers joined by break-off tabs. Snip the tabs and file the nubs flush.
- **At home:** PETG, 0.4 mm nozzle, 0.2 mm layers, 3–4 walls. Print `back_shell` with supports touching the build plate only: the lens-cap track is a 28 × 58 mm bridge. The other parts print without supports in the orientation of the plate file.

The parametric shell takes ergonomic ideas from the community [3.0b enclosure](https://github.com/helmarw/DIY-Thermocam/tree/master/Enclosure/3.0b) by helmarw.
