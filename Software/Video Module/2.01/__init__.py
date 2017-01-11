# DIY-Thermocam Video Module
# 2016 by Max Ritter
# www.diy-thermocam.net

# Version definition
version = "Version 2.01 from 18.12.2016"

# Commands
CMD_START = 100
CMD_GET_FWVERSION = 129
CMD_GET_HWVERSION = 138
CMD_GET_HQRESOLUTION = 141
CMD_FRAME_DISPLAY = 152
FRAME_NORMAL = 183

# Imports
import serial
import numpy
import pygame
import time

# Global Variables
fwVersion = int
hardwareVersion = int
hqRes = int
screen = None
ser = None


# Display some text on the screen
def displayText(msg, wait):
    # Fill background
    background = pygame.Surface(screen.get_size())
    background.fill((250, 250, 250))

    # Display some text
    font = pygame.font.Font(None, 36)
    text = font.render(msg, 1, (10, 10, 10))
    textpos = text.get_rect()
    textpos.centerx = background.get_rect().centerx
    textpos.centery = background.get_rect().centery
    background.blit(text, textpos)

    # Display build version and date
    font = pygame.font.Font(None, 24)
    text = font.render(version, 1, (10, 10, 10))
    textpos = text.get_rect()
    textpos.centerx = background.get_rect().centerx
    textpos.centery = 440
    background.blit(text, textpos)

    # Blit everything to the screen
    screen.blit(background, (0, 0))
    pygame.display.flip()

    # Opt: Wait a second, so the user can read the text
    if wait:
        time.sleep(1)


# Extracts RGB values from the lepton raw data
def extractRGB(buff):
    arr = numpy.fromstring(buff, dtype=numpy.uint16).newbyteorder('S')
    r = (((arr & 0xF800) >> 11) * 255.0 / 31.0).astype(numpy.uint8)
    g = (((arr & 0x07E0) >> 5) * 255.0 / 63.0).astype(numpy.uint8)
    b = (((arr & 0x001F) >> 0) * 255.0 / 31.0).astype(numpy.uint8)
    arr = numpy.column_stack((r, g, b))
    return arr


# Get a frame and display it on the screen
def getFrame():
    # Clear buffer
    ser.flushInput()
    ser.flushOutput()

    # Try to receive a frame
    try:
        # Send get display frame command
        ser.write(chr(CMD_FRAME_DISPLAY))

        # Wait for command response
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Send normal frame command
    if inData[0] == FRAME_NORMAL:

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
        pygame.display.flip()

    # Everything OK
    return True


# Get the hardware version
def getHWVersion():
    global ser, hardwareVersion

    # Show message
    displayText("Reading hardware version..", False)

    # Try to receive it
    try:
        # Send get hardware version command
        ser.write(chr(CMD_GET_HWVERSION))
        # Receive answer
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # Check if input is ACK from start
    if inData[0] == CMD_START:
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

    # Save hardware version
    hardwareVersion = inData[0]

    # Everything worked
    return True

# Get the firmware version
def getFWVersion():
    global fwVersion

    # Show message
    displayText("Reading firmware version..", False)

    # Try to receive it
    try:
        # Send get firmware version command
        ser.write(chr(CMD_GET_FWVERSION))
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
        ser.write(chr(CMD_GET_HQRESOLUTION))
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


# Send start command
def sendStart():
    # Send start and wait for ACK
    try:
        # Send start command
        ser.write(chr(CMD_START))
        # Read answer
        inData = map(ord, ser.read(1))

    # Error
    except serial.serialutil.SerialException:
        return False

    # Timeout
    if not inData:
        return False

    # ACK is not the expected value
    if inData[0] != CMD_START:
        return False

    # Everything worked
    return True


# Connect to the device
def connect():
    global ser

    # Display waiting message
    displayText("Waiting for device..", False)

    # Wait until a device is connected
    while True:
        # Try to open the ports
        try:
            ser = serial.Serial('/dev/ttyACM0', 115200)
        # Did not work, try again in 1s
        except serial.serialutil.SerialException:
            time.sleep(1)
            continue

        # If connected, leave loop
        if ser:
            break

    # Display an error message if port is not open
    if not ser.isOpen():
        return False

    # Show detected message
    displayText("Detected device!", False)
    # Give the device 5 seconds to boot
    ser.timeout = 5.0
    # Clear buffer
    ser.flushInput()
    ser.flushOutput()

    # Send start command
    while not sendStart():
        displayText("Put the device in live mode!", True)

    # Set the timeout to 3 seconds
    ser.timeout = 3.0

    # Get hardware version and HQ resolution
    if not getHWVersion() or not getFWVersion() or not getHQResolution():
        displayText("Update your firmware to 2.33 or greater!", False)
        time.sleep(3)
        ser.close()
        return False

    # Everything worked
    return True


# Init procedures
def setup():
    global screen

    # Init pygame
    pygame.init()

    # Init screen
    screen = pygame.display.set_mode((640, 480), pygame.FULLSCREEN)

    # Disable mouse
    pygame.mouse.set_visible(False)

    # Display welcome message
    displayText("DIY-Thermocam Video Module", True)


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

    # Receive display frame until error
    while True:
        # Error appeared
        if not getFrame():
            ser.close()
            displayText("Error receiving data, reconnect..", True)
            break


# Call of functions
setup()
while True:
    loop()
