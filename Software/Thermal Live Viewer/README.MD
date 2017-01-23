Compiled versions of the thermal live viewer for Windows can be downloaded here: 
http://diy-thermocam.bplaced.net/ThermalViewer/.

For this compiled version, the device needs to be connected to comport COM10. Check out this tutorial how to change to comport on Windows: http://plugable.com/2011/07/04/how-to-change-the-com-port-for-a-usb-serial-adapter-on-windows-7/.

The python source code can also be executed on Linux or Mac OSX. Use Python 2.x, not 3.x, otherwise there will be an issue with the serial communication.

If the comport is not COM10 on Windows, you need to change it. For Linux / Mac OSX, look at the output of ls /dev/tty.* in Terminal, whereby an output such as this would be seen:
dyn-736-872-21-211:~ Jason$ ls /dev/tty.*
/dev/tty.Bluetooth-Incoming-Port	/dev/tty.usbmodem2516811
/dev/tty.JasonsiPhone-

Looking at the entry with “tty.usbmodem...”, change the line
ser = serial.Serial("COM10", 115200)
to this:
ser = serial.Serial("/dev/tty.usbmodem1894391", 115200)

The number suffixing the path seems to change with each camera.

For the video recording functionality you need ffmpeg. It is included for the combiled Windows versions, otherwise you need to download the ffmpeg package from here: https://ffmpeg.org/download.html.

Change the line:

ffmpegCmd = 'ffmpeg -framerate %d -i %s/%%06d.jpg -codec copy %s' % \
            (round(frames), videoFolder, videoFolder + ".avi")

to this to get it to work under Linux / Mac OSX:

ffmpeg_loc = os.getcwd() + "/"
ffmpeg_loc = ffmpeg_loc.replace(' ', '\\ ')
ffmpegCmd = ffmpeg_loc + 'ffmpeg -framerate %d -i %s/%%06d.jpg -codec copy %s' % \
            (round(frames), videoFolder, videoFolder + ".avi”)
