---
seo:
  type: stackbit_page_meta
  title: 'Serial Protocol Docs'
  description: ''
  robots: []
  extra: []
template: docs
title: USB Serial Protocol

---

#### **Version 1.0 from 30.06.21 - Requires DIY-Thermocam V3 and firmware version >= 3.0.0**

This guide describes how you can communicate with your **DIY-Thermocam V3** over the **USB serial connection**. 

The **maximum transfer speed is 12 Mbit/s and is always active**, no matter what baud rate you use to connect to the device from your PC. This allows you to receive thermal frames with high speed.  The **device needs to be turned on, connected to the PC over USB and set to live mode**.  The protocol uses **big endian** for transmission of multibyte values, which means MSB first, then LSB.

To see how the **protocol is implemented inside the firmware** in C, check out [this file](https://github.com/maxritter/diy-thermocam/blob/master/firmware/3.0/src/hardware/connection.cpp). The [Thermal Live Viewer](https://github.com/maxritter/diy-thermocam/tree/master/software/thermal_live_viewer/) can be used on any Windows PC to use the protocol and communicate to the device. 

## Commands 

Those are the commands that can be send from the PC to the DIY-Thermocam over the Serial Connection. Sometimes they provide additional information in the data field. An explanation about the different options is given in the "Payload Data" section below.

Each command returns either its own command ID as response if it was executed successfully (ack) and does not return any additional data or 0x00 if it failed to execute (nack). In case any special data is returned, it is described in the return data column.

| Command | Name               | Payload Length | Payload Data                                                 | Return Length         | Return Data                                                  | Description                                                  |
| ------- | ------------------ | -------------- | ------------------------------------------------------------ | --------------------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| 0x64    | SetStart           | 0              | -                                                            | 1                     | ack / nack                                                   | Set the device into serial mode and waits for the next command from the list |
| 0xC8    | SetEnd             | 0              | -                                                            | 1                     | ack / nack                                                   | Exit serial mode and set the device back to live mode        |
| 0x6E    | GetRawLimits       | 0              | -                                                            | 4                     | minTemp MSB, minTemp LSB, maxTemp MSB, maxTemp LSB           | Get the lepton raw limits (14-bit) for the next frame        |
| 0x6F    | GetRawData         | 0              | -                                                            | 9600 / 38400          | 4800 / 19200 * (rawData[i] MSB, rawData[i] LSB)              | Get the lepton raw values for the next frame, 9600 byte (Lepton2) or 38400 byte (Lepton3); 14-bit raw values (0 … 16383) for each pixel, equivalent to infrared intensity |
| 0x70    | GetConfigData      | 0              | -                                                            | 10                    | see Config Data below                                        | Get the configuration data of the device, that does not change if not adjusted manually |
| 0x72    | GetCalibData       | 0              | -                                                            | 8                     | two float, split to four bytes each, LSB first, MSB at the end | Get the calibration data from the Lepton (offset and slope)  |
| 0x73    | GetSpotTemp        | 0              | -                                                            | 4                     | Float split into four bytes, LSB first, then MSB             | Get the spot temperature from the Lepton, in Fahrenheit or Celcius |
| 0x75    | GetTempPoints      | 0              | -                                                            | 384                   | 96 points (96 x 4), 2 bytes index, 2 bytes raw value, MSB first, then LSB | Get the temperature points that are shown on the screen      |
| 0x78    | SetShutterRun      | 0              | -                                                            | 1                     | ack / nack                                                   | Run the shutter flat-field-correction (FFC) on the Lepton    |
| 0x79    | SetShutterMode     | 1              | uint8_t - shutter_mode (see Shutter Mode below)              | 1                     | ack / nack                                                   | Set the Lepton shutter FFC mode to automatic or manual       |
| 0x7A    | SetFilterType      | 1              | uint8_t filter_type (see Filter Type below)                  | 1                     | ack / nack                                                   | Set the type of filter used to improve the image quality     |
| 0x7C    | GetBatteryStatus   | 0              | -                                                            | 1                     | 0 = 0% (empty), 100 = 100% (fully charged)                   | Get the current battery status in percentage                 |
| 0x7F    | GetDiagnostic      | 0              | -                                                            | 1                     | ack / nack                                                   | Get the diagnostic information containing the hardware status |
| 0x81    | GetFirmwareVersion | 0              | -                                                            | 2                     | FWVersion MSB, FWVersion LSB                                 | Get the current firmware version installed on the device     |
| 0x82    | SetLimitType       | 1              | uint8_t limit_type (see Limit Type below)                    | 1                     | ack / nack                                                   | Set limit type to limit locked or automatic mode             |
| 0x83    | SetTextColor       | 1              | uint8_t text_color (see Text Color below)                    | 1                     | ack / nack                                                   | Set the text color used to display the elements in live mode |
| 0x84    | SetColorScheme     | 1              | uint8_t color_scheme (see Color Scheme below)                | 1                     | ack / nack                                                   | Set current color scheme used for the conversion             |
| 0x85    | SetTempFormat      | 1              | uint8_t temp_format (see Temperature Format below)           | 1                     | ack / nack                                                   | Set the temperature format to Celsius or Fahrenheit          |
| 0x86    | SetShowSpot        | 1              | uint8_t show_spot (see Show Spot below)                      | 1                     | ack / nack                                                   | Show spot temperature information in the image or not        |
| 0x87    | SetShowColorbar    | 1              | uint8_t show_colorbar (see Show Colorbar below)              | 1                     | ack / nack                                                   | Show color bar information in the image or not               |
| 0x88    | SetShowMinMax      | 1              | uint8_t show_minmax (see Show MinMax below)                  | 1                     | ack / nack                                                   | Show the hottest, coldest or both points in the image or not |
| 0x89    | SetTempPoints      | 384            | 96 points (96 x 4 Byte): 2 byte index, 2 bytes value (see Temperature Points below) | 1                     | ack / nack                                                   | Set the temperature points to be shown on the screen (up to 96). The first two bytes are the index value that corresponds to the position in the thermal image (0 = (0,0) … 19200 = (159, 119)). The second two bytes are either set or not, depending on the point being disabled or enabled |
| 0x8A    | GetHwVersion       | 0              | -                                                            | 1                     | see Hardware Version below                                   | Get the hardware version of the device                       |
| 0x8B    | SetRotation        | 1              | uint8_t rotation (see Rotation below)                        | 1                     | ack / nack                                                   | Sets the display rotation on the device                      |
| 0x96    | GetRawFrame        | 0              | -                                                            | 1 + 9600 / 38400 + 16 | see Raw Frame below                                          | Get a raw frame containing the raw data and additional information  (160x120) |
| 0x97    | GetColorFrame      | 0              | -                                                            | 1 + 9600 / 38400 + 16 | same as Raw Frame, but raw values are converted to RGB565 and filter is applied | Get a color frame containing RGB565 colors that represent the thermal image (160x120) |
| 0x98    | GetDisplayFrame    | 0              | -                                                            | 1 + 153600            | commandID is the same as for the Raw Frame, followed by RGB565 colors (2 byte) for each pixel on the display | Get a display frame containing RGB565 colors that represent the content of the display (320x240) |
| 0x99    | GetFrameSave       | 0              | -                                                            | 1                     | ack / nack                                                   | Save a frame + converted image (if activated)] to the internal storage of the Thermocam |

