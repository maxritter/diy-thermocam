*** DIY-Thermocam Video Output Module Firmware ***

INSTALLATION

-Update the firmware on your DIY-Thermocam to version 2.38 or greater

-Open the black enclosure of the video output module
-Unplug the microSD card and put it into a card reader on your PC
-Open the "boot" partition showing up in your workspace
-Replace the two files "__init__.py" and "config.txt" with the files included in this archive
-Put the microSD card back into the Pi Zero and close the enclosure 


CHANGELOG

2.02
-Save a frame to the sd card of the Thermocam by pressing the save button on the device

2.01
-Added detection for HQ resolution on DIY-Thermocam V2

2.00
-Added compatibility with the DIY-Thermocam V2
-Now the display framebuffer is transmitted and shown

1.03
-Fixed a problem with device-freezing
-Updated the communication protocol
