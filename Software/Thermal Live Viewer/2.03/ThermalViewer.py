# DIY-Thermocam Thermal Live Viewer
# 2016 by Max Ritter
# www.diy-thermocam.net

# Version definition
version = "2.03"

# Start & Stop command
cmd_start = 100
cmd_end = 200
# serial terminal commands
cmd_get_rawlimits = 110
cmd_get_rawdata = 111
cmd_get_configdata = 112
cmd_get_calstatus = 113
cmd_get_calibdata = 114
cmd_get_spottemp = 115
cmd_set_time = 116
cmd_get_temppoints = 117
cmd_set_laser = 118
cmd_get_laser = 119
cmd_set_shutterrun = 120
cmd_set_shuttermode = 121
cmd_set_filtertype = 122
cmd_get_shuttermode = 123
cmd_get_batterystatus = 124
cmd_set_calslope = 125
cmd_set_caloffset = 126
cmd_get_diagnostic = 127
cmd_get_visualimg = 128
cmd_get_fwversion = 129
cmd_set_limits = 130
cmd_set_textcolor = 131
cmd_set_colorscheme = 132
cmd_set_tempformat = 133
cmd_set_showspot = 134
cmd_set_showcolorbar = 135
cmd_set_showminmax = 136
cmd_set_temppoints = 137
cmd_get_hwversion = 138
cmd_set_rotation = 139
cmd_set_calibration = 140
cmd_get_hqresolution = 141
# serial frame commands
cmd_rawframe = 150
cmd_colorframe = 151
cmd_displayframe = 152
# types of frame responses
frame_capture_thermal = 180
frame_capture_visual = 181
frame_capture_video = 182
frame_normal = 183

# Import section
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
import datetime
import io

# Include color schemes
import ColorSchemes

# Config variables
leptonVersion = int
hardwareVersion = int
fwVersion = int
calStatus = int
minData = int
maxData = int
spotTemp = float
colorScheme = int
tempFormat = bool
spotEnabled = bool
barEnabled = bool
minMaxEnabled = int
rotateScreen = False
textColor = int
filterType = int
calOffset = float
calSlope = float
adjustLimits = bool
laserEnabled = bool
hqRes = int

# Data storage
tempPoints = numpy.zeros((96, 2), dtype=int)
leptonData = None
leptonValues = None
imageRaw = numpy.zeros((320, 240), dtype=int)
image = numpy.zeros((320, 240, 3), dtype=int)
thermalImg = None

# Color Scheme
colorMap = ColorSchemes.colorMap_rainbow
colorElements = int
colorSchemeTotal = 19

# Text color
colorWhite = (255, 255, 255)
colorBlack = (0, 0, 0)
colorRed = (255, 0, 0)
colorGreen = (0, 255, 0)
colorBlue = (0, 0, 255)

# Buttons
buttonSaveThermal = None
buttonSaveVisual = None
buttonSaveVideo = None
buttonRenderMode = None
buttonSpot = None
buttonBar = None
buttonHotCold = None
buttonTextColor = None
buttonFilter = None
buttonColor = None
buttonLimits = None
buttonFormat = None
buttonRotation = None
buttonLaser = None
buttonShutter = None
buttonCalibration = None
allButtons = None

# Global Variables
device = "COM10"
screen = None
ser = None
calTimer = None
calTimeLeft = None
renderMode = True
recordVideo = False
videoFolder = None
videoCounter = 0
videoStart = None


# Set the color scheme
def setColorScheme(updateValue):
    global colorMap, colorElements, colorScheme

    # Update value
    if updateValue:
        # Pick new color scheme
        if colorScheme < (colorSchemeTotal - 1):
            colorScheme += 1
        else:
            colorScheme = 0

        # Send new value to the device
        try:
            # Send command and new value
            sendArray = (chr(cmd_set_colorscheme), chr(colorScheme))
            ser.write(sendArray)
            # Get ACK
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Check for ACK
        if inData[0] != cmd_set_colorscheme:
            print inData
            return False

    # Arctic
    if colorScheme == 0:
        colorMap = ColorSchemes.colorMap_arctic
        colorElements = 240
        buttonColor.caption = "Color: Arctic"
    # Black-Hot
    elif colorScheme == 1:
        colorMap = ColorSchemes.colorMap_blackHot
        colorElements = 224
        buttonColor.caption = "Color: Back-Hot"
    # Blue-Red
    elif colorScheme == 2:
        colorMap = ColorSchemes.colorMap_blueRed
        colorElements = 192
        buttonColor.caption = "Color: Blue-Red"
    # Coldest
    elif colorScheme == 3:
        colorMap = ColorSchemes.colorMap_coldest
        colorElements = 224
        buttonColor.caption = "Color: Coldest"
    # Contrast
    elif colorScheme == 4:
        colorMap = ColorSchemes.colorMap_contrast
        colorElements = 224
        buttonColor.caption = "Color: Contrast"
    # Double-Rainbow
    elif colorScheme == 5:
        colorMap = ColorSchemes.colorMap_doubleRainbow
        colorElements = 256
        buttonColor.caption = "Color: DoubleRain"
    # Gray-Red
    elif colorScheme == 6:
        colorMap = ColorSchemes.colorMap_grayRed
        colorElements = 224
        buttonColor.caption = "Color: Gray-Red"
    # Glowbow
    elif colorScheme == 7:
        colorMap = ColorSchemes.colorMap_glowBow
        colorElements = 224
        buttonColor.caption = "Color: Glowbow"
    # Grayscale
    elif colorScheme == 8:
        colorMap = ColorSchemes.colorMap_grayscale
        colorElements = 256
        buttonColor.caption = "Color: Grayscale"
    # Hottest
    elif colorScheme == 9:
        colorMap = ColorSchemes.colorMap_hottest
        colorElements = 224
        buttonColor.caption = "Color: Hottest"
    # Ironblack
    elif colorScheme == 10:
        colorMap = ColorSchemes.colorMap_ironblack
        colorElements = 256
        buttonColor.caption = "Color: Ironblack"
    # Lava
    elif colorScheme == 11:
        colorMap = ColorSchemes.colorMap_lava
        colorElements = 240
        buttonColor.caption = "Color: Lava"
    # Medical
    elif colorScheme == 12:
        colorMap = ColorSchemes.colorMap_medical
        colorElements = 224
        buttonColor.caption = "Color: Medical"
    # Rainbow
    elif colorScheme == 13:
        colorMap = ColorSchemes.colorMap_rainbow
        colorElements = 256
        buttonColor.caption = "Color: Rainbow"
    # Wheel 1
    elif colorScheme == 14:
        colorMap = ColorSchemes.colorMap_wheel1
        colorElements = 256
        buttonColor.caption = "Color: Wheel 1"
    # Wheel 2
    elif colorScheme == 15:
        colorMap = ColorSchemes.colorMap_wheel2
        colorElements = 256
        buttonColor.caption = "Color: Wheel 2"
    # Wheel 3
    elif colorScheme == 16:
        colorMap = ColorSchemes.colorMap_wheel3
        colorElements = 256
        buttonColor.caption = "Color: Wheel 3"
    # White-Hot
    elif colorScheme == 17:
        colorMap = ColorSchemes.colorMap_whiteHot
        colorElements = 224
        buttonColor.caption = "Color: White-Hot"
    # Yellow
    elif colorScheme == 18:
        colorMap = ColorSchemes.colorMap_yellow
        colorElements = 224
        buttonColor.caption = "Color: Yellow"

    # Update UI
    buttonColor.draw(screen)

    # Everything worked
    return True


