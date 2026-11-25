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
| **PCB V3.0** | **Verified & buildable** | Teensy micro-USB + external TP4056 + Pololu `U3V16F5` boost + wired switch/button + CR1220 coin cell (cut Teensy `VUSB–VIN`) | 100% through-hole hand soldering | Proven baseline build with standard hand tools |
| **PCB V4.0 (Zero-Solder)** | **`PROTOTYPE-PENDING`** | Factory-populated **USB-C** + **TI `BQ24074RGTR`** dynamic power-path charger (`4.4 V VSYS`) + **TI `TPS61022RWUR`** 5 V boost + `AO3401A`/`AO3400A` active reverse-polarity protection + `XC6206P332MR-G` 3.3 V RTC LDO (no coin cell) + onboard slide switch & tactile capture button + `P1`/`P2` USB D+/D- pogo pins + JST-PH + factory-soldered female headers | **Zero soldering** — snap in Teensy 4.1 (w/ pins), Lepton Breakout V2.0 (Lepton 3.5 or 3.1R), BuyDisplay 3.2", and slim `505060` UL/IEC 62133 LiPo | Turnkey PCBA orders from JLCPCB, Seeed Studio Fusion, or PCBWay |

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

- **Legacy V3.0 Acrylic (verified):** 6-plate 3 mm laser-cut acrylic (archived at git tag [`3.0.4`](https://github.com/maxritter/diy-thermocam/tree/3.0.4/enclosure)). Open in Inkscape or send to any laser-cutting shop using 3 mm matte acrylic (e.g. Acryl GS Grey).
- **2026 Parametric `build123d` CAD (`PROTOTYPE-PENDING`):** Generates both an improved 6-plate laser-cut kit (**SVG / DXF**) and a **screwdriver-only 3D-printable / CNC shell** (**STEP / STL / 3MF**) using `4× M2.5×6` front + `4× M2.5×8` back DIN 965 countersunk screws, a blind-roofed `1/4"-20` metal hex-nut tripod pocket (max tripod stud `≤ 8.0 mm`, no heat-set iron needed), guided shutter plunger and captive slide-switch actuator, an insulating `battery_cradle` + `battery_lid` for a slim `≤5.0 mm` `505060` 2000 mAh LiPo, a widened `13.4→20.2 mm` cone clearing both **Lepton 3.5** (`71° DFOV`) and **Lepton 3.1R** (`95° HFOV / 119° DFOV`) with zero vignetting, support-free microSD chute, Teensy Prog pinhole, and a flush top-open dovetail lens cap.
- **Community 3D-printed 3.0b:** Originally contributed by [helmarw/DIY-Thermocam](https://github.com/helmarw/DIY-Thermocam/tree/master/Enclosure/3.0b) and incorporated into the 2026 parametric design.

### Recommended 3D Printing Settings

- **Material:** PETG or matte PLA (dark interior walls minimize stray reflections).
- **Layer height:** 0.2 mm, 3 perimeters, 20% gyroid infill.
- **Supports:** None required when printed face-down.
- **Cloud fabrication:** Upload the `.STEP` or `.STL` exports directly to **Seeed Studio Fusion 3D/CNC**, **JLCPCB 3D** (MJF Nylon or SLA Resin), or **PCBWay**.
