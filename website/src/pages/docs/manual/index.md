---
seo:
  type: stackbit_page_meta
  title: 'Manual Docs'
  description: ''
  robots: []
  extra: []
template: docs
title: Manual

---
This manual explains the features of the **DIY-Thermocam V3** hardware and software:

#### Charging

The LED on the top of the device indicates if the device is charging (red) or whether the charging process is finished (green). Use any microUSB cable to charge the device.

#### Firmware Update

The firmware on the DIY-Thermocam can be updated over the Teensyduino loader on Windows, Linux and Mac OSX operating systems. 

#### Mass Storage Mode (MTP)

Connect your Thermocam to a PC in live mode via USB to enter the file transfer / mass storage mode. You can copy the raw data and BMP files as well as videos there.

#### Live Mode

- Press the push button on top of the device short (<1s) to save an image to the storage.
- Press it long (>1s) to start capturing a video or to create a series of interval images with adjustable delay.
- Touching the screen long in auto mode locks the current temperature range. Whenever manual limits have been set as presets in the main menu, you can toggle between them in manual mode.
- In the middle of the screen, you can see the object temperature from the single-point infrared sensor. On the right side of the screen, the color bar shows the relation between colors and absolute temperatures. More information can be enabled in the live display options in the main menu.
- If you connect the device to a PC with the USB cable you can enter the USB serial connection at any time when in live mode by using a compatible program like the Thermal Live Viewer. Touch the screen to close the connection and return to live mode.

#### Main Menu

The main menu can be accessed by touching the screen in live mode. 

- **Navigation**: Use the left and right arrow to navigate through the menu screens.
- **Return**: Press this button to exit the main menu and return to live mode.
- **Temperature points**: Add or remove live temperature reading points for every position inside the image.
- **Change limits**: You can choose between two modes: ‘Auto’ automatically finds the hottest and coldest value in every image and distributes the colors from the selected scheme evenly between them. ‘Manual’ lets you choose the minimum and maximum temperature, between which the color scheme is applied. 
- **Hot / cold mode**: Only available in thermal mode. Mark the hottest or coldest temperatures with an adjustable color. You can also select the level, above or under the marking should be done.
- **Load menu**: Here you can view all images and videos stored on the internal storage or SD card. You can navigate through the files by pressing the little arrows on the left and right side. The newest image or video will always load first. You can play / pause a video by touching the screen in the middle. There are also four additional options: Find (search an image or video by its time and date), convert (convert an image or video to bitmap file), delete (remove an image or video from the internal storage) and exit.
- **Settings menu**: It allows you to adjust the following presets: Display (change the temperature format, the display rotation and the screen timeout), Storage (format the internal storage, enable / disable the conversion of bitmap images or enable / disable the capture of a visual image for every saved thermal image) and Other (set time & date, calibrate battery gauge).
- **Change color**: Switch between the different color scales applied to the thermal image. There are eighteen different color schemes stored on the device, such as ‘Rainbow’ or ‘Ironblack’. 
- **Live display options**: You can change what information is shown in live mode: Battery on/off, time on/off, date on/off, spot on/off, bar on/off, hottest/coldest point detection (off, cold, hot or both), storage on/off, filter (Gaussian, box or disabled), text color (white, black, red, green or blue).
- **Trigger shutter**: Manually close and open the shutter on top of the FLIR Lepton sensor, in order to remove noise from the raw data and make the image uniform again. 
- **Display**: Turn the display off in order to save battery, while preserving all settings as well as the calibration data. Touch the screen again to reactivate it. 