# Set spot info
def setSpot(updateValue):
    global spotEnabled

    # Update value
    if updateValue:
        # Toggle value
        spotEnabled = not spotEnabled

        # Send new value to the device
        try:
            # Send command and new value
            sendArray = (chr(cmd_set_showspot), chr(spotEnabled))
            ser.write(sendArray)
            # Get ACK
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Check for ACK
        if inData[0] != cmd_set_showspot:
            return False

    # Update UI
    if spotEnabled:
        buttonSpot.caption = "Spot: On"
    else:
        buttonSpot.caption = "Spot: Off"
    buttonSpot.draw(screen)

    # Everything worked
    return True


# Set bar info
def setBar(updateValue):
    global barEnabled

    # Update value
    if updateValue:
        # Toggle value
        barEnabled = not barEnabled

        # Send new value to the device
        try:
            # Send command and new value
            sendArray = (chr(cmd_set_showcolorbar), chr(barEnabled))
            ser.write(sendArray)
            # Get ACK
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Check for ACK
        if inData[0] != cmd_set_showcolorbar:
            return False

    # Update UI
    if barEnabled:
        buttonBar.caption = "Bar: On"
    else:
        buttonBar.caption = "Bar: Off"
    buttonBar.draw(screen)

    # Everything worked
    return True


# Set min / max
def setMinMax(updateValue):
    global minMaxEnabled

    # Update value
    if updateValue:
        # Pick new calue
        if minMaxEnabled < 3:
            minMaxEnabled += 1
        else:
            minMaxEnabled = 0

        # Send new value to the device
        try:
            # Send command and new value
            sendArray = (chr(cmd_set_showminmax), chr(minMaxEnabled))
            ser.write(sendArray)
            # Get ACK
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Check for ACK
        if inData[0] != cmd_set_showminmax:
            return False

    # Update UI
    if minMaxEnabled == 0:
        buttonHotCold.caption = "MinMax: Off"
    elif minMaxEnabled == 1:
        buttonHotCold.caption = "MinMax: Cold"
    elif minMaxEnabled == 2:
        buttonHotCold.caption = "MinMax: Hot"
    else:
        buttonHotCold.caption = "MinMax: Both"
    buttonHotCold.draw(screen)

    # Everything worked
    return True


# Set text color
def setTextColor(updateValue):
    global textColor

    # Update value
    if updateValue:
        # Pick new calue
        if textColor < 4:
            textColor += 1
        else:
            textColor = 0

        # Send new value to the device
        try:
            # Send command and new value
            sendArray = (chr(cmd_set_textcolor), chr(textColor))
            ser.write(sendArray)
            # Get ACK
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Check for ACK
        if inData[0] != cmd_set_textcolor:
            return False

    # Update UI
    if textColor == 0:
        buttonTextColor.caption = "Text: White"
    elif textColor == 1:
        buttonTextColor.caption = "Text: Black"
    elif textColor == 2:
        buttonTextColor.caption = "Text: Red"
    elif textColor == 3:
        buttonTextColor.caption = "Text: Green"
    else:
        buttonTextColor.caption = "Text: Blue"
    buttonTextColor.draw(screen)

    # Everything worked
    return True


# Set filter type
def setFilterType(updateValue):
    global filterType

    # Update value
    if updateValue:
        # Pick new calue
        if filterType < 2:
            filterType += 1
        else:
            filterType = 0

        # Send new value to the device
        try:
            # Send command and new value
            sendArray = (chr(cmd_set_filtertype), chr(filterType))
            ser.write(sendArray)
            # Get ACK
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Check for ACK
        if inData[0] != cmd_set_filtertype:
            return False

    # Update UI
    if filterType == 0:
        buttonFilter.caption = "Filter: None"
    elif filterType == 1:
        buttonFilter.caption = "Filter: Gaussian"
    else:
        buttonFilter.caption = "Filter: Box"
    buttonFilter.draw(screen)

    # Everything worked
    return True


# Set limits
def setLimits(updateValue):
    global adjustLimits

    # Update value
    if updateValue:
        # Toggle value
        adjustLimits = not adjustLimits

        # Send new value to the device
        try:
            # Send command and new value
            sendArray = (chr(cmd_set_limits), chr(adjustLimits))
            ser.write(sendArray)
            # Get ACK
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Check for ACK
        if inData[0] != cmd_set_limits:
            return False

    # Update UI
    if adjustLimits:
        buttonLimits.caption = "Limits: Auto"
    else:
        buttonLimits.caption = "Limits: Locked"
    buttonLimits.draw(screen)

    # Everything worked
    return True


# Set format
def setFormat(updateValue):
    global tempFormat

    # Update value
    if updateValue:
        # Toggle value
        tempFormat = not tempFormat

        # Send new value to the device
        try:
            # Send command and new value
            sendArray = (chr(cmd_set_tempformat), chr(tempFormat))
            ser.write(sendArray)
            # Get ACK
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Check for ACK
        if inData[0] != cmd_set_tempformat:
            return False

    # Update UI
    if tempFormat:
        buttonFormat.caption = "Format: " u"\N{DEGREE SIGN}" + "F"
    else:
        buttonFormat.caption = "Format: " u"\N{DEGREE SIGN}" + "C"
    buttonFormat.draw(screen)

    # Everything worked
    return True


