# DIY-Thermocam V3 Firmware #

GNU General Public License v3.0, copyright by Max Ritter

**The following hardware is supported:**

- **DIY-Thermocam V3** (2021) - Lepton2.5 / Lepton3.5 only

  

----------

**THIS GUIDE IS ONLY REQUIRED, IF YOU WANT TO MAKE CHANGES TO THE FIRMWARE ON YOUR OWN!**

If you just want to flash the newest version of the firmware to your device, follow the Firmware Update Guide in the Document section and use one of the pre-compiled versions (*.hex) from **[here](https://github.com/maxritter/DIY-Thermocam/releases)**.

----------

This guide should work on **all common operating systems** (**Windows, Linux & Mac OSX**).

**Download** and **install** the following programs:

1. [VS Code](https://code.visualstudio.com/)
2. [PlatformIO for VS Code](https://platformio.org/install/ide?install=vscode)

Then **clone this repo**:
`git clone https://github.com/maxritter/diythermocam_firmware.git`.

The **Teensyduino** version that comes pre-installed with PlatformIO does not support Mass Storage Mode (MTP), so we **need to update it**. Extract the content of `other/platformio_teensy4.zip` to: `C:\Users\<YOUR_ACCOUNT>\.platformio\packages\framework-arduinoteensy` (filepath on Windows or Mac is different, I am using Windows) and overwrite any existing files.

Now **start VS Code** and open the folder that you have just cloned with **File -> Open Folder**. 

**PlatformIO should initialize itself automatically** and you see the buttons to **Build, Upload and Clean** the project in the blue bar **at the bottom**. 

**Before you click "Upload"**, make sure the Thermocam is **connected to the PC** via a micro USB cable and **turned on**. The **Teensy CLI** should then automatically detect it and **flash the .hex file to the board**. After a restart the changes should be present on the device.

