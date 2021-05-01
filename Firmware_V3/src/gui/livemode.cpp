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
#include <temperature.h>
#include <TimeLib.h>
#include <hardware.h>
#include <thermal.h>
#include <gui.h>
#include <create.h>
#include <fonts.h>
#include <EEPROM.h>
#include <livemode.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Display battery status in percentage */
void displayBatteryStatus()
{
	//Check battery status every 60 seconds
	if (batTimer == 0)
		batTimer = millis();
	if ((millis() - batTimer) > 60000)
	{
		checkBattery();
		batTimer = millis();
	}

	//USB Power only
	if (batPercentage == -1)
		display_print((char *)"USB Power", 225, 0);

	//Low Battery
	else if (batPercentage == 0)
		display_print((char *)"LOW", 270, 0);

	//Display battery status in percentage
	else
	{
		//Charging, show plus symbol
		if (isUSBConnected() && (batPercentage != 100))
		{
			display_printNumI(batPercentage, 270, 0, 3, ' ');
			display_print((char *)"%", 300, 0);
			display_print((char *)"+", 310, 0);
		}

		//Not charging
		else
		{
			display_printNumI(batPercentage, 280, 0, 3, ' ');
			display_print((char *)"%", 310, 0);
		}
	}
}

/* Display the current time on the screen*/
void displayTime()
{
	display_printNumI(hour(), 5, 228, 2, '0');
	display_print((char *)":", 20, 228);
	display_printNumI(minute(), 27, 228, 2, '0');
	display_print((char *)":", 42, 228);
	display_printNumI(second(), 49, 228, 2, '0');
}

/* Display the date on screen */
void displayDate()
{
	display_printNumI(day(), 5, 0, 2, '0');
	display_print((char *)".", 20, 0);
	display_printNumI(month(), 27, 0, 2, '0');
	display_print((char *)".", 42, 0);
	display_printNumI(year(), 49, 0, 4);
}

/* Display the current temperature mode on top*/
void displayTempMode()
{
	char buffer[10];

	if (limitsLocked)
		sprintf(buffer, " LOCKED");
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
		display_printNumF(rawToTemp(minTempVal), 2, xpos, ypos);
	else
		display_printNumF(rawToTemp(maxTempVal), 2, xpos, ypos);
}

/* Display free space on screen*/
void displayFreeSpace()
{
	display_print(sdInfo, 205, 228);
}

/* Show the current spot temperature on screen*/
void showSpot()
{
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
void displayInfos()
{
	//Set text color
	changeTextColor();
	//Set font and background
	display_setBackColor(VGA_TRANSPARENT);
	display_setFont((uint8_t *)smallFont);

	//Set write to image, not display
	display_writeToImage = true;

	//If  not saving image or video
	if ((imgSave != imgSave_create) && (!videoSave))
	{
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

	//Show the color bar
	if (colorbarEnabled)
		showColorBar();

	//Show the temperature points
	showTemperatures();

	//Set write back to display
	display_writeToImage = false;
}
