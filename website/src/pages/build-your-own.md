---
title: Build Your Own
subtitle: >-
  This guide enables you to build your own DIY-Thermocam V3 at home easily. It's
  like LEGO, but for grown-ups!
seo:
  title: DIY-Thermocam | Open-source thermal imaging for the rest of us
  description: >-
    The DIY-Thermocam is a do-it-yourself infrared camera, based on the popular
    FLIR Lepton long-wave infrared sensor and the Arduino compatible Teensy 4.1
    microcontroller.
  robots: []
  extra:
    - name: 'og:image'
      value: >-
        https://cdn.forestry.io/res2/kehY9d3Z81zIpkkR3LMUsMsU7i4WKUbI6bEfHfrCCEs/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDU1dOTlFXY3dQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tOTdl/MWEzN2RjYmE2MTQ5/MWMzNzkzMjI0NDU1/MzUxNDU4MzIwMjc0/MC9Mb2dvX0xhcmdl/LnBuZw
      keyName: property
      relativeUrl: false
    - name: 'og:title'
      value: DIY-Thermocam | Open-source thermal imaging for everyone
      keyName: property
      relativeUrl: false
    - name: 'og:description'
      value: >-
        An open-source, do-it-yourself thermal imager based on the popular FLIR
        Lepton long-wave-infrared array sensor.
      keyName: property
      relativeUrl: false
  type: stackbit_page_meta
template: page
---
# Prerequisites

