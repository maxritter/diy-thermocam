<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/Logo.png" width="500">
<br>
<br>

The **DIY-Thermocam** is a **do-it-yourself** infrared camera, based on the popular **FLIR Lepton** long-wave-infrared array sensor.  The **material price** of all components is at about **400€**, which makes the device one of the **cheapest standalone solution** for thermal imaging on the market.

<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/Thermocam.jpg" width="500">

The aim of this project is to give **private persons**, **educational institutes** and **companies** access to a **portable**,  **affordable** and **customizable** thermal imaging plattform.
 
There are **various applications** like finding **heat leaks** in the insulation of buildings, the analysis of **electrical** or **mechanical components**, the **detection** of **persons** / animals or even mounting it on a **drone**, together with the additional **video out module**.
 
Constructed as a **self-assembly solution**, the DIY-Thermocam allows you to take **advantage** of the **versatile possibilities** of thermal imaging and to **save money** at one go.
 
**Everything**, from **software** to **hardware**, is completely **open-source**! This allows everyone to **modify** or **extend** the functionalities of the device to their **own needs**!

A **[scientific paper](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/Scientific%20Paper.pdf)** is now available, giving an **overview** about the whole **project** and its **capabilities**.

<a href="https://www.facebook.com/diythermocam/"><img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/Website/FacebookButton.jpg"></a>

----------

**HOW TO MAKE IT**

You can **make** your **own DIY-Thermocam V2** at home **easily**.  For the tools, you only need a **simple soldering iron**, some solder tin, a nippers and a screwdriver. The **design** of the **PCB** is very **basic**: No SMD components, only big through-hole parts and modules.  The **enclosure** can be assembled easily only with **bolts & nuts**, no glue required. An **[illustrated assembly guide](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/AssemblyGuide_03.pdf)** will lead you through the process, step-by-step.

On the **part side**, you need the **components** from the **support package** and the **missing parts** from the **other parts list** below.

The DIY-Thermocam works with the **FLIR Lepton2** (80 x 60) and the **FLIR Lepton3** (160 x 120) LWIR sensor. The **hardware revision** is **detected automatically** by the firmware on startup. 

The **[firmware update guide](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/FirmwareUpdate_06.pdf)** helps you to update your device to the **newest firmware**. In order to understand to **various device functionalities**, have a look at the **[manual](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/Manual_13.pdf)**. The **structure of the raw data files** is explained **[here](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/RawData_07.pdf)**, and the **open USB serial protocol** **[here](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/SerialProtocol_15.pdf)**.


----------

**SUPPORT PACKAGE**

The **support package** contains **most of the components** required to **build** the **DIY-Thermocam V2**. You can **acquire** it by **following the link** below, **leaving your address** in the comment field. **International shipping** is **included** in the donation. **Alternatively**, you can also use the **provided links** on the right side to buy all the components separately **on your own**.

[**-> Get the support package <-**](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=8MTC38TANMB24)