# Set rotation
def setRotation(updateValue):
    global rotateScreen

    # Update value
    if updateValue:
        # Toggle value
        rotateScreen = not rotateScreen

        # Send new value to the device
        try:
            # Send command and new value
            sendArray = (chr(cmd_set_rotation), chr(rotateScreen))
            ser.write(sendArray)
            # Get ACK
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Check for ACK
        if inData[0] != cmd_set_rotation:
            return False

    # Update UI
    if rotateScreen:
        buttonRotation.caption = "Rotation: On"
    else:
        buttonRotation.caption = "Rotation: Off"
    buttonRotation.draw(screen)

    # Everything worked
    return True


# Set Laser
def setLaser(updateValue):
    global laserEnabled

    # DIY-Thermocam V2 does not have a laser
    if hardwareVersion == 1:
        return True

    # Update value
    if updateValue:
        # Toggle value
        laserEnabled = not laserEnabled

        # Send new value to the device
        try:
            # Send command and new value
            sendArray = (chr(cmd_set_laser), chr(laserEnabled))
            ser.write(sendArray)
            # Get ACK
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

        # Check for ACK
        if inData[0] != cmd_set_laser:
            return False

    # Update UI
    if laserEnabled:
        buttonLaser.caption = "Laser: On"
    else:
        buttonLaser.caption = "Laser: Off"
    buttonLaser.draw(screen)

    # Everything worked
    return True


# Run the shutter
def runShutter():
    # Show message
    displayText("Trigger Shutter..", False)

    # Send new value to the device
    try:
        # Send command
        ser.write(chr(cmd_set_shutterrun))
        # Get ACK
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Check for ACK
    if inData[0] != cmd_set_shutterrun:
        return False

    # Wait two second
    time.sleep(2)

    # Everything worked
    return True


# Run the calibration
def runCalibration():
    global calOffset, calSlope

    # Show message
    displayText("Start Calibration..", False)

    # Send new value to the device
    try:
        # Send command
        ser.write(chr(cmd_set_calibration))
        # Get ACK
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Wait until ACK received
    while inData[0] != cmd_set_calibration:

        # If not ACK, check if status valid
        if inData[0] > 100:
            return False

        # Display current status
        message = "Calibration Status: " + str(inData[0]) + "%"
        displayText(message, False)

        # Receive the next status or ACK
        try:
            # Get data
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

        # Timeout
        if not inData:
            return False

    # Show finish message
    displayText("Calibration completed!", True)

    # Get the new offset and slope
    try:
        # Send command
        ser.write(chr(cmd_get_calibdata))
        # Get data
        inData = map(ord, ser.read(8))

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
    calOffset = [inData[0], inData[1], inData[2], inData[3]]
    calOffset = ''.join(chr(i) for i in calOffset)
    calOffset = float(struct.unpack('f', calOffset)[0])

    # Read calibration slope
    calSlope = [inData[4], inData[5], inData[6], inData[7]]
    calSlope = ''.join(chr(i) for i in calSlope)
    calSlope = float(struct.unpack('f', calSlope)[0])

    # Show new slope and offset
    message = "Slope: " + str(round(calSlope, 4)) + ", Offset: " + str(round(calOffset, 4))
    displayText(message, True)
    time.sleep(2)

    # Everything worked
    return True


# Draw the buttons on the screen
def drawButtons():
    global buttonSaveThermal, buttonSaveVisual, buttonSaveVideo, buttonRenderMode, \
        buttonSpot, buttonBar, buttonHotCold, buttonTextColor, \
        buttonFilter, buttonColor, buttonLimits, buttonFormat, \
        buttonRotation, buttonLaser, buttonShutter, buttonCalibration, allButtons

    # First row
    buttonRenderMode = pygbutton.PygButton((11, 491, 147, 27), 'Mode: Rendering')
    buttonSaveThermal = pygbutton.PygButton((168, 491, 147, 27), 'Save Thermal')
    buttonSaveVisual = pygbutton.PygButton((325, 491, 147, 27), 'Save Visual')
    buttonSaveVideo = pygbutton.PygButton((482, 491, 147, 27), 'Record Video')

    # Second row
    buttonSpot = pygbutton.PygButton((11, 528, 147, 27), 'Spot: On')
    buttonBar = pygbutton.PygButton((168, 528, 147, 27), 'Bar: On')
    buttonHotCold = pygbutton.PygButton((325, 528, 147, 27), 'MinMax: Hot')
    buttonTextColor = pygbutton.PygButton((482, 528, 147, 27), 'Text: Black')

    # Third row
    buttonFilter = pygbutton.PygButton((11, 565, 147, 27), 'Filter: Gaussian')
    buttonColor = pygbutton.PygButton((168, 565, 147, 27), 'Color: Rainbow')
    buttonLimits = pygbutton.PygButton((325, 565, 147, 27), 'Limits: Auto')
    buttonFormat = pygbutton.PygButton((482, 565, 147, 27), 'Format: ' u'\N{DEGREE SIGN}' + 'F')

    # Fourth row
    buttonRotation = pygbutton.PygButton((11, 602, 147, 27), 'Rotation: Off')
    buttonLaser = pygbutton.PygButton((168, 602, 147, 27), 'Laser: Off')
    buttonShutter = pygbutton.PygButton((325, 602, 147, 27), 'Trigger Shutter')
    buttonCalibration = pygbutton.PygButton((482, 602, 147, 27), 'Run Calibration')

    # Group them all together for refresh
    allButtons = (buttonSaveThermal, buttonSaveVisual, buttonSaveVideo, buttonRenderMode,
                  buttonSpot, buttonBar, buttonHotCold, buttonTextColor,
                  buttonFilter, buttonColor, buttonLimits, buttonFormat,
                  buttonRotation, buttonLaser, buttonShutter, buttonCalibration)

    # Draw the buttons
    for b in allButtons:
        b.draw(screen)

    # Update screen
    pygame.display.flip()


# Apply filter to the raw data
def applyFilter():
    global imageRaw

    # Repeat array four times for Lepton2
    if leptonVersion == 0:
        array2d = leptonValues.reshape(60, 80)
        array2dBig = array2d.repeat(4, axis=0).repeat(4, axis=1)
        imageRaw = numpy.transpose(array2dBig)

    # Repeat array two times for Lepton3
    else:
        array2d = leptonValues.reshape(120, 160)
        array2dBig = array2d.repeat(2, axis=0).repeat(2, axis=1)
        imageRaw = numpy.transpose(array2dBig)

    # Apply the gaussian blur filter
    if filterType == 1:
        imageRaw = scipy.ndimage.filters.gaussian_filter(imageRaw, 1.33, mode='nearest')
    # Apply box blur filter
    if filterType == 2:
        imageRaw = scipy.ndimage.filters.uniform_filter(imageRaw, 3)


