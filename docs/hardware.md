# Hardware and Fabrication

All PCB, schematic, BOM, and enclosure files live in [`hardware/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware) under **GPL-3.0**. Build every piece yourself with a home 3D printer, laser cutter, and soldering iron, or order factory-assembled boards and shells from **JLCPCB**, **Seeed Studio Fusion**, or **PCBWay**.

## Schematic

![DIY-Thermocam V3 schematic](/images/device/schematic.jpg)

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

## PCB Revisions (V3.0 Classic vs. V4.0 Zero-Solder)

Board outline: **89.4 × 68.4 mm**, **1.6 mm FR-4**, **2-layer**, 1 oz copper, Lead-Free HASL or ENIG finish.

| Revision | Status | Power, USB &amp; Controls | Assembly | Best For |
| --- | --- | --- | --- | --- |
| **PCB V3.0** | **Proven default** (tag `3.0.4`) | Teensy micro-USB, TP4056 charger, Pololu `U3V16F5` boost, wired switch/button, CR1220 coin cell (cut `VUSB–VIN`) | Through-hole hand soldering | Proven build with standard hand tools |
| **PCB V4.0** | **`PROTOTYPE-PENDING`** | USB-C, TI `BQ24074`, TI `TPS61022`, polarity safety, `XC6206` RTC, SMD controls | **Zero soldering** — plug in Teensy 4.1, Lepton, Display, and `505060` LiPo | Turnkey PCBA (JLCPCB, Seeed, PCBWay) |

> **Prototype gate:** V3.0 remains the proven default build until the V4.0 PCBA and 3D-printed enclosure complete physical verification.

### Ordering Bare PCBs or Turnkey PCBA

1. Review the interactive [Bill of Materials](/docs/partlist/) and pick your revision.
2. Download the Gerbers (plus BOM and CPL files for V4.0 PCBA) from [`hardware/pcb/fab/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/pcb/fab). Archived V3.0 Eagle files live at git tag [`3.0.4`](https://github.com/maxritter/diy-thermocam/tree/3.0.4/pcb).
3. Upload the fabrication package to **JLCPCB**, **Seeed Studio Fusion**, or **PCBWay**.

## Enclosures (Laser-Cut Acrylic and 3D/CNC Shell)

Enclosure sources and exports live in [`hardware/enclosure/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/enclosure):

- **V3.0 Acrylic (proven baseline):** 6-plate 3 mm laser-cut acrylic archived at git tag [`3.0.4`](https://github.com/maxritter/diy-thermocam/tree/3.0.4/enclosure). Send to any laser-cutting shop using 3 mm matte cast acrylic.
- **Parametric `build123d` CAD (`PROTOTYPE-PENDING`):** Generates both a 6-plate 3 mm laser-cut kit (`SVG`, `DXF`) and a screwdriver-only 3D/CNC shell (`STEP`, `STL`, `3MF`). Fastens with `4× M2.5×6` front and `4× M2.5×8` back DIN 965 countersunk screws, adds a blind-roofed `1/4"-20` hex-nut tripod pocket (max stud `≤ 8.0 mm`), guided actuators, insulating `505060` battery cradle and lid, dovetail lens cap, and a widened `13.4→20.2 mm` cone clearing both Lepton 3.5 (`71° DFOV`) and Lepton 3.1R (`119° DFOV`).
- **Community 3.0b Enclosure:** Originally contributed by [helmarw/DIY-Thermocam](https://github.com/helmarw/DIY-Thermocam/tree/master/Enclosure/3.0b), inspiring ergonomic details of the parametric shell.

### Recommended 3D Printing Settings

- **Material:** PETG (recommended) or matte PLA with dark interior walls to reduce reflections.
- **Layer height:** 0.2 mm, 3–4 walls, 20–25% gyroid infill.
- **Supports:** None required when printed in the exported `Z = 0` orientation.
- **Cloud fabrication:** Upload the `.step` or `.stl` exports to **JLCPCB 3D**, **Seeed Studio Fusion**, or **PCBWay** (MJF PA12 Nylon or CNC aluminum).