<table style="width: 57%">
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PCB.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Printed Circuit Board<br />
		Against a small donation, you will receive the PCB by post</td>
		<td>
		<a href="https://oshpark.com/shared_projects/ddHY98QR">OSHPark.com</a><br />
		Or use the 
		<a href="https://github.com/maxritter/DIY-Thermocam/tree/master/PCB">gerber files</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Enclosure.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Enclosure<br />
		Against a small donation, you will receive the enclosure by post</td>
		<td>
		<a href="http://www.ponoko.com/design-your-own/products/diy-thermocam-v2-enclosure-13930">
		Ponoko.com</a><br />
		Or use the 
		<a href="https://github.com/maxritter/DIY-Thermocam/tree/master/Enclosure">design files</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Charging.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">TP4057 Charging Module<br />
		Including charging LED&nbsp;</td>
		<td>
		<a href="http://www.ebay.com/itm/Dual-TP4057-Lithium-Battery-1A-LED-Charging-Board-Li-ion-Battery-Charger-Mudule-/400614252936?hash=item5d46786188:g:WTEAAOSwal5YJTmW">
		Ebay.com</a><br />
		<a href="https://www.amazon.com/Solu-Lithium-Battery-Charging-Reverse/dp/B00YZV7RTW">
		Amazon.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Booster.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Pololu U3V12F5<br />
		5V voltage booster&nbsp;</td>
		<td>
		<a href="http://www.ebay.com/itm/5V-Step-Up-Voltage-Regulator-U3V12F5-/201448950785?hash=item2ee74b0c01:g:mwwAAOSw-0xYa86A">
		Ebay.com</a><br />
		<a href="https://www.pololu.com/product/2115">Pololu.com</a><br />
		<a href="https://www.flikto.de/products/pololu-5v-step-up-voltage-regulator-u3v12f5">
		Flikto.de</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PowerSwitch.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">E-Switch R6ABLKBLKFF<br />
		Power switch</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/e-switch/R6ABLKBLKFF/EG1526-ND/301973">
		Digikey.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PushButton.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">RAFI 1.10107.0110104<br />
		Push button</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/rafi-usa/1.10107.0110104/1715-1019-ND/6227436">
		Digikey.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/USBSwitch.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">E-Switch EG1201A<br />
		USB power switch</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/e-switch/EG1201A/EG1902-ND/101723">
		Digikey.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/LipoCon.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">JST S2B-PH-K-S<br />
		Connect the lipo to the PCB&nbsp;</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/jst-sales-america-inc/S2B-PH-K-S(LF)(SN)/455-1719-ND/926626">
		Digikey.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/SDSlot.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Wurth 693063020911<br />
		SD slot for the SD card on the PCB</td>
		<td>
		<a href="http://www.digikey.com/products/en?keywords=%09732-3818-1-ND">
		Digikey.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/SDAdapter.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">MicroSD adapter<br />
		Short microSD to SD adapter for the internal storage card&nbsp;</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/seeed-technology-co-ltd/328030004/328030004-ND/5487856">
		Digikey.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DisplayHeader.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Display connector<br />
		40-pin 2.54mm female header&nbsp;</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/sullins-connector-solutions/SFH11-PBPC-D20-ST-BK/S9200-ND/1990093">
		Digikey.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/LeptonHeader.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Lepton board connector<br />
		8-pin 2.54mm female header&nbsp;</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/sullins-connector-solutions/PPPC081LGBN-RC/S5483-ND/775941">
		Digikey.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/PinHeader.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Pin header strip<br />
		40-pin 2.54mm male header&nbsp;</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/4-103741-0/A26509-40-ND/297917">
		Digikey.com</a> (2 pcs)</td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/CoinHolder.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Keystone 3001<br />
		Holder for the coin cell battery&nbsp;</td>
		<td>
		<a href="http://www.digikey.com/products/en?keywords=Keystone%203001">
		Digikey.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/CoinCell.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">CR1220 coin cell battery&nbsp;<br />
		Battery for the real-time-clock</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/energizer-battery-company/CR1220VP/N033-ND/704851">
		Digikey.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Res47.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">4.7K 1/4W 1% resistor<br />
		Multi-purpose resistor&nbsp;</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/stackpole-electronics-inc/RNMF14FTC4K70/S4.7KCACT-ND/2617510">
		Digikey.com</a> (4 pcs)</td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Res10K.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">10K 1/4W 1% resistor&nbsp;<br />
		Multi-purpose resistor</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/stackpole-electronics-inc/CF14JT10K0/CF14JT10K0CT-ND/1830374">
		Digikey.com</a> (2 pcs)&nbsp;</td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/LipoProt.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Lipo protector<br />
		Double-sided adhesive tape for the lipo protection on the PCB&nbsp;</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/3m-tc/12X12-6-9495LE/3M9744-ND/2649289">
		Digikey.com</a><br />
		Or use any thick tape</td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/CableRight.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Angled microUSB cable<br />
		For connection of the Thermocam to the PC&nbsp;and 
		charging of the device</td>
		<td>
		<a href="http://www.ebay.com/itm/Down-Angled-Micro-USB-5-Pin-Male-to-USB-A-Male-Data-Charge-Cable-for-Cell-Phone-/291869468911?hash=item43f4c6a8ef:g:9RgAAOSwxg5X0ovy">
		Ebay.com</a><br />
		Or use any USB cable</td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Tripod.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Mini tripod<br />
		Used as a stand for the device&nbsp;</td>
		<td>
		<a href="http://www.ebay.com/itm/New-Mini-Flexible-Pocket-Size-Metal-Tripod-for-Digital-Camera-Sony-Canon-Nikon-/121351046836?hash=item1c411606b4:g:gCIAAOSwP~tW3qCx">
		Ebay.com</a><br />
		Or use any tripod</td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://github.com/maxritter/DIY-Thermocam/raw/master/Images/BOM/TripodSocket.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Tripod socket&nbsp;</td>
		<td>
		<a href="https://www.amazon.com/The-Hillman-Group-2511-13-Pack/dp/B00HYLZ33W/">
		Amazon.com</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Screw M2 x 10&nbsp;(6 pcs)</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/ScrewM25x6.png">
		Details</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Distance M2 x 3 (6 pcs)&nbsp;</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DistanceM2x3.png">
		Details</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Distance M2 x 3.5 (2 pcs)&nbsp;</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DistanceM2x35.png">
		Details</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Screw M2 x 8 (5 pcs)&nbsp;</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/ScrewM2x8.png">
		Details</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Screw M2.5 x 6 Black (8 pcs)&nbsp;</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/ScrewM25x6.png">Details</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Bolt M2.5 x 12 (4 pcs)&nbsp;</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DistanceM2.5x12.png">
		Details</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Bolt M2.5 x 11 (4 pcs)&nbsp;</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DistanceM2.5x11.png">
		Details</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Bolt M2.5 x 5 (4 pcs)&nbsp;</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/DistanceM2.5x5.png">
		Details</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Nut M2 (5 pcs)&nbsp;</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/NutM2.png">
		Details</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Nut M2 Plastic (6 pcs)&nbsp;</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/NutM2.png">
		Details</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;</td>
		<td style="width: 235px">Washer M2 (3 pcs)&nbsp;</td>
		<td>
		<a href="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/WasherM2.png" >
		Details</a></td>
	</tr>