# Converts the Lepton raw values to RGB colors
def convertColors():
    global image, imageRaw

    # Calculate the scale
    scale = (colorElements - 1.0) / (maxData - minData)

    # Check if values are out of color scheme bounds
    array_np = numpy.asarray(imageRaw)
    low_values_indices = array_np < minData
    array_np[low_values_indices] = minData
    high_values_indices = array_np > maxData
    array_np[high_values_indices] = maxData
    imageRaw = array_np

    # Calculate the lookup vale (0-255)
    imageRaw[0:, 0:] = (imageRaw[0:, 0:] - minData) * scale
    imageRaw[0:, 0:] = 3 * imageRaw[0:, 0:]
    image[0:, 0:, 0] = imageRaw[0:, 0:]
    image[0:, 0:, 1] = imageRaw[0:, 0:] + 1
    image[0:, 0:, 2] = imageRaw[0:, 0:] + 2
    image = numpy.take(colorMap, image)


# Convert the Lepton 8 bit data to 16 bit data
def convertLeptonData():
    global leptonValues

    # For Lepton2, convert 4800 values
    if leptonVersion == 0:
        leptonDataReshaped = numpy.array(leptonData).reshape(4800, 2)
    # For Lepton3, convert 19200 values
    else:
        leptonDataReshaped = numpy.array(leptonData).reshape(19200, 2)

    # Assign them to the lepton values array
    leptonValues[0:] = (leptonDataReshaped[0:, 0] * 256) + leptonDataReshaped[0:, 1]


# Display some text on the screen
def displayText(msg, wait):
    if screen == None:
        print msg
        return

    # Fill background
    background = pygame.Surface((640, 480))
    background.fill((250, 250, 250))

    # Display some text
    font = pygame.font.SysFont("Times New Roman", 36)
    text = font.render(msg, 1, (10, 10, 10))
    textpos = text.get_rect()
    textpos.centerx = background.get_rect().centerx
    textpos.centery = background.get_rect().centery
    background.blit(text, textpos)

    # Blit everything to the screen
    screen.blit(background, (0, 0))
    pygame.display.flip()

    # Opt: Wait a second, so the user can read the text
    if wait:
        time.sleep(1)


# Starts or stops video recording
def videoRecord():
    global recordVideo, videoCounter, videoFolder, videoStart
    # Start recording a video
    if not recordVideo:
        # Show message
        displayText("Recording video..", True)

        # Update button
        buttonSaveVideo.caption = "Stop Recording"
        buttonSaveVideo.draw(screen)

        # Create dir if required
        if not os.path.exists("videos"):
            os.makedirs("videos")
        owd = os.getcwd()
        os.chdir("videos")
        videoFolder = time.strftime("%Y%m%d%H%M%S")
        os.mkdir(videoFolder)
        videoFolder = os.path.join("videos", videoFolder)
        os.chdir(owd)

        # Set record video to true and save time
        recordVideo = True
        videoStart = time.time()

    # Stop recording a video
    else:
        # Calculate the number of frames
        frames = (videoCounter * 1.0) / (time.time() - videoStart)

        # Show stop message
        displayText("Stop recording..", True)

        # Update button
        buttonSaveVideo.caption = "Record Video"
        buttonSaveVideo.draw(screen)

        # Show converting message
        displayText("Converting..", False)

        # Use ffmpeg to convert the frames to avi
        ffmpegCmd = 'ffmpeg -framerate %d -i %s/%%06d.jpg -codec copy %s' % \
                    (round(frames), videoFolder, videoFolder + ".avi")
        os.system(ffmpegCmd)

        # Remove the folder with the single frames
        shutil.rmtree(videoFolder, True)

        # Clear marker
        videoFolder = None
        recordVideo = False
        videoCounter = 0


# Saves a thermal image as JPEG
def saveThermal():
    # Check if thermal image exists and not recording video
    if (thermalImg is not None) and (recordVideo == False):

        # Create dir if required
        if not os.path.exists("thermal"):
            os.makedirs("thermal")

        # Save image to dir
        pygame.image.save(thermalImg, os.path.join("thermal", time.strftime("%Y%m%d%H%M%S.jpg", time.gmtime())))

        # Show message
        displayText("Thermal image saved!", True)


# Receive and save the visual image as JPEG
def saveVisual():
    # Show message
    displayText("Transfer visual image..", False)

    # Set the timeout higher
    if hardwareVersion == 0:
        ser.timeout = 10.0
    else:
        ser.timeout = 5.0

    # Receive the visual image data length
    try:
        # Send visual img and high res command
        sendArray = (chr(cmd_get_visualimg), chr(1))
        ser.write(sendArray)
        # Get visual image length
        inData = map(ord, ser.read(2))

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
    dataLen = inData[0] * 256 + inData[1]

    # Try to read JPEG bytestream
    try:
        # Get data by length
        inData = map(ord, ser.read(dataLen))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Set timeout back
    ser.timeout = 3.0

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
    f = io.open(os.path.join("visual", time.strftime("%Y%m%d%H%M%S.jpg", time.gmtime())), 'wb')
    f.write(byteArray)
    f.close()

    # Show message
    time.sleep(0.1)
    displayText("Visual image saved!", True)

    # Everything worked
    return True


# Close the application and end connection
def endConnection():
    # Show message
    displayText("Exiting..", True)

    # Send end command over serial port
    try:
        if ser:
            if ser.isOpen():
                ser.write(chr(cmd_end))
                ser.close()

    # Discard error
    except serial.serialutil.SerialException:
        pass

    # Exit application
    pygame.quit()
    sys.exit()


# Check if the user wants to exit
def checkExit():
    # Just return if GUI stuff hasn't been setup yet
    if screen == None:
        return

    # User wants to exit
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            endConnection()


# Check if the warmup has been completed
def checkWarmup():
    global calStatus

    # If calibration status is warmup and longer than remaining time, switch to done
    if calStatus == 0 and (time.time() - calTimer) >= calTimeLeft:
        calStatus = 1
        return True

    # Still in warmup
    elif calStatus == 0:
        return False

    # Calibration done
    return True

