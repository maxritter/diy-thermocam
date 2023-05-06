/*
*
* FIRST START - Menu that is displayed on the first device start
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
#include <EEPROM.h>
#include <display.h>
#include <fonts.h>
#include <touchscreen.h>
#include <colorschemes.h>
#include <settingsmenu.h>
#include <TimeLib.h>
#include <sdcard.h>
#include <hardware.h>
#include <gui.h>
#include <connection.h>
#include <firststart.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Check if the first start needs to be done */
boolean checkFirstStart()
{
	return EEPROM.read(eeprom_firstStart) != eeprom_setValue;
}

/* Show welcome Screen for the first start procedure */
void welcomeScreen()
{
	//Background & Title
	display_fillScr(200, 200, 200);
	display_setBackColor(200, 200, 200);
	display_setFont(smallFont);
	display_printC("Welcome to the", CENTER, 20);
	display_setFont(bigFont);
	display_printC("DIY-Thermocam V3", CENTER, 60, VGA_BLUE);

	//Explanation
	display_setFont(smallFont);
	display_printC("This is the first time setup.", CENTER, 110);
	display_printC("It will guide you through the", CENTER, 140);
	display_printC("basic settings for your device.", CENTER, 170);
	display_printC("-> Please touch screen <-", CENTER, 210, VGA_BLUE);

	//Wait for touch press
	while (!touch_touched())
		;

	//Touch release again
	while (touch_touched())
		;
}

/* Shows an info screen during the first start procedure */
void infoScreen(String *text, bool cont)
{
	//Background & Title
	display_fillScr(200, 200, 200);
	display_setBackColor(200, 200, 200);
	display_setFont(bigFont);
	display_printC(text[0], CENTER, 20, VGA_BLUE);

	//Content
	display_setFont(smallFont);
	display_printC(text[1], CENTER, 55);
	display_printC(text[2], CENTER, 80);
	display_printC(text[3], CENTER, 105);
	display_printC(text[4], CENTER, 130);

	//Show hint to touch the screen
	if (cont)
	{
		display_printC(text[5], CENTER, 155);
		display_printC(text[6], CENTER, 180);
		display_printC("-> Please touch screen <-", CENTER, 212, VGA_BLUE);
		//Wait for touch press
		while (!touch_touched())
			;
		//Touch release again
		while (touch_touched())
			;
	}

	//Show more information
	else
	{
		display_printC(text[5], CENTER, 180);
		display_printC(text[6], CENTER, 205);
	}
}

/* Setting screen for the time and date */
void timeDateScreen()
{
	//Content
	String text[7];
	text[0] = "Set Time & Date";
	text[1] = "In the next screens, you can";
	text[2] = "set the time and date, so ";
	text[3] = "that it matches your current";
	text[4] = "time zone. If the settings do";
	text[5] = "not survive a reboot, check";
	text[6] = "the coin cell battery voltage.";
	infoScreen(text);

	//Reset values
	setTime(12, 30, 30, 15, 6, 2021);

	//Adjust time
	timeMenu(true);
	timeMenuHandler(true);

	//Adjust date
	dateMenu(true);
	dateMenuHandler(true);
}

/* Setting screen for the temperature format */
void tempFormatScreen()
{
	//Content
	String text[7];
	text[0] = "Set Temp. Format";
	text[1] = "In the next screen, you can";
	text[2] = "set the temperature format ";
	text[3] = "for the temperature display_";
	text[4] = "Choose between Celsius or";
	text[5] = "Fahrenheit, the conversion will";
	text[6] = "be done automatically.";
	infoScreen(text);

	//Temperature format menu
	tempFormatMenu(true);
}

/* Setting screen for the convert image option */
void convertImageScreen()
{
	//Content
	String text[7];
	text[0] = "Convert DAT to BMP";
	text[1] = "In the next screen, select if";
	text[2] = "you also want to create a bitmap";
	text[3] = "file for every saved thermal";
	text[4] = "raw image file on the device. ";
	text[5] = "You can still convert images man-";
	text[6] = "ually in the load menu later.";
	infoScreen(text);

	//Convert image menu
	convertImageMenu(true);
}

