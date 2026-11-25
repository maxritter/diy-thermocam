/*
*
* CONNECTION - Communication protocol for the USB serial data transmission
*
* DIY-Thermocam Firmware
*
* GNU General Public License v3.0
*
* Copyright by Max Ritter
*
* http://www.diy-thermocam.net
* https://github.com/maxritter/diy-thermocam
*
*/

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <hardware.h>
#include <gui.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include <thermal.h>
#include <display.h>
#include <lepton.h>
#include <save.h>
#include <touchscreen.h>
#include <hardware.h>
#include <livemode.h>
#include <sdcard.h>
#include <create.h>
#include <connection_v2.h>
#include <connection.h>

/*######################### STATIC DATA DECLARATIONS ##########################*/

//Command, default send frame
static byte sendCmd = FRAME_NORMAL;
static bool s_v1FrameConsumed = true;
static uint8_t s_v1TxBuf[1024];

/*######################## STATIC FUNCTION BODIES #############################*/

/* Wait up to 100ms until count bytes are available, the PC may split a payload */
static bool serialWait(int count)
{
	long timer = millis();
	while ((Serial.available() < count) && ((millis() - timer) < 100))
		;
	return Serial.available() >= count;
}

/* Read a 16-bit value, MSB first. Two reads in one expression have no fixed order */
static uint16_t serialRead16()
{
	uint16_t msb = Serial.read();
	uint16_t lsb = Serial.read();
	return (msb << 8) | lsb;
}

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Get integer out of a text string */
int getInt(String text)
{
	char temp[6];
	text.toCharArray(temp, 5);
	int x = atoi(temp);
	return x;
}

/* Enter the serial connection mode if no display attached */
bool checkNoDisplay()
{
	//No connection to ILI9341 and touch screen -> go to USB serial
	if (!checkDiagnostic(diag_display) && !checkDiagnostic(diag_touch))
		return true;
	//Display connected
	return false;
}

/* Send the lepton raw limits */
void sendRawLimits()
{
	//Send min
	Serial.write((minValue & 0xFF00) >> 8);
	Serial.write(minValue & 0x00FF);
	//Send max
	Serial.write((maxValue & 0xFF00) >> 8);
	Serial.write(maxValue & 0x00FF);
}

/* Send the lepton raw data (buffered bulk writes, big-endian v1 wire format) */
void sendRawData(bool color)
{
	size_t idx = 0;
	auto flush_v1 = [&]() {
		if (idx > 0) {
			Serial.write(s_v1TxBuf, idx);
			idx = 0;
		}
	};
	if ((leptonVersion == leptonVersion_2_5_shutter) && (!color))
	{
		for (int line = 0; line < 60; line++)
		{
			for (int column = 0; column < 80; column++)
			{
				uint16_t result = smallBuffer[(line * 2 * 160) + (column * 2)];
				s_v1TxBuf[idx++] = (result >> 8) & 0xFF;
				s_v1TxBuf[idx++] = result & 0xFF;
				if (idx >= sizeof(s_v1TxBuf)) flush_v1();
			}
		}
	}
	else
	{
		for (int i = 0; i < 19200; i++)
		{
			uint16_t v = smallBuffer[i];
			s_v1TxBuf[idx++] = (v >> 8) & 0xFF;
			s_v1TxBuf[idx++] = v & 0xFF;
			if (idx >= sizeof(s_v1TxBuf)) flush_v1();
		}
	}
	flush_v1();
}

/* Sends the framebuffer (buffered bulk writes, big-endian v1 wire format) */
void sendFramebuffer()
{
	size_t idx = 0;
	for (uint32_t i = 0; i < 76800; i++)
	{
		uint16_t v = bigBuffer[i];
		s_v1TxBuf[idx++] = (v >> 8) & 0xFF;
		s_v1TxBuf[idx++] = v & 0xFF;
		if (idx >= sizeof(s_v1TxBuf)) {
			Serial.write(s_v1TxBuf, idx);
			idx = 0;
		}
	}
	if (idx > 0) Serial.write(s_v1TxBuf, idx);
}