</table>

----------

**OTHER PARTS**

The following list contains **parts that are not included** in the **support package**. You need to **buy** them over one of the **provided links** on the right side. **Digikey.com** provides **free international shipment** for all orders **over 100€**, so I recommend buying the Lepton Module, Spot Sensor, Teensy and SD card from them **to reach that value**. I also recommend [**extracting**](http://hardtechlife.com/teardown-flir-one-for-android/) the FLIR Lepton3 sensor from a **FLIR One**. The **Lepton2** also works, but currently its **not possible** to buy it from **outside the US** due to new regulations.

<table style="width: 57%">
<tr>
		<td style="width: 111px">
		<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Lepton.png" style="color: rgb(0, 0, 0); font-family: &quot;Times New Roman&quot;; font-size: medium; font-style: normal; font-variant-ligatures: normal; font-variant-caps: normal; font-weight: normal; letter-spacing: normal; orphans: 2; text-align: start; text-indent: 0px; text-transform: none; white-space: normal; widows: 2; word-spacing: 0px; -webkit-text-stroke-width: 0px; user-select: none;" /></td>
		<td style="width: 235px">FLIR Lepton2/Lepton3 shuttered<br />
		Long-wave-infrared array sensor<br />
		</td>
		<td>
		FLIR One -
		<a href="https://www.amazon.co.uk/gp/offer-listing/B01478QCHU/ref=sr_1_1_olp?ie=UTF8&amp;qid=1486731192&amp;sr=8-1&amp;keywords=FLIR+One&amp;condition=used">
		<strong>Android</strong></a> 
		or
		<a href="https://www.amazon.co.uk/gp/offer-listing/B0123QLZJ8/ref=sr_1_3_olp?ie=UTF8&amp;qid=1486731192&amp;sr=8-3&amp;keywords=FLIR+One&amp;condition=used">
		<strong>iOS</strong></a> (<strong>Lepton3</strong>)<br />
		Alternative 1: 		<a href="http://www.digikey.com/product-detail/en/flir/500-0659-01/500-0659-01-ND/5215153">
		Digikey.com</a> (<strong>Lepton2</strong>) - US ONLY<br />
		Alternative 2:
		<a href="https://store.groupgets.com/#!/50°-Shuttered-FLIR-Lepton-Thermal-Imager/p/52199502/category=0">
		GroupGets.com</a> (<strong>Lepton2</strong>)</td>
	</tr>
<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/LeptonModule.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">FLIR Lepton Breakout Board<br />
		Interface the Lepton over SPI / I2C&nbsp;</td>
		<td>
		<a href="http://www.digikey.com/products/en/development-boards-kits-programmers/evaluation-boards-sensors/795?k=FLIR%20Lepton">
		<strong>Digikey.com</strong></a><br />
		Alternative 1:
		<a href="https://store.groupgets.com/#!/FLIR-Lepton-Breakout-Board/p/52239830/category=0">
		GroupGets.com</a></td>
	</tr>
<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/SpotSensor.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">MLX90614-BCF<br />
		Spot sensor for absolute temperature measurement&nbsp;</td>
		<td>
		<strong>
		<a href="http://www.digikey.com/product-detail/en/melexis-technologies-nv/MLX90614ESF-BCF-000-TU/MLX90614ESF-BCF-000-TU-ND/3641020">
		Digikey.com</a></strong><br />
		Alternative 1:
		<a href="http://as-electronic.net/MLX90614ESF-BCF-000">As-electronic.net</a></td>
	</tr>
<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Teensy36.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Teensy 3.6<br />
		Cortex M4 Microcontroller&nbsp;</td>
		<td>
		<strong>
		<a href="http://www.digikey.com/product-detail/en/sparkfun-electronics/DEV-14057/1568-1442-ND/6235190">
		Digikey.com</a></strong><br />
		Alternative 1:
		<a href="https://www.pjrc.com/store/teensy36.html">PJRC.com</a><br />
		Alternative 2:
		<a href="https://www.flikto.de/products/teensy-3-6-v3-6-usb-mikrocontroller">
		Flikto.de</a></td>
	</tr>
	<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Storage.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">SanDisk 8GB microSD card<br />
		Can be any size, but needs to be Class4 microSD&nbsp;</td>
		<td>
		<strong><a href="http://www.digikey.com/products/en?keywords=P17033-ND">
		Digikey.com</a></strong><br />
		Alternative 1:
		<a href="https://www.amazon.com/SanDisk-microSD-High-Capacity-microSDHC/dp/B00488G6P8/ref=sr_1_2?ie=UTF8&amp;qid=1486731377&amp;sr=8-2&amp;keywords=8GB+microSD">
		Amazon.com</a></td>
	</tr>
<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/VisualCam.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">Arducam V2 Mini 2MP<br />
		Visual camera module&nbsp;</td>
		<td>
		<strong>
		<a href="http://www.ebay.com/itm/Arducam-2MP-V2-Mini-Camera-Shield-with-ESP8266-Nano-Esp-12F-Evaluation-Kits-/291833972833?hash=item43f2a90861:g:gUYAAOSwqfNXnrKR">
		Ebay.com</a></strong><br />
		Alternative 1:
		<a href="http://www.uctronics.com/arducam-2mp-v2-mini-camera-shield-with-esp8266-nano-esp-12f-evaluation-kits.html">
		Uctronics.com</a></td>
	</tr>
<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Display.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">3.2" TT LCD Display Module<br />
		Configuration: 5V, Pin header 4-wire SPI, resistive touch, no font chip</td>
		<td>
		<a href="http://www.buydisplay.com/default/3-2-inch-capacitive-touchscreen-240x320-tft-lcd-module-display">
		<strong>BuyDisplay.com</strong></a></td>
	</tr>
<tr>
		<td style="width: 111px">
		&nbsp;<img src="https://raw.githubusercontent.com/maxritter/DIY-Thermocam/master/Images/BOM/Battery.png" style="-webkit-user-select: none" /></td>
		<td style="width: 235px">3.7V Lithium Polymer Battery&nbsp;<br />
		With JST-PH connector, max. dimens.: 60mm (w) x 55mm (l) x 6.5mm (h)</td>
		<td>
		<a href="https://www.aliexpress.com/item/Free-shipping-1pcs-lot-655560-3-7-V-lithium-polymer-battery-3000-mah-DIY-mobile-emergency/32788025260.html?spm=2114.01010208.3.60.bQsjkq&amp;ws_ab_test=searchweb0_0,searchweb201602_2_10065_10068_10000009_10000032_10084_10000025_10083_10080_10000029_10082_10081_10000028_10110_10111_10112_10060_10113_10114_10062_10115_10056_503_10055_10054_10059_10000044_10099_501_10078_10079_10000022_10000012_429_10103_10073_10000015_10102_10101_10096_10000035_10000018_10000019_10052_10053_10050_10107_10051_10106,searchweb201603_10,afswitch_5,single_sort_1_default&amp;btsid=895a580d-2b5e-4679-97c6-2793638cf17a">
		<strong>Aliexpress.com</strong></a><br />
		Alternative 1:
		<a href="https://www.sparkfun.com/products/8483">Sparkfun.com</a><br />
		Alternative 2:
		<a href="https://shop.mikroe.com/accessories/batteries/li-polymer-battery-37v-2000mah">
		Mikroe.com</a></td>
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
 - **160 x 120** pixel **FLIR Lepton3** long-wave infrared array sensor for live
   thermographic images
 - Frame rate of up to **9 FPS** (US export compliance) over the serial
   connection, 5 FPS on the device itself
 - **2 MP visual camera** to capture optical images, that can be used in a
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
 - **8GB internal storage**, accessible as an exchangeable SD / microSD
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

<table align="" style="border: 0px solid #000000;" class="mceEditable" data-mce-contenteditable="true" border="0" cellpadding="3" cellspacing="0" height="100%" width="100%">
    <tbody>
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
                Thermal sensitivity</td>
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
                -40° C to 200° C
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
                -70° C to 380° C
            </td>
            <td>
                -20° C to 250° C
            </td>
        </tr>

        <tr>
            <td>
                Spot sensor temp. accuracy</td>
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
                Price</td>
            <td class="auto-style1">
                <strong>400€</strong></td>
            <td class="auto-style2">
                <strong>2000€</strong></td>
        </tr>
    </tbody>
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

<table style="width: 65%">
	<tr>
		<td style="width: 235px">Raspberry Pi Zero</td>
		<td>
		<a href="https://thepihut.com/products/raspberry-pi-zero?variant=14062715972">
		ThePiHut.com</a></td>
	</tr>
	<tr>
		<td style="width: 235px">Sandisk 8GB microSD</td>
		<td>
		<a href="http://www.ebay.com/itm/SANDISK-8GB-MOBILE-ULTRA-MicroSD-Class-10-UHS-48MB-S-SDHC-TF-FLASH-">
		Ebay.com</a></td>
	</tr>
	<tr>
		<td style="width: 235px">microUSB cable</td>
		<td>
		<a href="http://www.ebay.com/itm/NEW-ORIGINAL-SAMSUNG-MICRO-USB-CHARGING-CABLE-FOR-OEM-GALAXY-S3-S4-S6-NOTE-4-/262426066726?var=561224615039&amp;hash=item3d19cfff26:g:UOEAAOSwSclXLl3j">
		Ebay.com</a></td>
	</tr>
	<tr>
		<td style="width: 235px">Enclosure</td>
		<td>
		<a href="https://www.amazon.com/SB-High-Quality-Raspberry-Zero/dp/B01CQ3UCXS/ref=sr_1_2?ie=UTF8&amp;qid=1483872527&amp;sr=8-2&amp;keywords=SB+High+Quality+Raspberry+Pi">
		Amazon.com</a></td>
	</tr>
	<tr>
		<td style="width: 235px">USB-OTG cable</td>
		<td>
		<a href="http://www.ebay.com/itm/Micro-USB-Male-to-USB-2-0-Female-Host-OTG-Adapter-Cable-for-Nexus-7-/231386794712?hash=item35dfba4ad8:g:61IAAOSwA3dYcJFT">
		Ebay.com</a></td>
	</tr>
	<tr>
		<td style="width: 235px">Mini HDMI adapter</td>
		<td>
		<a href="http://www.ebay.com/itm/HDMI-MALE-MINI-TO-HDMI-FEMALE-MONITOR-ADAPTER-CONNECTOR-LAPTOP-CONVERTER-VIDEO-/252130803657?hash=item3ab42abfc9:g:ibwAAOSw5VFWICCJ">
		Ebay.com</a></td>
	</tr>
	<tr>
		<td style="width: 235px">2-pin female header</td>
		<td>
		<a href="http://www.digikey.com/product-detail/en/sullins-connector-solutions/PPTC021LGBN-RC/S5438-ND/775896">
		Digikey.com</a></td>
	</tr>
	</table>

Download and flash the **[complete image](https://drive.google.com/open?id=0B5k537ZMNlFANUdpaTRFb2pqcXM)** to the SD card, it runs a **python script** that you can modify to your own needs.

Check out the [**video output module manual**](https://github.com/maxritter/DIY-Thermocam/raw/master/Documents/VideoModule_01.pdf) how to **assemble** and **use** the module.

---------- 

**Contact me** by **[mail](mailto:mail@maxritter.net)** if you have any **questions** / **feedback** / **improvements**

**Last updated**: 10.02.2017
