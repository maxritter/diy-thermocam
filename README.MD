<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/Logo.png" width="500">
<br>

The **DIY-Thermocam** is a **do-it-yourself** infrared camera, based on the popular **FLIR Lepton** long-wave-infrared array sensor.  The **material price** of all components is at about **300€**, which makes the device the **cheapest standalone solution** for thermal imaging.

The aim of this project is to give **private persons**, **educational institutes** and **companies** access to a **portable**,  **affordable** and **customizable** thermal imaging plattform.
 
There are **various applications** like finding **heat leaks** in the insulation of buildings, the analysis of **electrical** or **mechanical components**, the **detection** of **persons** / animals or even mounting it on a **drone**, together with the additional **video out module**.
 
Constructed as a **self-assembly solution**, the DIY-Thermocam allows you to take **advantage** of the **versatile possibilities** of thermal imaging and to **save money** at one go.
 
**Everything**, from **software** to **hardware**, is completely **open-source**! This allows everyone to **modify** or **extend** the functionalities of the device to their **own needs**!

**Donate** to **support this project**. In return, will receive the **newest revision** of the **printed circuit board** by post. All **other components** can be bought from **distributors** on the **bill of material list** (BOM) inside the document folder. The **enclosure** can be created with a **3D printer** or a **laser cutter**.

**[Donate &  Get your free PCB](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=MSX6ULDY5KKA)**

----------

**Features:**

 - Fast ARM Cortex M4 processor (240MHz), based on the popular, Arduino
   compatible Teensy 3.6
 - 160 x 120 pixel FLIR Lepton3 long-wave infrared array sensor for live
   thermographic images
 - Frame rate of up to 9 FPS (US export compliance) over the serial
   connection, 5 FPS on the device itself
 - 2 MP  visual camera to capture optical images, that can be used in a
   combined image
 - MLX90614 single point-infrared sensor for high-precision spot
   temperatures (10° FOV)
 - HDMI or analog video output  capabilities (640x480 pixel) over
   external video output module
 - 3 operating modes: thermal only, thermal + visual, video recording
 - 18 different color schemes including rainbow, ironblack, grayscale,
   hot & cold
 - 3.2 inch LCD touch display with bright colors,  wide angle and resistive touch
 - Save thermal and optical images with a resolution of 640x480 pixels
   on the device
 - Save real-time videos and interval images with different time-lapse
   settings
 - 8 GB internal storage, accessible as an exchangeable SD / microSD
   slot
 - 2500 mAh lithium polymer battery for long operation time (4-6 hours)
 - Open-source firmware written in Arduino compatible C/C++ code
 - Regular firmware updates with new features, flashable over a
   standalone firmware updater
 - Standalone thermal viewer application to save high-quality thermal
   images & videos  on the computer
 - Fully compatible to the comprehensive thermal analysis software
   ThermoVision by Joe-C
 - Use simple commands to receive all thermal & configuration data over
   the USB serial port with high speed

----------

**Comparison Table**

<table align="" style="border: 0px solid #000000;" class="mceEditable" data-mce-contenteditable="true" border="0" cellpadding="3" cellspacing="0" height="100%" width="100%">
 <tbody>
 <tr>
 <td style="background-color: #cccccc;">
 <strong>Feature</strong>
 </td>
 <td style="border: 0px solid transparent; background-color: #cccccc;">
 <b>DIY-Thermocam V2</b>
 </td>
 <td style="background-color: #cccccc;">
 <strong>FLIR E6</strong>
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Thermal resolution
 </td>
 <td>
 160 x 120
 </td>
 <td>
 160 x 120
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Thermal sensitivity <span style="color: #ff0000;"><strong>*</strong></span>
 </td>
 <td>
 &lt; 0.05° C (50 mK)
 </td>
 <td>
 &lt; 0.06° C (60 mK)
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Thermal temp. range
 </td>
 <td>
 -40° C to 200° C
 </td>
 <td>
 <span style="line-height: 26.6667px;">-20° C to 250° C</span>
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Field-of-view (FOV)
 </td>
 <td>
 56 deg HFOV, 71 deg diagonal 
 </td>
 <td>
 <span><span style="line-height: 26.6667px;">45 deg × 34 deg</span></span>
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Display
 </td>
 <td>
 3.2" 320x240 , touch input
 </td>
 <td>
 3.0" 320x240, no touch input
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Spot sensor temp. range
 </td>
 <td>
 -70° C to 380° C
 </td>
 <td>
 -20° C to 250° C
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Spot sensor temp. accuracy <strong><span style="color: #ff0000;">*</span></strong>
 </td>
 <td>
 0.5° C over wide range
 </td>
 <td>
 ±2 ° C or ±2%
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Temp. measurement mode
 </td>
 <td>
 every position, multiple positions
 </td>
 <td>
 spot (center) mode
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Image modes
 </td>
 <td>
 IR image, visual image, combined
 </td>
 <td>
 IR image, visual image. MSX
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Color schemes
 </td>
 <td>
 18 different color schemes
 </td>
 <td>
 rainbow, iron, grayscale
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Storage mode
 </td>
 <td>
 picture and video
 </td>
 <td>
 picture only
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Storage capacity
 </td>
 <td>
 8 GB internal storage
 </td>
 <td>
 500 sets of images
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 File format
 </td>
 <td>
 standard BMP and raw data
 </td>
 <td>
 standard JPEG and raw data
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Operation time
 </td>
 <td>
 about 4-6 hours
 </td>
 <td>
 about 4 hours
 </td>
 <td>
 
 </td>
 </tr>

 <tr>
 <td>
 Weight
 </td>
 <td>
 255g
 </td>
 <td>
 575g
 </td>
 <td>
 
 </td>
 </tr>
 </tbody>
</table>

----------

**Device Screenshots**
<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/Screenshots.PNG" width="700">