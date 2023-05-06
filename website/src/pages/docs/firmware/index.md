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

To flash the firmware to the DIY-Thermocam, use the Teensy Loader CLI that works under Windows, Linux and Mac OSX.

First get the newest version of the firmware from [here](https://https://github.com/maxritter/diy-thermocam/releases) and extract the .hex file from the zip archive to your computer.

Next, download the Teensy Load CLI from [here](https://github.com/PaulStoffregen/teensy_loader_cli) and install it according to the instructions. Remove the backside of the device and press the button on the Teensy microcontroller while it is connected to your PC via USB to put it into programmer mode:

![](https://www.diy-thermocam.net/images/manual/flash.png)

 The flash procedure can be started like this:

```
teensy_loader_cli -w --mcu=TEENSY41 firmware.hex
```

Afterwards, you should see the device rebooting with the logo on the screen. Turn it off to continue with the last steps.

## Development

You need to **setup the development IDE** if you want to make your **own changes** to the [open-source firmware of the DIY-Thermocam](https://https://github.com/maxritter/diy-thermocam/tree/master/firmware). This guide should work on **all common operating systems** (**Windows, Linux & macOS**).

**Download** and **install** the following programs:

1. [VS Code](https://code.visualstudio.com/)
2. [PlatformIO Core](https://docs.platformio.org/en/latest//core/installation.html)
3. [PlatformIO for VS Code](https://platformio.org/install/ide?install=vscode)

The **Teensyduino** version that comes pre-installed with PlatformIO does not support Mass Storage Mode (MTP), so we **need to update it**. Extract the content of [platformio_teensy4.zip](3.0/other/platformio_teensy4.zip) to: `C:\Users\<YOUR_ACCOUNT>\.platformio` (filepath on Windows or macOS is different, I am using Windows) and overwrite any existing files.

Now **start VS Code** and open the folder that you have just cloned with **File -> Open Folder**. 

**PlatformIO should initialize itself automatically** and you see the buttons to **Build, Upload and Clean** the project in the blue bar **at the bottom**. 

**Before you click "Upload"**, make sure the Thermocam is **connected to the PC** via a micro USB cable and **turned on**. The **Teensy CLI** should then automatically detect it and **flash the .hex file to the board**. After a restart the changes should be present on the device.

