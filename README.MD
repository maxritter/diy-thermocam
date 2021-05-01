<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/Logo.png" width="500">



The **DIY-Thermocam V3** is a **low-cost, do-it-yourself** thermal imager, based on the popular radiometric **FLIR Lepton** long-wave-infrared array sensor. 

<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/Thermocam.jpg" width="500">


The aim of this project is to give **private persons**, **educational institutes** and **companies** access to a **portable**,  **affordable** and **customizable** thermal imaging platform.

There are **various applications** like finding **heat leaks** in the insulation of buildings, the analysis of **electrical** or **mechanical components**, the **detection** of **persons** / animals or even mounting it on a **drone**, together with the additional **video out module**.

Constructed as a **self-assembly solution**, the DIY-Thermocam allows you to take **advantage** of the **versatile possibilities** of thermal imaging and to **save money** at one go.

**Everything**, from **software** to **hardware**, is completely **open-source**! This allows everyone to **modify** or **extend** the functionalities of the device to their **own needs**!



# Building Instructions

You can build your **own DIY-Thermocam V3** at home **easily** in a **do-it-yourself approach**. An **[illustrated assembly guide](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/AssemblyGuide_06.pdf)** leads you through the **process** of **constructing** the device, **step-by-step**.

For the **tools**, you only need a **simple soldering iron**, some solder tin, a nippers and a screwdriver. The **design** of the **PCB** is very **basic**: No SMD components, only big through-hole parts and modules.  Check out the **[schematic](https://github.com/maxritter/DIY-Thermocam/blob/master/Documents/Schematic_3.0.pdf)** and the **[pinmap](https://github.com/maxritter/DIY-Thermocam/blob/master/Documents/Pinmap_3.0.pdf)** to see how the connections from and to the microcontroller work.

In order to understand to **various device functionalities**, have a look at the **[manual](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/Manual_13.pdf)**. The **structure of the raw data files** is explained **[here](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/RawData_07.pdf)**, and the **open USB serial protocol** **[here](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/SerialProtocol_15.pdf)**.

The **enclosure** has been **laser cutted** from 3mm acryl plastic and can be assembled with only **bolts & nuts**, no glue is required. 


### DIY-Thermocam V3 KIT

The **self-assembly KIT** contains **all required components apart from the FLIR Lepton 2.5/3.5 and the Lepton Breakout Board V2** (need to be bought separately from either [Digikey.com](https://www.digikey.com/) or [Groupgets.com](https://store.groupgets.com/)). Please **subscribe to the list below** to **get notified** when the **KIT goes on sale**:

#### **[-> Subscribe Now <-](https://mailchi.mp/3e0df5458871/diy-thermocam-v3-kit)**




<table style="width: 100%">
   <tr>
      <td style="width: 112px">
         <img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Lepton.png" style="color: rgb(0, 0, 0); font-family: &quot;Times New Roman&quot;; font-size: medium; font-style: normal; font-variant-ligatures: normal; font-variant-caps: normal; font-weight: normal; letter-spacing: normal; orphans: 2; text-align: start; text-indent: 0px; text-transform: none; white-space: normal; widows: 2; word-spacing: 0px; -webkit-text-stroke-width: 0px; user-select: none;" />
      </td>
      <td >
	  <a href="https://www.digikey.com/product-detail/en/flir/500-0771-01/500-0771-01-ND/7606616">
	  <strong>FLIR Lepton 3.5</strong></a> <strong>Has to be bought separately</strong><br />
         160x120 Radiometric LWIR sensor<br />
      </td>
   </tr>
   <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/LeptonModule.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <a href="https://www.digikey.com/en/products/detail/flir-lepton/250-0577-00/10385179?s=N4IgTCBcDaIGYBsCWAnABAgpgBwC4HsA7NAIxUwEMBrfAV1xAF0BfIA">
	  <strong>Lepton Breakout Board V2</strong></a> <strong>Has to be bought separately</strong><br />
         Interface the Lepton over SPI / I2C&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PCB.png" style="-webkit-user-select: none" />
      </td>
      <td ><a href="https://github.com/maxritter/DIY-Thermocam/tree/master/PCB">
	  <strong>Printed Circuit Board V3</strong></a><br />
         89.4mm (w) x 68.4mm (h), 1.6mm thickness, 2 layers</td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Enclosure.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <a href="https://github.com/maxritter/DIY-Thermocam/tree/master/Enclosure">
	  <strong>Enclosure V3</strong></a><br />
         2D laser-cut from 3mm gray acryl plastic</td>
   </tr>
   <tr>
      <td style="width: 110px"  >
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Teensy41.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>Teensy 4.1</strong><br />
         Cortex M7 Microcontroller&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 110px"  >
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Battery.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>3.7V Lipo Battery</strong><br />
         2000mAh capacity&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 110px"  >
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Storage.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>8GB MicroSD Card</strong> 
	  <br />
         Class 10 card</td>
   </tr>
    <tr>
      <td style="width: 113px" >
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Display.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>3.2" TFT LCD Display Module</strong><br />
         With touch panel
      </td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Charging.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>Lipo Charging Module</strong><br />
         TP4057 including charging LED&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 110px"  >
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Booster.png" style="-webkit-user-select: none" />
      </td>
      <td>
	  <strong>5V Voltage Booster</strong><br />
         Pololu U3V12F5&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PowerSwitch.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>Power Switch</strong><br />
         NKK Switches CWT12AAS1</td>
   </tr>
   <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PushButton.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>Push button
      </strong><br />
	  RAFI 1.10107.0110104</td>
   </tr>
   <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DisplayHeader.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>Display connector</strong><br />
         40-pin 2.54mm female header&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/LeptonHeader.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>Lepton board connector</strong><br />
         20-pin 2.54mm female header&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PinHeader.png" style="-webkit-user-select: none" />
      </td>
      <td ><strong>2x Pin header strip</strong><br />
         40-pin 2.54mm male header&nbsp;
      </td>
   </tr>
  <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Teensy41Header.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>Female Header</strong><br />
         36-pin 0.1" Female header - pack of 5&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/CoinHolder.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>Coin cell battery&nbsp;holder</strong><br />
	  Keystone 3001</td>
   </tr>
   <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/CoinCell.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>Battery for the real-time-clock
      </strong><br />
	  <strong>CR1220 coin cell battery</strong><br />
      </td>
   </tr>
   <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Res47.png" style="-webkit-user-select: none" />
      </td>
      <td ><strong>4x 4.7K 1/4W 1% resistor</strong><br />
         Multi-purpose resistor&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 112px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Res10K.png" style="-webkit-user-select: none" />
      </td>
      <td >&nbsp;<strong>2x 10K 1/4W 1% resistor</strong><br />
         Multi-purpose resistor
      </td>
   </tr>
      <tr>
      <td style="width: 110px"  >
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/USBCable.png" style="-webkit-user-select: none" />
      </td>
      <td >
	  <strong>Micro USB Cable</strong><br />
         120cm A to Micro B USB Cable&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/TripodSocket.png" style="-webkit-user-select: none" />
      </td>
     <td >
	  <strong>Tripod Socket</strong><br />
         The Hillman Group 2511 1/4-20 x 5/16 x 3/4 Inch Brad Hole Tee Nut&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2x16.png" width="100" /></td>
      <td ><strong>4x M2x16</strong></td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2x3.png" width="100" /></td>
      <td ><strong>4x Spacer M2x12</strong> </td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2x8.png" width="100" /></td>
      <td ><strong>3x Screw M2x8</strong></td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2.5x6.png" width="100" /></td>
      <td >
	  <strong>8x Screw M2.5x6</strong></td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2.5x12.png" width="100" /></td>
      <td ><strong>4x Standoff M2.5x20</strong></td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2.5x11.png" width="100" /></td>
      <td ><strong>4x Standoff M2.5x11</strong></td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2.5x5.png" width="100" /></td>
      <td ><strong>4x Standoff M2.5x5</strong></td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2N_2.png" width="100" /></td>
      <td ><strong>3x Nut M2</strong></td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;
		 <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/NutM2.png" width="100" /></td>
      <td ><strong>4x Nut M2 Plastic</strong></td>
   </tr>
   <tr>
      <td style="width: 113px" >
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2W.png" width="100" /></td>
      <td ><strong>3x Washer M2</strong></td>
   </tr>
</table>



# Device Images

***Reflect V2, will be updated soon for V3!***

<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/DeviceFront.jpg" width="500">

<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/DeviceOpen.jpg" width="500">

<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/DeviceBack.jpg" width="500">


<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/DeviceTop.jpg" width="500">

<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/DeviceBottom.jpg" width="500">



# Features of V3


 - Fast **ARM Cortex M7** processor (**600MHz**), based on the popular, **Arduino** compatible **Teensy 4.1**
 - Frame rate of up to **9 FPS** (US export compliance)
 - **18 different color schemes** including rainbow, ironblack, grayscale, hot & cold
 - 3.2 inch **LCD touch display** with bright colors,  wide angle and resistive touch
 - **Save thermal images** with a resolution of 640x480 pixels on the device
 - **Save** real-time **videos** and interval images with different time-lapse settings
 - **Internal storage** over microSD card, accessible as a Mass Storage device on any PC via USB
 - **2000 mAh lithium polymer battery** for long operation time (4-6 hours)
 - **Open-source firmware** written in **Arduino compatible C/C++** code
 - Regular **firmware updates** with new features, flashable over a cross-OS firmware updater
 - **Standalone thermal viewer application** to save high-quality thermal images & videos  on the computer
 - Fully **compatible** to the comprehensive **thermal analysis software** ThermoVision by Joe-C
 - Use simple **commands** to receive all thermal & configuration data over the **USB serial port** with high speed

<table style="width: 100%">
   <tr>
      <td style="background-color: #cccccc;">
         <strong>Feature</strong>
      </td>
      <td style="border: 0px solid transparent; background-color: #cccccc;">
         <b>DIY-Thermocam V3</b>
      </td>
      <td style="background-color: #cccccc;">
         <strong>FLIR E6</strong>
      </td>
   </tr>
   <tr>
      <td>
         Thermal resolution
      </td>
      <td>
         80x60 (Lepton2.5) or 160x120 (Lepton3.5)
      </td>
      <td>
         160x120
      </td>
   </tr>
   <tr>
      <td>
         Thermal sensitivity
      </td>
      <td>
         &lt; 0.05° C (50 mK)
      </td>
      <td>
         &lt; 0.06° C (60 mK)
      </td>
   </tr>
   <tr>
      <td>
         Thermal temp. range
      </td>
      <td>
         -10°C - +140°C or -10°C - +450°C
      </td>
      <td>
         <span style="line-height: 26.6667px;">-20°C to 250°C</span>
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
   </tr>
   <tr>
      <td>
         Storage capacity
      </td>
      <td>
         Internal storage over microSD card
      </td>
      <td>
         500 sets of images
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
   </tr>
</table>


# Device UI

<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/Screenshots.PNG" width="700">


The **latest releases** of device firmware can be found **[here](https://github.com/maxritter/DIY-Thermocam/tree/master/Firmware/Releases)**. For the firmware **SDK**, check out **[this instruction](https://github.com/maxritter/DIY-Thermocam/tree/master/Firmware/Source)**.



# Sample Thermal Images

<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/ThermalImg.PNG" width="700">


More thermal images in **full resolution** can be found **[here](https://github.com/maxritter/DIY-Thermocam/tree/master/Images/Thermal)**.



# Thermal Image Comparison


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/ThermalComp.jpg" width="700">


This is a **comparison** between the **DIY-Thermocam**, the **Seek XR** and a **FLIR E8** thermal imager.



# Desktop Software


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/Live Viewer and Updater.PNG" width="500">


The standalone PC **[thermal live viewer](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Thermal%20Live%20Viewer)** software allows you to **capture** HQ **thermal** and **visual images**, as well as **videos** right on the **computer**.  Various **settings** can be **changed** over the UI, including a calibration and two **different transfer modes**. The software is written in **Python**, so it can be modified to your own needs and executed on **any OS**. New **firmware updates** can be flashed to the device easily with the **[firmware updater](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Firmware%20Updater)** application.


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/ThermoVision.png" width="700">


There is a powerful **[thermal analysis software](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Thermal%20Analysis%20Software)** for the PC, called **ThermoVision**. The **image browser** can show all thermal & visual images on the internal storage, when the device is connected over USB. They can then be loaded with one click. The software allows you to **analyze** and **edit** the **thermal raw data files** right from the device, create **combined images** and **stream & record live images** (thermal & visual) from the Thermocam. You can even set **temperature thresholds**, which trigger a specific action like opening a file, saving an image or sending a command to the serial port.


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/DataViewer.png" width="600">


The second possibility to **view** and **edit the raw data** files right on the **PC**, is the **[thermal data viewer](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Thermal%20Data%20Viewer)**. It allows various functions to alter the thermal range, **add measurement points** as well as various filters. The program is also capable of **converting whole folders** of raw data frames into images (JPG, BMP or PNG) or avi videos. The **live tab** allows you to stream live images from the device and the calibration tab offers functions to edit the **raw-to-absolution temperature conversion**. You can also convert videos or interval pictures with the **[video converter](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Video%20Converter)** to avi videos.