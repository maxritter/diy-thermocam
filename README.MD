<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/Logo.png" width="500">
<br>
<br>

**Update #2:** **Firmware 2.47** now supports the **low gain mode** for the **Lepton3.5**, so it can measure temperatures up to **+450°C**!

**Update #1:** **haraldg** has written a nice set of tools for **post-processing** the thermal raw data files. Check it out **[here](https://github.com/haraldg/thermocam-tools)**. He also published a short article about **manual calibration** [here](https://github.com/maxritter/DIY-Thermocam/blob/master/Documents/ManualCalibration_01.pdf).

----------


The **DIY-Thermocam** is a **low-cost, do-it-yourself** thermal imager, based on the popular **FLIR Lepton** long-wave-infrared array sensor. 


<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/Thermocam.jpg" width="500">


The aim of this project is to give **private persons**, **educational institutes** and **companies** access to a **portable**,  **affordable** and **customizable** thermal imaging plattform.
 
There are **various applications** like finding **heat leaks** in the insulation of buildings, the analysis of **electrical** or **mechanical components**, the **detection** of **persons** / animals or even mounting it on a **drone**, together with the additional **video out module**.
 
Constructed as a **self-assembly solution**, the DIY-Thermocam allows you to take **advantage** of the **versatile possibilities** of thermal imaging and to **save money** at one go.
 
**Everything**, from **software** to **hardware**, is completely **open-source**! This allows everyone to **modify** or **extend** the functionalities of the device to their **own needs**!

A **[scientific paper](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/Scientific%20Paper.pdf)** is also available, giving an **overview** about the whole **project** and its **capabilities**.

<a href="https://www.facebook.com/diythermocam/"><img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/FacebookButton.jpg"></a> 

----------


**HOW TO MAKE IT**


You can **make** your **own DIY-Thermocam V2** at home **easily**.  For the tools, you only need a **simple soldering iron**, some solder tin, a nippers and a screwdriver. The **design** of the **PCB** is very **basic**: No SMD components, only big through-hole parts and modules.  The **enclosure** can be assembled easily only with **bolts & nuts**, no glue required. 


An **[illustrated assembly guide](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/AssemblyGuide_05.pdf)** will lead you through the **process** of **constructing** the device, **step-by-step**. You should have soldered at least **one small project before**, but with that experience you **should be fine**.


The DIY-Thermocam works with the **FLIR Lepton2.x** (**80 x 60**) and the **FLIR Lepton3.x** (**160 x 120**) LWIR sensor, including the **radiometric versions** (**Lepton2.5** only at the moment). The **hardware revision** is **detected automatically** by the firmware **on startup** and you can **change** the sensor just with your **fingers**. 


The **Lepton2.0** and **Lepton3.0** sensors do **not** have in an **internal calibration**. An **external spot sensor**, the **MLX90614** is used to generate a **transfer function** for raw values to absolute temperatures conversion. When using a **radiometric Lepton** (**Lepton2.5** or **Lepton3.5**), the **spot sensor** is **not required**.