# Add or remove a temperature point
def tempFunc(mousePos, add):

    # If warmup is not completed, return
    if not checkWarmup():
        return True

    # Check if we have some space left when adding a point
    pos = int
    if add:
        # Go through the array and find the next free pos
        for i in range(0, 96):
            if tempPoints[i][0] == 0:
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
        tempPoints[pos][0] = xpos + (ypos * 160) + 1
        # Temperature value will be refreshed in the next frame
        tempPoints[pos][1] = 0

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
                        if tempPoints[i][0] == index:
                            # Set to invalid
                            tempPoints[i][0] = 0
                            # Reset value
                            tempPoints[i][1] = 0

    # Update tempPoints on device
    try:
        # Build send array
        sendArray = (chr(cmd_set_temppoints),)
        for i in range(0, 96):
            # Index
            sendArray += (chr((tempPoints[i][0] & 0xFF00) >> 8),)
            sendArray += (chr(tempPoints[i][0] & 0x00FF),)
            # Value
            sendArray += (chr((tempPoints[i][1] & 0xFF00) >> 8),)
            sendArray += (chr(tempPoints[i][1] & 0x00FF),)

        # Send byte array
        ser.write(sendArray)

        # Wait for ACK
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Check if ACK matches
    if inData[0] != cmd_set_temppoints:
        return False

    # Everything OK
    return True


# Function to calculate temperature out of Lepton raw value
def calFunction(rawValue):
    # Calculate the temperature in Celcius out of the coefficients
    temp = (calSlope * rawValue) + calOffset

    # Convert to Fahrenheit if needed
    if tempFormat:
        temp = (9.0 / 5.0) * temp + 32.0

    # Return absolute temperature
    return temp


# Get the current text color
def getTextColor():
    color = None
    if textColor == 0:
        color = colorWhite
    if textColor == 1:
        color = colorBlack
    if textColor == 2:
        color = colorRed
    if textColor == 3:
        color = colorGreen
    if textColor == 4:
        color = colorBlue
    return color


# Show the spot temperature
def showSpot():
    global thermalImg, spotTemp

    # Choose font
    font = pygame.font.SysFont("Times New Roman", 26)

    # Get the color
    color = getTextColor()

    # Add circle
    pygame.draw.circle(thermalImg, color, (320, 240), 24, 2)
    # Add lines
    pygame.draw.line(thermalImg, color, (272, 240), (296, 240), 2)
    pygame.draw.line(thermalImg, color, (344, 240), (368, 240), 2)
    pygame.draw.line(thermalImg, color, (320, 192), (320, 216), 2)
    pygame.draw.line(thermalImg, color, (320, 264), (320, 288), 2)

    # Create the string
    tmpstr = "{:.2f}".format(spotTemp)
    text = font.render(tmpstr, 1, color)

    # Show it on the screen
    textpos = text.get_rect()
    textpos.centerx = thermalImg.get_rect().centerx
    textpos.centery = thermalImg.get_rect().centery + 65
    thermalImg.blit(text, textpos)

# Draw a temperature measurement on the screen
def drawTemperature(xpos, ypos, rawValue, minMax):
    # Choose font
    font = pygame.font.SysFont("Times New Roman", 26)

    # Get the color
    color = getTextColor()

    # Text
    text = None

    # When warmup is complete
    if checkWarmup():
        # Calculate absolute temperature
        temp = calFunction(rawValue)

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
    pygame.draw.circle(thermalImg, color, (xpos, ypos), 2)

    # Calc x position for the temperature
    if checkWarmup():
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
    thermalImg.blit(text, (xpos, ypos))

# Show the min / max position
def showMinMax():
    minValue = 65535
    minTempPos = int
    maxValue = 0
    maxTempPos = int

    # For Lepton2
    if leptonVersion == 0:
        elements = 4800
    # For Lepton3
    else:
        elements = 19200

    # Go through the lepton values
    for i in range(0, elements):
        # We found minTemp
        if leptonValues[i] < minValue:
            minValue = leptonValues[i]
            minTempPos = i
        # We found maxTemp
        if leptonValues[i] > maxValue:
            maxValue = leptonValues[i]
            maxTempPos = i

    # Show minimum position
    if minMaxEnabled & 1:
        # Get position
        if leptonVersion == 0:
            xpos = (minTempPos % 80) * 8
            ypos = (minTempPos / 80) * 8
        else:
            xpos = (minTempPos % 160) * 4
            ypos = (minTempPos / 160) * 4

        # Draw it on the screen
        drawTemperature(xpos, ypos, minValue, 1)

    # Show maximum position
    if minMaxEnabled & 2:
        # Get position
        if leptonVersion == 0:
            xpos = (maxTempPos % 80) * 8
            ypos = (maxTempPos / 80) * 8
        else:
            xpos = (maxTempPos % 160) * 4
            ypos = (maxTempPos / 160) * 4

        # Draw it on the screen
        drawTemperature(xpos, ypos, maxValue, 2)

# Show the remaining warmup time
def showWarmupTime():
    global thermalImg

    # Choose font
    font = pygame.font.SysFont("Times New Roman", 26)

    # Get the color
    color = getTextColor()

    # Create text
    warmupLeft = calTimeLeft - (time.time() - calTimer)
    warmupStr = "{:.0f}".format(warmupLeft)

    text = font.render("Sensor warmup, " + warmupStr + "s left", 1, color)

    # Calculate position
    textpos = text.get_rect()
    textpos.centerx = thermalImg.get_rect().centerx
    textpos.centery = 440

    # Insert into thermal image
    screen.blit(text, textpos)

# Show the color bar
def showColorbar():
    global thermalImg

    # Calculate height
    height = 480 - ((480 - colorElements) / 2)

    # Choose font
    font = pygame.font.SysFont("Times New Roman", 26)

    # Display color bar
    for i in range(0, colorElements - 1):
        red = colorMap[i * 3]
        green = colorMap[(i * 3) + 1]
        blue = colorMap[(i * 3) + 2]
        pygame.draw.line(thermalImg, (red, green, blue), (590, height - i), (630, height - i))

    # Calculate min and max
    minTemp = calFunction(minData)
    maxTemp = calFunction(maxData)

    # Calculate step
    step = (maxTemp - minTemp) / 3.0

    # Get the color
    color = getTextColor()

    # Draw min temp
    tempstr = "{:.0f}".format(minTemp)
    text = font.render(tempstr, 1, color)
    thermalImg.blit(text, (540, height - 5))

    # Draw temperatures after min before max
    tempstr = "{:.0f}".format(minTemp + step)
    text = font.render(tempstr, 1, color)
    thermalImg.blit(text, (540, height - 5 - (colorElements / 3)))
    tempstr = "{:.0f}".format(minTemp + (2 * step))
    text = font.render(tempstr, 1, color)
    thermalImg.blit(text, (540, height - 5 - (2 * (colorElements / 3))))

    # Draw max temp
    tempstr = "{:.0f}".format(maxTemp)
    text = font.render(tempstr, 1, color)
    thermalImg.blit(text, (540, height - 5 - (3 * (colorElements / 3))))


