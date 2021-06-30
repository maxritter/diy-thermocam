---
seo:
  type: stackbit_page_meta
  title: Firmware Docs
  description: ''
  robots: []
  extra: []
template: docs
title: Firmware

---

# Flashing

# Development

You need to setup the development IDE if you want to make your own changes to the open-source firmware of the DIY-Thermocam. 

Download and install the following programs:

VS Code
PlatformIO Core
PlatformIO for VS Code
Then clone this repo: git clone https://github.com/maxritter/DIY-Thermocam/tree/master/Firmware_V3.

The Teensyduino version that comes pre-installed with PlatformIO does not support Mass Storage Mode (MTP), so we need to update it. Extract the content of other/platformio_teensy4.zip to: C:\Users\<YOUR_ACCOUNT>\.platformio (filepath on Windows or Mac is different, I am using Windows) and overwrite any existing files.

Now start VS Code and open the folder that you have just cloned with File -> Open Folder.

PlatformIO should initialize itself automatically and you see the buttons to Build, Upload and Clean the project in the blue bar at the bottom.

Before you click "Upload", make sure the Thermocam is connected to the PC via a micro USB cable and turned on. The Teensy CLI should then automatically detect it and flash the .hex file to the board. After a restart the changes should be present on the device.