The **[firmware update guide](https://github.com/maxritter/DIY-Thermocam/blob/master/Documents/FirmwareUpdate_07.pdf)** helps you to update your device to the **newest firmware**. In order to understand to **various device functionalities**, have a look at the **[manual](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/Manual_13.pdf)**. 


For going deeper, the **structure of the raw data files** is explained **[here](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/RawData_07.pdf)**, and the **open USB serial protocol** **[here](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/SerialProtocol_15.pdf)**.




----------


**REQUIRED PARTS**

In order to **make your own DIY-Thermocam V2**,   please use the **part list below** to acquire all the components **on your own**. **Digikey.com** offers **free international shipment** for all orders **over 100€**, so I recommend buying most of the components from them. 

You can find all of the **screws, bolts, nuts & washers** at **[Ettinger](https://www.ettinger.de/en)** and maybe also at a **local construction market** near you.  If you do not have a  VAT and still want to buy the components from Ettinger, write a mail to [sales@ettinger.de](mailto:sales@ettinger.de) and ask for a quote.

I do **not** offer **pre-assembled** devices and am also **not responsible** for the **quality** and **completeness** of the kit from **GroupGets**.


<table style="width: 100%">
   <tr>
      <td style="width: 111px">
         <img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Lepton.png" style="color: rgb(0, 0, 0); font-family: &quot;Times New Roman&quot;; font-size: medium; font-style: normal; font-variant-ligatures: normal; font-variant-caps: normal; font-weight: normal; letter-spacing: normal; orphans: 2; text-align: start; text-indent: 0px; text-transform: none; white-space: normal; widows: 2; word-spacing: 0px; -webkit-text-stroke-width: 0px; user-select: none;" />
      </td>
      <td style="width: 235px">FLIR Lepton (Shuttered)<br />
         Long-wave-infrared array sensor<br />
      </td>
      <td>
	     Lepton3.5:
		 <a href="https://www.digikey.com/product-detail/en/flir/500-0771-01/500-0771-01-ND/7606616">
		 <strong>Digikey.com</strong></a> or
		 <a href="https://store.groupgets.com/collections/frontpage/products/flir-lepton-3-5">
		 GroupGets.com</a><br />
         Lepton3.0:
		 <a href="https://www.digikey.com/products/en?keywords=500-0726-01">
		 <strong>Digikey.com</strong></a> or
		 <a href="https://store.groupgets.com/collections/flir-lepton-accessories/products/l3">
		 GroupGets.com</a><br />
		 Lepton2.5:
		 <a href="https://www.digikey.com/product-detail/en/flir/500-0763-01/500-0763-01-ND/6250105">
		 <strong>Digikey.com</strong></a> or
		 <a href="https://store.groupgets.com/collections/flir-lepton-accessories/products/radiometric-flir-lepton-2-5">
		 GroupGets.com</a><br />
		 Lepton2.0:
		 <a href="https://www.digikey.com/product-detail/en/flir/500-0659-01/500-0659-01-ND/5215153">
		 <strong>Digikey.com</strong></a> or
		 <a href="https://store.groupgets.com/collections/flir-lepton-accessories/products/50-shuttered-flir-lepton-thermal-imager">
		 GroupGets.com</a></td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/LeptonModule.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">FLIR Lepton Breakout Board
         Interface the Lepton over SPI / I2C&nbsp;
      </td>
      <td>
         <a href="https://www.digikey.com/product-detail/en/flir/250-0587-00/250-0587-00-ND/5266963">
         <strong>Digikey.com</strong></a><br />
         <a href="https://store.groupgets.com/collections/flir-lepton-accessories/products/flir-lepton-breakout-board">GroupGets.com</a></td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/SpotSensor.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">MLX90614-BCF<br />
         Spot sensor for absolute temperature measurement - <strong>not required for 
         Lepton2.5 / Lepton3.5</strong></td>
      <td>
         <strong>
         <a href="https://www.digikey.com/product-detail/en/melexis-technologies-nv/MLX90614ESF-BCF-000-SP/MLX90614ESF-BCF-000-SP-ND/5414794">Digikey.com</a></strong><br />
         <a href="http://as-electronic.net/MLX90614ESF-BCF-000">As-electronic.net</a></td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Teensy36.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Teensy 3.6<br />
         Cortex M4 Microcontroller&nbsp;
      </td>
      <td>
         <strong>
         <a href="http://www.digikey.com/product-detail/en/sparkfun-electronics/DEV-14057/1568-1442-ND/6235190">
         Digikey.com</a></strong><br />
         <a href="https://www.pjrc.com/store/teensy36.html">PJRC.com</a><br />
         <a href="https://www.flikto.de/products/teensy-3-6-v3-6-usb-mikrocontroller">
         Flikto.de</a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Storage.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">SanDisk 8GB microSD card<br />
         Can be any size, but needs to be Class4 microSD&nbsp;
      </td>
      <td>
         <a href="http://www.ebay.com/itm/SanDisk-32GB-16GB-8GB-Micro-SD-SD-HC-TF-Memory-Card-Retail-Package-Class-4-C4-/152291118415?var=&amp;hash=item2375420d4f:m:mhCKhMZoMK-EaKQbv04Y5Ag">
		 <strong>Ebay.com</strong></a><br />
         <a href="https://www.amazon.com/SanDisk-microSD-High-Capacity-microSDHC/dp/B00488G6P8/ref=sr_1_7?ie=UTF8&amp;qid=1494677828&amp;sr=8-7&amp;keywords=Sandisk+8GB">Amazon.com</a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Arducam.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Arducam V1 Mini 2MP<br />
         Visual camera module&nbsp;
      </td>
      <td>
         <strong>
         <a href="https://www.ebay.com/itm/Arducam-Mini-module-Camera-Shield-w-2-MP-OV2640-for-Arduino-UNO-Mega2560-board-/281639729684">
         Ebay.com</a></strong><br />
         <a href="http://www.watterott.com/de/2MP-Mini-Kamera-Board-OV2640">
         Watterott.com</a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Display.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">3.2" TT LCD Display Module<br />
         Configuration: Interface: Pin Header Connection-4-wire-SPI, Power Supply: 5V, Touch Panel: 3.2" Resistive, Font Chip: None 
      </td>
      <td>
         <a href="http://www.buydisplay.com/default/3-2-inch-capacitive-touchscreen-240x320-tft-lcd-module-display">
         <strong>BuyDisplay.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Battery.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">3.7V Lithium Polymer Battery&nbsp;<br />
         With JST-PH connector, max. dimens.: 60mm (w) x 55mm (l) x 6.5mm (h)
      </td>
      <td>
         <a href="https://www.ebay.com/itm/3-7V-2000mAh-Rechargeable-Battery-Li-Po-Polymer-For-Tablet-PC-DVD-GPS-PAD-505060/121965187238?epid=760566634&hash=item1c65b110a6:g:MQAAAOSwXetZUxxN"><strong>Ebay.com</strong></a><br />
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PCB.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Printed Circuit Board<br />
         89.4mm (w) x 68.4mm (h), 1.6mm thickness, 2 layers</td>
      <td>
         <a href="https://oshpark.com/shared_projects/ddHY98QR"><strong>OSHPark.com</strong></a><br />
		 <a href="http://www.smart-prototyping.com/PCB-Prototyping.html">
		 Smart-Prototyping.com</a><br />
         Or use the 
         <a href="https://github.com/maxritter/DIY-Thermocam/tree/master/PCB">gerber files</a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Enclosure.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Enclosure<br />
         2D laser-cut from 3mm black acryl plastic (one side frosted)</td>
      <td>
         <a href="http://www.ponoko.com/design-your-own/products/diy-thermocam-v2-enclosure-13930">
         <strong>Ponoko.com</strong></a><br />
		 <a href="http://www.formulor.de/">Formulor.de</a><br />
         Or use the 
         <a href="https://github.com/maxritter/DIY-Thermocam/tree/master/Enclosure">design files</a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Charging.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">TP4057 Charging Module<br />
         Including charging LED&nbsp;
      </td>
      <td>
         <a href="https://www.ebay.com/itm/TP4057-5V-1A-LED-Charger-Charging-Module-For-3-7V-Li-ion-Lithium-18650-Battery/122754421232?hash=item1c94bbd1f0:g:EssAAOSws8VZ3ZSX">
         <strong>Ebay.com</strong></a><br />
         <a href="https://www.amazon.com/Solu-Lithium-Battery-Charging-Reverse/dp/B00YZV7RTW">
         Amazon.com</a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Booster.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Pololu U3V12F5<br />
         5V voltage booster&nbsp;
      </td>
      <td>
         <a href="https://www.amazon.com/gp/product/B016YHHIIY/ref=od_aui_detailpages00?ie=UTF8&psc=1">
         <strong>Amazon.com</strong></a><br />
         <a href="https://www.pololu.com/product/2115">Pololu.com</a><br />
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PowerSwitch.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">E-Switch R6ABLKBLKFF<br />
         Power switch
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/e-switch/R6ABLKBLKFF/EG1526-ND/301973">
         <strong>Digikey.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PushButton.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">RAFI 1.10107.0110104<br />
         Push button
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/rafi-usa/1.10107.0110104/1715-1019-ND/6227436">
         <strong>Digikey.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/USBSwitch.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">E-Switch EG1201A<br />
         USB power switch
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/e-switch/EG1201A/EG1902-ND/101723">
         <strong>Digikey.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/LipoCon.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">JST S2B-PH-K-S<br />
         Connect the lipo to the PCB&nbsp;
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/jst-sales-america-inc/S2B-PH-K-S(LF)(SN)/455-1719-ND/926626">
         <strong>Digikey.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/SDSlot.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Wurth 693063020911<br />
         SD slot for the SD card on the PCB
      </td>
      <td>
         <a href="http://www.digikey.com/products/en?keywords=%09732-3818-1-ND">
         <strong>Digikey.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/SDAdapter.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">MicroSD adapter<br />
         Short microSD to SD adapter for the internal storage card&nbsp;
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/seeed-technology-co-ltd/328030004/328030004-ND/5487856">
         <strong>Digikey.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DisplayHeader.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Display connector<br />
         40-pin 2.54mm female header&nbsp;
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/sullins-connector-solutions/SFH11-PBPC-D20-ST-BK/S9200-ND/1990093">
         <strong>Digikey.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/LeptonHeader.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Lepton board connector<br />
         8-pin 2.54mm female header&nbsp;
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/sullins-connector-solutions/PPPC081LGBN-RC/S5483-ND/775941">
         <strong>Digikey.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PinHeader.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Pin header strip<br />
         40-pin 2.54mm male header&nbsp;
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/4-103741-0/A26509-40-ND/297917">
         <strong>Digikey.com</strong></a> (2 pcs)
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/CoinHolder.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Keystone 3001<br />
         Holder for the coin cell battery&nbsp;
      </td>
      <td>
         <a href="http://www.digikey.com/products/en?keywords=Keystone%203001">
         <strong>Digikey.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/CoinCell.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">CR1220 coin cell battery&nbsp;<br />
         Battery for the real-time-clock
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/energizer-battery-company/CR1220VP/N033-ND/704851">
         <strong>Digikey.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Res47.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">4.7K 1/4W 1% resistor<br />
         Multi-purpose resistor&nbsp;
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/stackpole-electronics-inc/RNMF14FTC4K70/S4.7KCACT-ND/2617510">
         <strong>Digikey.com</strong></a> (4 pcs)
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Res10K.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">10K 1/4W 1% resistor&nbsp;<br />
         Multi-purpose resistor
      </td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/stackpole-electronics-inc/CF14JT10K0/CF14JT10K0CT-ND/1830374">
         <strong>Digikey.com</strong></a> (2 pcs)&nbsp;
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/LipoProt.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Lipo protector<br />
         Double-sided adhesive tape for the lipo protection on the PCB&nbsp;
      </td>
      <td>
         <a href="https://www.ebay.com/itm/200mmX100mm-3M-9495LE-300LSE-Double-Sided-Transparent-Clear-Adhesive-Tape/281869275116?hash=item41a0b7cfec%3Am%3AmR2yJrCTWwgBQsqxZ6HHr2A&var=580847054098">
		 <strong>Ebay.com</strong></a><br />
         Or use any thick tape
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/CableRight.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Angled microUSB cable<br />
         For connection of the Thermocam to the PC&nbsp;and 
         charging of the device
      </td>
      <td>
         <a href="http://www.ebay.com/itm/Down-Angled-Micro-USB-5-Pin-Male-to-USB-A-Male-Data-Charge-Cable-for-Cell-Phone-/291869468911?hash=item43f4c6a8ef:g:9RgAAOSwxg5X0ovy">
         <strong>Ebay.com</strong></a><br />
         Or use any USB cable
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Tripod.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Mini tripod<br />
         Used as a stand for the device&nbsp;
      </td>
      <td>
         <a href="https://www.ebay.com/itm/Universal-Mini-Flexible-Portable-Tripod-Stand-for-Digital-Camera-HD-Nikon-Canon/381340421241?epid=14009503154&hash=item58c9a90c79:g:zbQAAOSwpmNbKRpD">
         <strong>Ebay.com</strong></a><br />
         Or use any tripod
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/TripodSocket.png" style="-webkit-user-select: none" />
      </td>
      <td style="width: 235px">Tripod socket&nbsp;</td>
      <td>
         <a href="https://www.amazon.com/The-Hillman-Group-2511-13-Pack/dp/B00HYLZ33W/">
         <strong>Amazon.com</strong></a>
      </td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2x10.png" width="100" /></td>
      <td style="width: 235px">Screw M2x10 (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/ScrewM25x6.png">Details</a>)</td>
      <td>
         &nbsp;<a href="https://www.ettinger.de/en/product/0114142"><strong>Ettinger.de</strong></a> 
		 (6 pcs)</td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2x3.png" width="100" /></td>
      <td style="width: 235px">Spacer M2x3 (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DistanceM2x3.png">Details</a>)</td>
      <td>
         <a href="https://www.ettinger.de/en/product/0581030"><strong>
		 Ettinger.de</strong></a> (6 pcs)</td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2x3.5.png" width="100" /></td>
      <td style="width: 235px">Spacer M2x3.5 (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DistanceM2x35.png">Details</a>)</td>
      <td>
         <a href="https://www.ettinger.de/en/product/0581035"><strong>
		 Ettinger.de</strong></a> (2 pcs)</td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2x8.png" width="100" /></td>
      <td style="width: 235px">Screw M2x8 (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/ScrewM2x8.png">Details</a>)</td>
      <td>
         <a href="https://www.ettinger.de/en/product/0112138"><strong>
		 Ettinger.de</strong></a> (5 pcs)</td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2.5x6.png" width="100" /></td>
      <td style="width: 235px">Screw M2.5x6 Black (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/ScrewM25x6.png">Details</a>
      )</td>
      <td>
         <a href="http://www.ebay.com/itm/M2-5-Black-10-9-Grade-Alloy-Steel-Hex-Socket-BUTTON-HEAD-Screws-/282445943742?var=581589331786&amp;hash=item41c31713be:m:m1ckYrXP_7-t3VDbQpbZHZA">
		 <strong>Ebay.com</strong></a> (8 pcs)</td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2.5x12.png" width="100" /></td>
      <td style="width: 235px">Standoff M2.5x12 (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DistanceM2.5x12.png">Details</a>)</td>
      <td>
         &nbsp;<a href="https://www.ettinger.de/en/product/0502123"><strong>Ettinger.de</strong></a> 
		 (4 pcs)</td>
   </tr>
   <tr>
      <td style="width: 111px; height: 23px;">
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2.5x11.png" width="100" /></td>
      <td style="width: 235px; height: 23px;">Standoff M2.5x11 (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DistanceM2.5x11.png">Details</a>)</td>
      <td style="height: 23px">
         <a href="https://www.ettinger.de/en/product/0512113"><strong>
		 Ettinger.de</strong></a> (4 pcs)</td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2.5x5.png" width="100" /></td>
      <td style="width: 235px">Standoff M2.5x5 (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DistanceM2.5x5.png">Details</a>)</td>
      <td>
         <a href="https://www.ettinger.de/en/product/0512053"><strong>
		 Ettinger.de</strong></a> (4 pcs)</td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2N_2.png" width="100" /></td>
      <td style="width: 235px">Nut M2 (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/NutM2.png">Details</a>)</td>
      <td>
         <a href="https://www.ettinger.de/en/product/0210011"><strong>
		 Ettinger.de</strong></a> (5 pcs)</td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;
         <a href="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2N_1.png">
		 <img class="auto-style1" height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/NutM2.png" width="100" /></a></td>
      <td style="width: 235px">Nut M2 Plastic (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/NutM2.png">Details</a>)</td>
      <td>
         <a href="https://www.ettinger.de/en/product/0205016"><strong>
		 Ettinger.de</strong></a> (6 pcs)</td>
   </tr>
   <tr>
      <td style="width: 111px">
         &nbsp;
         <img height="100" src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/M2W.png" width="100" /></td>
      <td style="width: 235px">Washer M2 (<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/WasherM2.png" >Details</a>)</td>
      <td>
         <a href="https://www.ettinger.de/en/product/0301013"><strong>
		 Ettinger.de</strong></a> (3 pcs)</td>
   </tr>
</table>


----------


**DEVICE IMAGES**


<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/DeviceFront.jpg" width="500">


<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/DeviceOpen.jpg" width="500">


<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/DeviceBack.jpg" width="500">


<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/DeviceTop.jpg" width="500">


<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/DeviceBottom.jpg" width="500">


----------


**FEATURES**


 - Fast **ARM Cortex M4** processor (**240MHz**), based on the popular, **Arduino**
   compatible **Teensy 3.6**
 - **160x120 (Lepton3)** or **80x60 (Lepton2)** long-wave infrared array sensor for live
   thermographic images
 - Frame rate of up to **9 FPS** (US export compliance) over the serial
   connection, 5 FPS on the device itself
 - **2MP visual camera** to capture optical images, that can be used in a
   combined image
 - **MLX90614 single point**-infrared sensor for high-precision spot
   temperatures (10° FOV)
 - **HDMI** or **analog video** output  capabilities (640x480 pixel) over
   external video output module
 - **3 operating modes**: thermal only, thermal + visual, video recording
 - **18 different color schemes** including rainbow, ironblack, grayscale,
   hot & cold
 - 3.2 inch **LCD touch display** with bright colors,  wide angle and resistive touch
 - **Save thermal** and **visual images** with a resolution of 640x480 pixels
   on the device
 - **Save** real-time **videos** and interval images with different time-lapse
   settings
 - **Up to 8GB internal storage**, accessible as an exchangeable SD / microSD
   slot
 - **2500 mAh lithium polymer battery** for long operation time (4-6 hours)
 - **Open-source firmware** written in **Arduino compatible C/C++** code
 - Regular **firmware updates** with new features, flashable over a
   standalone firmware updater
 - **Standalone thermal viewer application** to save high-quality thermal
   images & videos  on the computer
 - Fully **compatible** to the comprehensive **thermal analysis software**
   ThermoVision by Joe-C
 - Use simple **commands** to receive all thermal & configuration data over
   the **USB serial port** with high speed


----------


**FEATURE TABLE**


<table style="width: 100%">
   <tr>
      <td style="background-color: #cccccc;">
         <strong>Feature</strong>
      </td>
      <td style="border: 0px solid transparent; background-color: #cccccc;">
         <b>DIY-Thermocam</b>
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
         80 x 60 or 160 x 120
      </td>
      <td>
         160 x 120
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
         -10° C - +140° C or -10° C - +450° C (Lepton3.5)
      </td>
      <td>
         <span style="line-height: 26.6667px;">-20° C to 250° C</span>
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
         Spot sensor temp. range
      </td>
      <td>
         -70° C to 380° C (Lepton2.0/3.0 only)
      </td>
      <td>
         -20° C to 250° C
      </td>
   </tr>
   <tr>
      <td>
         Spot sensor temp. accuracy
      </td>
      <td>
         0.5° C over wide range
      </td>
      <td>
         ±2 ° C or ±2%
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
         Image modes
      </td>
      <td>
         IR image, visual image, combined
      </td>
      <td>
         IR image, visual image. MSX
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
         8 GB internal storage
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
         Operation time
      </td>
      <td>
         about 4-6 hours
      </td>
      <td>
         about 4 hours
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
   <tr>
      <td>
         Price
      </td>
      <td class="auto-style1">
         <strong>~500€</strong>
      </td>
      <td class="auto-style2">
         <strong>2000€</strong>
      </td>
   </tr>
</table>


----------


**DEVICE FIRMWARE**


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/Screenshots.PNG" width="700">


The **latest releases** of device firmware can be found **[here](https://github.com/maxritter/DIY-Thermocam/tree/master/Firmware/Releases)**. For the firmware **SDK**, check out **[this instruction](https://github.com/maxritter/DIY-Thermocam/tree/master/Firmware/Source)**.


----------


**THERMAL IMAGES**


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/ThermalImg.PNG" width="700">


More thermal images in **full resolution** can be found **[here](https://github.com/maxritter/DIY-Thermocam/tree/master/Images/Thermal)**.


----------


**THERMAL IMAGE COMPARISON**


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/ThermalComp.jpg" width="700">


This is a **comparison** between the **DIY-Thermocam**, the **Seek XR** and a **FLIR E8** thermal imager.


----------


**SOFTWARE**


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/Live Viewer and Updater.PNG" width="500">


The standalone PC **[thermal live viewer](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Thermal%20Live%20Viewer)** software allows you to **capture** HQ **thermal** and **visual images**, as well as **videos** right on the **computer**.  Various **settings** can be **changed** over the UI, including a calibration and two **different transfer modes**. The software is written in **Python**, so it can be modified to your own needs and executed on **any OS**. New **firmware updates** can be flashed to the device easily with the **[firmware updater](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Firmware%20Updater)** application.


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/ThermoVision.png" width="700">


There is a powerful **[thermal analysis software](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Thermal%20Analysis%20Software)** for the PC, called **ThermoVision**. The **image browser** can show all thermal & visual images on the internal storage, when the device is connected over USB. They can then be loaded with one click. The software allows you to **analyze** and **edit** the **thermal raw data files** right from the device, create **combined images** and **stream & record live images** (thermal & visual) from the Thermocam. You can even set **temperature thresholds**, which trigger a specific action like opening a file, saving an image or sending a command to the serial port.


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/DataViewer.png" width="600">


The second possibility to **view** and **edit the raw data** files right on the **PC**, is the **[thermal data viewer](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Thermal%20Data%20Viewer)**. It allows various functions to alter the thermal range, **add measurement points** as well as various filters. The program is also capable of **converting whole folders** of raw data frames into images (JPG, BMP or PNG) or avi videos. The **live tab** allows you to stream live images from the device and the calibration tab offers functions to edit the **raw-to-absolution temperature conversion**. You can also convert videos or interval pictures with the **[video converter](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Video%20Converter)** to avi videos.


----------


**VIDEO OUTPUT MODULE**


<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/Website/VideoOutput.jpg" width="300">


If you want to mount the DIY-Thermocam on a **drone** or just wanna **output** the **video signal** to a **TV** screen, consider adding the **external video out module** to your device. 


It just needs to be plugged into the **microUSB cable** of the Thermocam and is also supplied with **power** by the **internal battery**. 


It is capable of providing a **HDMI signal** or **analog video output** (**PAL & NTSC**) with a resolution of **640x480** pixel. 


You need the **following parts**: 


<table style="width: 100%">
   <tr>
      <td style="width: 235px">Raspberry Pi Zero</td>
      <td>
         <a href="https://thepihut.com/products/raspberry-pi-zero?variant=14062715972">
         ThePiHut.com</a>
      </td>
   </tr>
   <tr>
      <td style="width: 235px">Sandisk 8GB microSD</td>
      <td>
         <a href="https://www.ebay.com/itm/SanDisk-32GB-16GB-8GB-Micro-SD-SD-HC-TF-Memory-Card-Retail-Package-Class-4-C4/152291118415?epid=1200079740&hash=item2375420d4f%3Am%3AmUGMwSe_qfLwgGGYCXauK4Q&var=451490926483">
         Ebay.com</a>
      </td>
   </tr>
   <tr>
      <td style="width: 235px">microUSB cable</td>
      <td>
         <a href="https://www.ebay.com/itm/Rapid-Charge-Micro-USB-Cable-Fast-Charging-For-Tablet-Android-Smartphone/263383418541?hash=item3d52e006ad%3Am%3AmZBxWznWnpAS_LiXK2TcO-Q&var=562394099416">
         Ebay.com</a>
      </td>
   </tr>
   <tr>
      <td style="width: 235px">Enclosure</td>
      <td>
         <a href="https://www.amazon.com/SB-High-Quality-Raspberry-Zero/dp/B01CQ3UCXS/ref=sr_1_2?ie=UTF8&amp;qid=1483872527&amp;sr=8-2&amp;keywords=SB+High+Quality+Raspberry+Pi">
         Amazon.com</a>
      </td>
   </tr>
   <tr>
      <td style="width: 235px">USB-OTG cable</td>
      <td>
         <a href="http://www.ebay.com/itm/Micro-USB-Male-to-USB-2-0-Female-Host-OTG-Adapter-Cable-for-Nexus-7-/231386794712?hash=item35dfba4ad8:g:61IAAOSwA3dYcJFT">
         Ebay.com</a>
      </td>
   </tr>
   <tr>
      <td style="width: 235px">Mini HDMI adapter</td>
      <td>
         <a href="http://www.ebay.com/itm/HDMI-MALE-MINI-TO-HDMI-FEMALE-MONITOR-ADAPTER-CONNECTOR-LAPTOP-CONVERTER-VIDEO-/252130803657?hash=item3ab42abfc9:g:ibwAAOSw5VFWICCJ">
         Ebay.com</a>
      </td>
   </tr>
   <tr>
      <td style="width: 235px">2-pin female header</td>
      <td>
         <a href="http://www.digikey.com/product-detail/en/sullins-connector-solutions/PPTC021LGBN-RC/S5438-ND/775896">
         Digikey.com</a>
      </td>
   </tr>
</table>


Download and flash the **[complete image](https://drive.google.com/open?id=0B5k537ZMNlFANUdpaTRFb2pqcXM)** to the SD card, it runs a **python script** that you can modify to your own needs.


Check out the [**video output module manual**](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/VideoModule_01.pdf) how to **assemble** and **use** the module.
<!--stackedit_data:
eyJoaXN0b3J5IjpbNjA2ODgzMDk4XX0=
-->