/* Sends the configuration data */
void sendConfigData()
{
	//Lepton Version
	Serial.write(leptonVersion);
	//Rotation
	Serial.write(rotationVert);
	//Send color scheme
	Serial.write(colorScheme);
	//Send the temperature format
	Serial.write(tempFormat);
	//Send the show spot attribute
	Serial.write(spotEnabled);
	//Send the show colorbar attribute
	Serial.write(colorbarEnabled);
	//Send the show hottest / coldest attribute
	Serial.write(minMaxPoints);
	//Send the text color
	Serial.write(textColor);
	//Send the filter type
	Serial.write(filterType);
	//Send adjust limits allowed
	Serial.write((autoMode) && (!limitsLocked));
}


/* Sends the calibration data */
void sendCalibrationData()
{
	uint8_t farray[4];

	//Send the calibration offset first
	float calOffset = -273.15;
	floatToBytes(farray, (float)calOffset);
	for (int i = 0; i < 4; i++)
		Serial.write(farray[i]);
	//Send the calibration slope
	floatToBytes(farray, (float)leptonCalSlope);
	for (int i = 0; i < 4; i++)
		Serial.write(farray[i]);
}

/* Sends the spot temp*/
void sendSpotTemp()
{
	//Array to store the byte-converted float value
	uint8_t farray[4];

	//Convert float to bytes
	floatToBytes(farray, spotTemp);

	//Send the four bytes out
	for (int i = 0; i < 4; i++)
		Serial.write(farray[i]);
}

/* Send the temperature points */
void sendTempPoints()
{
	for (byte i = 0; i < 96; i++)
	{
		//Send index value
		Serial.write((tempPoints[i][0] & 0xFF00) >> 8);
		Serial.write(tempPoints[i][0] & 0x00FF);
		//Send raw value
		Serial.write((tempPoints[i][1] & 0xFF00) >> 8);
		Serial.write(tempPoints[i][1] & 0x00FF);
	}
}

/* Send the battery status in percentage */
void sendBatteryStatus()
{
	Serial.write(batPercentage);
}

/* Send the current firmware version */
void sendFWVersion()
{
	Serial.write((fwVersion & 0xFF00) >> 8);
	Serial.write(fwVersion & 0x00FF);
}

/* Set the temperature limits */
void setLimits()
{
	//If not enough data available, leave
	if (!serialWait(1))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Read byte from serial port
	byte read = Serial.read();

	//Check if it has a valid number
	if ((read >= 0) && (read <= 1))
	{
		//Lock limits
		if (read == 0)
			limitsLocked = true;

		//Auto mode
		else
		{
			//Enable auto mode
			autoMode = true;
			//Disable limits locked
			limitsLocked = false;
		}
	}
	//Send invalid
	else
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Send ACK
	Serial.write(CMD_SET_LIMITS);
}

/* Set the text color */
void setTextColor()
{
	//If not enough data available, leave
	if (!serialWait(1))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Read byte from serial port
	byte read = Serial.read();

	//Check if read result is valid
	if ((read >= textColor_white) && (read <= textColor_blue))
	{
		//Set text color to input
		textColor = read;
		//Change it
		changeTextColor();
		//Save to EEPROM
		EEPROM.write(eeprom_textColor, textColor);
	}
	//Send invalid
	else
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Send ACK
	Serial.write(CMD_SET_TEXTCOLOR);
}

/* Set the color scheme */
void setColorScheme()
{
	//If not enough data available, leave
	if (!serialWait(1))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Read byte from serial port
	byte read = Serial.read();

	//Check if it has a valid number
	if ((read >= 0) && (read <= (colorSchemeTotal - 1)))
	{
		//Set color scheme to input
		colorScheme = read;
		//Select right color scheme
		selectColorScheme();
		//Save to EEPROM
		EEPROM.write(eeprom_colorScheme, colorScheme);
	}
	//Send invalid
	else
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Send ACK
	Serial.write(CMD_SET_COLORSCHEME);
}

/* Set the temperature format */
void setTempFormat()
{
	//If not enough data available, leave
	if (!serialWait(1))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Read byte from serial port
	byte read = Serial.read();

	//Check if it has a valid number
	if ((read >= 0) && (read <= 1))
	{
		//Set temperature format to input
		tempFormat = read;
		//Save to EEPROM
		EEPROM.write(eeprom_tempFormat, tempFormat);
	}
	//Send invalid
	else
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Send ACK
	Serial.write(CMD_SET_TEMPFORMAT);
}

