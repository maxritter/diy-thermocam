# Thermal Live Viewer

This application allows you to connect to your DIY-Thermocam from a PC over the USB serial protocol and stream live thermal images. 

The compiled version only works under Windows. You need to make sure that your Thermocam is listed as COM10, see [this guide how to change it](http://plugable.com/2011/07/04/how-to-change-the-com-port-for-a-usb-serial-adapter-on-windows-7/).

The program has been written under Python, so you can also execute it under Linux or Mac OSX using the Python source files. You can download them [here](https://www.dropbox.com/s/znqpvhpn3wtnun0/ThermalLiveViewer_3.00_src.zip?dl=1). The Thermal Live Viewer will also work on Linux based SOPCs like the Raspberry Pi, but it might need some small adjustments for resolution and framerate / performance.