## Payload Data 

### Shutter Mode

| Value | Description |
| ----- | ----------- |
| 0x00  | Manual      |
| 0x01  | Automatic   |

### Filter Type

| Value | Description   |
| ----- | ------------- |
| 0x00  | Disabled      |
| 0x01  | Gaussian Blur |
| 0x02  | Box Blur      |

### Limit Type

| Value | Description              |
| ----- | ------------------------ |
| 0x00  | Limits locked            |
| 0x01  | Auto mode, limits adjust |

### Text Color

| Value | Description |
| ----- | ----------- |
| 0x00  | White       |
| 0x01  | Black       |
| 0x02  | Red         |
| 0x03  | Green       |
| 0x04  | Blue        |

### Color Scheme

| Value | Description    |
| ----- | -------------- |
| 0x00  | Arctic         |
| 0x01  | Black-Hot      |
| 0x02  | Blue-Red       |
| 0x03  | Coldest        |
| 0x04  | Contrast       |
| 0x05  | Double-Rainbow |
| 0x06  | Gray-Red       |
| 0x07  | Glowbow        |
| 0x08  | Grayscale      |
| 0x09  | Hottest        |
| 0x0A  | Ironblack      |
| 0x0B  | Lava           |
| 0x0C  | Medical        |
| 0x0D  | Rainbow        |
| 0x0E  | Wheel 1        |
| 0x0F  | Wheel 2        |
| 0x10  | Wheel 3        |
| 0x11  | White-Hot      |
| 0x12  | Yellow         |

