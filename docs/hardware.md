# Hardware & Fabrication

All PCB, schematic, BOM, and enclosure files live in the `hardware/` directory of the [diy-thermocam monorepo](https://github.com/maxritter/diy-thermocam/tree/master/hardware) under **GPL-3.0**. You can manufacture every piece yourself (home 3D printer, laser cutter, hand soldering) or order factory-assembled boards and 3D-printed shells directly from **Seeed Studio Fusion**, **JLCPCB**, or **PCBWay**.

## Schematic

![DIY-Thermocam V3 schematic](/images/device/schematic.jpg)

## Teensy 4.1 Pinmap

| Teensy 4.1 Pin | DIY-Thermocam Function |
|---|---|
| `D2` | Capture Push Button (active low) |
| `D3` | FLIR Lepton VSync interrupt |
| `D5` | Resistive Touch Screen interrupt |
| `D9` | Touch Screen Chip Select (`SPI0`) |
| `D10` | Display Data/Command (`SPI0`) |
| `D11` | `SPI0` MOSI (Display & Touch) |
| `D12` | `SPI0` MISO (Touch) |
| `D13` | `SPI0` SCK (Display & Touch) |
| `D18` | `I2C` SDA (Lepton CCI) |
| `D19` | `I2C` SCL (Lepton CCI) |
| `D21` | TFT LCD Chip Select (`SPI0`) |
| `D22` | LCD Backlight Enable |
| `D23` | Battery Voltage Divider ADC |
| `D26` | `SPI1` MOSI (Lepton VoSPI) |
| `D27` | `SPI1` SCK (Lepton VoSPI) |
| `D38` | FLIR Lepton Chip Select (`SPI1`) |
| `D39` | `SPI1` MISO (Lepton VoSPI) |
| `A16` | USB Power Sense Divider ADC |

## PCB Revisions (V3.0 Verified vs. V4 Zero-Solder PCBA)

Board outline: **89.4 × 68.4 mm**, **1.6 mm FR-4**, **2-layer**, 1 oz copper, HASL or ENIG finish.

| Revision | Status | Power, USB & Controls | Assembly Style | Best For |
|---|---|---|---|---|
| **PCB V3.0** | **Verified & buildable** | Teensy micro-USB + external TP4056 + Pololu boost + wired switch/button (cut Teensy `VUSB–VIN`) | 100% through-hole hand soldering | Proven baseline build with standard hand tools |
| **PCB V4.0 (Zero-Solder)** | **Prototype-pending** | Factory-populated **USB-C** + **TI BQ24075** power-path charger + onboard **5V boost** + onboard slide switch & tactile capture button + JST-PH + factory-soldered female headers | **Zero soldering** — snap in Teensy 4.1 (w/ pins), Lepton Breakout V2.0, and BuyDisplay 3.2" | Turnkey PCBA orders from Seeed Studio Fusion, JLCPCB, or PCBWay |

> **Prototype gate:** V3.0 remains the default verified build until the V4.0 zero-solder PCBA and 3D-printed enclosure complete physical assembly and bring-up testing.

### Ordering Bare PCBs or Turnkey PCBA

1. Review the interactive [Bill of Materials](/docs/partlist/) and choose your hardware revision.
2. Download the Gerber/Excellon (plus BOM and CPL pick-and-place files for PCBA) from [`hardware/pcb/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/pcb) (native KiCad 9/10 sources plus archived Eagle V3.0 files).
3. Upload the fabrication package directly to:
   - **Seeed Studio Fusion** — one-stop PCBA + 3D printing / CNC enclosure in one shipment.
   - **JLCPCB** — standard 2-layer FR-4 (`89.4 × 68.4 mm`) + LCSC PCBA + MJF/SLA 3D printing.
   - **PCBWay** — bare PCB, PCBA, or CNC/SLS enclosure fabrication.

## Enclosures (Laser-Cut Acrylic & Screwdriver-Only 3D Shell)

Enclosure sources live in [`hardware/enclosure/`](https://github.com/maxritter/diy-thermocam/tree/master/hardware/enclosure):

- **Legacy V3.0 Acrylic (verified):** 6-plate 3 mm laser-cut acrylic (`hardware/enclosure/legacy-v3.0/enclosure.svg`). Open in Inkscape or send to any laser-cutting shop using 3 mm matte acrylic (e.g. Acryl GS Grey).
- **2026 Parametric `build123d` CAD (prototype-pending):** Generates both an improved 6-plate laser-cut kit (**SVG / DXF**) and a **screwdriver-only 2-piece 3D-printable / CNC shell** (**STEP / STL / 3MF**) with captive M2.5 and 1/4"-20 hex nut pockets (no heat-set soldering iron needed), a 3D-printed shutter button plunger, widened Lepton aperture (≥ 8.5 mm to eliminate top-edge thermal vignetting), aligned USB & LED ports, Teensy Prog pinhole, and an integrated snap-fit lens cap.
- **Community 3D-printed 3.0b:** Originally contributed by [helmarw/DIY-Thermocam](https://github.com/helmarw/DIY-Thermocam/tree/master/Enclosure/3.0b) and incorporated into the 2026 parametric design.

### Recommended 3D Printing Settings

- **Material:** PETG or matte PLA (dark interior walls minimize stray reflections).
- **Layer height:** 0.2 mm, 3 perimeters, 20% gyroid infill.
- **Supports:** None required when printed face-down.
- **Cloud fabrication:** Upload the `.STEP` or `.STL` exports directly to **Seeed Studio Fusion 3D/CNC**, **JLCPCB 3D** (MJF Nylon or SLA Resin), or **PCBWay**.
