---
title: Software
subtitle: >-
  Extend the capabilities of the DIY-Thermocam V3 towards a software ecosystem,
  that offers limitless possibilities for your own applications
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
        https://www.diy-thermocam.net/images/device/inside.jpg
      keyName: property
      relativeUrl: false
    - name: 'og:title'
      value: DIY-Thermocam | Open-source thermal imaging for everyone
      keyName: property
      relativeUrl: false
  type: stackbit_page_meta
template: page
_template: page
---

# Thermal Analysis Software

![](https://www.diy-thermocam.net/images/software/thermal_analysis_sw_1.png)

The Thermal Analysis Software is a powerful desktop software to view, modify and convert raw images created on the DIY-Thermocam V3. The image browser can show all images inside a folder, you can edit them in various ways and export the outputs to an image file. The is provided by a third party author, you can get more information about its capabilities on the [authors site](http://joe-c.de/pages/projekte/thermovision.php). To get started, launch the application and drag-and-drop any Thermocam Raw file into the main windows to edit it.

[**Download**](https://github.com/maxritter/diy-thermocam/blob/master/software/thermal_analysis_software) (Windows only)

# Thermal Live Viewer

![](https://www.diy-thermocam.net/images/software/thermal_live_viewer.png)

The standalone Thermal Live Viewer application allows you to capture HQ thermal images and videos right on the computer. Various settings can be changed over the UI.

[**Download**](https://github.com/maxritter/diy-thermocam/blob/master/software/thermal_live_viewer/) (Python)

# Thermal Data Viewer

![](/images/software/thermal_data_viewer.png)

The Thermal Data Viewer is next to the Thermal Analysis Software software another possibility to view, edit and convert raw data files on your PC. It allows various functions to alter the thermal range, add measurement points as well as various filters. The program is also capable of converting whole folders of raw data frames into images (JPG, BMP or PNG) or avi videos.

[**Download**](https://github.com/maxritter/diy-thermocam/blob/master/software/thermal_data_viewer/) (Windows only)

# **Video Converter**

The DIY-Thermocam allows to capture continous or time-lapse videos on the device, but they are still recorded as single frames and not merged into a single video (due to the limitatations of the microcontroller concerning modern video codecs). You can use the Video Converter to convert any number of frames from your internal storage into a movie file.

[**Download**](https://github.com/maxritter/diy-thermocam/blob/master/software/video_converter/) (Windows only)
