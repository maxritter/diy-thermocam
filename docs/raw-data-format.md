# Raw Data Format (`.DAT`)

Every still image and video or time-lapse frame saved by DIY-Thermocam stores 14-bit radiometric sensor data in a binary `.DAT` file. The layout stays 100% compatible across V1, V2, V3, and V4 cameras, Desktop Studio, and legacy Windows tools (`ThermoVision`, `Thermal Data Viewer`).

Reference firmware writer: [`firmware/src/thermal/save.cpp`](https://github.com/maxritter/diy-thermocam/blob/master/firmware/src/thermal/save.cpp).

## Valid File Sizes

Identify the sensor geometry and optional temperature-point block directly from file length:

| Sensor | Resolution | Without Temp Points | With 96 Temp Points (`+384 B`) |
| --- | --- | ---: | ---: |
| **FLIR Lepton 2.x** (`2.5`) | `80 × 60` (`4,800` px) | `9,621` bytes | `10,005` bytes |
| **FLIR Lepton 3.x** (`3.1R` / `3.5`) | `160 × 120` (`19,200` px) | `38,421` bytes | `38,805` bytes |

## Binary Field Layout

| Offset (Lepton 3.x) | Offset (Lepton 2.x) | Size (B) | Endianness / Type | Field | Description |
| ---: | ---: | ---: | --- | --- | --- |
| `0` | `0` | `38,400` / `9,600` | Big-endian `u16[]` | **Lepton raw values** | 14-bit radiometric pixel counts (`0..16383`), row-major `(0,0)` to `(W-1,H-1)`, MSB first then LSB. |
| `38,400` | `9,600` | `2` | Big-endian `u16` | **Min raw value** | Minimum raw count mapped to the bottom of the color scale (fixed in Manual range mode). |
| `38,402` | `9,602` | `2` | Big-endian `u16` | **Max raw value** | Maximum raw count mapped to the top of the color scale. |
| `38,404` | `9,604` | `4` | Little-endian `f32` | **Spot temperature** | Center spot temperature in °C or °F (matches `temp_format`). IEEE 754 float, LSB first. |
| `38,408` | `9,608` | `1` | `u8` | **Color scheme** | Palette index (`0..18`, see table below). |
| `38,409` | `9,609` | `1` | `u8` | **Temperature format** | `0` = Celsius (°C), `1` = Fahrenheit (°F). |
| `38,410` | `9,610` | `1` | `u8` | **Show spot** | `0` = hidden, `1` = visible. |
| `38,411` | `9,611` | `1` | `u8` | **Show color bar** | `0` = hidden, `1` = visible. |
| `38,412` | `9,612` | `1` | `u8` | **Show min/max** | `0` = off, `1` = min only, `2` = max only, `3` = min & max. |
| `38,413` | `9,613` | `4` | Little-endian `f32` | **Calibration offset** | Linear offset for raw-to-temperature conversion (IEEE 754 float, LSB first). |
| `38,417` | `9,617` | `4` | Little-endian `f32` | **Calibration slope** | Linear slope for raw-to-temperature conversion (IEEE 754 float, LSB first). |
| `38,421` | `9,621` | `384` *(opt.)* | `96 × (u16 BE, u16 BE)` | **Temperature points** | Up to 96 user-placed points: 2-byte 1-based pixel index (`0` = unused, `1` = `(0,0)`, `19200` = `(159,119)`) followed by 2-byte raw count. |

## Raw-to-Temperature Conversion

Convert any 14-bit raw pixel value to absolute temperature using the stored calibration slope and offset:

```text
absTemp = rawValue * calibrationSlope + calibrationOffset
```

On radiometric FLIR Lepton 2.5 and 3.5 sensors in TLinear high-gain mode, typical values are `calibrationSlope = 0.01` K/count and `calibrationOffset = -273.15` °C. A raw count of `30000` then equals `26.85 °C`.

## Color Scheme Indices

Defined in [`firmware/include/globaldefines.h`](https://github.com/maxritter/diy-thermocam/blob/master/firmware/include/globaldefines.h) and [`firmware/src/general/colorschemes.cpp`](https://github.com/maxritter/diy-thermocam/blob/master/firmware/src/general/colorschemes.cpp):

| ID | Name | ID | Name | ID | Name |
| ---: | --- | ---: | --- | ---: | --- |
| `0` | Arctic | `7` | Glowbow | `14` | Wheel 1 |
| `1` | Black-Hot | `8` | Grayscale | `15` | Wheel 2 |
| `2` | Blue-Red | `9` | Hottest | `16` | Wheel 3 |
| `3` | Coldest | `10` | Ironblack | `17` | White-Hot |
| `4` | Contrast | `11` | Lava | `18` | Yellow |
| `5` | Double-Rainbow | `12` | Medical | | |
| `6` | Gray-Red | `13` | Rainbow | | |

## Non-Destructive Sidecar Metadata (`.thermo.json`)

Desktop Studio leaves the `.DAT` byte layout untouched so edited captures stay readable on the camera itself. Desktop annotations (ROIs, per-ROI emissivity and reflected temperature overrides, isotherms, and report notes) save beside the capture in `<filename>.thermo.json`.
