---
seo:
  type: stackbit_page_meta
  title: 'Hardware Docs'
  description: ''
  robots: []
  extra: []
template: docs
title: Hardware
weight: 
excerpt: ''

---
## Schematic

![](https://www.diy-thermocam.net/images/Schematic.JPG)

## Pinmap

| Teensy 4.1 Pin | Thermocam V3 Function          |
| -------------- | ------------------------------ |
| D2             | Push Button                    |
| D3             | FLIR Lepton VSync              |
| D5             | Touch Screen Interrupt         |
| D9             | Touch Screen Chip Select (SPI) |
| D10            | Touch Screen DC (SPI)          |
| D11            | SPI0 MOSI                      |
| D12            | SPI0 MISO                      |
| D13            | SPI0 SCK                       |
| D18            | I2C SDA                        |
| D19            | I2C SDL                        |
| D21            | LCD Chip Select (SPI)          |
| D22            | LCD Backlight Enable           |
| D23            | Battery Measure                |
| D26            | SPI1 MOSI                      |
| D27            | SPI1 SCK                       |
| D38            | FLIR Lepton Chip Select (SPI)  |
| D39            | SPI1 MISO                      |
| A16            | USB Voltage Measure            |

## PCB

The printed circuit board has been created with **[Autosoft Eagle](http://www.autodesk.com/education/free-software/eagle)**. Use this software to open the .sch (Schematic) or .brd (Board) files.

You can also use the Gerber.zip and upload it to any PCB service of your choice to manufacture the board, for example [PCB Protoyping](http://www.smart-prototyping.com/PCB-Prototyping.html). Dimensions are 89.4mm (w) x 68.4mm (h), 1.6mm thickness and 2 layers.

[![FRONT](https://github.com/maxritter/DIY-Thermocam/raw/master/PCB/3.0/FRONT.PNG)](https://github.com/maxritter/DIY-Thermocam/blob/master/PCB/3.0/FRONT.PNG)

[![FRONT](https://github.com/maxritter/DIY-Thermocam/raw/master/PCB/3.0/BACK.PNG)](https://github.com/maxritter/DIY-Thermocam/blob/master/PCB/3.0/BACK.PNG)

**[Get PCB Revision 3.0](https://github.com/maxritter/DIY-Thermocam/tree/master/PCB/3.0)**

## Enclosure

The enclosure has been designed with **[Google Sketchup](https://www.sketchup.com/plans-and-pricing/sketchup-free)** in 3D and converted to 2D files to laser-cut them. The 2D SVG file can be opened with **[Inkscape](https://inkscape.org/)**.

**[Get Enclosure Revision 3.0](https://github.com/maxritter/DIY-Thermocam/tree/master/Enclosure/3.0)**

helmarw has created an updated design for 3D printing, than can be found **[here](https://github.com/helmarw/DIY-Thermocam/tree/master/Enclosure/3.0b)**:

![](https://user-images.githubusercontent.com/10408121/118656304-b2819880-b7ea-11eb-9f83-297ed9089c39.jpg)

