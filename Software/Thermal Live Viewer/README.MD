Compiled versions of the thermal live viewer for Windows can be [downloaded here](http://diy-thermocam.bplaced.net/ThermalViewer/).

If the comport is not COM10 on Windows, you need to [change it](http://plugable.com/2011/07/04/how-to-change-the-com-port-for-a-usb-serial-adapter-on-windows-7/).

On Linux, look for changes in the list of devices in /dev/ - the camera will be listed as a terminal device in the manner of "ttyACM0". You can also list them in the terminal with the "ls /dev/tty*" command.

Edit the "ThermalViewer.py" file, find the line that reads "device = "COM10" under the "Global Variables" section and change it to the full path of the device, for example:

```
device = "/dev/ttyACM0"
```

And save the file.

If there are problems getting the UI to show up, run the program in terminal (e.g. "python ./ThermalViewer.py"). If there are missing dependencies, they will be listed. The program depends on packages like pygame,pybutton and scipy - if they are missing, you can install them using the command "pip install", for example "pyp install pygame"

For the video recording functionality you need ffmpeg. It is included for the combiled Windows versions, otherwise you need to [download](https://ffmpeg.org/download.html) the ffmpeg package.

Change the line:

```
ffmpegCmd = 'ffmpeg -framerate %d -i %s/%%06d.jpg -codec copy %s' % \
            (round(frames), videoFolder, videoFolder + ".avi")
```

to this to get it to work under Linux / Mac OSX:

```
ffmpeg_loc = os.getcwd() + "/"
ffmpeg_loc = ffmpeg_loc.replace(' ', '\\ ')
ffmpegCmd = ffmpeg_loc + 'ffmpeg -framerate %d -i %s/%%06d.jpg -codec copy %s' % \
            (round(frames), videoFolder, videoFolder + ".avi”)
```