### Temperature Format

| Value | Description |
| ----- | ----------- |
| 0x00  | Celsius     |
| 0x01  | Fahrenheit  |

### Show Spot

| Value | Description |
| ----- | ----------- |
| 0x00  | Disabled    |
| 0x01  | Enabled     |

### Show Colorbar

| Value | Description |
| ----- | ----------- |
| 0x00  | Disabled    |
| 0x01  | Enabled     |

### Show MinMax

| Value | Description                     |
| ----- | ------------------------------- |
| 0x00  | Disabled                        |
| 0x01  | Minimum temperature only        |
| 0x02  | Maximum temperature only        |
| 0x03  | Minimum and maximum temperature |

### Temperature Points

| Value | Description                     |
| ----- | ------------------------------- |
| 0x00  | Point on this index is disabled |
| 0x01  | Point on this index is enabled  |

### Rotation

| Value | Description        |
| ----- | ------------------ |
| 0x00  | Normal Orientation |
| 0x01  | 180° Rotated       |

## Return Data

#### **Config Data**

- leptonVersion (1 Byte): 0 = Lepton2 shutter, 1 = Lepton3 shutter, 2 = Lepton2 no shutter
- rotationEnabled (1 Byte): 0 = Normal orientation, 1 = Device rotated by 180 degree
- colorScheme (1 Byte): Number definitions can be looked up under Color Scheme above
- tempFormat (1 Byte): 0 = Celsius, 1 = Fahrenheit
- spotEnabled (1 Byte): 0 = Disabled, 1 = Spot temperature displayed
- colorbarEnabled (1 Byte): 0 = Disabled, 1 = Colorbar displayed
- minMaxEnabled (1 Byte): 0 = Disabled, 1 = Min, 2 = Max, 3 = Both
- textColor (1 byte): 0 = White, 1 = Black, 2 = Red, 3 = Green, 4 = Blue
- filtertype (1 byte): 0 = Disabled, 1 = Gaussian blur, 2 = Box blur
- adjustLimits (1 Byte): 0 = Manual mode, limits fixed, 1 = Auto mode, limits adjust  

### Hardware Version

| Value | Description      |
| ----- | ---------------- |
| 0x01  | DIY-Thermocam V1 |
| 0x02  | DIY-Thermocam V2 |
| 0x03  | DIY-Thermocam V3 |

### Raw Frame

- commandId (1 byte): 

  | Value | Description                                                  |
  | ----- | ------------------------------------------------------------ |
  | 0xB4  | Push button was pressed short; user wants to save a thermal image  (only used in Thermal Live Viewer) |
  | 0xB5  | Touch screen was pressed short; user wants to save a visual image  (only used in Thermal Live Viewer) |
  | 0xB7  | No button pressed: normal frame, followed by the byte stream  (should be used by default) |

- rawData (9600 (Lepton2.5) / 38400 (Lepton3.5) byte): 14-bit raw values (0 … 16383) for each pixel, equivalent to infrared intensity

- rawLimits (4 byte): minTemp MSB, minTemp LSB, maxTemp MSB, maxTemp LSB  

- spotTemp (4 byte): Float split into four bytes, LSB first, then MSB 

- calibData (8 byte): Two float offset and slope, split to four bytes each, LSB first, MSB at the end  