/* Set the show spot information */
void setShowSpot()
{
	//If not enough data available, leave
	if (!serialWait(1))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Read byte from serial port
	byte read = Serial.read();

	//Check if it has a valid number
	if ((read >= 0) && (read <= 1))
	{
		//Set show spot to input
		spotEnabled = read;
		//Save to EEPROM
		EEPROM.write(eeprom_spotEnabled, spotEnabled);
	}
	//Send invalid
	else
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Send ACK
	Serial.write(CMD_SET_SHOWSPOT);
}

/* Set the show colorbar information */
void setShowColorbar()
{
	//If not enough data available, leave
	if (!serialWait(1))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Read byte from serial port
	byte read = Serial.read();

	//Check if it has a valid number
	if ((read >= 0) && (read <= 1))
	{
		//Set show colorbar to input
		colorbarEnabled = read;
		//Save to EEPROM
		EEPROM.write(eeprom_colorbarEnabled, colorbarEnabled);
	}
	//Send invalid
	else
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Send ACK
	Serial.write(CMD_SET_SHOWCOLORBAR);
}

/* Set the show colorbar information */
void setMinMax()
{
	//If not enough data available, leave
	if (!serialWait(1))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Read byte from serial port
	byte read = Serial.read();

	//Check if it has a valid number
	if ((read >= minMaxPoints_disabled) && (read <= minMaxPoints_both))
	{
		//Set show colorbar to input
		minMaxPoints = read;
		//Save to EEPROM
		EEPROM.write(eeprom_minMaxPoints, minMaxPoints);
	}
	//Send invalid
	else
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Send ACK
	Serial.write(CMD_SET_SHOWMINMAX);
}

/* Set the shutter mode */
void setShutterMode()
{
	//If not enough data available, leave
	if (!serialWait(1))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Read byte from serial port
	byte read = Serial.read();

	//Check if it has a valid number
	if ((read >= 0) && (read <= 1))
		//Set lepton shutter mode
		lepton_ffcMode(read);
	//Send invalid
	else
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Send ACK
	Serial.write(CMD_SET_SHUTTERMODE);
}

/* Set the fitler type */
void setFilterType()
{
	//If not enough data available, leave
	if (!serialWait(1))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Read byte from serial port
	byte read = Serial.read();

	//Check if it has a valid number
	if ((read >= filterType_none) && (read <= filterType_box))
	{
		//Set filter type to input
		filterType = read;
		//Save to EEPROM
		EEPROM.write(eeprom_filterType, filterType);
	}
	//Send invalid
	else
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Send ACK
	Serial.write(CMD_SET_FILTERTYPE);
}

/* Set the rotation */
void setRotation()
{
	//If not enough data available, leave
	if (!serialWait(1))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Read byte from serial port
	byte read = Serial.read();

	//Check if it has a valid number
	if ((read >= 0) && (read <= 1))
	{
		//Set rotation to input
		rotationVert = read;
		//Apply to display
		setDisplayRotation();
		//Save to EEPROM
		EEPROM.write(eeprom_rotationVert, rotationVert);
	}
	//Send invalid
	else
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Send ACK
	Serial.write(CMD_SET_ROTATION);
}

/* Send the hardware version */
void sendHardwareVersion()
{
	//Send hardware version
	Serial.write(2);
}

/* Send the diagnostic information */
void sendDiagnostic()
{
	//Send the diag byte
	Serial.write(diagnostic);
}

/* Set temperature points array */
void setTempPoints()
{
	//If not enough data available, leave
	if (!serialWait(384))
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Go through the temp points array
	for (byte i = 0; i < 96; i++)
	{
		//Read index
		tempPoints[i][0] = serialRead16();

		//Correct old not_set marker
		if (tempPoints[i][0] == 65535)
			tempPoints[i][0] = 0;

		//Read value
		tempPoints[i][1] = serialRead16();
	}

	//Refresh live sensor values for the newly set points & force next raw_frame() refresh
	refreshTempPoints();
	s_v1FrameConsumed = true;

	//Send ACK
	Serial.write(CMD_SET_TEMPPOINTS);
}

static void v1EnsureFreshFrame()
{
	if (s_v1FrameConsumed) {
		lepton_acquireFrame(true);
		getSpotTemp();
		refreshTempPoints();
		if (autoMode && !limitsLocked) limitValues();
		else if (minMaxPoints != minMaxPoints_disabled) refreshMinMax();
	}
	s_v1FrameConsumed = true;
}

