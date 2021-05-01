<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/Logo.png" width="500">



The **DIY-Thermocam V3** is a **low-cost, do-it-yourself** thermal imager, based on the popular radiometric **FLIR Lepton** long-wave-infrared array sensor. 

<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/Thermocam.jpg" width="500">


The aim of this project is to give **private persons**, **educational institutes** and **companies** access to a **portable**,  **affordable** and **customizable** thermal imaging platform.

There are **various applications** like finding **heat leaks** in the insulation of buildings, the analysis of **electrical** or **mechanical components**, the **detection** of **persons** / animals or even mounting it on a **drone** and **recording continuous or time-lapse images**.

Constructed as a **self-assembly solution**, the device allows you to take **advantage** of the **versatile possibilities** of thermal imaging. **Everything**, from **software** to **hardware**, is completely **open-source**! This allows everyone to **modify** or **extend** the functionalities of the device to their **own needs**!

The **DIY-Thermocam V3 self-assembly KIT** contains **all required components apart from the FLIR Lepton 2.5/3.5 and the Lepton Breakout Board V2** (need to be bought separately from either [Digikey.com](https://www.digikey.com/) or [Groupgets.com](https://store.groupgets.com/)). Please **subscribe to the list below** to **get notified** when the **KIT goes on sale**:

#### **[-> Subscribe Now <-](https://mailchi.mp/3e0df5458871/diy-thermocam-v3-kit)**



# Sample Thermal Images

<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/ThermalImg.PNG" width="700">


More thermal images in **full resolution** can be found **[here](https://github.com/maxritter/DIY-Thermocam/tree/master/Images/Thermal)**.



# Building Instructions

You can build your **own DIY-Thermocam V3** at home **easily** in a **do-it-yourself approach**. An **[illustrated assembly guide](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/AssemblyGuide_06.pdf)** leads you through the **process** of **constructing** the device, **step-by-step**.

For the **tools**, you only need a **simple soldering iron**, some solder tin, a nippers and a screwdriver. The **design** of the **PCB** is very **basic**: No SMD components, only big through-hole parts and modules.  Check out the **[schematic](https://github.com/maxritter/DIY-Thermocam/blob/master/Documents/Schematic_3.0.pdf)** and the **[pinmap](https://github.com/maxritter/DIY-Thermocam/blob/master/Documents/Pinmap_3.0.pdf)** to see how the connections from and to the microcontroller work.

The **enclosure** has been **laser cutted** from 3mm acryl plastic and can be assembled with only **bolts & nuts**, no glue is required. 



# Features of V3

In order to understand to **various device functionalities**, have a look at the **[manual](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/Manual_13.pdf)**. The **structure of the raw data files** is explained **[here](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/RawData_07.pdf)**, and the **open USB serial protocol** **[here](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/SerialProtocol_15.pdf)**. The main features of the device are:


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

The **3.2 touch LCD display** provides a **fully functional thermographic camera UI** that has many features:

<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/Screenshots.PNG" width="700">

Thanks to the powerful **Teensy 4.1 microcontroller** the whole firmware **runs very smoothly** and is able to display thermal images from the **Lepton at full speed (9 images per second)**.



# Thermal Image Comparison

This is a **comparison** between the **DIY-Thermocam**, the **Seek XR** and a **FLIR E8** thermal imager:

<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/ThermalComp.jpg" width="700">



# Desktop Software

<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/ThermoVision.png" width="700">

There is a powerful **[thermal analysis software](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Thermal%20Analysis%20Software)** for the PC, called **ThermoVision**. The **image browser** can show all thermal & visual images on the internal storage, when the device is connected over USB. They can then be loaded with one click. The software allows you to **analyze** and **edit** the **thermal raw data files** right from the device, create **combined images** and **stream & record live images** (thermal & visual) from the Thermocam. You can even set **temperature thresholds**, which trigger a specific action like opening a file, saving an image or sending a command to the serial port.


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/Live Viewer and Updater.PNG" width="500">


The standalone PC **[thermal live viewer](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Thermal%20Live%20Viewer)** software allows you to **capture** HQ **thermal** and **visual images**, as well as **videos** right on the **computer**.  Various **settings** can be **changed** over the UI, including a calibration and two **different transfer modes**. The software is written in **Python**, so it can be modified to your own needs and executed on **any OS**. New **firmware updates** can be flashed to the device easily with the **[firmware updater](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Firmware%20Updater)** application.




<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/DataViewer.png" width="600">


The second possibility to **view** and **edit the raw data** files right on the **PC**, is the **[thermal data viewer](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Thermal%20Data%20Viewer)**. It allows various functions to alter the thermal range, **add measurement points** as well as various filters. The program is also capable of **converting whole folders** of raw data frames into images (JPG, BMP or PNG) or avi videos. The **live tab** allows you to stream live images from the device and the calibration tab offers functions to edit the **raw-to-absolution temperature conversion**. You can also convert videos or interval pictures with the **[video converter](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Video%20Converter)** to avi videos.