/* Format the SD card for the first time */
void firstFormat()
{
	//Format the SD card
	showFullMessage((char *)"Formatting SD card..");
	if (formatCard())
		showFullMessage((char *)"Formatting finished!", true);
	else
		showFullMessage((char *)"Error during formatting!", true);
	delay(1000);
}

/* Show the first start complete screen */
void firstStartComplete()
{
	//Content
	String text[7];
	text[0] = "Setup completed";
	text[1] = "The first-time setup is";
	text[2] = "now complete. Please reboot";
	text[3] = "the device by turning the";
	text[4] = "power switch off and on again.";
	text[5] = "Afterwards, you will be redirected";
	text[6] = "to the first start helper menu.";
	infoScreen(text, false);

	//Wait for hard-reset
	while (true)
		;
}

/* Check if the live mode helper needs to be shown */
boolean checkLiveModeHelper()
{
	return EEPROM.read(eeprom_liveHelper) != eeprom_setValue;
}

/* Help screen for the first start of live mode */
void liveModeHelper()
{
	String text[7];

	//Hint screen for the live mode #1
	text[0] = "First time helper";
	text[1] = "To enter the menu in live mode";
	text[2] = "touch the screen short. Pressing";
	text[3] = "the push button on top of the";
	text[4] = "device short saves an image to";
	text[5] = "the internal storage, pressing";
	text[6] = "it longer records a video.";
	infoScreen(text);

	//Hint screen for the live mode #2
	text[1] = "You can lock the temperature limits";
	text[2] = "by touching the screen longer in ";
	text[3] = "live mode. Doing it again switches";
	text[4] = "back to auto mode. Connecting a USB";
	text[5] = "cable will allow you to enter the";
	text[6] = "mass storage mode on your PC.";
	infoScreen(text);

	//Show waiting message
	showFullMessage((char *)"Please wait..");

	//Set EEPROM marker to complete
	EEPROM.write(eeprom_liveHelper, eeprom_setValue);
}

/* Set the EEPROM values to default for the first time */
void stdEEPROMSet()
{
	//Set device EEPROM settings
	EEPROM.write(eeprom_rotationVert, false);
	EEPROM.write(eeprom_rotationHorizont, false);
	EEPROM.write(eeprom_spotEnabled, false);
	EEPROM.write(eeprom_colorbarEnabled, true);
	EEPROM.write(eeprom_batteryEnabled, true);
	EEPROM.write(eeprom_timeEnabled, true);
	EEPROM.write(eeprom_dateEnabled, true);
	EEPROM.write(eeprom_storageEnabled, true);
	EEPROM.write(eeprom_textColor, textColor_white);
	EEPROM.write(eeprom_minMaxPoints, minMaxPoints_max);
	EEPROM.write(eeprom_screenOffTime, screenOffTime_disabled);
	EEPROM.write(eeprom_hotColdMode, hotColdMode_disabled);

	//Set Color Scheme to Rainbow
	EEPROM.write(eeprom_colorScheme, colorScheme_rainbow);

	//Set filter type to box blur
	EEPROM.write(eeprom_filterType, filterType_gaussian);

	//Set disable shutter to false
	EEPROM.write(eeprom_noShutter, false);

	//Set enter mass storage mode to disabled
	EEPROM.write(eeprom_massStorage, 0);

	//Battery gauge standard compensation values
	EEPROM.write(eeprom_batComp, 6);

	//Set current firmware version
	EEPROM.write(eeprom_fwVersionHigh, (fwVersion & 0xFF00) >> 8);
	EEPROM.write(eeprom_fwVersionLow, fwVersion & 0x00FF);

	//Set first start marker to true
	EEPROM.write(eeprom_firstStart, eeprom_setValue);

	//Set live helper to false to show it the next time
	EEPROM.write(eeprom_liveHelper, false);
}

/* First start setup*/
void firstStart()
{
	//Clear EEPROM
	clearEEPROM();

	//Welcome screen
	welcomeScreen();

	//Hint screen for the time and date settings
	timeDateScreen();

	//Hint screen for temperature format setting
	tempFormatScreen();

	//Hint screen for the convert image settings
	convertImageScreen();

	//Format SD card for the first time
	firstFormat();

	//Set EEPROM values
	stdEEPROMSet();

	//Show completion message
	firstStartComplete();
}