/* Sends a raw or color frame */
void sendFrame(bool color)
{
	//Ensure a fresh frame is ready before answering
	if (sendCmd == FRAME_NORMAL)
		v1EnsureFreshFrame();

	//Send type of frame response
	Serial.write(sendCmd);

	//Send frame
	if (sendCmd == FRAME_NORMAL)
	{
		//Convert to colors
		if (color)
		{
			//Apply low-pass filter
			if (filterType == filterType_box)
				boxFilter();
			else if (filterType == filterType_gaussian)
				gaussianFilter();
			//Convert to RGB565
			convertColors(true);
		}
		//Send raw data
		sendRawData(color);

		//Send limits
		sendRawLimits();
		//Send spot temp
		sendSpotTemp();
		//Send calibration data
		sendCalibrationData();
	}
	//Switch back to send frame the next time
	else
		sendCmd = FRAME_NORMAL;
}

/* Saves a frame to the internal sd card*/
void saveFrame()
{
	if (getSDSpace() < 1000)
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Build save filename from the current time & date
	createSDName(saveFilename);

	//Enable image save marker
	imgSave = imgSave_create;

	//Create image and save raw file (buffered 8 KB writes!)
	createThermalImg();

	//Save Bitmap image if activated
	if (convertEnabled)
	{
		displayInfos();
		saveBuffer(saveFilename);
	}

	//Disable image save marker
	imgSave = imgSave_disabled;

	//Send ACK
	Serial.write(CMD_FRAME_SAVE);
	Serial.send_now();
}

/* Sends the display content as frame */
void sendDisplayFrame()
{
	if (sendCmd == FRAME_NORMAL)
		v1EnsureFreshFrame();

	//Send type of frame response
	Serial.write(sendCmd);

	//Send frame
	if (sendCmd == FRAME_NORMAL)
	{
		//Apply low-pass filter
		if (filterType == filterType_box)
			boxFilter();
		else if (filterType == filterType_gaussian)
			gaussianFilter();

		//Resize to big buffer 
		smallToBigBuffer();

		//Convert lepton data to RGB565 colors
		convertColors();

		//Display additional information
		imgSave = imgSave_create;
		displayInfos();
		imgSave = imgSave_disabled;

		//Send the framebuffer
		sendFramebuffer();
	}

	//Switch back to send frame the next time
	else
		sendCmd = FRAME_NORMAL;
}

/* Evaluates commands from the serial port*/
bool serialHandler()
{
	//Read command from Serial Port
	byte recCmd = Serial.read();

	//Decide what to do
	switch (recCmd)
	{
		//Send raw limits
	case CMD_GET_RAWLIMITS:
		sendRawLimits();
		break;
		//Send raw data
	case CMD_GET_RAWDATA:
		sendRawData();
		break;
		//Send config data
	case CMD_GET_CONFIGDATA:
		sendConfigData();
		break;
		//Send calibration data
	case CMD_GET_CALIBDATA:
		sendCalibrationData();
		break;
		//Send spot temp
	case CMD_GET_SPOTTEMP:
		sendSpotTemp();
		break;
		//Send temperature points
	case CMD_GET_TEMPPOINTS:
		sendTempPoints();
		break;
		//Run the shutter
	case CMD_SET_SHUTTERRUN:
		lepton_ffc();
		//Send ACK
		Serial.write(CMD_SET_SHUTTERRUN);
		break;
		//Set shutter mode
	case CMD_SET_SHUTTERMODE:
		setShutterMode();
		break;
		//Set the filter type
	case CMD_SET_FILTERTYPE:
		setFilterType();
		break;
		//Send battery status
	case CMD_GET_BATTERYSTATUS:
		sendBatteryStatus();
		break;
		//Send firmware version
	case CMD_GET_FWVERSION:
		sendFWVersion();
		break;
		//Set limits
	case CMD_SET_LIMITS:
		setLimits();
		break;
		//Set limits to locked
	case CMD_SET_TEXTCOLOR:
		setTextColor();
		break;
		//Change colorscheme
	case CMD_SET_COLORSCHEME:
		setColorScheme();
		break;
		//Set temperature format
	case CMD_SET_TEMPFORMAT:
		setTempFormat();
		break;
		//Set show spot temp
	case CMD_SET_SHOWSPOT:
		setShowSpot();
		break;
		//Set show color bar
	case CMD_SET_SHOWCOLORBAR:
		setShowColorbar();
		break;
		//Set show min max
	case CMD_SET_SHOWMINMAX:
		setMinMax();
		break;
		//Set temperature points
	case CMD_SET_TEMPPOINTS:
		setTempPoints();
		break;
		//Get hardware version
	case CMD_GET_HWVERSION:
		sendHardwareVersion();
		break;
		//Set rotation
	case CMD_SET_ROTATION:
		setRotation();
		break;
		//Get diagnostic information
	case CMD_GET_DIAGNOSTIC:
		sendDiagnostic();
		break;
		//Send raw frame
	case CMD_FRAME_RAW:
		sendFrame(false);
		break;
		//Send color frame
	case CMD_FRAME_COLOR:
		sendFrame(true);
		break;
		//Send display frame
	case CMD_FRAME_DISPLAY:
		sendDisplayFrame();
		break;
		//Save display frame
	case CMD_FRAME_SAVE:
		saveFrame();
		break;
		//End connection
	case CMD_END:
		return true;
		//Start connection, send ACK
	case CMD_START:
		Serial.write(CMD_START);
		break;
		//Invalid command
	default:
		Serial.write(CMD_INVALID);
		break;
	}
	Serial.flush();
	return false;
}

