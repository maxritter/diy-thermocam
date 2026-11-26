# Hardware and Fabrication

All PCB, schematic, BOM, and enclosure files live in [`hardware/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware) under **GPL-3.0**. Order the factory-assembled board from **JLCPCB**, **Seeed Studio Fusion**, or **PCBWay**, and print or laser-cut the enclosure yourself or order it from the same services.

## V4 hardware

> **V4 is experimental and still in development.** The V4 board and shell haven't been built and tested on a real camera yet. You're welcome to try them, but there's no guarantee they work. Already have a V3? It can update its firmware and use Studio today.

![Exploded render of DIY-Thermocam V4: front shell, display, board with modules, battery and back shell](/images/v4/exploded.jpg)

![KiCad render of the V4 board, module side, with the Teensy, Lepton and battery sockets](/images/v4/pcb-modules.png)

## Schematic

The full V4.0 schematic is in [`hardware/pcb/fab/diy-thermocam-v4.0-schematic.pdf`](https://github.com/maxritter/diy-thermocam/blob/master/hardware/pcb/fab/diy-thermocam-v4.0-schematic.pdf).

## Teensy 4.1 Pinmap

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

### Ordering the PCBA

1. Review the interactive [Bill of Materials](/docs/partlist/).
2. Download the package for your fab from [`hardware/pcb/fab/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/pcb/fab): `jlcpcb/`, `seeed/`, or `pcbway/`, each with a Gerber zip, BOM, and CPL file.
3. Upload the package to **JLCPCB**, **Seeed Studio Fusion**, or **PCBWay**.

## Enclosures (Laser-Cut Acrylic and 3D/CNC Shell)

![DIY-Thermocam V4.0 3D-printable and CNC enclosure CAD renders](/images/build-v4/v4-3d-enclosure-hero.png)

![DIY-Thermocam V4.0 6-plate 3 mm laser-cut acrylic enclosure and cutting sheet](/images/build-v4/v4-laser-enclosure-preview.png)

Enclosure sources and exports live in [`hardware/enclosure/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/enclosure):

- **V4.0 parametric [build123d](https://github.com/gumyr/build123d) CAD:** Generates a screwdriver-only 3D-printed shell (`STEP`, `STL`, `3MF`) and a 6-plate 3 mm acrylic case (`SVG`, `DXF`) on one M2.5 standoff stack. Includes a `1/4"-20` tripod pocket (max stud `≤ 8.0 mm`), insulating `505060` battery cradle, dovetail lens cap, and wide `13.4→20.2 mm` lens cone clearing Lepton 3.5 (`71°`) and 3.1R (`119°`). See the enclosure README for fastener order numbers.
- **Community 3.0b Enclosure:** Originally contributed by [helmarw/DIY-Thermocam](https://github.com/helmarw/DIY-Thermocam/tree/master/Enclosure/3.0b), inspiring ergonomic details of the parametric shell.

### Recommended 3D Printing Settings

- **Material:** PETG (recommended) or matte PLA with dark interior walls to reduce reflections.
- **Layer height:** 0.2 mm, 3–4 walls, 20–25% gyroid infill.
- **Supports:** None required when printed in the exported `Z = 0` orientation.
- **Cloud fabrication:** Upload the `.step` or `.stl` exports to **JLCPCB 3D**, **Seeed Studio Fusion**, or **PCBWay** (MJF PA12 Nylon or CNC aluminum).
