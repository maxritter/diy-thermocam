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
* https://github.com/maxritter/DIY-Thermocam
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
#include <connection.h>

/*######################### STATIC DATA DECLARATIONS ##########################*/

//Command, default send frame
static byte sendCmd = FRAME_NORMAL;

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

/* Send the lepton raw data*/
void sendRawData(bool color)
{
	//For the Lepton2.5 sensor, write 4800 raw values
	if ((leptonVersion == leptonVersion_2_5_shutter) && (!color))
	{
		for (int line = 0; line < 60; line++)
		{
			for (int column = 0; column < 80; column++)
			{
				uint16_t result = smallBuffer[(line * 2 * 160) + (column * 2)];
				Serial.write((result & 0xFF00) >> 8);
				Serial.write(result & 0x00FF);
			}
		}
	}
	//For the Lepton3.5 sensor, write 19200 raw values
	else
	{
		for (int i = 0; i < 19200; i++)
		{
			Serial.write((smallBuffer[i] & 0xFF00) >> 8);
			Serial.write(smallBuffer[i] & 0x00FF);
		}
	}
}

/* Sends the framebuffer */
void sendFramebuffer()
{
	for (uint32_t i = 0; i < 76800; i++)
	{
		Serial.write((bigBuffer[i] & 0xFF00) >> 8);
		Serial.write(bigBuffer[i] & 0x00FF);
	}
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

/* Sets the time */
void setTime()
{
	//Wait for time string, maximum 1 second
	uint32_t timer = millis();
	while (!Serial.available() && ((millis() - timer) < 1000))
		;

	//If there was no timestring
	if (Serial.available() == 0)
	{
		//Send ACK and return
		Serial.write(CMD_SET_TIME);
		return;
	}

	//Read time
	String dateIn = Serial.readString();

	//Check if valid
	if (getInt(dateIn.substring(0, 4) >= 2021))
	{
		//Set the clock
		setTime(getInt(dateIn.substring(11, 13)), getInt(dateIn.substring(14, 16)), getInt(dateIn.substring(17, 19)),
				getInt(dateIn.substring(8, 10)), getInt(dateIn.substring(5, 7)), getInt(dateIn.substring(0, 4)));
		//Set the RTC
		Teensy3Clock.set(now());
	}

	//Send ACK
	Serial.write(CMD_SET_TIME);
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
	if (Serial.available() < 1)
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
	if (Serial.available() < 1)
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
	if (Serial.available() < 1)
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
	if (Serial.available() < 1)
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
	if (Serial.available() < 1)
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
	if (Serial.available() < 1)
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
	if (Serial.available() < 1)
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
	if (Serial.available() < 1)
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
	if (Serial.available() < 1)
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
	if (Serial.available() < 1)
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

/* Send the HQ Resolution information */
void sendHQResolution()
{
	Serial.write(1);
}

/* Set temperature points array */
void setTempPoints()
{
	//If not enough data available, leave
	if (Serial.available() < 384)
	{
		Serial.write(CMD_INVALID);
		return;
	}

	//Go through the temp points array
	for (byte i = 0; i < 96; i++)
	{
		//Read index
		tempPoints[i][0] = (Serial.read() << 8) + Serial.read();

		//Correct old not_set marker
		if (tempPoints[i][0] == 65535)
			tempPoints[i][0] = 0;

		//Read value
		tempPoints[i][1] = (Serial.read() << 8) + Serial.read();
	}

	//Send ACK
	Serial.write(CMD_SET_TEMPPOINTS);
}

/* Sends a raw or color frame */
void sendFrame(bool color)
{
	//Send type of frame response
	Serial.write(sendCmd);
	Serial.flush();

	//Send frame
	if (sendCmd == FRAME_NORMAL)
	{
		//Clear all serial buffers
		Serial.clear();
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

	//Create image and save raw file
	lepton_startFrame();
	createThermalImg();

	//Save Bitmap image if activated
	if (convertEnabled)
	{
		displayInfos();
		saveBuffer(saveFilename);
	}

	//Refresh free space
	refreshFreeSpace();

	//Disable image save marker
	imgSave = imgSave_disabled;

	//Send ACK
	Serial.write(CMD_FRAME_SAVE);
}

/* Sends the display content as frame */
void sendDisplayFrame()
{
	//Send type of frame response
	Serial.write(sendCmd);
	Serial.flush();

	//Send frame
	if (sendCmd == FRAME_NORMAL)
	{
		//Find min / max position
		if (minMaxPoints != minMaxPoints_disabled)
			refreshMinMax();

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
		//Send the calibration status
	case CMD_GET_CALSTATUS:
		Serial.write(CMD_INVALID);
		break;
		//Send calibration data
	case CMD_GET_CALIBDATA:
		sendCalibrationData();
		break;
		//Send spot temp
	case CMD_GET_SPOTTEMP:
		sendSpotTemp();
		break;
		//Change time
	case CMD_SET_TIME:
		setTime();
		break;
		//Send temperature points
	case CMD_GET_TEMPPOINTS:
		sendTempPoints();
		break;
		//Toggle laser
	case CMD_SET_LASER:
		Serial.write(CMD_INVALID);
		break;
		//Send laser state
	case CMD_GET_LASER:
		Serial.write(CMD_INVALID);
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
		//Get the shutter mode
	case CMD_GET_SHUTTERMODE:
		Serial.write(CMD_INVALID);
		break;
		//Send battery status
	case CMD_GET_BATTERYSTATUS:
		sendBatteryStatus();
		break;
		//Set calibration offset
	case CMD_SET_CALOFFSET:
		Serial.write(CMD_INVALID);
		break;
		//Set calibration slope
	case CMD_SET_CALSLOPE:
		Serial.write(CMD_INVALID);
		break;
		//Send visual image
	case CMD_GET_VISUALIMG:
		Serial.write(CMD_INVALID);
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
		//Run calibration
	case CMD_SET_CALIBRATION:
		Serial.write(CMD_INVALID);
		break;
		//Get diagnostic information
	case CMD_GET_DIAGNOSTIC:
		sendDiagnostic();
		break;
		//Get HQ resolution information
	case CMD_GET_HQRESOLUTION:
		sendHQResolution();
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
	endTime = millis() - startTime;

	//Short press - take visual image
	if (endTime < 1000)
	{
		sendCmd = FRAME_CAPTURE_VISUAL;
		return false;
	}

	//Long press
	return true;
}

/* Check for serial connection */
void checkSerial()
{
	//If start command received
	if ((Serial.available() > 0) && (Serial.read() == CMD_START))
	{
		serialMode = true;
		serialConnect();
		serialMode = false;
		lepton_startFrame();
	}

	//Another command received, discard it
	else if ((Serial.available() > 0))
		Serial.read();
}

/* Check for updater requests */
void checkForUpdater()
{
	//We received something
	if (Serial.available() > 0)
	{
		//Read command from Serial Port
		byte recCmd = Serial.read();
		//Decide what to do
		switch (recCmd)
		{
			//Send firmware version
		case CMD_GET_FWVERSION:
			sendFWVersion();
			break;
			//Get hardware version
		case CMD_GET_HWVERSION:
			sendHardwareVersion();
			break;
			//Get diagnostic information
		case CMD_GET_DIAGNOSTIC:
			sendDiagnostic();
			break;
			//Get HQ resolution information
		case CMD_GET_HQRESOLUTION:
			sendHQResolution();
			break;
			//Send the calibration status
		case CMD_GET_CALSTATUS:
			Serial.write(CMD_INVALID);
			break;
			//Send battery status
		case CMD_GET_BATTERYSTATUS:
			sendBatteryStatus();
			break;

			//Start connection, send ACK
		case CMD_START:
			Serial.write(CMD_START);
			break;
		}
		Serial.flush();
	}
}

/* Go into video output mode and wait for connected module */
void serialOutput()
{
	//Send the frames
	while (true)
	{

		//Abort transmission when touched long or save visual when short
		if (touch_touched() && checkDiagnostic(diag_touch))
			if (touchHandler())
				break;

		//Get the temps
		lepton_startFrame();
		lepton_getFrame();

		//Get the spot temperature
		getSpotTemp();

		//Refresh the temp points
		refreshTempPoints();

		//Find min and max if not in manual mode and limits not locked
		if ((autoMode) && (!limitsLocked))
			limitValues();

		//Check button press if not in terminal mode
		if (extButtonPressed())
			buttonHandler();

		//Check for serial commands
		if (Serial.available() > 0)
		{
			//Check for exit
			if (serialHandler())
				break;
		}
	}
}

/* Method to init some basic values in case no display is used */
void serialInit()
{
	//Read all settings from EEPROM
	readEEPROM();

	//Select color scheme
	selectColorScheme();

	//Clear show temp array
	clearTempPoints();

	//Receive and send commands over serial port
	while (true)
		serialOutput();
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
	delay(1000);

	//Clear all serial buffers
	Serial.clear();
}