# Show the selected temperature points on the screen
def showTemperatures():
    global thermalImg

    # Go through the tempPoints array
    for i in range(0, 96):
        # Get index
        index = tempPoints[i][0]

        # Check if the tempPoint is active
        if index != 0:

            # Index value is between 1 and max
            index -= 1

            # Calculate x and y position
            xpos = (index % 160)
            ypos = (index / 160)

            # Update raw value
            if leptonVersion == 0:
                tempPoints[i][1] = leptonValues[(xpos / 2) + (ypos / 2) * 80]
            else:
                tempPoints[i][1] = leptonValues[xpos + (ypos * 160)]

            # Calculate screen position
            xpos *= 4
            ypos *= 4

            # Check if too close to border
            if ypos <= 20:
                ypos = 21

            # Draw it on the screen
            drawTemperature(xpos, ypos, tempPoints[i][1], 0)


# Display the thermal image on the screen
def displayThermalImage():
    global videoCounter

    # Check if thermal image has been created
    if thermalImg is None:
        return

    # Show the spot temperature if enabled
    if spotEnabled:
        showSpot()

    # Show the colorbar if enabled and warmup completed
    if barEnabled and checkWarmup():
        showColorbar()

    # Show the min / max pos if enabled
    if minMaxEnabled:
        showMinMax()

    # Show temperatures if warmup completed
    if checkWarmup():
        showTemperatures()

    # Refresh screen
    screen.blit(thermalImg, (0, 0))

    # Capture to file if video enabled
    if recordVideo:
        pygame.image.save(thermalImg, os.path.join(videoFolder, '{0:06d}'.format(videoCounter) + ".jpg"))
        videoCounter += 1


# Main method to create the thermal image
def createThermalImage():
    global thermalImg

    # Convert 8 bit to 16 bit data
    convertLeptonData()

    # Apply gaussian blur
    applyFilter()

    # Convert to colors
    convertColors()

    # Resize image to 640x480 with nearest neighbour
    imageBig = scipy.misc.imresize(image, (640, 480), interp='nearest', mode=None)

    # Convert to pygame surface and save as thermal image
    thermalImg = pygame.surfarray.make_surface(imageBig)


# Extracts RGB values from the lepton raw data
def extractRGB(buff):
    arr = numpy.fromstring(buff, dtype=numpy.uint16).newbyteorder('S')
    r = (((arr & 0xF800) >> 11) * 255.0 / 31.0).astype(numpy.uint8)
    g = (((arr & 0x07E0) >> 5) * 255.0 / 63.0).astype(numpy.uint8)
    b = (((arr & 0x001F) >> 0) * 255.0 / 31.0).astype(numpy.uint8)
    arr = numpy.column_stack((r, g, b))
    return arr


# Get a stream frame from the display
def getStream():
    global thermalImg

    # Calc length based on HW and HQRes
    if hardwareVersion == 0 or hqRes == 0:
        length = 38400
    else:
        length = 153600

    # Try to receive a frame
    try:
        frameBuffer = ser.read(length)

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
    rgbValues = extractRGB(frameBuffer)

    # Convert RGB values to image
    if hardwareVersion == 0 or hqRes == 0:
        img = pygame.image.frombuffer(rgbValues, (160, 120), 'RGB')
    else:
        img = pygame.image.frombuffer(rgbValues, (320, 240), 'RGB')

    # Scale image smooth
    thermalImg = pygame.transform.smoothscale(img, (640, 480))

    # Refresh screen
    screen.blit(thermalImg, (0, 0))

    # Everything OK
    return True


# Get a whole set of raw data from the Lepton
def getFrameData():
    global leptonData, minData, maxData, spotTemp, calOffset, calSlope

    # Receive 9600 byte for Lepton2
    if leptonVersion == 0:
        length = 9600
    # Receive 38400 byte for Lepton3
    else:
        length = 38400

    # Try to receive the data
    try:
        leptonData = map(ord, ser.read(length))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Length does not match
    if len(leptonData) != length:
        return False

    # Timeout
    if not leptonData:
        return False

    # Try to receive the additional data
    try:
        additionalData = map(ord, ser.read(16))

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
    minData = additionalData[0] * 256 + additionalData[1]
    maxData = additionalData[2] * 256 + additionalData[3]

    # Read spot temp
    spotTemp = [additionalData[4], additionalData[5], additionalData[6], additionalData[7]]
    spotTemp = ''.join(chr(i) for i in spotTemp)
    spotTemp = float(struct.unpack('f', spotTemp)[0])

    # Read calibration offset
    calOffset = [additionalData[8], additionalData[9], additionalData[10], additionalData[11]]
    calOffset = ''.join(chr(i) for i in calOffset)
    calOffset = float(struct.unpack('f', calOffset)[0])

    # Read calibration slope
    calSlope = [additionalData[12], additionalData[13], additionalData[14], additionalData[15]]
    calSlope = ''.join(chr(i) for i in calSlope)
    calSlope = float(struct.unpack('f', calSlope)[0])

    # Everything worked
    return True