![](https://cdn.forestry.io/res2/B1n3GNdrd8BwBLUxPMurAfEwZ5yy-h6TSVUsEqH7LJo/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDU2xFMVVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tYWY0/ZmNhM2ZhNDRmYWQ5/ZDRlMDEwMjhkNTIy/MjI0YWYyZGU4ZjFm/My8xLmpwZw)

The [**DIY-Thermocam V3 KIT**](https://py.pl/1aOzh9) contains **all required parts apart from the FLIR Lepton3.5 sensor and FLIR Lepton Breakout Board V2**:

*   1 x PCB V3.0

*   6 x 2D Laser Cutted Enclosure Parts

*   1 x Charging Module

*   1 x Charging LED

*   1 x 5V Step-Up Converter

*   1 x 3.2" TFT LCD Touch Display

*   1 x Teensy 4.1 Microcontroller

*   1 x Red Lipo Protector

*   1 x 8GB microSD Card

*   4 x Screw M2x18

*   4 x Standoff M2x12

*   3 x Screw M2x8

*   4 x Distance Bolt M2.5x11

*   4 x Distance Bolt M2.5x20

*   4 x Distance Bolt M2.5x5

*   4 x Nut M2 Plastic

*   3 x Nut M2 Metal

*   3 x Washer M2

*   8 x Screw M2.5x6

*   1 x Tripod

*   1 x Tripod Socket

*   3 x Cable Piece Red

*   3 x Cable Piece Black

*   1 x Lithium Polymer Battery

*   1 x microUSB Cable

*   1 x Coin Cell Battery

*   2 x Male Header Strip

*   2 x Female Header Strip

*   2 x Shrinking Tube

*   1 x Power Switch

*   1 x Push Button

*   1 x Coin Cell Holder

*   1 x Display Connector

*   1 x Lepton Connector

*   4 x 4.7k Resistor

*   2 x 10k Resistor

In case you do not already own it, you can buy the [**FLIR Lepton Breakout Board V2**](https://www.flir.eu/products/lepton-breakout-board-v2.0/) separately from one of those sources:

*   [Groupgets.com](https://store.groupgets.com/products/lepton-breakout-board-v2-0)

*   [Digikey.com](https://www.digikey.com/en/products/detail/flir-lepton/250-0577-00/10385179)

*   [Mouser.com](https://eu.mouser.com/ProductDetail/FLIR-Lepton/250-0577-00?qs=DRkmTr78QARne0IUCYtsyA%3D%3D)

In case you do not already own it, you can buy the [**FLIR Lepton3.5**](https://www.flir.eu/products/lepton/?model=3.5%20Lepton) sensor separately from one of those sources:

*   [Groupgets.com](https://store.groupgets.com/products/flir-lepton-3-5)

*   [Digikey.com](https://www.digikey.com/en/products/detail/flir-lepton/500-0771-01/7606616)

*   [Mouser.com](https://eu.mouser.com/ProductDetail/FLIR-Lepton/500-0771-01?qs=DRkmTr78QAQNv%2FBEKfCn%252BQ%3D%3D)

For the **tools**, you need the following ones in order to follow the assembly process:

![](https://cdn.forestry.io/res2/MJhD9QBhahfNBqUfKOQpGtqZp5W00rnh3YyS-jsRlMY/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDU2swMVVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tYTI2/ODRlY2ZkY2E2ZDkw/NzgyMDFkMGY4MGE3/OGY3Njc4YzM4ZjA1/YS8yLmpwZw)

*   Soldering Iron

*   Soldering Tin

*   Cutting Pliers

*   Sharp Knife

*   Screwdriver

*   Tweezer

Eventually you may need a file in case there are any overhanging parts on the header strips.

# Building Steps

During the following steps, each tool or part that is added to this step is display in **bold text.**

#### 1.

Grab the **Teensy 4.1 Microcontroller** and the two **Male Header Strips**. Use the **Cutting Pliers** to cut one 24-pin strip from each pre-cutted strip, so you have two in total. Make sure that each of those long strips has a flat side as indicated on the image below, so they do not stand out of PCB later. Use one of the remaining strips to cut one 5-pin strip and one 1-pin strip:

![](https://cdn.forestry.io/res2/HpbTzlI_RwWjNy2oCtNbsca_i-YkHsLZMJfuk7\_7hHM/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVDJkUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMzcy/MDJiN2Y1MWU2YjYz/MTgxOTMwYTJkNmMz/NTc2MmJhOWY5MDc5/OC8zLmpwZw)

#### 2.

Put the two long 24-pin strips into their marked positions on the backside of the Teensy 4.1 Microcontroller and make sure the flat sides are on the side where the USB port is. Then put in the 5-pin and the 1-pin strip and use the **Soldering Iron** and some **Soldering Tin** to solder all strips into their positions. Make sure all strips stand in a right angle towards the PCB of the Teensy before you solder them:

![](https://cdn.forestry.io/res2/\_jUFjLupySHeDA3UmF9SDSpgu7cCG9o5DrV7YJLagmE/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVDNOUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNzM3/MTFmYjViZGE4NmZl/ZjFiM2M2N2M5YjMw/NDMxNjRiNDUwNjEx/Yi80LmpwZw)

#### 3.

Use the **Sharp Knive** to cut the connection between the VUSB and VIN pads on the backside of the Teensy 4.1 Microcontroller, so the Thermocam does not power up when the USB cable is connected but the power switch is turned off. Use the **Multimeter** and its connection checker mode to verify that the two pads are really separated. If they are not, use the Sharp Knive again until there is no connection between the pads anymore:

![](https://cdn.forestry.io/res2/NY6bUppParDR-R3k7tlHKQIQhbg_W7jb8Fpg0Bi49jk/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVDBsUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNjUx/ZDMwNmNmMDhhNDg4/YjJhNWVmZTE0Njg3/MWNjZmFjZDZjZTQ2/ZC81LmpwZw)

#### 4.

Put the **8GB microSD Card** into its socket on the top of the Teensy 4.1 Microcontroller. This storage can be accessed as a mass storage device over the microUSB cable on a PC. You do not need to format the card beforehand, this will be done in the first time setup later:

![](https://cdn.forestry.io/res2/-W1lZqFeXAwoOfIDQcW4LrQzM1IxkRC4YhfnKussMm0/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVDBWUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMWYz/ZjI5NmU0ZDkxZmNj/ZDdjMDAwMjk2NzBm/ZjRmYTRhZGUyZjk0/OS82LmpwZw)

#### 5.

Grab the **PCB V3.0** and the two **Female Header Strips**. Use the **Cutting Pliers** in the same way you did for the male headers and cut a 24-pin strip from each of the pre-cutted strips, so one side is flat as marked on the image. Then cut the remaining 5-pin and 1-pin strips, each from another pre-cutted strip so that you have at least two 10-pin strips left for a later step:

![](https://cdn.forestry.io/res2/jx9fuSq5ssRbLaWfHChKMr8F6m98GolbTBuZtehP2II/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVFRSUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tZGU5/NTdhMjQyMjhlZDdh/OTY1YTJmZWRlNDhm/YmIwM2MxNTQ1Y2Rj/ZC83LmpwZw)

#### 6.

Put the female headers on top of the male headers on the backside of the Teensy 4.1 Microcontroller. This avoids soldering the Teensy directly to the PCB and therefore simplifies any repairs or later adjustments later. In case the 5-pin header does not fit in well, use a **File** to drawfile its edges:

![](https://cdn.forestry.io/res2/BtUZ8YoU7ln9nR0nJN96yW9YksM2exEqDyUusEJvAqw/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVG5OUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tOGY4/ZDNiZjU0MTU5Mzdk/MTMwYzE2ZjAyMDBj/NGI5YmYzNDU1OGRh/OS84LmpwZw)

#### 7.

Put the Teensy 4.1 Microcontroller into the marked position on top of the PCB V3.0, and make sure it sits really tight:

![](https://cdn.forestry.io/res2/09F2DGWSW-9\_IYCz_K20mrFdmGDHVd7X_heIDxav7ro/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVUd0UlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNzM2/YWM0ODA0YzQzMDhl/ZThhZThjNGY3NmM2/YzE3ZmJhZmM2MmYz/Yy85LmpwZw)

#### 8.

Carefully solder each of the holes on the backside of the PCB:

![](https://cdn.forestry.io/res2/IgOAXvGjBXzMlRzTVHOZ4wQRXzG9YbyqQ-uUyecEchE/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVHpCUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMGNj/MGE0NmUyNGY4NmU2/YzgyMzE0ZGEyNjZk/MWJlYmRjNzE0Zjhi/ZS8xMC5qcGc)

#### 9.

Turn the PCB around and make sure that none of the two long male and female headers of the Teensy 4.1 Microcontroller do stand out over the edges of the PCB. In case they do, use a file to make them flat again, otherwise the enclosure will not fit later. Then grab the remaining **Male Header Strips** and cut one 3-pin strip and four 1-pin strips out of them. Use them to put the blue **Charging Module** into its position on the top of the PCB:

![](https://cdn.forestry.io/res2/s46jYWIc-n0eDXnn_fetyb9uI0EQlBNZsBEddN6ce4I/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVUdkUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tZGU3/NmQ4MDg0NDI2ZTBl/NzNlOGFhNzY5MGY1/ZGFiYzY2YTc3N2Yw/NC8xMS5qcGc)

#### 10.

Turn the PCB around and solder the seven pins, then use the **Cutting Pliers** to remove the overlapping parts:

![](https://cdn.forestry.io/res2/fC2H02YakBzV5EGAknt4oXGu5L5ud5QwSNJ8WMK5jzk/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVURCUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tZTBi/ODFhOGY2YjIwZDgy/ZDA3ZjhiYmI3YTQ3/NGNhNTBmZDI0MTVh/OC8xMi5qcGc)

#### 11.

Grab the **5V Step-Up Converter** and cut a 3-pin strip from the remaining **Male Header Strips**, then put them into the marked position on the image:

![](https://cdn.forestry.io/res2/Yd9U-LVh1iXIdPN26uSibQTQd6rP6KarafW5-CyzJFQ/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVURSUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNWVl/YzI3MzE3ZTZlYmM3/ZjNlYmIxYmVlMTFh/NTU0ZWMwYzhiODk4/Ni8xMy5qcGc)

#### 12.

Turn the PCB around and solder the three pins, then use the **Cutting Pliers** to remove the overlapping parts:

![](https://cdn.forestry.io/res2/mIcOf4u3RYoFLm2ePoyk2dKhf6a-oW54X-MUSoR968I/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDVUhOUlVWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tZmY1/N2I0ZjIyZTM2NmE1/NjViZDY4YzFmYTA0/NjUzMGJlNTZhMjU3/MS8xNC5qcGc)

#### 13.

Grab the **Display Connector** and put it into the marked position on the backside of the PCB by making sure the little notch in the middle is directed towards the inner side of the PCB. Then carefully solder the fourty pins on the backside:

![](https://cdn.forestry.io/res2/YGtGm1DB6TcBBBIfo8aIW1Kp_pqP7lo02YXGOiFopJY/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUVUxU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMzA4/OTMxNzk5MzEyZWU0/NDU5YWNmNTdiMmRm/ZWNlNTJlZTZhNjI0/MS8xNS5qcGc)

#### 14.

Grab the **Charging LED** and put it into the marked position on the backside of the PCB, so that the shorter leg is inside the ground hole labeled "G":

![](https://cdn.forestry.io/res2/P-0Nz5ZKMI-9HvN5oe5qE7t-uj201aT2KmxSsdsGm6g/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUVdOU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tYjY2/YTVlNDRlYTQ2Mjcx/NTNjM2U5OTBjYzcz/OWFjMDdkZDQyNWMy/Zi8xNi5qcGc)

#### 15.

Turn the PCB around and solder the three pins, then use the **Cutting Pliers** to remove the overlapping parts:

![](https://cdn.forestry.io/res2/h5iF1eTC7kT64UZNiUU696YDGw2lQLDKYRpX5t4o82Q/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUWtsU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMDM4/ZWMyNjkwMzYwZjNk/NTc3NWVhODhiZjYy/ZDVhZDQxN2YyODQ1/OC8xNy5qcGc)

#### 16.

Grab the four **4.7K Resistors** and the two **10K Resistors** and put them into their marked positions on the top of the PCB:

![](https://cdn.forestry.io/res2/C-KOt543nO-Qd4TeZP-fA8bKNio8VIk95zhjmMjBAW0/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUWsxU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tOTYw/MzVmMDJkNmI4NzM3/YjJhNzc1M2Y1OTEw/YjBiMjE3NjI0MWRm/YS8xOC5qcGc)

#### 17.

Turn the PCB around and solder the twelve pins, then use the **Cutting Pliers** to remove the overlapping parts:

![](https://cdn.forestry.io/res2/E0XzLbL\_4zmZgH5jPgMwQs4MBYa2ZNZr0M53H7nEmS0/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUVRoU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMzhj/MTE5MGQxNWEzZjRi/MjM0NzQ2YWE5N2I1/MzM0OWFlYjJhZTA1/Ni8xOS5qcGc)

#### 18.

Put the **Coin Cell Holder** into its marked position on the top of the PCB. Do not insert the coin cell battery yet:

![](https://cdn.forestry.io/res2/KilkcACk3pgBB5rZeuUZFZP8cwclTF-iqIUS23UInik/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUVRCU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tOWQ4/MTMxYTM1NTg1MGJj/NjIxNmJhMGIzYmNk/MWE0M2QwNWRiNGRk/MS8yMC5qcGc)

#### 19.

Turn the PCB around and solder the two soldering joins of the coin cell holder:

![](https://cdn.forestry.io/res2/9SPjKBTTWaIiU5XLjGiEWwFrbPUlEcsmoBEQdPL6Ubo/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUWpSU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNjE5/ZTZkNWQ3MDZmMWE5/ZGI4YzViNDI5MzNk/ZTQxNjUyZmJiMmJl/Ni8yMS5qcGc)

#### 20.

Grab the remaining piece of the two **Female Header Strips** and cut them into two 10-pin strips that can be inserted into the position marked below:

![](https://cdn.forestry.io/res2/9uaNnCrX-EOBkoVRpyITrzRuYthd_OQR_TR4rkzcMIo/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUW10U1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMDFl/ODkzNDc2ZDI0OTUx/NjVkNzBmYTZkOTBh/YzhkNzFlNTE4NGEy/NC8yMi5qcGc)

#### 21.

Turn the PCB around and solder the twenty pins:

![](https://cdn.forestry.io/res2/Hrs6Tr0JpBcN6wpK8rUR3WjcElSnJ2gvylyn9Vd\_8oc/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUWpCU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tZGY5/YzE4ZGYxOTFkZmI2/ZGU4MDcwNjlhMTEz/M2Q1ZWFjZmVhOTZj/Mi8yMy5qcGc)

#### 22.

Grab the four **Screw M2x18** put them into the marked positions on the PCB:

![](https://cdn.forestry.io/res2/rmf4DUPxB_B5y324wtFpbhvPEInWlxJVFMDv2a018v0/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUTJ0U1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMWQ3/ZDQ1YTA0ZDE5Y2M2/MTc4NzYxZjcxYmQ3/ZGQyMzgwNzZlYTM2/Ni8yNC5qcGc)

#### 23.

Turn the PCB around and make sure the screws do not drop out. Then grab the **Red Lipo Protector**, remove the protection foil from one side and glue it to the back of the PCB. Make sure it does not stand over the two marked holes:

![](https://cdn.forestry.io/res2/zNdP06qLjKvMfszNxjs19fa0pKVdzhE37KJvYBioUno/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUXpSU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tYTlm/NzgzNDdjMmYxZmU5/ZjEyNjg2NTYxZGMw/ZDFmYWJhZDE3MGY3/Mi8yNS5qcGc)

#### 24.

Grab the four black **Standoff M2x12** and put them on top of the four screws:

![](https://cdn.forestry.io/res2/LV-J1ZxPTXaVwAf4TyhWxtVctxz1QMS54iBlPcsIE-g/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUTJkU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMTA2/NmYxNzI5NzVmZDEw/M2VlNDFkMDYzMDE2/MzdlMmE3MGEzYmEw/MC8yNi5qcGc)

#### 25.

Grab the **Lepton Breakout Board V2** and put the right-angle **Lepton Header** into it. Make sure there is a tight connection between both as indicated on the image:

![](https://cdn.forestry.io/res2/xciNST4xoNhoiCUUM0tHSToz-uwupl8pr9tQ2-rkDTE/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUTFGU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tZjc3/M2E5MzJiZmZhNTc3/ZmZiNGRmMjQ5Zjlh/NzIzNjFhY2NlMGNm/OC8yNy5qcGc)

#### 26.

Put the Lepton Beakout Module V2 on top of the four screws and insert the Lepton Header into the female headers on the PCB. Make sure everything sits tight, then secure it with four Nut M2 Plastic:

![](https://cdn.forestry.io/res2/egnVNhjwOFXg59cI0NjFpslTjW0Po0MuV1BQFs\_8kFQ/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUTJOU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMWJk/ODJmMDlhYTkxNDFi/YTM1MGUyMTAxYWE3/MDU3YmI3ZjM4N2Vh/MC8yOC5qcGc)

#### 27.

Grab the **Lithium Polymer Battery**, one **Red Wire** and one **Black Wire**. Make sure the battery is aligned as on the image below, then solder the red wire to the plus pin and the black wire to the minus pin by applying some solder tin two both pads:

![](https://cdn.forestry.io/res2/\_PeLQo8zbJ7TfqU9sHvGyTJQJLGH98EpxxUV6axLPs0/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUkVWU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tOWE2/YzllZTM5NmEzYTFj/OGExYWZkMWI2Y2Ix/MGUzZTc5YWZlZDRm/OS8yOS5qcGc)

#### 28.

Remove the second protection foil from the Lipo Protector and push the Lithium Battery against it. Then solder the red wire to the plus pin of the connection point labeled "LIPO" on the backside of the PCB, and the black cable to the minus pin:

![](https://cdn.forestry.io/res2/Ae9o3F-IcMvRQ4qBf0gRLt4ZyFKBCpOj17lBjsU0N9k/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUkhkU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tYTJk/NjZjOThiNGZjNzJi/ODJmNDk3ODUwOGRj/ZGFmNTdkNDlkN2Ew/MS8zMC5qcGc)

#### 29.

Grab the first side part of the **Enclosure** and the **Power Switch**. Insert the switch into its position by aligning the enclosure part as on the image below. Grab the remaining two **Red Wires** and solder them to the middle and right pin of the switch:

![](https://cdn.forestry.io/res2/v0EJiourLIEm4bLgXHpX1OpdiDjUl7KkO7Zd4SbmZSY/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUjBGU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tM2Y3/NjI5N2IzMTUzYzhj/Njk5MmMyNmMyMmJi/YzNlZmJjZDZjYmU2/Yy8zMS5qcGc)

#### 30.

Solder the two red wires to the pins on the PCB labeled "SWITCH". The direction does not play a role:

![](https://cdn.forestry.io/res2/2wgCz91ZMgbUtriqrjuz9BcbX-Pl7QaP8kWtEm0mnQw/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUmpSU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tYTM4/MjY3NmUyYzE0ZDkx/YmVkZjJlZWM0ZDQ1/YmUzNzZhOTEyMTFh/ZS8zMi5qcGc)

#### 31.

Grab the upper side of the **Enclosure** and the **Push Button**. Remove the plastic ring from the push button, put it through the hole and make sure the enclosure part is aligned as display on the image, so that the charging LED hole is closer towards you. Then secure the button with the plastic ring and solder the two remaining **Black Wires** to each pin of of the push button. Finally use the two **Shrinking Tubes** to cover the pads:

![](https://cdn.forestry.io/res2/Quxt7lr8dUMFtVHPKh_c-7a_YGeveke1b3AdPmb-T4Q/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUkdkU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tZjky/YjJmM2YzNzEzM2Uy/ZmZhMGI2NzE5OGFl/ZjEzOTUzNzhiZWU0/NS8zMy5qcGc)

#### 32.

Solder the two black wires to the pins on the PCB labeled "BUTTON". The direction does not play a role:

![](https://cdn.forestry.io/res2/QrGQ3faL2aWLJtHuYTZiX-9gXXYpqNbVLcr9ZPPifVc/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUm5OU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNjZl/Nzg2NTA3MTRkZjY5/YmViMWQyZTk2MGI5/N2EzZDYzNDk3NmVk/ZS8zNC5qcGc)

#### 33.

Grab the **3.2" TFT LCD Touch Display**, remove the protection foil from the screen and insert the four **Distance Bolt M2.5x5** and the four **Distance Bolt M2.5x11** in its holes:

![](https://cdn.forestry.io/res2/FPZdkVUHsWgYwN5JYFiqi7G-K8RsH3gGeMvSzsmcRqA/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUkhOU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tYTVi/ZmZmZGFjMWUyYzRj/MjBlN2NkZWZhMzMy/YTU3NmMyNzYwNDBk/My8zNS5qcGc)

#### 34.

Insert the display into the display header and put the distance bolts through the holes on the PCB. Make sure everything sits tight and none of the wires in between is blocking anything:

![](https://cdn.forestry.io/res2/AwjgUu-E_QIIA7c4jEFcAcTNTe6Zp_JJ9Cx_RWit7Ac/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUkdOU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNzA5/ZWQ4NGZmYzQ4NjRh/ZDM5NjA3ZWE5N2M0/MDFkZDdkZjEwMjI2/My8zNi5qcGc)

#### 35.

Grab the four **Distance Bolt M2.5x20** and screw them on top of the four Distance Bolt M2.5x11:

![](https://cdn.forestry.io/res2/xWPU8D8FjJAduGg0ScgP\_88itTDU3OkzZXZWd-qF7Y4/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUkRSU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNTg0/Mzc3NTA4ZWI5YTZi/NWQxNDYyMzUwYTY2/NjZhMGZhMGRmMGMz/Yy8zNy5qcGc)

#### 36.

Grab the bottom side part of the Enclosure, the **Tripod Socket** and the three **Screw M2x8** and put them into their positions:

![](https://cdn.forestry.io/res2/fHxy\_1FRR42v4Lzzd7KIzkQM-mzGZmt-Zqx3un8lFKw/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUldkU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNDkz/YWM4MWFhZTkzZmJi/NTliMjAyNjRjODk3/MzIyNjAwZDk1Y2Nj/MS8zOC5qcGc)

#### 37.

Then secure the bolts with three **Nut M2 Metal** and use a **Screwdriver** to make sure everything sits tight:

![](https://cdn.forestry.io/res2/oQ5MXsZsMmNx9UoWBNezFnTNSwkx3TZfovf3cTioLVg/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUlZWU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tN2Zi/YjAwNzZjYmUyYmU0/NmZjOTczODViZjQ2/MGIyZDU5ZTg2NWIw/ZC8zOS5qcGc)

#### 38.

Grab the front part of the **Enclosure** and use a **Screwdriver** to secure it with four Screw **M2.5x6**. Make sure the front plate is in the right position first, if not turn it upside down:

![](https://cdn.forestry.io/res2/-Bp2uMLaXwhPhLhd0tDoDHNV4jGXhN8savkJHYQ5\_dE/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUlhkU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNTRk/MmQxZGY4ODQ4MTk5/OWFhNDhiYjczMjU3/NzQxM2IwN2NiNzZh/OS80MC5qcGc)

#### 39.

Use a **Tweezer** or your fingers to carefully put the **Lepton2.5** or **Lepton3.5** into the socket of the Lepton Breakout Board V2. There is only one position it fits in. Make sure it snaps in and clicks on the top and bottom, otherwise there will be no connection to the Lepton possible. Next put the **Coin Cell Battery** inside the Coin Cell Holder by having the plus on the top as indicated on the holder:

![](https://cdn.forestry.io/res2/zsNinK44\_XTTm8fbQQkz1cGHc_h8pQ45TObyUCYSMBc/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUlRoU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tN2Rm/ZTY0MjM5NTc4N2Mw/OTA5OTAzMjI1NTA1/N2U5YjQ0YTliMTdk/NC80MS5qcGc)

#### 40.

Grab the remaining two sides of the **Enclosure** and put them into their positions:

![](https://cdn.forestry.io/res2/hEgucMvzRVsYWuGKrcI_oupky08Hp-\_Tin-m6iGWsok/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUmtGU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tMmE4/ZmQyYWEyZmM3NGFi/N2Y3ZGIxMTdlOWRj/Yjk2NzAwNGNiYTFh/OS80Mi5qcGc)

#### 41.

Make sure the USB port of the Teensy 4.1 Microcontroller fits inside the cutout of the bottom side of the Enclosure:

![](https://cdn.forestry.io/res2/xPne-Q1DkwNs7Wp9nPUchdWvljxQARjG0V0PW6JFZRo/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUlhOU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tYzAy/Mzk5ZWU2YWVkMzU4/YTlmN2I5MTg5ZTdi/YjFhMGM1YjBmNTgw/ZS80My5qcGc)

#### 42.

Grab the backside of the **Enclosure** and put it on top of the four sides. Make sure the Lepton is centered inside the cutout, otherwise turn the backside upside down. Then secure it with the remaining four **Screw M2.5x6** by using a **Screwdriver**. Be careful to not tighten the screws too much, otherwise the backplate may break:

![](https://cdn.forestry.io/res2/iV2ijWERRIUTZbkHni63n0cu1de-xRjQfq_KlZwnD70/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUm10U1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tZjEy/YjdlMjNjYmM0N2Qz/ZDY0MmRmMjA0ZDA4/M2QxMTNmZWIxMzA2/ZS80NC5qcGc)

#### 43.

Grab the **Tripod** and screw it into the Tripod Socket, then put the **microUSB Cable** into the USB port of the Teensy 4.1 Microcontroller:

![](https://cdn.forestry.io/res2/wm8Gedj5PRK9fhqtgxCIa2mD608oAnoixwbw-FK1R4Y/fit/512/512/sm/0/aHR0cHM6Ly9hcHAu/Zm9yZXN0cnkuaW8v/cmFpbHMvYWN0aXZl/X3N0b3JhZ2UvYmxv/YnMvZXlKZmNtRnBi/SE1pT25zaWJXVnpj/MkZuWlNJNklrSkJh/SEJDUmxWU1VWRXdQ/U0lzSW1WNGNDSTZi/blZzYkN3aWNIVnlJ/am9pWW14dllsOXBa/Q0o5ZlE9PS0tNjcx/YjE3MmYyODU2Zjk4/NjYyN2YwY2UzNzFm/YmI4NmVmNjhjMzcw/YS80NS5qcGc)

#### 44.

Finally, follow the instruction [**here**](https://github.com/maxritter/DIY-Thermocam/tree/master/Software/Firmware%20Updater) to flash the newest firmware to the device. You can find the latest firmware release [**here**](https://github.com/maxritter/DIY-Thermocam/releases).

Congratulations, you have assembled your own thermal camera! Check out the [**documentation**](https://www.diy-thermocam.net/docs) to learn more about the capabilities of the DIY-Thermocam V3 and also have a look at the open-source [**software ecosystem**](https://www.diy-thermocam.net/software), that extends the device capabilites on your computer.
