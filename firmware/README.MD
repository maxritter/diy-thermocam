# DIY-Thermocam - Firmware

## THIS GUIDE IS ONLY REQUIRED, IF YOU WANT TO MAKE CHANGES TO THE FIRMWARE ON YOUR OWN

This guide should work on **all common operating systems** (**Windows, Linux & macOS**).

**Download** and **install** the following programs:

1. [VS Code](https://code.visualstudio.com/)
2. [PlatformIO Core](https://docs.platformio.org/en/latest//core/installation.html)
3. [PlatformIO for VS Code](https://platformio.org/install/ide?install=vscode)

The **Teensyduino** version that comes pre-installed with PlatformIO does not support Mass Storage Mode (MTP), so we **need to update it**. Extract the content of [platformio_teensy4.zip](3.0/other/platformio_teensy4.zip) to: `C:\Users\<YOUR_ACCOUNT>\.platformio` (filepath on Windows or macOS is different, I am using Windows) and overwrite any existing files.

Now **start VS Code** and open the folder that you have just cloned with **File -> Open Folder**. 

**PlatformIO should initialize itself automatically** and you see the buttons to **Build, Upload and Clean** the project in the blue bar **at the bottom**. 

**Before you click "Upload"**, make sure the Thermocam is **connected to the PC** via a micro USB cable and **turned on**. The **Teensy CLI** should then automatically detect it and **flash the .hex file to the board**. After a restart the changes should be present on the device.

