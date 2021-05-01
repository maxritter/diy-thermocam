# DIY-Thermocam V2 Firmware #

GNU General Public License v3.0, copyright by Max Ritter

**The following hardware is supported:**

- **DIY-Thermocam V2** (2017) - Lepton2.0 / Lepton2.5 / Lepton3.0 / Lepton3.5

----------

**THIS GUIDE IS ONLY REQUIRED, IF YOU WANT TO MAKE CHANGES TO THE FIRMWARE ON YOUR OWN!**

If you just want to flash the newest version of the firmware to your device, follow the Firmware Update Guide in the Document section and use one of the pre-compiled versions (*.hex) from **[here](https://github.com/maxritter/DIY-Thermocam/releases)**.

----------

This guide should work on **all common operating systems** (**Windows, Linux & Mac OSX**).

**Download** and **install** the following programs:

1. **[Arduino 1.8.6+](https://www.arduino.cc/en/Main/Software)**
2. **[Teensyduino 1.43+](https://www.pjrc.com/teensy/td_download.html)**
3. **[Sloeber Eclipse](https://github.com/Sloeber/arduino-eclipse-plugin/releases/tag/4_2)**

Then **clone this repo** to a path without empty spaces (f. ex. "C:\Sandbox\DIYThermocam_Firmware\") with `git clone https://github.com/maxritter/diythermocam_firmware.git`.

Now **start Sloeber Eclipse**.

Go to Windows -> Preferences -> Arduino and **set the Private Library Path** to the folder of your Arduino library installation (f. ex. "C:\Program Files (x86)\Arduino\libraries"). Also **set the Private Hardware Path** to the path of your Arduino hardware installation (f. ex. "C:\Program Files (x86)\Arduino\hardware").

**Import the project** with File -> Import -> General -> Existing Projects into Workspace, select the root directory of this repo and click Finish.

Now you should be able to build the project with Arduino -> Verify and upload it to the Thermocam with Arduino -> Upload Sketch. Have fun!