# Get the config data
def getConfigData():
    global leptonVersion, leptonValues, rotateScreen, colorScheme, tempFormat, spotEnabled, \
        barEnabled, minMaxEnabled, adjustLimits, textColor, filterType, laserEnabled

    # Show message
    displayText("Reading config data..", False)

    # Try to receive the config data
    try:
        # Send config receive command
        ser.write(chr(cmd_get_configdata))
        # get data
        configData = map(ord, ser.read(10))

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
    leptonVersion = configData[0]
    # Lepton 2 non shuttered, make it to zero
    if leptonVersion == 2:
        leptonVersion = 0

    # Create lepton values array by size
    if leptonVersion == 0:
        leptonValues = numpy.zeros(4800, dtype=int)
    if leptonVersion == 1:
        leptonValues = numpy.zeros(19200, dtype=int)

    # Read rotation
    rotateScreen = configData[1]
    setRotation(False)
    # Read color scheme
    colorScheme = configData[2]
    setColorScheme(False)
    # Read temp format
    tempFormat = configData[3]
    setFormat(False)
    # Read spot enabled
    spotEnabled = configData[4]
    setSpot(False)
    # Read colorbar enabled
    barEnabled = configData[5]
    setBar(False)
    # Read min max enabled
    minMaxEnabled = configData[6]
    setMinMax(False)
    # Read text color
    textColor = configData[7]
    setTextColor(False)
    # Read filter type
    filterType = configData[8]
    setFilterType(False)
    # Read adjust limits allowed
    adjustLimits = configData[9]
    setLimits(False)
    # Laser is disabled by default
    laserEnabled = False
    setLaser(False)

    # Update screen
    pygame.display.flip()

    # Everything worked
    return True


# Serial handler to receive and send data
def serialHandler():
    # Clear buffer
    ser.flushInput()
    ser.flushOutput()

    # Try to get a new frame from the device
    try:
        # Send get raw frame command
        if renderMode:
            ser.write(chr(cmd_rawframe))
        # Send a get display frame command
        else:
            ser.write(chr(cmd_displayframe))

        # Wait for command response
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Parse the command
    cmd = inData[0]

    # Save thermal image command
    if cmd == frame_capture_thermal:
        saveThermal()

    # Save visual image command
    elif cmd == frame_capture_visual:
        if not saveVisual():
            displayText("Error saving visual image!", True)

    # Start or stop video command
    elif cmd == frame_capture_video:
        videoRecord()

    # Send normal frame command
    elif cmd == frame_normal:
        # Rendering mode
        if renderMode:
            # Get lepton frame data
            if not getFrameData():
                return False
            # Create the thermal image
            createThermalImage()

            # Display it on the screen
            displayThermalImage()

        # Streaming mode
        else:
            # Get the display buffer
            if not getStream():
                return False

        # Show warmup time when not completed
        if not checkWarmup():
            showWarmupTime()

    # Everything worked
    return True


# Read the temperature points from the device
def getTempPoints():
    global tempPoints

    # Show message on screen
    displayText("Getting temperature points..", False)

    # Try to get the temperature points
    try:
        # Send set time command
        ser.write(chr(cmd_get_temppoints))
        # Wait for response
        inData = map(ord, ser.read(384))

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
        tempPoints[i][0] = inData[i * 4] * 256 + inData[(i * 4) + 1]
        # Correct index if required
        if tempPoints[i][0] == 65535:
            tempPoints[i][0] = 0
        # Value
        tempPoints[i][1] = inData[(i * 4) + 2] * 256 + inData[(i * 4) + 3]

    # Everything worked
    return True


# Set the render mode
def setRenderMode():
    global renderMode

    # Toggle
    renderMode = not renderMode

    # Update UI
    if renderMode:
        buttonRenderMode.caption = "Mode: Rendering"
    else:
        buttonRenderMode.caption = "Mode: Streaming"
    buttonRenderMode.draw(screen)


# Set the time on the Thermocam
def setTime():
    # Show message on screen
    displayText("Trying to set the time..", False)

    # Get the system time
    t = datetime.datetime.now()
    timeOut = t.strftime("%Y-%m-%d %H:%M:%S\n")

    # Try to set the time on the system
    try:
        # Send set time command
        ser.write(chr(cmd_set_time))
        # Set time as bytestream
        ser.write(bytes(timeOut))
        # Wait for ACK
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # ACK is not the expected value
    if inData[0] != cmd_set_time:
        return False

    # Everything worked
    return True


# Get the hardware version
def getHWVersion():
    global hardwareVersion

    # Show message
    displayText("Reading hardware version..", False)

    # Try to receive it
    try:
        # Send get hardware version command
        ser.write(chr(cmd_get_hwversion))
        # Receive answer
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Check if input is ACK from start
    if inData[0] == cmd_start:
        # Try to read the next byte
        try:
            # Receive answer
            inData = map(ord, ser.read(1))

        # Error
        except serial.serialutil.SerialException:
            return False

            # Timeout
        if not inData:
            return False

    # Check if hardware version is valid
    if (inData[0] != 0) and (inData[0] != 1):
        return False

    # Everything worked
    hardwareVersion = inData[0]
    return True

# Get the firmware version
def getFWVersion():
    global fwVersion

    # Show message
    displayText("Reading firmware version..", False)

    # Try to receive it
    try:
        # Send get firmware version command
        ser.write(chr(cmd_get_fwversion))
        # Receive answer
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Check if firmware version is valid
    if inData[0] < 233:
        return False

    # Save firmware version
    fwVersion = inData[0]

    # Everything worked
    return True

# Get the HQ resolution for Thermocam V2
def getHQResolution():
    global ser, hqRes

    # Show message
    displayText("Reading HQ resolution..", False)

    # Try to receive it
    try:
        # Send get hardware version command
        ser.write(chr(cmd_get_hqresolution))
        # Receive answer
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Check if hq resolution is valid
    if (inData[0] != 0) and (inData[0] != 1):
        return False

    # Save hq resolution
    hqRes = inData[0]

    # Everything worked
    return True

# Get the calibration status
def getCalStatus():
    global ser, calStatus, calTimeLeft, calTimer

    # Show message
    displayText("Reading Calibration Status..", False)

    # Try to receive it
    try:
        # Send get hardware version command
        ser.write(chr(cmd_get_calstatus))
        # Receive answer
        inData = map(ord, ser.read(2))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Return if size does not match
    if len(inData) != 2:
        return False

    # Check if calibration status is valid
    if (inData[0] != 0) and (inData[0] != 1):
        return False

    # Save calibration status
    calStatus = inData[0]

    # If warmup
    if calStatus == 0:
        # Save time left
        calTimeLeft = inData[1]
        # Limit value
        if calTimeLeft < 1:
            calTimeLeft = 1
        if calTimeLeft > 30:
            calTimeLeft = 30

        # Start timer
        calTimer = time.time()

    # Everything worked
    return True

