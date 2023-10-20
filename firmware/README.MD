# DIY-Thermocam - Firmware

### THIS GUIDE IS ONLY REQUIRED, IF YOU WANT TO MAKE CHANGES TO THE FIRMWARE ON YOUR OWN

In case you want to flash the newest version of the firmware to your device without modification, **go to the release section of this repo**.

This guide should work on **all common operating systems** (**Windows, Linux & macOS**).

You need to have [VS Code](https://code.visualstudio.com/) with the [PlatformIO extension](https://platformio.org/install/ide?install=vscode) installed and download the zip archive of this repository. Then **unpack the 3.0 folder inside the firmware subfolder**, **start VS Code** and open this folder  with **File -> Open Folder**. 

**PlatformIO should initialize itself automatically** and you see the buttons to **Build, Upload and Clean** the project in the blue bar **at the bottom**. If this is not the case, **set it up manually using the instructions provided [here](https://platformio.org/install/ide?install=vscode)**.

**Before you click "Upload"**, make sure the Thermocam is **connected to the PC** via a micro USB cable and **turned on**. The **Teensy CLI** should then automatically detect it and **flash the .hex file to the board**. After a restart the changes should be present on the device.
