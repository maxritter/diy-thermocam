The standalone firmware updater only runs on Windows XP or greater.

You can also use the Teensy Loader, in case the firmware updater does not work for you. 

In the unlikely case that the update failed and the device does not start any more, load the firmware file again and start the flash procedure, the status should display “Waiting for device...”. Then short the pins P & G on the Teensy for one second  (https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/TeensyProgram.png) on the V1 or the PROGRAM jumper on the V2, while the device is turned on an connected to the computer. Afterwards, the flash starts.

For more information, check out the firmware update guide inside the document folder.

If you want to flash the hex files on Linux or Mac OSX, use the Teensyduino uploader from https://www.pjrc.com/teensy/td_download.html.