/* Evaluate button presses */
void buttonHandler()
{
	//Count the time to choose selection
	long startTime = millis();
	delay(10);
	long endTime = millis() - startTime;

	//As long as the button is pressed
	while (extButtonPressed() && (endTime <= 1000))
		endTime = millis() - startTime;

	//Short press - request to save a thermal image
	if (endTime < 1000)
	{
		sendCmd = FRAME_CAPTURE_THERMAL;
	}

	//Long press - request to start or stop a video
	else
	{
		sendCmd = FRAME_CAPTURE_VIDEO;
		//Wait until button release
		while (extButtonPressed())
			;
	}
}

/* Evaluate touch presses */
bool touchHandler()
{
	//Count the time to choose selection
	long startTime = millis();
	delay(10);
	long endTime = millis() - startTime;

	//Wait for touch release, but not longer than a second
	if (touch_capacitive)
	{
		while ((touch_touched()) && (endTime <= 1000))
			endTime = millis() - startTime;
	}
	else
	{
		while ((!digitalRead(pin_touch_irq)) && (endTime <= 1000))
			endTime = millis() - startTime;
	}

	return true;
}

/* Check for serial connection: auto-detect Protocol v2 ("TC2\0") vs temporary Phase 3 legacy v1 (0x64) */
void checkSerial()
{
	checkSerialV2OrV1(false);
}

/* Go into video output mode and wait for connected module (non-blocking double-buffered!) */
void serialOutput()
{
	lepton_startFrame();
	s_v1FrameConsumed = true;

	while (true)
	{
		//Check for serial commands IMMEDIATELY (<1 ms latency!)
		if (Serial.available() > 0)
		{
			if (serialHandler())
				break;
		}

		//Abort transmission when touched long
		if (checkDiagnostic(diag_touch) && touch_touched())
			if (touchHandler())
				break;

		//Acquire newly completed Lepton frame asynchronously (never blocks commands!)
		if (lepton_hasNewFrame())
		{
			lepton_acquireFrame(false);
			getSpotTemp();
			refreshTempPoints();
			if ((autoMode) && (!limitsLocked))
				limitValues();
			else if (minMaxPoints != minMaxPoints_disabled)
				refreshMinMax();
			s_v1FrameConsumed = false;
		}

		//Check button press
		if (extButtonPressed())
			buttonHandler();
	}
}

/* Method to init some basic values in case no display is used (headless v2 + v1 auto-detect) */
void serialInit()
{
	readEEPROM();
	selectColorScheme();
	clearTempPoints();
	lepton_startFrame();

	while (true)
	{
		checkSerialV2OrV1(true);
		if (lepton_hasNewFrame()) {
			lepton_acquireFrame(false);
		}
	}
}

/* Tries to establish a connection to a thermal viewer or video output module*/
void serialConnect()
{
	//Show message
	showFullMessage((char *)"Serial connection detected");
	display_print((char *)"Touch screen long to return", CENTER, 170);
	delay(1000);

	//Disable screen backlight
	disableScreenLight();

	//Send ACK for Start
	Serial.write(CMD_START);

	//Go to the serial output
	serialOutput();

	//Send ACK for End
	Serial.write(CMD_END);

	//Re-Enable display backlight
	enableScreenLight();

	//Show message
	showFullMessage((char *)"Connection ended, return..");
	delay(250);
}
