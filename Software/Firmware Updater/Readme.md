**Firmware Updater**

To flash the firmware to the DIY-Thermocam, use the Teensy Loader that works under Windows, Linux and Mac OSX.

First get the newest version of the firmware from [here](https://github.com/maxritter/DIY-Thermocam/releases) and extract the .hex file from the zip archive to your computer.

For Windows, the executables are available in this folder. Load the hex file with "teensy.exe", then launch "teensy_reboot.exe" while the Thermocam is connected to the PC and turned it on. The flash procedure should start. 

For Linux and Mac OSX download "Teensyduino" from [here](https://www.pjrc.com/teensy/td_download.htm) and flash the firmware in a similar way than for Windows.

In the unlikely case that the update failed and the device **does not start any more**, press the push button on the Teensy to put it into bootloader mode, then try to flash again.