import serial
import numpy
import pygame
import pygbutton
import os
import scipy
import time
import scipy.ndimage
import sys
import shutil
import struct
import platform
import io
from skimage.transform import resize
from src.colorschemes import ColorSchemes
import cv2

class LiveViewer:
    def __init__(self, serial_port):
        self.colorschemes = ColorSchemes()
        self.serial_port = serial_port

        # Start & Stop command
        self.cmd_start = 100
        self.cmd_end = 200

        # Serial terminal commands
        self.cmd_get_rawlimits = 110
        self.cmd_get_rawdata = 111
        self.cmd_get_configdata = 112
        self.cmd_get_calstatus = 113
        self.cmd_get_calibdata = 114
        self.cmd_get_spottemp = 115
        self.cmd_set_time = 116
        self.cmd_get_temppoints = 117
        self.cmd_set_laser = 118
        self.cmd_get_laser = 119
        self.cmd_set_shutterrun = 120
        self.cmd_set_shuttermode = 121
        self.cmd_set_filtertype = 122
        self.cmd_get_shuttermode = 123
        self.cmd_get_batterystatus = 124
        self.cmd_set_calslope = 125
        self.cmd_set_caloffset = 126
        self.cmd_get_diagnostic = 127
        self.cmd_get_visualimg = 128
        self.cmd_get_fwversion = 129
        self.cmd_set_limits = 130
        self.cmd_set_textcolor = 131
        self.cmd_set_colorscheme = 132
        self.cmd_set_tempformat = 133
        self.cmd_set_showspot = 134
        self.cmd_set_showcolorbar = 135
        self.cmd_set_showminmax = 136
        self.cmd_set_temppoints = 137
        self.cmd_get_hwversion = 138
        self.cmd_set_rotation = 139
        self.cmd_set_calibration = 140
        self.cmd_get_hqresolution = 141

        # Serial frame commands
        self.cmd_rawframe = 150
        self.cmd_colorframe = 151
        self.cmd_displayframe = 152

        # Types of frame responses
        self.frame_capture_thermal = 180
        self.frame_capture_visual = 181
        self.frame_capture_video = 182
        self.frame_normal = 183

        # Config variables
        self.leptonVersion = int
        self.hardwareVersion = int
        self.fwVersion = int
        self.calStatus = 1
        self.minData = int
        self.maxData = int
        self.spotTemp = float
        self.colorScheme = int
        self.tempFormat = bool
        self.spotEnabled = bool
        self.barEnabled = bool
        self.minMaxEnabled = int
        self.rotateScreen = False
        self.textColor = int
        self.filterType = int
        self.calOffset = float
        self.calSlope = float
        self.adjustLimits = bool
        self.hqRes = int

        # Data storage
        self.tempPoints = numpy.zeros((96, 2), dtype=int)
        self.leptonData = None
        self.leptonValues = None
        self.imageRaw = numpy.zeros((320, 240), dtype=int)
        self.image = numpy.zeros((320, 240, 3), dtype=int)
        self.thermalImg = None

        # Color Scheme
        self.colorMap = self.colorschemes.colorMap_rainbow
        self.colorElements = int
        self.colorSchemeTotal = 19

        # Text color
        self.colorWhite = (255, 255, 255)
        self.colorBlack = (0, 0, 0)
        self.colorRed = (255, 0, 0)
        self.colorGreen = (0, 255, 0)
        self.colorBlue = (0, 0, 255)

        # Buttons
        self.buttonSaveThermal = None
        self.buttonSaveVideo = None
        self.buttonRenderMode = None
        self.buttonSpot = None
        self.buttonBar = None
        self.buttonHotCold = None
        self.buttonTextColor = None
        self.buttonFilter = None
        self.buttonColor = None
        self.buttonLimits = None
        self.buttonFormat = None
        self.buttonShutter = None
        self.allButtons = None

        # Global Variables
        self.screen = None
        self.ser = None
        self.calTimer = None
        self.calTimeLeft = None
        self.renderMode = True
        self.recordVideo = False
        self.videoFolder = None
        self.videoCounter = 0
        self.videoStart = None

    def resource_path(self, relative_path):
        """Get absolute path to resource, works for dev and for PyInstaller"""
        base_path = os.path.abspath("./res")
        return os.path.join(base_path, relative_path)

    # Set the color scheme
    def setColorScheme(self, updateValue):
        # Update value
        if updateValue:
            # Pick new color scheme
            if self.colorScheme < (self.colorSchemeTotal - 1):
                self.colorScheme += 1
            else:
                self.colorScheme = 0

            # Send new value to the device
            try:
                # Send command and new value
                sendArray = (chr(self.cmd_set_colorscheme), chr(self.colorScheme))
                self.ser.write(b"".join([x.encode() for x in sendArray]))

                # Get ACK
                inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

            # Error
            except serial.serialutil.SerialException:
                return False

            # Timeout
            if not inData[1]:
                return False

            # Check for ACK
            if inData[1] != self.cmd_set_colorscheme:
                return False

        # Arctic
        if self.colorScheme == 0:
            self.colorMap = self.colorschemes.colorMap_arctic
            self.colorElements = 240
            self.buttonColor.caption = "Color: Arctic"
        # Black-Hot
        elif self.colorScheme == 1:
            self.colorMap = self.colorschemes.colorMap_blackHot
            self.colorElements = 224
            self.buttonColor.caption = "Color: Back-Hot"
        # Blue-Red
        elif self.colorScheme == 2:
            self.colorMap = self.colorschemes.colorMap_blueRed
            self.colorElements = 192
            self.buttonColor.caption = "Color: Blue-Red"
        # Coldest
        elif self.colorScheme == 3:
            self.colorMap = self.colorschemes.colorMap_coldest
            self.colorElements = 224
            self.buttonColor.caption = "Color: Coldest"
        # Contrast
        elif self.colorScheme == 4:
            self.colorMap = self.colorschemes.colorMap_contrast
            self.colorElements = 224
            self.buttonColor.caption = "Color: Contrast"
        # Double-Rainbow
        elif self.colorScheme == 5:
            self.colorMap = self.colorschemes.colorMap_doubleRainbow
            self.colorElements = 256
            self.buttonColor.caption = "Color: DoubleRain"
        # Gray-Red
        elif self.colorScheme == 6:
            self.colorMap = self.colorschemes.colorMap_grayRed
            self.colorElements = 224
            self.buttonColor.caption = "Color: Gray-Red"
        # Glowbow
        elif self.colorScheme == 7:
            self.colorMap = self.colorschemes.colorMap_glowBow
            self.colorElements = 224
            self.buttonColor.caption = "Color: Glowbow"
        # Grayscale
        elif self.colorScheme == 8:
            self.colorMap = self.colorschemes.colorMap_grayscale
            self.colorElements = 256
            self.buttonColor.caption = "Color: Grayscale"
        # Hottest
        elif self.colorScheme == 9:
            self.colorMap = self.colorschemes.colorMap_hottest
            self.colorElements = 224
            self.buttonColor.caption = "Color: Hottest"
        # Ironblack
        elif self.colorScheme == 10:
            self.colorMap = self.colorschemes.colorMap_ironblack
            self.colorElements = 256
            self.buttonColor.caption = "Color: Ironblack"
        # Lava
        elif self.colorScheme == 11:
            self.colorMap = self.colorschemes.colorMap_lava
            self.colorElements = 240
            self.buttonColor.caption = "Color: Lava"
        # Medical
        elif self.colorScheme == 12:
            self.colorMap = self.colorschemes.colorMap_medical
            self.colorElements = 224
            self.buttonColor.caption = "Color: Medical"
        # Rainbow
        elif self.colorScheme == 13:
            self.colorMap = self.colorschemes.colorMap_rainbow
            self.colorElements = 256
            self.buttonColor.caption = "Color: Rainbow"
        # Wheel 1
        elif self.colorScheme == 14:
            self.colorMap = self.colorschemes.colorMap_wheel1
            self.colorElements = 256
            self.buttonColor.caption = "Color: Wheel 1"
        # Wheel 2
        elif self.colorScheme == 15:
            self.colorMap = self.colorschemes.colorMap_wheel2
            self.colorElements = 256
            self.buttonColor.caption = "Color: Wheel 2"
        # Wheel 3
        elif self.colorScheme == 16:
            self.colorMap = self.colorschemes.colorMap_wheel3
            self.colorElements = 256
            self.buttonColor.caption = "Color: Wheel 3"
        # White-Hot
        elif self.colorScheme == 17:
            self.colorMap = self.colorschemes.colorMap_whiteHot
            self.colorElements = 224
            self.buttonColor.caption = "Color: White-Hot"
        # Yellow
        elif self.colorScheme == 18:
            self.colorMap = self.colorschemes.colorMap_yellow
            self.colorElements = 224
            self.buttonColor.caption = "Color: Yellow"

        # Update UI
        self.buttonColor.draw(self.screen)

        # Everything worked
        return True

    # Set spot info
    def setSpot(self, updateValue):
        # Update value
        if updateValue:
            # Toggle value
            self.spotEnabled = not self.spotEnabled

            # Send new value to the device
            try:
                # Send command and new value
                sendArray = (chr(self.cmd_set_showspot), chr(self.spotEnabled))
                self.ser.write(b"".join([x.encode() for x in sendArray]))

                # Get ACK
                inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

            # Error
            except serial.serialutil.SerialException:
                return False

            # Timeout
            if not inData[1]:
                return False

            # Check for ACK
            if inData[1] != self.cmd_set_showspot:
                return False

        # Update UI
        if self.spotEnabled:
            self.buttonSpot.caption = "Spot: On"
        else:
            self.buttonSpot.caption = "Spot: Off"
        self.buttonSpot.draw(self.screen)

        # Everything worked
        return True

    # Set bar info
    def setBar(self, updateValue):
        # Update value
        if updateValue:
            # Toggle value
            self.barEnabled = not self.barEnabled

            # Send new value to the device
            try:
                # Send command and new value
                sendArray = (chr(self.cmd_set_showcolorbar), chr(self.barEnabled))
                self.ser.write(b"".join([x.encode() for x in sendArray]))

                # Get ACK
                inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

            # Error
            except serial.serialutil.SerialException:
                return False

            # Timeout
            if not inData[1]:
                return False

            # Check for ACK
            if inData[1] != self.cmd_set_showcolorbar:
                return False

        # Update UI
        if self.barEnabled:
            self.buttonBar.caption = "Bar: On"
        else:
            self.buttonBar.caption = "Bar: Off"
        self.buttonBar.draw(self.screen)

        # Everything worked
        return True

    # Set min / max
    def setMinMax(self, updateValue):
        # Update value
        if updateValue:
            # Pick new calue
            if self.minMaxEnabled < 3:
                self.minMaxEnabled += 1
            else:
                self.minMaxEnabled = 0

            # Send new value to the device
            try:
                # Send command and new value
                sendArray = (chr(self.cmd_set_showminmax), chr(self.minMaxEnabled))
                self.ser.write(b"".join([x.encode() for x in sendArray]))

                # Get ACK
                inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

            # Error
            except serial.serialutil.SerialException:
                return False

            # Timeout
            if not inData[1]:
                return False

            # Check for ACK
            if inData[1] != self.cmd_set_showminmax:
                return False

        # Update UI
        if self.minMaxEnabled == 0:
            self.buttonHotCold.caption = "MinMax: Off"
        elif self.minMaxEnabled == 1:
            self.buttonHotCold.caption = "MinMax: Cold"
        elif self.minMaxEnabled == 2:
            self.buttonHotCold.caption = "MinMax: Hot"
        else:
            self.buttonHotCold.caption = "MinMax: Both"
        self.buttonHotCold.draw(self.screen)

        # Everything worked
        return True

    # Set text color
    def setTextColor(self, updateValue):
        # Update value
        if updateValue:
            # Pick new calue
            if self.textColor < 4:
                self.textColor += 1
            else:
                self.textColor = 0

            # Send new value to the device
            try:
                # Send command and new value
                sendArray = (chr(self.cmd_set_textcolor), chr(self.textColor))
                self.ser.write(b"".join([x.encode() for x in sendArray]))

                # Get ACK
                inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

            # Error
            except serial.serialutil.SerialException:
                return False

            # Timeout
            if not inData[1]:
                return False

            # Check for ACK
            if inData[1] != self.cmd_set_textcolor:
                return False

        # Update UI
        if self.textColor == 0:
            self.buttonTextColor.caption = "Text: White"
        elif self.textColor == 1:
            self.buttonTextColor.caption = "Text: Black"
        elif self.textColor == 2:
            self.buttonTextColor.caption = "Text: Red"
        elif self.textColor == 3:
            self.buttonTextColor.caption = "Text: Green"
        else:
            self.buttonTextColor.caption = "Text: Blue"
        self.buttonTextColor.draw(self.screen)

        # Everything worked
        return True

    # Set filter type
    def setFilterType(self, updateValue):
        # Update value
        if updateValue:
            # Pick new calue
            if self.filterType < 2:
                self.filterType += 1
            else:
                self.filterType = 0

            # Send new value to the device
            try:
                # Send command and new value
                sendArray = (chr(self.cmd_set_filtertype), chr(self.filterType))
                self.ser.write(b"".join([x.encode() for x in sendArray]))

                # Get ACK
                inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

            # Error
            except serial.serialutil.SerialException:
                return False

            # Timeout
            if not inData[0]:
                return False

            # Check for ACK
            if inData[0] != self.cmd_set_filtertype:
                return False

        # Update UI
        if self.filterType == 0:
            self.buttonFilter.caption = "Filter: None"
        elif self.filterType == 1:
            self.buttonFilter.caption = "Filter: Gaussian"
        else:
            self.buttonFilter.caption = "Filter: Box"
        self.buttonFilter.draw(self.screen)

        # Everything worked
        return True

    # Set limits
    def setLimits(self, updateValue):
        # Update value
        if updateValue:
            # Toggle value
            self.adjustLimits = not self.adjustLimits

            # Send new value to the device
            try:
                # Send command and new value
                sendArray = (chr(self.cmd_set_limits), chr(self.adjustLimits))
                self.ser.write(b"".join([x.encode() for x in sendArray]))

                # Get ACK
                inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

            # Error
            except serial.serialutil.SerialException:
                return False

            # Timeout
            if not inData[1]:
                return False

            # Check for ACK
            if inData[1] != self.cmd_set_limits:
                return False

        # Update UI
        if self.adjustLimits:
            self.buttonLimits.caption = "Limits: Auto"
        else:
            self.buttonLimits.caption = "Limits: Locked"
        self.buttonLimits.draw(self.screen)

        # Everything worked
        return True

    # Set format
    def setFormat(self, updateValue):
        # Update value
        if updateValue:
            # Toggle value
            self.tempFormat = not self.tempFormat

            # Send new value to the device
            try:
                # Send command and new value
                sendArray = (chr(self.cmd_set_tempformat), chr(self.tempFormat))
                self.ser.write(b"".join([x.encode() for x in sendArray]))

                # Get ACK
                inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

            # Error
            except serial.serialutil.SerialException:
                return False

            # Timeout
            if not inData[1]:
                return False

            # Check for ACK
            if inData[1] != self.cmd_set_tempformat:
                return False

        # Update UI
        if self.tempFormat:
            self.buttonFormat.caption = "Format: " "\N{DEGREE SIGN}" + "F"
        else:
            self.buttonFormat.caption = "Format: " "\N{DEGREE SIGN}" + "C"
        self.buttonFormat.draw(self.screen)

        # Everything worked
        return True

    # Set rotation
    def setRotation(self, updateValue):
        # Update value
        if updateValue:
            # Toggle value
            self.rotateScreen = not self.rotateScreen

            # Send new value to the device
            try:
                # Send command and new value
                sendArray = (chr(self.cmd_set_rotation), chr(self.rotateScreen))
                self.ser.write(b"".join([x.encode() for x in sendArray]))

                # Get ACK
                inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

            # Error
            except serial.serialutil.SerialException:
                return False

            # Timeout
            if not inData[1]:
                return False

            # Check for ACK
            if inData[1] != self.cmd_set_rotation:
                return False

        # Everything worked
        return True

    # Run the shutter
    def runShutter(self):
        # Show message
        self.displayText("Trigger Shutter..", False)

        # Send new value to the device
        try:
            # Send command
            self.ser.write(chr(self.cmd_set_shutterrun).encode())

            # Get ACK
            inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData[0]:
            return False

        # Check for ACK
        if inData[0] != self.cmd_set_shutterrun:
            return False

        # Wait two second
        time.sleep(2)

        # Everything worked
        return True

    # Run the calibration
    def runCalibration(self):
        # Show message
        self.displayText("Start Calibration..", False)

        # Send new value to the device
        try:
            # Send command
            self.ser.write(chr(self.cmd_set_calibration).encode())

            # Get ACK
            inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData[1]:
            return False

        # Wait until ACK received
        while inData[1] != self.cmd_set_calibration:
            # If not ACK, check if status valid
            if inData[1] > 100:
                return False

            # Display current status
            message = "Calibration Status: " + str(inData) + "%"
            self.displayText(message, False)

            # Receive the next status or ACK
            try:
                # Get data
                inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

            # Error
            except serial.serialutil.SerialException:
                return False

            # Timeout
            if not inData[1]:
                return False

        # Show finish message
        self.displayText("Calibration completed!", True)

        # Get the new offset and slope
        try:
            # Send command
            self.ser.write(chr(self.cmd_get_calibdata).encode())
            # Get data
            inData = list(map(lambda x: ord(chr(x)), self.ser.read(8)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Return if size does not match
        if len(inData) != 8:
            return False

        # Read calibration offset
        self.calOffset = [inData, inData[1], inData[2], inData[3]]
        self.calOffset = "".join(chr(i) for i in self.calOffset)
        self.calOffset = float(struct.unpack("f", self.calOffset)[0])

        # Read calibration slope
        self.calSlope = [inData[4], inData[5], inData[6], inData[7]]
        self.calSlope = "".join(chr(i) for i in self.calSlope)
        self.calSlope = float(struct.unpack("f", self.calSlope)[0])

        # Show new slope and offset
        message = (
            "Slope: "
            + str(round(self.calSlope, 4))
            + ", Offset: "
            + str(round(self.calOffset, 4))
        )
        self.displayText(message, True)
        time.sleep(2)

        # Everything worked
        return True

    # Draw the buttons on the screen
    def drawButtons(self):
        # First row
        self.buttonRenderMode = pygbutton.PygButton(
            (11, 491, 147, 27), "Mode: Rendering"
        )
        self.buttonSaveThermal = pygbutton.PygButton((168, 491, 147, 27), "Save Image")
        self.buttonSaveVideo = pygbutton.PygButton((325, 491, 147, 27), "Record Video")
        self.buttonShutter = pygbutton.PygButton((482, 491, 147, 27), "Trigger Shutter")

        # Second row
        self.buttonSpot = pygbutton.PygButton((11, 528, 147, 27), "Spot: On")
        self.buttonBar = pygbutton.PygButton((168, 528, 147, 27), "Bar: On")
        self.buttonHotCold = pygbutton.PygButton((325, 528, 147, 27), "MinMax: Hot")
        self.buttonTextColor = pygbutton.PygButton((482, 528, 147, 27), "Text: Black")

        # Third row
        self.buttonFilter = pygbutton.PygButton((11, 565, 147, 27), "Filter: Gaussian")
        self.buttonColor = pygbutton.PygButton((168, 565, 147, 27), "Color: Rainbow")
        self.buttonLimits = pygbutton.PygButton((325, 565, 147, 27), "Limits: Auto")
        self.buttonFormat = pygbutton.PygButton(
            (482, 565, 147, 27), "Format: " "\N{DEGREE SIGN}" + "F"
        )

        # Group them all together for refresh
        self.allButtons = (
            self.buttonSaveThermal,
            self.buttonSaveVideo,
            self.buttonRenderMode,
            self.buttonSpot,
            self.buttonBar,
            self.buttonHotCold,
            self.buttonTextColor,
            self.buttonFilter,
            self.buttonColor,
            self.buttonLimits,
            self.buttonFormat,
            self.buttonShutter,
        )

        # Draw the buttons
        for b in self.allButtons:
            b.draw(self.screen)

        # Update screen
        pygame.display.flip()

    # Apply filter to the raw data
    def applyFilter(self):
        # Repeat array four times for Lepton2
        if self.leptonVersion == 0:
            array2d = self.leptonValues.reshape(60, 80)
            array2dBig = array2d.repeat(4, axis=0).repeat(4, axis=1)
            self.imageRaw = numpy.transpose(array2dBig)

        # Repeat array two times for Lepton3
        else:
            array2d = self.leptonValues.reshape(120, 160)
            array2dBig = array2d.repeat(2, axis=0).repeat(2, axis=1)
            self.imageRaw = numpy.transpose(array2dBig)

        # Apply the gaussian blur filter
        if self.filterType == 1:
            self.imageRaw = scipy.ndimage.filters.gaussian_filter(
                self.imageRaw, 1.33, mode="nearest"
            )
        # Apply box blur filter
        if self.filterType == 2:
            self.imageRaw = scipy.ndimage.filters.uniform_filter(self.imageRaw, 3)

    # Converts the Lepton raw values to RGB colors
    def convertColors(self):
        # Calculate the scale
        scale = (self.colorElements - 1.0) / (self.maxData - self.minData)

        # Check if values are out of color scheme bounds
        array_np = numpy.asarray(self.imageRaw)
        low_values_indices = array_np < self.minData
        array_np[low_values_indices] = self.minData
        high_values_indices = array_np > self.maxData
        array_np[high_values_indices] = self.maxData
        self.imageRaw = array_np

        # Calculate the lookup vale (0-255)
        self.imageRaw[0:, 0:] = (self.imageRaw[0:, 0:] - self.minData) * scale
        self.imageRaw[0:, 0:] = 3 * self.imageRaw[0:, 0:]
        self.image[0:, 0:, 0] = self.imageRaw[0:, 0:]
        self.image[0:, 0:, 1] = self.imageRaw[0:, 0:] + 1
        self.image[0:, 0:, 2] = self.imageRaw[0:, 0:] + 2
        self.image = numpy.take(self.colorMap, self.image)

    # Convert the Lepton 8 bit data to 16 bit data
    def convertLeptonData(self):
        # For Lepton2, convert 4800 values
        if self.leptonVersion == 0:
            leptonDataReshaped = numpy.array(self.leptonData).reshape(4800, 2)
        # For Lepton3, convert 19200 values
        else:
            leptonDataReshaped = numpy.array(self.leptonData).reshape(19200, 2)

        # Assign them to the lepton values array
        self.leptonValues[0:] = (leptonDataReshaped[0:, 0] * 256) + leptonDataReshaped[
            0:, 1
        ]

    # Display some text on the screen
    def displayText(self, msg, wait):
        # Fill background
        background = pygame.Surface((640, 480))
        background.fill((250, 250, 250))

        # Display some text
        font = pygame.font.Font(self.resource_path("freesansbold.ttf"), 36)
        text = font.render(msg, 1, (10, 10, 10))
        textpos = text.get_rect()
        textpos.centerx = background.get_rect().centerx
        textpos.centery = background.get_rect().centery
        background.blit(text, textpos)

        # Blit everything to the screen
        self.screen.blit(background, (0, 0))
        pygame.display.flip()

        # Opt: Wait a second, so the user can read the text
        if wait:
            time.sleep(1)

    def SaveVideoFile(self,image_folder,output_video):
        # Get all image files in the folder (assuming they're named sequentially)
        images = [img for img in os.listdir(image_folder) if img.endswith(".jpg")]
        images.sort()  # Ensure images are in the correct order (e.g., image001, image002, etc.)



        # Read the first image to get the size (all images should be the same size)
        first_image = cv2.imread(os.path.join(image_folder, images[0]))
        height, width, _ = first_image.shape

        # Define the codec and create VideoWriter object
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')  # for .mp4 format
        fps = 4  # Frames per second in the video

        # Create VideoWriter object to save the video
        video_writer = cv2.VideoWriter(output_video, fourcc, fps, (width, height))

        # Loop through images and add them to the video
        for image in images:
            img_path = os.path.join(image_folder, image)
            img = cv2.imread(img_path)
            video_writer.write(img)

        # Release the video writer object
        video_writer.release()

        print(f"Video saved as {output_video}")

    # Starts or stops video recording
    def videoRecord(self):
        # Start recording a video
        if not self.recordVideo:
            # Show message
            self.displayText("Recording video..", True)

            # Update button
            self.buttonSaveVideo.caption = "Stop Recording"
            self.buttonSaveVideo.draw(self.screen)

            # Create dir if required
            if not os.path.exists("videos"):
                os.makedirs("videos")
            owd = os.getcwd()
            os.chdir("videos")
            self.videoFolder = time.strftime("%Y%m%d%H%M%S")
            os.mkdir(self.videoFolder)
            self.videoFolder = os.path.join("videos", self.videoFolder)
            os.chdir(owd)

            # Set record video to true and save time
            self.recordVideo = True
            self.videoStart = time.time()

        # Stop recording a video
        else:
            # Calculate the number of frames
            frames = (self.videoCounter * 1.0) / (time.time() - self.videoStart)

            # Show stop message
            self.displayText("Stop recording..", True)

            # Update button
            self.buttonSaveVideo.caption = "Record Video"
            self.buttonSaveVideo.draw(self.screen)

            # Show converting message
            self.displayText("Converting..", False)

            # Use ffmpeg to convert the frames to avi
            ffmpegCmd = self.resource_path(
                "ffmpeg"
            ) + " -framerate %d -i %s/%%06d.jpg -codec copy %s" % (
                round(frames),
                self.videoFolder,
                self.videoFolder + ".avi",
            )
            split_string = ffmpegCmd.split(' ')
            
            # Get the path for the images and the video directory
            video_save_folder = split_string[-1]
            image_folder_for_video = video_save_folder[:-4]

            self.SaveVideoFile(image_folder_for_video,video_save_folder)

            # Remove the folder with the single frames
            shutil.rmtree(self.videoFolder, True)

            # Clear marker
            self.videoFolder = None
            self.recordVideo = False
            self.videoCounter = 0

    # Saves a thermal image as JPEG
    def saveThermal(self):
        # Check if thermal image exists and not recording video
        if (self.thermalImg is not None) and (self.recordVideo is False):
            # Create dir if required
            if not os.path.exists("thermal"):
                os.makedirs("thermal")

            # Save image to dir
            pygame.image.save(
                self.thermalImg,
                os.path.join(
                    "thermal", time.strftime("%Y%m%d%H%M%S.jpg", time.gmtime())
                ),
            )

            # Show message
            self.displayText("Thermal image saved!", True)

    # Receive and save the visual image as JPEG
    def saveVisual(self):
        # Show message
        self.displayText("Transfer visual image..", False)

        # Set the timeout higher
        if self.hardwareVersion == 0:
            self.ser.timeout = 10.0
        else:
            self.ser.timeout = 5.0

        # Receive the visual image data length
        try:
            # Send visual img and high res command
            sendArray = (chr(self.cmd_get_visualimg), chr(1))
            self.ser.write(b"".join([x.encode() for x in sendArray]))
            # Get visual image length
            inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Return if size does not match
        if len(inData) != 2:
            return False

        # Calculate data length
        dataLen = inData * 256 + inData[1]

        # Try to read JPEG bytestream
        try:
            # Get data by length
            inData = list(map(lambda x: ord(chr(x)), self.ser.read(dataLen)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Set timeout back
        self.ser.timeout = 3.0

        # Timeout
        if not inData:
            return False

        # Return if size does not match
        if len(inData) != dataLen:
            return False

        # Create dir if required
        if not os.path.exists("visual"):
            os.makedirs("visual")

        # Save the data to file
        byteArray = bytearray(inData)
        f = io.open(
            os.path.join("visual", time.strftime("%Y%m%d%H%M%S.jpg", time.gmtime())),
            "wb",
        )
        f.write(byteArray)
        f.close()

        # Show message
        time.sleep(0.1)
        self.displayText("Visual image saved!", True)

        # Everything worked
        return True

    # Close the application and end connection
    def endConnection(self):
        # Show message
        self.displayText("Exiting..", True)

        # Send end command over serial port
        try:
            if self.ser:
                if self.ser.isOpen():
                    self.ser.write(chr(self.cmd_end).encode())
                    self.ser.close()

        # Discard error
        except serial.serialutil.SerialException:
            pass

        # Exit application
        pygame.quit()
        sys.exit()

    # Check if the user wants to exit
    def checkExit(self):
        # User wants to exit
        for event in pygame.event.get():
            if event.type == pygame.locals.QUIT:
                self.endConnection()

    # Check if the warmup has been completed
    def checkWarmup(self):
        # If calibration status is warmup and longer than remaining time, switch to done
        if self.calStatus == 0 and (time.time() - self.calTimer) >= self.calTimeLeft:
            self.calStatus = 1
            return True

        # Still in warmup
        elif self.calStatus == 0:
            return False

        # Calibration done
        return True

    # Add or remove a temperature point
    def tempFunc(self, mousePos, add):
        # If warmup is not completed, return
        if not self.checkWarmup():
            return True

        # Check if we have some space left when adding a point
        pos = int
        if add:
            # Go through the array and find the next free pos
            for i in range(0, 96):
                if self.tempPoints[i][0] == 0:
                    pos = i
                    break
            # Maximum number reached, exit
            if pos == 96:
                return True

        # Get x and y position from mouse click
        xpos = mousePos[0]
        ypos = mousePos[1]

        # Check if inside the frame
        if xpos < 0 or xpos > 639 or ypos < 0 or ypos > 479:
            return True

        # Divide to match array size
        xpos /= 4
        ypos /= 4

        # Add point
        if add:
            # Index is between 1 and max
            self.tempPoints[pos][0] = xpos + (ypos * 160) + 1
            # Temperature value will be refreshed in the next frame
            self.tempPoints[pos][1] = 0

        # Remove point
        else:
            for x in range(xpos - 10, xpos + 10):
                for y in range(ypos - 10, ypos + 10):
                    # Calculate index
                    index = x + (y * 160) + 1
                    # If index is valid
                    if 1 <= index <= 19200:
                        # Go through the tempPoints
                        for i in range(0, 96):
                            # We found the right one
                            if self.tempPoints[i][0] == index:
                                # Set to invalid
                                self.tempPoints[i][0] = 0
                                # Reset value
                                self.tempPoints[i][1] = 0

        # Update tempPoints on device
        try:
            # Build send array
            sendArray = (chr(self.cmd_set_temppoints),)
            for i in range(0, 96):
                # Index
                sendArray += (chr((self.tempPoints[i][0] & 0xFF00) >> 8),)
                sendArray += (chr(self.tempPoints[i][0] & 0x00FF),)
                # Value
                sendArray += (chr((self.tempPoints[i][1] & 0xFF00) >> 8),)
                sendArray += (chr(self.tempPoints[i][1] & 0x00FF),)

            # Send byte array
            self.ser.write(b"".join([x.encode() for x in sendArray]))

            # Wait for ACK
            inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData[1]:
            return False

        # Check if ACK matches
        if inData[1] != self.cmd_set_temppoints:
            return False

        # Everything OK
        return True

    # Function to calculate temperature out of Lepton raw value
    def calFunction(self, rawValue):
        # Calculate the temperature in Celcius out of the coefficients
        temp = (self.calSlope * rawValue) + self.calOffset

        # Convert to Fahrenheit if needed
        if self.tempFormat:
            temp = (9.0 / 5.0) * temp + 32.0

        # Return absolute temperature
        return temp

    # Get the current text color
    def getTextColor(self):
        color = None
        if self.textColor == 0:
            color = self.colorWhite
        if self.textColor == 1:
            color = self.colorBlack
        if self.textColor == 2:
            color = self.colorRed
        if self.textColor == 3:
            color = self.colorGreen
        if self.textColor == 4:
            color = self.colorBlue
        return color

    # Show the spot temperature
    def showSpot(self):
        # Choose font
        font = pygame.font.Font(self.resource_path("freesansbold.ttf"), 26)

        # Get the color
        color = self.getTextColor()

        # Add circle
        pygame.draw.circle(self.thermalImg, color, (320, 240), 24, 2)
        # Add lines
        pygame.draw.line(self.thermalImg, color, (272, 240), (296, 240), 2)
        pygame.draw.line(self.thermalImg, color, (344, 240), (368, 240), 2)
        pygame.draw.line(self.thermalImg, color, (320, 192), (320, 216), 2)
        pygame.draw.line(self.thermalImg, color, (320, 264), (320, 288), 2)

        # Create the string
        tmpstr = "{:.2f}".format(self.spotTemp)
        text = font.render(tmpstr, 1, color)

        # Show it on the screen
        textpos = text.get_rect()
        textpos.centerx = self.thermalImg.get_rect().centerx
        textpos.centery = self.thermalImg.get_rect().centery + 65
        self.thermalImg.blit(text, textpos)

    # Draw a temperature measurement on the screen
    def drawTemperature(self, xpos, ypos, rawValue, minMax):
        # Choose font
        font = pygame.font.Font(self.resource_path("freesansbold.ttf"), 26)

        # Get the color
        color = self.getTextColor()

        # Text
        text = None

        # When warmup is complete
        if self.checkWarmup():
            # Calculate absolute temperature
            temp = self.calFunction(rawValue)

            # Create a string out of the temp
            tempstr = "{:.2f}".format(temp)
            text = font.render(tempstr, 1, color)

        # Show min and max as char or skip
        else:
            if minMax == 0:
                return
            if minMax == 1:
                text = font.render("C", 1, color)
            if minMax == 2:
                text = font.render("H", 1, color)

        # Draw marker
        pygame.draw.circle(self.thermalImg, color, (xpos, ypos), 2)

        # Calc x position for the temperature
        if self.checkWarmup():
            xpos -= 35
            if xpos > 569:
                xpos = 569
        else:
            xpos -= 10
            if xpos > 599:
                xpos = 599
        if xpos < 0:
            xpos = 0

        # Calc y position for the temperature
        ypos += 12
        if ypos > 449:
            ypos = 449

        # Put it into the image
        self.thermalImg.blit(text, (xpos, ypos))

    # Show the min / max position
    def showMinMax(self):
        minValue = 65535
        minTempPos = int
        maxValue = 0
        maxTempPos = int

        # For Lepton2
        if self.leptonVersion == 0:
            elements = 4800
        # For Lepton3
        else:
            elements = 19200

        # Go through the lepton values
        for i in range(0, elements):
            # We found minTemp
            if self.leptonValues[i] < minValue:
                minValue = self.leptonValues[i]
                minTempPos = i
            # We found maxTemp
            if self.leptonValues[i] > maxValue:
                maxValue = self.leptonValues[i]
                maxTempPos = i

        # Show minimum position
        if self.minMaxEnabled & 1:
            # Get position
            if self.leptonVersion == 0:
                xpos = (minTempPos % 80) * 8
                ypos = (minTempPos / 80) * 8
            else:
                xpos = (minTempPos % 160) * 4
                ypos = (minTempPos / 160) * 4

            # Draw it on the screen
            self.drawTemperature(xpos, ypos, minValue, 1)

        # Show maximum position
        if self.minMaxEnabled & 2:
            # Get position
            if self.leptonVersion == 0:
                xpos = (maxTempPos % 80) * 8
                ypos = (maxTempPos / 80) * 8
            else:
                xpos = (maxTempPos % 160) * 4
                ypos = (maxTempPos / 160) * 4

            # Draw it on the screen
            self.drawTemperature(xpos, ypos, maxValue, 2)

    # Show the remaining warmup time
    def showWarmupTime(self):
        # Choose font
        font = pygame.font.Font(self.resource_path("freesansbold.ttf"), 26)

        # Get the color
        color = self.getTextColor()

        # Create text
        warmupLeft = self.calTimeLeft - (time.time() - self.calTimer)
        warmupStr = "{:.0f}".format(warmupLeft)

        text = font.render("Sensor warmup, " + warmupStr + "s left", 1, color)

        # Calculate position
        textpos = text.get_rect()
        textpos.centerx = self.thermalImg.get_rect().centerx
        textpos.centery = 440

        # Insert into thermal image
        self.screen.blit(text, textpos)

    # Show the color bar
    def showColorbar(self):
        # Calculate height
        height = 480 - ((480 - self.colorElements) / 2)

        # Choose font
        font = pygame.font.Font(self.resource_path("freesansbold.ttf"), 26)

        # Display color bar
        for i in range(0, self.colorElements - 1):
            red = self.colorMap[i * 3]
            green = self.colorMap[(i * 3) + 1]
            blue = self.colorMap[(i * 3) + 2]
            pygame.draw.line(
                self.thermalImg,
                (red, green, blue),
                (590, height - i),
                (630, height - i),
            )

        # Calculate min and max
        minTemp = self.calFunction(self.minData)
        maxTemp = self.calFunction(self.maxData)

        # Calculate step
        step = (maxTemp - minTemp) / 3.0

        # Get the color
        color = self.getTextColor()

        # Draw min temp
        tempstr = "{:.0f}".format(minTemp)
        text = font.render(tempstr, 1, color)
        self.thermalImg.blit(text, (540, height - 5))

        # Draw temperatures after min before max
        tempstr = "{:.0f}".format(minTemp + step)
        text = font.render(tempstr, 1, color)
        self.thermalImg.blit(text, (540, height - 5 - (self.colorElements / 3)))
        tempstr = "{:.0f}".format(minTemp + (2 * step))
        text = font.render(tempstr, 1, color)
        self.thermalImg.blit(text, (540, height - 5 - (2 * (self.colorElements / 3))))

        # Draw max temp
        tempstr = "{:.0f}".format(maxTemp)
        text = font.render(tempstr, 1, color)
        self.thermalImg.blit(text, (540, height - 5 - (3 * (self.colorElements / 3))))

    # Show the selected temperature points on the screen
    def showTemperatures(self):
        # Go through the tempPoints array
        for i in range(0, 96):
            # Get index
            index = self.tempPoints[i][0]

            # Check if the tempPoint is active
            if index != 0:
                # Index value is between 1 and max
                index -= 1

                # Calculate x and y position
                xpos = index % 160
                ypos = index / 160

                # Update raw value
                if self.leptonVersion == 0:
                    self.tempPoints[i][1] = self.leptonValues[
                        (xpos / 2) + (ypos / 2) * 80
                    ]
                else:
                    self.tempPoints[i][1] = self.leptonValues[xpos + (ypos * 160)]

                # Calculate screen position
                xpos *= 4
                ypos *= 4

                # Check if too close to border
                if ypos <= 20:
                    ypos = 21

                # Draw it on the screen
                self.drawTemperature(xpos, ypos, self.tempPoints[i][1], 0)

    # Display the thermal image on the screen
    def displayThermalImage(self):
        # Check if thermal image has been created
        if self.thermalImg is None:
            return

        # Show the spot temperature if enabled
        if self.spotEnabled:
            self.showSpot()

        # Show the colorbar if enabled and warmup completed
        if self.barEnabled and self.checkWarmup():
            self.showColorbar()

        # Show the min / max pos if enabled
        if self.minMaxEnabled:
            self.showMinMax()

        # Show temperatures if warmup completed
        if self.checkWarmup():
            self.showTemperatures()

        # Refresh screen
        self.screen.blit(self.thermalImg, (0, 0))

        # Capture to file if video enabled
        if self.recordVideo:
            pygame.image.save(
                self.thermalImg,
                os.path.join(
                    self.videoFolder, "{0:06d}".format(self.videoCounter) + ".jpg"
                ),
            )
            self.videoCounter += 1

    # Main method to create the thermal image
    def createThermalImage(self):
        # Convert 8 bit to 16 bit data
        self.convertLeptonData()

        # Apply gaussian blur
        self.applyFilter()

        # Convert to colors
        self.convertColors()

        # Resize image to 640x480 with nearest neighbour
        imageBig = resize(
            self.image, (640, 480), order=0, mode="constant", anti_aliasing=False
        )

        # Convert to pygame surface and save as thermal image
        self.thermalImg = pygame.surfarray.make_surface(imageBig)

    # Extracts RGB values from the lepton raw data
    def extractRGB(self, buff):
        # Updated to use the new numpy 2.0 syntax for byte order conversion
        arr = numpy.frombuffer(buff, dtype=numpy.uint16)
        arr = arr.view(arr.dtype.newbyteorder('S'))
        
        r = (((arr & 0xF800) >> 11) * 255.0 / 31.0).astype(numpy.uint8)
        g = (((arr & 0x07E0) >> 5) * 255.0 / 63.0).astype(numpy.uint8)
        b = (((arr & 0x001F) >> 0) * 255.0 / 31.0).astype(numpy.uint8)
        arr = numpy.column_stack((r, g, b))
        return arr

    # Get a stream frame from the display
    def getStream(self):
        # Calc length based on HW and HQRes
        if self.hardwareVersion == 0 or self.hqRes == 0:
            length = 38400
        else:
            length = 153600

        # Try to receive a frame
        try:
            frameBuffer = self.ser.read(length)

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not frameBuffer:
            return False

        # Not enough data received
        if len(frameBuffer) != length:
            return False

        # Extract RGB values from the frame buffer
        rgbValues = self.extractRGB(frameBuffer)

        # Convert RGB values to image
        if self.hardwareVersion == 0 or self.hqRes == 0:
            img = pygame.image.frombuffer(rgbValues, (160, 120), "RGB")
        else:
            img = pygame.image.frombuffer(rgbValues, (320, 240), "RGB")

        # Scale image smooth
        self.thermalImg = pygame.transform.smoothscale(img, (640, 480))

        # Refresh screen
        self.screen.blit(self.thermalImg, (0, 0))

        # Everything OK
        return True

    # Get a whole set of raw data from the Lepton
    def getFrameData(self):
        # Receive 9600 byte for Lepton2
        if self.leptonVersion == 0:
            length = 9600
        # Receive 38400 byte for Lepton3
        else:
            length = 38400

        # Try to receive the data
        try:
            self.leptonData = list(map(lambda x: ord(chr(x)), self.ser.read(length)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Length does not match
        if len(self.leptonData) != length:
            return False

        # Timeout
        if not self.leptonData:
            return False

        # Try to receive the additional data
        try:
            additionalData = list(map(lambda x: ord(chr(x)), self.ser.read(16)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not additionalData:
            return False

        # Return if size does not match
        if len(additionalData) != 16:
            return False

        # Read min & max
        self.minData = additionalData[0] * 256 + additionalData[1]
        self.maxData = additionalData[2] * 256 + additionalData[3]

        # Read spot temp
        self.spotTemp = [
            additionalData[4],
            additionalData[5],
            additionalData[6],
            additionalData[7],
        ]
        self.spotTemp = b"".join(bytes([i]) for i in self.spotTemp)
        self.spotTemp = struct.unpack("<f", self.spotTemp)[0]

        # Read calibration offset
        self.calOffset = [
            additionalData[8],
            additionalData[9],
            additionalData[10],
            additionalData[11],
        ]
        self.calOffset = b"".join(bytes([i]) for i in self.calOffset)
        self.calOffset = struct.unpack("<f", self.calOffset)[0]

        # Read calibration slope
        self.calSlope = [
            additionalData[12],
            additionalData[13],
            additionalData[14],
            additionalData[15],
        ]
        self.calSlope = b"".join(bytes([i]) for i in self.calSlope)
        self.calSlope = struct.unpack("<f", self.calSlope)[0]

        # Everything worked
        return True

    # Get the config data
    def getConfigData(self):
        # Show message
        self.displayText("Reading config data..", False)

        # Try to receive the config data
        try:
            # Send config receive command
            self.ser.write(chr(self.cmd_get_configdata).encode())
            # Get data
            configData = list(map(lambda x: ord(chr(x)), self.ser.read(10)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not configData:
            return False

        # Return if size does not match
        if len(configData) != 10:
            return False

        # Read lepton version
        self.leptonVersion = configData[0]
        # Lepton 2 non shuttered, make it to zero
        if self.leptonVersion == 2:
            self.leptonVersion = 0

        # Create lepton values array by size
        if self.leptonVersion == 0:
            self.leptonValues = numpy.zeros(4800, dtype=int)
        if self.leptonVersion == 1:
            self.leptonValues = numpy.zeros(19200, dtype=int)

        # Read rotation
        self.rotateScreen = configData[1]
        self.setRotation(False)
        # Read color scheme
        self.colorScheme = configData[2]
        self.setColorScheme(False)
        # Read temp format
        self.tempFormat = configData[3]
        self.setFormat(False)
        # Read spot enabled
        self.spotEnabled = configData[4]
        self.setSpot(False)
        # Read colorbar enabled
        self.barEnabled = configData[5]
        self.setBar(False)
        # Read min max enabled
        self.minMaxEnabled = configData[6]
        self.setMinMax(False)
        # Read text color
        self.textColor = configData[7]
        self.setTextColor(False)
        # Read filter type
        self.filterType = configData[8]
        self.setFilterType(False)
        # Read adjust limits allowed
        self.adjustLimits = configData[9]
        self.setLimits(False)

        # Update screen
        pygame.display.flip()

        # Everything worked
        return True

    # Serial handler to receive and send data
    def serialHandler(self):
        # Clear buffer
        self.ser.flushInput()
        self.ser.flushOutput()

        # Try to get a new frame from the device
        try:
            # Send get raw frame command
            if self.renderMode:
                self.ser.write(chr(self.cmd_rawframe).encode())
            # Send a get display frame command
            else:
                self.ser.write(chr(self.cmd_displayframe).encode())

            # Wait for command response
            inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData[1]:
            return False

        # Parse the command
        cmd = inData[1]

        # Save thermal image command
        if cmd == self.frame_capture_thermal:
            self.saveThermal()

        # Save visual image command
        elif cmd == self.frame_capture_visual:
            if not self.saveVisual():
                self.displayText("Error saving visual image!", True)

        # Start or stop video command
        elif cmd == self.frame_capture_video:
            self.videoRecord()

        # Send normal frame command
        elif cmd == self.frame_normal:
            # Rendering mode
            if self.renderMode:
                # Get lepton frame data
                if not self.getFrameData():
                    return False
                # Create the thermal image
                self.createThermalImage()

                # Display it on the screen
                self.displayThermalImage()

            # Streaming mode
            else:
                # Get the display buffer
                if not self.getStream():
                    return False

            # Show warmup time when not completed
            if not self.checkWarmup():
                self.showWarmupTime()

        # Everything worked
        return True

    # Read the temperature points from the device
    def getTempPoints(self):
        # Show message on screen
        self.displayText("Getting temperature points..", False)

        # Try to get the temperature points
        try:
            # Send set time command
            self.ser.write(chr(self.cmd_get_temppoints).encode())
            # Wait for response
            inData = list(map(lambda x: ord(chr(x)), self.ser.read(384)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Return if size does not match
        if len(inData) != 384:
            return False

        # Save temperature points
        for i in range(0, 96):
            # Index
            self.tempPoints[i][0] = inData[i * 4] * 256 + inData[(i * 4) + 1]
            # Correct index if required
            if self.tempPoints[i][0] == 65535:
                self.tempPoints[i][0] = 0
            # Value
            self.tempPoints[i][1] = inData[(i * 4) + 2] * 256 + inData[(i * 4) + 3]

        # Everything worked
        return True

    # Set the render mode
    def setRenderMode(self):
        # Toggle
        self.renderMode = not self.renderMode

        # Update UI
        if self.renderMode:
            self.buttonRenderMode.caption = "Mode: Rendering"
        else:
            self.buttonRenderMode.caption = "Mode: Streaming"
        self.buttonRenderMode.draw(self.screen)

    # Get the diagnostic information
    def getDiagnostic(self):
        # Show message
        self.displayText("Reading diagnostic infos..", False)

        # Try to receive it
        try:
            # Send get diagnostic command
            self.ser.write(chr(self.cmd_get_diagnostic).encode())

            # Receive answer
            inData = list(map(lambda x: ord(chr(x)), self.ser.read(2)))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData[0]:
            return False

        cmd = inData[0]

        # Check if there are diagnostic error(s)
        if cmd != 255:
            # Check Lepton I2C
            if not (cmd >> 6) & 1:
                self.displayText("Lepton I2C error", True)
                return False
            # Check Lepton SPI
            if not (cmd >> 7) & 1:
                self.displayText("Lepton SPI error", True)
                return False
            # Check Spot Sensor
            if not (cmd >> 0) & 1:
                self.displayText("Spot Sensor error", True)
                return False

        # Everything worked
        return True

    # Send start command
    def sendStart(self):
        # Send start and wait for ACK
        try:
            # Send start command
            self.ser.write(chr(self.cmd_start).encode())
            # Read answer
            inData = self.ser.read(1)
            if inData:
                inData = ord(inData)
            else:
                return False

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # ACK is not the expected value
        if inData != self.cmd_start:
            return False

        # Everything worked
        return True

    # Connect to the device
    def connect(self):
        # Display waiting message
        self.displayText(f"Waiting for device on {self.serial_port}..", False)

        # Check if device is connected to serial port
        while True:
            # Check if the user wants to exit
            self.checkExit()

            # Try to open the ports
            try:
                self.ser = serial.Serial(self.serial_port, 115200)
            # Did not work, try again in 1s
            except serial.serialutil.SerialException:
                time.sleep(1)
                continue

            # If connected, leave loop
            if self.ser:
                break

        # Show detected message
        self.displayText("Detected device!", False)
        # Give the device 5 seconds to boot
        self.ser.timeout = 5.0
        # Clear buffer
        self.ser.flushInput()
        self.ser.flushOutput()

        # Send start command
        while not self.sendStart():
            # Check if the user wants to exit
            self.checkExit()
            # Display message
            self.displayText("Put the device in live mode!", True)

        # Set the timeout to 3 seconds
        self.ser.timeout = 3.0

        # Get diagnostic infos
        if not self.getDiagnostic():
            self.displayText("There were hardware errors, exit..", True)
            self.endConnection()

        # Get configuration data
        if not self.getConfigData():
            self.ser.close()
            return False

        # Get temperature points
        if not self.getTempPoints():
            self.ser.close()
            return False

        # Everything worked
        return True

    # Check if the mouse has been pressed or a serial command is received
    def eventHandler(self):
        # Check for button clicks
        for event in pygame.event.get():
            # Exit
            if event.type == pygame.QUIT:
                self.endConnection()

            # Click on image
            if event.type == pygame.MOUSEBUTTONUP:
                # Get mouse position
                mousePos = pygame.mouse.get_pos()

                # Left mouse - add point
                if event.button == 1:
                    if not self.tempFunc(mousePos, True):
                        self.displayText("Unable to add point on device!", True)

                # Right mouse - remove point
                if event.button == 3:
                    if not self.tempFunc(mousePos, False):
                        self.displayText("Unable to remove point on device!", True)

            # Save Thermal
            if "click" in self.buttonSaveThermal.handleEvent(event):
                self.saveThermal()

            # Record video
            if "click" in self.buttonSaveVideo.handleEvent(event):
                self.videoRecord()

            # Render mode
            if "click" in self.buttonRenderMode.handleEvent(event):
                self.setRenderMode()

            # Show spot
            if "click" in self.buttonSpot.handleEvent(event):
                if not self.setSpot(True):
                    self.displayText("Error setting showSpot on device!", True)

            # Show bar
            if "click" in self.buttonBar.handleEvent(event):
                if not self.setBar(True):
                    self.displayText("Error setting showBar on device!", True)

            # Show min / max
            if "click" in self.buttonHotCold.handleEvent(event):
                if not self.setMinMax(True):
                    self.displayText("Error setting min/max device!", True)

            # Text color
            if "click" in self.buttonTextColor.handleEvent(event):
                if not self.setTextColor(True):
                    self.displayText("Error setting textColor on device!", True)

            # Filter strength
            if "click" in self.buttonFilter.handleEvent(event):
                if not self.setFilterType(True):
                    self.displayText("Error setting filterType on device!", True)

            # Color scheme
            if "click" in self.buttonColor.handleEvent(event):
                if not self.setColorScheme(True):
                    self.displayText("Error setting colorScheme on device!", True)

            # Temperature limits
            if "click" in self.buttonLimits.handleEvent(event):
                if not self.setLimits(True):
                    self.displayText("Error setting adjustLimits on device!", True)

            # Temperature format
            if "click" in self.buttonFormat.handleEvent(event):
                if not self.setFormat(True):
                    self.displayText("Error setting tempFormat on device!", True)

            # Shutter
            if "click" in self.buttonShutter.handleEvent(event):
                if not self.runShutter():
                    self.displayText("Error triggering the shutter!", True)

    # Init procedures
    def setup(self):
        # Add environment variable for Windows XP
        if platform.release() == 6:
            os.environ["SDL_VIDEODRIVER"] = "windlib"

        # Init pygame
        pygame.init()

        # Set icon
        programIcon = pygame.image.load(self.resource_path("icon.jpg"))
        pygame.display.set_icon(programIcon)

        # Set window resolution
        self.screen = pygame.display.set_mode((640, 600))
        self.screen.fill((230, 230, 230))

        # Set windows title
        pygame.display.set_caption("DIY-Thermocam - Thermal Live Viewer")

        # Add the buttons
        self.drawButtons()

        # Welcome message
        self.displayText("Thermal Live Viewer", True)

    def loop(self):
        # Try to establish a connection
        while not self.connect():
            self.displayText("Error connecting to device..", True)

        self.displayText("Connected to DIY-Thermocam V3!", True)

        while True:
            # Check for serial events
            if not self.serialHandler():
                self.ser.close()
                self.displayText("Connection lost, reconnect..", True)
                break

            # Check for mouse press
            self.eventHandler()

            # Update screen
            pygame.display.flip()

    def run(self):
        self.setup()
        while True:
            self.loop()
