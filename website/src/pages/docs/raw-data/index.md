---
seo:
  type: stackbit_page_meta
  title: 'Raw Data Docs'
  description: ''
  robots: []
  extra: []
template: docs
title: Raw Data Structure
weight: 
excerpt: ''

---

This guide describes how to interpret the raw data files (*.DAT), which are created by the device for each saved image and video / time-lapse frame on the internal storage. A sample .DAT file can be found [here](https://github.com/maxritter/DIY-Thermocam/blob/master/Software/Thermal%20Analysis%20Software/Sample.DAT) with the corresponding thermal images linked [here](https://github.com/maxritter/DIY-Thermocam/blob/master/Software/Thermal%20Analysis%20Software/Sample.JPG).

The structure of the file is define as following - see “method “SaveRawData” [here](https://github.com/maxritter/DIY-Thermocam/blob/master/Firmware_V3/src/thermal/save.cpp) as reference: 

- **Lepton raw values** - 9600 byte (Lepton2) or 38400 byte (Lepton3) 
  - 4800 / 19200 raw values for the Lepton2 / Lepton3, split to two bytes, MSB first, then LSB 
  - Raw value is 14-bit (0 … 16383) for each pixel, equivalent to infrared intensity 
  - Difference can be made by the total file size (10005 byte Lepton2 vs. 38805 Lepton3) 
- **Minimum temp** - 2 byte 
  - 14-bit minimum raw value to apply the color scheme, could be fixed if in manual mode o Split to two bytes, MSB first, then LSB. Reconstruction analog to the lepton raw values 
- **Maximum temp** - 2 byte 
  - 14-bit maximum raw value, same format than the minimum temperature value 
- **Spot sensor temp** - 4 byte 
  - Object temperature measured by the spot sensor; Float, split to four bytes, LSB first
  - Format is Celsius or Fahrenheit, depending on the set temperature format 
- **Color scheme** - 1 byte 
  - Number of the color scheme, the image was created with
  - Number definitions can be looked up in [this file](https://github.com/maxritter/DIY-Thermocam/blob/master/Firmware_V3/include/globaldefines.h) under "Color scheme numbers"
- **Temperature format** - 1 byte 
  - The temperature format is applied to all absolute temperatures 
  - Celsius (0) or Fahrenheit (1) 
- **Show spot** - 1 byte
  - Display the MLX90614 spot temperature information in an image or not 
  - Disabled (0) or Enabled (1) 
- **Show color bar** - 1 byte 
  - Display the radiometric color bar in an image or not 
  - Disabled (0) or Enabled (1) 
- **Show minimum / maximum point** - 1 byte 
  - Show the minimum and / or maximum temperature point(s) in an image or not 
  - Disabled (0), Min only (1), Max only (2), Min & Max (3) 
- **Calibration offset** - 4 byte 
  - Offset for the raw-to-abs temperature conversion 
  - Float, split to four bytes, LSB first. Reconstruction analog to the spot sensor temp 
- **Calibration slope** - 4 byte 
  - Slope for the raw-to-abs temperature conversion 
  - Float, split to four bytes, LSB first. Reconstruction analog to the spot sensor temp 

The absolute temperature can be calculated out of each raw value by using the following formula: 
`absTemp = rawValue * calibrationSlope + calibrationOffset` 

- **Temperature points** - 384 byte 
  - 96 different temperature points, containing the index position and lepton raw value 
  - They are manually defined by the user and could be skipped if not utilized 
  - The first two bytes are the index position, second two bytes are the lepton raw value 
  - Index position: (0,0) = 1; (159, 119) = 19200; 0 = not set, otherwise set; MSB first, then LSB