# Get the diagnostic information
def getDiagnostic():
    # Show message
    displayText("Reading diagnostic infos..", False)

    # Try to receive it
    try:
        # Send get diagnostic command
        ser.write(chr(cmd_get_diagnostic))
        # Receive answer
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Check if there are diagnostic error(s)
    if inData[0] != 255:
        # Check Lepton I2C
        if not (inData[0] >> 6) & 1:
            displayText("Lepton I2C error", True)
            return False
        # Check Lepton SPI
        if not (inData[0] >> 7) & 1:
            displayText("Lepton SPI error", True)
            return False
        # Check Spot Sensor
        if not (inData[0] >> 0) & 1:
            displayText("Spot Sensor error", True)
            return False

    # Everything worked
    return True


# Send start command
def sendStart():
    # Send start and wait for ACK
    try:
        # Send start command
        ser.write(chr(cmd_start))
        # Read answer
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # ACK is not the expected value
    if inData[0] != cmd_start:
        return False

    # Everything worked
    return True


# Connect to the device
def connect():
    global ser, hardwareVersion, device

    # Display waiting message
    displayText("Waiting for device on " + device + " ...", False)

    # Check if device is connected
    while True:
        # Check if the user wants to exit
        checkExit()

        # Try to open the ports
        try:
            ser = serial.Serial(device, 115200)
        # Did not work, try again in 1s
        except serial.serialutil.SerialException:
            time.sleep(1)
            continue

        # If connected, leave loop
        if ser:
            break

    # Show detected message
    displayText("Detected device!", False)
    # Give the device 5 seconds to boot
    ser.timeout = 5.0
    # Clear buffer
    ser.flushInput()
    ser.flushOutput()

    # Send start command
    while not sendStart():
        # Check if the user wants to exit
        checkExit()
        # Display message
        displayText("Put the device in live mode!", True)

    # Set the timeout to 3 seconds
    ser.timeout = 3.0

    # Get hardware version, firmware version, HQ resolution (V2 only) and calibration status
    if not getHWVersion() or not getFWVersion() or not getHQResolution() or not getCalStatus():
        displayText("Update your firmware to 2.35 or greater!", False)
        time.sleep(3)
        endConnection()

    # Get diagnostic infos
    if not getDiagnostic():
        displayText("There were hardware errors, exit..", True)
        endConnection()

    # Get configuration data
    if not getConfigData():
        ser.close()
        return False

    # Get temperature points
    if not getTempPoints():
        ser.close()
        return False

    # Set the time
    if not setTime():
        displayText("Unable to set the time!", True)

    # Everything worked
    return True


# Check if the mouse has been pressed or a serial command is received
def eventHandler():
    # Check for button clicks
    for event in pygame.event.get():
        # Exit
        if event.type == pygame.QUIT:
            endConnection()

        # Click on image
        if event.type == pygame.MOUSEBUTTONUP:
            # Get mouse position
            mousePos = pygame.mouse.get_pos()

            # Left mouse - add point
            if event.button == 1:
                if not tempFunc(mousePos, True):
                    displayText("Unable to add point on device!", True)

            # Right mouse - remove point
            if event.button == 3:
                if not tempFunc(mousePos, False):
                    displayText("Unable to remove point on device!", True)

        # Save Thermal
        if 'click' in buttonSaveThermal.handleEvent(event):
            saveThermal()

        # Save Visual
        if 'click' in buttonSaveVisual.handleEvent(event):
            if not saveVisual():
                displayText("Unable to save the visual image!", True)

        # Record video
        if 'click' in buttonSaveVideo.handleEvent(event):
            videoRecord()

        # Render mode
        if 'click' in buttonRenderMode.handleEvent(event):
            setRenderMode()

        # Show spot
        if 'click' in buttonSpot.handleEvent(event):
            if not setSpot(True):
                displayText("Error setting showSpot on device!", True)

        # Show bar
        if 'click' in buttonBar.handleEvent(event):
            if not setBar(True):
                displayText("Error setting showBar on device!", True)

        # Show min / max
        if 'click' in buttonHotCold.handleEvent(event):
            if not setMinMax(True):
                displayText("Error setting min/max device!", True)

        # Text color
        if 'click' in buttonTextColor.handleEvent(event):
            if not setTextColor(True):
                displayText("Error setting textColor on device!", True)

        # Filter strength
        if 'click' in buttonFilter.handleEvent(event):
            if not setFilterType(True):
                displayText("Error setting filterType on device!", True)

        # Color scheme
        if 'click' in buttonColor.handleEvent(event):
            if not setColorScheme(True):
                displayText("Error setting colorScheme on device!", True)

        # Temperature limits
        if 'click' in buttonLimits.handleEvent(event):
            if not setLimits(True):
                displayText("Error setting adjustLimits on device!", True)

        # Temperature format
        if 'click' in buttonFormat.handleEvent(event):
            if not setFormat(True):
                displayText("Error setting tempFormat on device!", True)

        # Rotation
        if 'click' in buttonRotation.handleEvent(event):
            if not setRotation(True):
                displayText("Error setting rotation on device!", True)

        # Laser
        if 'click' in buttonLaser.handleEvent(event):
            if not setLaser(True):
                displayText("Error setting laserEnabled on device!", True)

        # Shutter
        if 'click' in buttonShutter.handleEvent(event):
            if not runShutter():
                displayText("Error triggering the shutter!", True)

        # Calibration
        if 'click' in buttonCalibration.handleEvent(event):
            if not runCalibration():
                displayText("Error running the calibration!", True)

# Init procedures
def setup():
    global screen

    # Add environment variable for Windows XP
    if platform.release() == 6:
        os.environ['SDL_VIDEODRIVER'] = 'windlib'

    # Init pygame
    pygame.init()

    # Set window resolution
    screen = pygame.display.set_mode((640, 640))
    screen.fill((230, 230, 230))

    # Set windows title
    pygame.display.set_caption("Thermal Live Viewer " + version)

    # Add the buttons
    drawButtons()

    # Welcome message
    displayText("Thermal Live Viewer", True)


# Main Program
def loop():
    # Try to establish a connection
    while not connect():
        displayText("Error connecting to device..", True)

    # Connection established
    if hardwareVersion == 0:
        displayText("Connected to DIY-Thermocam V1!", True)
    else:
        displayText("Connected to DIY-Thermocam V2!", True)

    while True:
        # Check for serial events
        if not serialHandler():
            ser.close()
            displayText("Connection lost, reconnect..", True)
            break

        # Check for mouse press
        eventHandler()

        # Update screen
        pygame.display.flip()


# Call of functions
if __name__ == "__main__":
    if len(sys.argv) > 1:
        device = sys.argv[1]
    setup()
    while True:
        loop()
