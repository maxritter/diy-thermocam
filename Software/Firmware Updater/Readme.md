The standalone firmware updater only runs on Windows XP or greater.

You can also use the Teensy Loader, in case the firmware updater does not work for you. 

In the unlikely case that the update failed and the device does not start any more, short the pins P & G on the Teensy for 1s: https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/TeensyCut.png or short the PROGRAM jumper on the PCB of the DIY-Thermocam V2.

If you want to flash the hex files on Linux or Mac OSX, use the Teensyduino uploader from https://www.pjrc.com/teensy/td_download.html.
