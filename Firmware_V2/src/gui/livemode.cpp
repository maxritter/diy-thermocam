/*
*
* LIVE MODE - GUI functions used in the live mode
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
#include <display.h>
#include <battery.h>
#include <Time.h>
#include <calibration.h>
#include <thermal.h>
#include <gui.h>
#include <create.h>
#include <fonts.h>
#include <EEPROM.h>
#include <livemode.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Display battery status in percentage */
void displayBatteryStatus() {
	//Check battery status every 60 seconds
	if (batTimer == 0)
		batTimer = millis();
	if ((millis() - batTimer) > 60000) {
		checkBattery();
		batTimer = millis();
	}

	//USB Power only
	if (batPercentage == -1)
		display_print((char*) "USB Power", 225, 0);

	//Low Battery
	else if (batPercentage == 0)
		display_print((char*) "LOW", 270, 0);

	//Display battery status in percentage
	else {
		//Charging, show plus symbol
		if ((analogRead(pin_usb_measure) > 50) && (batPercentage != 100))
		{
			display_printNumI(batPercentage, 270, 0, 3, ' ');
			display_print((char*) "%", 300, 0);
			display_print((char*) "+", 310, 0);
		}

		//Not charging
		else
		{
			display_printNumI(batPercentage, 280, 0, 3, ' ');
			display_print((char*) "%", 310, 0);
		}
	}
}

/* Display the current time on the screen*/
void displayTime() {
	//Tiny font
	if ((teensyVersion == teensyVersion_old) || (!hqRes)) {
		display_printNumI(hour(), 5, 228, 2, '0');
		display_print((char*) ":", 23, 228);
		display_printNumI(minute(), 27, 228, 2, '0');
		display_print((char*) ":", 45, 228);
		display_printNumI(second(), 49, 228, 2, '0');
	}

	//Small font
	else
	{
		display_printNumI(hour(), 5, 228, 2, '0');
		display_print((char*) ":", 20, 228);
		display_printNumI(minute(), 27, 228, 2, '0');
		display_print((char*) ":", 42, 228);
		display_printNumI(second(), 49, 228, 2, '0');
	}
}

/* Display the date on screen */
void displayDate() {
	//Tiny font
	if ((teensyVersion == teensyVersion_old) || (!hqRes)) {
		display_printNumI(day(), 5, 0, 2, '0');
		display_print((char*) ".", 23, 0);
		display_printNumI(month(), 27, 0, 2, '0');
		display_print((char*) ".", 45, 0);
		display_printNumI(year(), 49, 0, 4);
	}

	//Small font
	else
	{
		display_printNumI(day(), 5, 0, 2, '0');
		display_print((char*) ".", 20, 0);
		display_printNumI(month(), 27, 0, 2, '0');
		display_print((char*) ".", 42, 0);
		display_printNumI(year(), 49, 0, 4);
	}
}

/* Display the warmup message on screen*/
void displayWarmup() {
	char buffer[25];

	//Create string
	sprintf(buffer, "Sensor warmup, %2ds left", (int)abs(30 - ((millis() - calTimer) / 1000)));
	//Tinyfont
	if ((teensyVersion == teensyVersion_old) || (!hqRes))
		display_print(buffer, 45, 200);
	//Smallfont
	else
		display_print(buffer, 65, 200);
}

/* Display the current temperature mode on top*/
void displayTempMode()
{
	char buffer[10];

	//Warmup
	if (calStatus == cal_warmup)
		sprintf(buffer, " WARMUP");
	//Limits locked
	else if (limitsLocked)
		sprintf(buffer, " LOCKED");
	//Auto mode
	else if (autoMode)
		sprintf(buffer, "  AUTO");
	//Temperature presets
	else
	{
		//Check which preset is active
		byte minMaxPreset;
		byte read = EEPROM.read(eeprom_minMaxPreset);
		if ((read >= minMax_preset1) && (read <= minMax_preset3))
			minMaxPreset = read;
		else
			minMaxPreset = minMax_temporary;
		//Choose corresponding text
		if ((minMaxPreset == minMax_preset1) && (EEPROM.read(eeprom_minMax1Set) == eeprom_setValue))
			sprintf(buffer, "PRESET 1");
		else if ((minMaxPreset == minMax_preset2) && (EEPROM.read(eeprom_minMax2Set) == eeprom_setValue))
			sprintf(buffer, "PRESET 2");
		else if ((minMaxPreset == minMax_preset3) && (EEPROM.read(eeprom_minMax3Set) == eeprom_setValue))
			sprintf(buffer, "PRESET 3");
		else
			sprintf(buffer, " MANUAL");
	}

	//Low resolution display
	if ((teensyVersion == teensyVersion_old) || (!hqRes))
		display_print(buffer, 120, 0);
	//HQ resolution display
	else
		display_print(buffer, 140, 0);
}

