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

## Flashing

To flash the firmware to the DIY-Thermocam, use the Teensy Loader that works under Windows, Linux and Mac OSX.

First get the newest version of the firmware from [here](https://github.com/maxritter/DIY-Thermocam/releases) and extract the .hex file from the zip archive to your computer.

For Windows, the two executables are available in [this folder](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Firmware%20Updater). Load the hex file with "teensy.exe", then launch "teensy_reboot.exe" while the Thermocam is connected to the PC and turned it on. The flash procedure should start. 

For Linux and Mac OSX download "Teensyduino" from [here](https://www.pjrc.com/teensy/td_download.htm) and flash the firmware in a similar way than for Windows.

In the unlikely case that the update failed and the device **does not start any more**, press the push button on the Teensy to put it into bootloader mode, then try to flash again.

## Development

You need to **setup the development IDE** if you want to make your **own changes** to the [open-source firmware of the DIY-Thermocam](https://github.com/maxritter/DIY-Thermocam/tree/master/Firmware_V3). This guide should work under all operating systems.

**Download** and **install** the following programs:

1. [VS Code](https://code.visualstudio.com/)
2. [PlatformIO Core](https://docs.platformio.org/en/latest//core/installation.html)
3. [PlatformIO for VS Code](https://platformio.org/install/ide?install=vscode)

Then **clone this repo**:
`git clone https://github.com/maxritter/DIY-Thermocam/tree/master/Firmware_V3`.

The **Teensyduino** version that comes pre-installed with PlatformIO does not support Mass Storage Mode (MTP), so we **need to update it**. Extract the content of `other/platformio_teensy4.zip` to: `C:\Users\<YOUR_ACCOUNT>\.platformio` (filepath on Windows or Mac is different, I am using Windows) and overwrite any existing files.

Now **start VS Code** and open the folder that you have just cloned with **File -> Open Folder**. 

**PlatformIO should initialize itself automatically** and you see the buttons to **Build, Upload and Clean** the project in the blue bar **at the bottom**. 

**Before you click "Upload"**, make sure the Thermocam is **connected to the PC** via a micro USB cable and **turned on**. The **Teensy CLI** should then automatically detect it and **flash the .hex file to the board**. After a restart the changes should be present on the device.