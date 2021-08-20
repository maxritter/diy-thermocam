**Firmware Updater**

To flash the firmware to the DIY-Thermocam, use the Teensy Loader that works under Windows, Linux and Mac OSX.

First get the newest version of the firmware from [here](https://github.com/maxritter/DIY-Thermocam/releases) and extract the .hex file from the zip archive to your computer.

For Windows, the executables are available in this folder. Load the hex file with "teensy.exe", then launch "teensy_reboot.exe" while the Thermocam is connected to the PC and turned it on. The flash procedure should start. 

For Linux and Mac OSX download "Teensyduino" from [here](https://www.pjrc.com/teensy/td_download.htm) and flash the firmware in a similar way than for Windows.

In the unlikely case that the update failed and the device **does not start any more**, press the push button on the Teensy to put it into bootloader mode, then try to flash again.

In case you get an error during flash that the **file is too big**, consider one of the following options:

- The USB cable only has charging capabilities but no data connections, you need another one
- You are using a Teensy Loader Version older than 1.53. Either download the newest version for Windows / Mac / Linux from [PJRC](https://www.pjrc.com/teensy/td_download.html) or use the Windows Exe from [here](https://github.com/maxritter/DIY-Thermocam/raw/master/Software/Firmware Updater/teensy.exe)
- You need to flash a small hex file first and then load the Thermocam Firmware afterwards. Try [this hex file](https://forum.pjrc.com/attachment.php?s=6e010ffce2ab01a14e514c1e9063589f&attachmentid=23272&d=1610630749)