/* Display the minimum and maximum point on the screen */
void displayMinMaxPoint(bool min)
{
	int16_t xpos, ypos;

	//Calculate x and y position
	if (min)
		calculatePointPos(&xpos, &ypos, minTempPos);
	else
		calculatePointPos(&xpos, &ypos, maxTempPos);

	//Draw the marker
	display_drawLine(xpos, ypos, xpos, ypos);

	//Warmup completed, show absolute temp
	if (calStatus != cal_warmup)
	{
		//Calc x position for the text
		xpos -= 20;
		if (xpos < 0)
			xpos = 0;
		if (xpos > 279)
			xpos = 279;

		//Calc y position for the text
		ypos += 15;
		if (ypos > 229)
			ypos = 229;

		//Show min or max value as absolute temperature
		if (min)
			display_printNumF(calFunction(minTempVal), 2, xpos, ypos);
		else
			display_printNumF(calFunction(maxTempVal), 2, xpos, ypos);
	}

	//Warmup, show C / H
	else
	{
		//Calc x and y position
		xpos += 4;
		if (xpos >= 310)
			xpos -= 10;
		if (ypos > 230)
			ypos = 230;

		//Show min or max value as symbol
		if (min)
			display_print('C', xpos, ypos);
		else
			display_print('H', xpos, ypos);
	}
}

/* Display free space on screen*/
void displayFreeSpace() {
	//Tinyfont
	if ((teensyVersion == teensyVersion_old) || (!hqRes))
		display_print(sdInfo, 197, 228);
	//Smallfont
	else
		display_print(sdInfo, 220, 228);
}

/* Show the current spot temperature on screen*/
void showSpot() {
	char buffer[10];

	//Draw the spot circle
	display_drawCircle(160, 120, 12);

	//Draw the lines
	display_drawLine(136, 120, 148, 120);
	display_drawLine(172, 120, 184, 120);
	display_drawLine(160, 96, 160, 108);
	display_drawLine(160, 132, 160, 144);

	//Convert spot temperature to char array
	floatToChar(buffer, spotTemp);

	//Print value on display
	display_print(buffer, 145, 150);
}

/* Display addition information on the screen */
void displayInfos() {
	//Set text color
	changeTextColor();
	//Set font and background
	display_setBackColor(VGA_TRANSPARENT);

	//For Teensy 3.6, set small font
	if ((teensyVersion == teensyVersion_new) && (hqRes))
		display_setFont((uint8_t*) smallFont);
	//For Teensy 3.1 / 3.2, set tiny font
	else
		display_setFont((uint8_t*) tinyFont);

	//Set write to image, not display
	display_writeToImage = true;

	//Check warmup
	checkWarmup();

	//If  not saving image or video
	if ((imgSave != imgSave_create) && (!videoSave)) {
		//Show battery status in percantage
		if (batteryEnabled)
			displayBatteryStatus();
		//Show the time
		if (timeEnabled)
			displayTime();
		//Show the date
		if (dateEnabled)
			displayDate();
		//Show storage information
		if (storageEnabled)
			displayFreeSpace();
		//Display warmup if required
		if (calStatus == cal_warmup)
			displayWarmup();
		//Display temperature mode
		displayTempMode();
	}

	//Show the minimum / maximum points
	if (minMaxPoints & minMaxPoints_min)
		displayMinMaxPoint(true);
	if (minMaxPoints & minMaxPoints_max)
		displayMinMaxPoint(false);

	//Show the spot in the middle
	if (spotEnabled)
		showSpot();

	//Show the color bar when warmup is over and if enabled, not in visual mode
	if ((colorbarEnabled) && (calStatus != cal_warmup) && (displayMode != displayMode_visual))
		showColorBar();

	//Show the temperature points
	showTemperatures();

	//Set write back to display
	display_writeToImage = false;
}
