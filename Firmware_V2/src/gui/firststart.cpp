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
* https://github.com/maxritter/DIY-Thermocam
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
#include <calibration.h>
#include <Time.h>
#include <mlx90614.h>
#include <sd.h>
#include <hardware.h>
#include <gui.h>
#include <connection.h>
#include <firststart.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Check if the first start needs to be done */
boolean checkFirstStart() {
	return EEPROM.read(eeprom_firstStart) != eeprom_setValue;
}

/* Show welcome Screen for the first start procedure */
void welcomeScreen() {
	//Background & Title
	display_fillScr(200, 200, 200);
	display_setBackColor(200, 200, 200);
	display_setFont(smallFont);
	display_printC("Welcome to the", CENTER, 20);
	display_setFont(bigFont);

	//DIY-Thermocam V2
	if (teensyVersion == teensyVersion_new)
		display_printC("DIY-Thermocam V2", CENTER, 60, VGA_BLUE);

	//DIY-Thermocam V1
	else if (mlx90614Version == mlx90614Version_new)
		display_printC("DIY-Thermocam V1", CENTER, 60, VGA_BLUE);

	//Thermocam V4
	else
		display_printC("Cheap-Thermocam V4", CENTER, 60, VGA_BLUE);

	//Explanation
	display_setFont(smallFont);
	display_printC("This is the first time setup.", CENTER, 110);
	display_printC("It will guide you through the", CENTER, 140);
	display_printC("basic settings for your device.", CENTER, 170);
	display_printC("-> Please touch screen <-", CENTER, 210, VGA_BLUE);

	//Wait for touch press or updater request
	while (!touch_touched())
		checkForUpdater();

	//Touch release again
	while (touch_touched());
}

/* Shows an info screen during the first start procedure */
void infoScreen(String* text, bool cont) {
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
	if (cont) {
		display_printC(text[5], CENTER, 155);
		display_printC(text[6], CENTER, 180);
		display_printC("-> Please touch screen <-", CENTER, 212, VGA_BLUE);
		//Wait for touch press or updater request
		while (!touch_touched())
			checkForUpdater();
		//Touch release again
		while (touch_touched());
	}

	//Show more information
	else {
		display_printC(text[5], CENTER, 180);
		display_printC(text[6], CENTER, 205);
	}
}

/* Setting screen for the time and date */
void timeDateScreen() {
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
	setTime(12, 30, 30, 15, 6, 2017);

	//Adjust time
	timeMenu(true);
	timeMenuHandler(true);

	//Adjust date
	dateMenu(true);
	dateMenuHandler(true);
}

/* Setting screen for the temperature format */
void tempFormatScreen() {
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
void convertImageScreen() {
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

/* Setting screen for the visual image option */
void visualImageScreen() {
	//Content
	String text[7];
	text[0] = "Save visual image";
	text[1] = "In the next screen, choose";
	text[2] = "if you want to save an addi-";
	text[3] = "tional visual image together";
	text[4] = "with each saved thermal image.";
	text[5] = "Enable this if you want to";
	text[6] = "create combined images on the PC.";
	infoScreen(text);

	//Visual image menu
	visualImageMenu(true);
}

/* Setting screen for the combined image alignment */
void combinedAlignmentScreen() {
	//Content
	String text[7];
	text[0] = "Combined Alignment";
	text[1] = "In the next screen, you can";
	text[2] = "move, increase or decrease";
	text[3] = "the visual image on top of the";
	text[4] = "thermal image. Press the A-button";
	text[5] = "to change the alpha transparency";
	text[6] = "and touch the middle to refresh.";
	infoScreen(text);

	//Set color scheme to rainbow
	colorMap = colorMap_rainbow;
	colorElements = 256;

	//Disable showmenu
	showMenu = showMenu_disabled;

	//Adjust combined menu
	adjustCombinedNewMenu(true);
}

/* Setting screen for the calibration procedure */
void calibrationHelperScreen() {
	//Content
	String text[7];
	text[0] = "Calibration";
	text[1] = "Before using the device, you need";
	text[2] = "to calibrate it first. Point the ";
	text[3] = "device to different hot and cold";
	text[4] = "objects in the surrounding area";
	text[5] = "slowly, until the calibration";
	text[6] = "process has been completed.";
	infoScreen(text);

	//Calibration procedure
	calibrationProcess(false, true);
}

/* Format the SD card for the first time */
void firstFormat() {
	//ThermocamV4 or DIY-Thermocam V2, check SD card
	if ((mlx90614Version == mlx90614Version_old) ||
		(teensyVersion == teensyVersion_new)) {

		//Show message
		showFullMessage((char*) "Checking SD card..");

		//Check for SD card
		if (!checkSDCard()) {
			showFullMessage((char*) "Please insert SD card");
			//Wait until card is inserted
			while (!checkSDCard())
				delay(1000);
		}
	}

	//Format the SD card
	showFullMessage((char*) "Formatting SD card..");
	formatCard();
}

/* Show the first start complete screen */
void firstStartComplete() {
	//Content
	String text[7];
	text[0] = "Setup completed";
	text[1] = "The first-time setup is";
	text[2] = "now complete. Please reboot";
	text[3] = "the device by turning the";
	text[4] = "power switch off and on again.";
	text[5] = "Afterwards, you will be redirected";
	text[6] = "to the align combined menu.";
	infoScreen(text, false);

	//Wait for hard-reset
	while (true);
}

/* Check if the live mode helper needs to be shown */
boolean checkLiveModeHelper() {
	return EEPROM.read(eeprom_liveHelper) != eeprom_setValue;
}

/* Help screen for the first start of live mode */
void liveModeHelper() {
	String text[7];

	//Do the first time calibration if spot sensor working and not using radiometric Lepton
	if (checkDiagnostic(diag_spot) && (leptonVersion != leptonVersion_2_5_shutter)
		&& (leptonVersion != leptonVersion_3_5_shutter))
		calibrationHelperScreen();

	//Hint screen for the live mode #1
	text[0] = "First time helper";
	text[1] = "To enter the live mode menu,";
	text[2] = "touch the screen. 'Exit' will";
	text[3] = "bring you to the main menu.";
	text[4] = "Pressing the push button on";
	text[5] = "top of the device short takes";
	text[6] = "an image, long records a video.";
	infoScreen(text);

	//Hint screen for the live mode #2
	if ((leptonVersion != leptonVersion_2_5_shutter) && (leptonVersion != leptonVersion_3_5_shutter)) {
		text[1] = "The device needs 30 seconds to";
		text[2] = "warmup the sensor, more functions";
		text[3] = "will be activated afterwards. You";
	}
	else {
		text[1] = "The device has detected that you";
		text[2] = "are using the radiometric Lepton2.5";
		text[3] = "sensor and adjusted to that. You";
	}
	text[4] = "can lock the limits or toggle dif-";
	text[5] = "ferent temperature limits by pres-";
	text[6] = "sing the screen long in live mode.";
	infoScreen(text);

	//Show waiting message
	showFullMessage((char*)"Please wait..");

	//Set EEPROM marker to complete
	EEPROM.write(eeprom_liveHelper, eeprom_setValue);
}


/* Set the EEPROM values to default for the first time */
void stdEEPROMSet() {
	//Flash spot sensor, not for radiometric Lepton
	if ((leptonVersion != leptonVersion_2_5_shutter) && (leptonVersion != leptonVersion_3_5_shutter)) {
		//Show message
		showFullMessage((char*) "Flashing spot EEPROM settings..");

		//Only if spot sensor is connected
		if (checkDiagnostic(diag_spot))
		{
			//Set spot maximum temp to 380°C
			mlx90614_setMax();
			//Set spot minimum temp to -70°
			mlx90614_setMin();
			//Set spot filter settings
			mlx90614_setFilter();
			//Set spot emissivity to 0.9
			mlx90614_setEmissivity();
		}
	}

	//Set device EEPROM settings
	EEPROM.write(eeprom_rotationVert, false);
	EEPROM.write(eeprom_rotationHorizont, false);
	EEPROM.write(eeprom_spotEnabled, false);
	EEPROM.write(eeprom_colorbarEnabled, true);
	EEPROM.write(eeprom_batteryEnabled, true);
	EEPROM.write(eeprom_timeEnabled, true);
	EEPROM.write(eeprom_dateEnabled, true);
	EEPROM.write(eeprom_storageEnabled, true);
	EEPROM.write(eeprom_displayMode, displayMode_thermal);
	EEPROM.write(eeprom_textColor, textColor_white);
	EEPROM.write(eeprom_minMaxPoints, minMaxPoints_max);
	EEPROM.write(eeprom_screenOffTime, screenOffTime_disabled);
	EEPROM.write(eeprom_hotColdMode, hotColdMode_disabled);

	//Set Color Scheme to Rainbow
	EEPROM.write(eeprom_colorScheme, colorScheme_rainbow);

	//Set filter type to box blur
	EEPROM.write(eeprom_filterType, filterType_gaussian);

	//For DIY-Thermocam V2, set HQ res to true
	if (teensyVersion == teensyVersion_new)
		EEPROM.write(eeprom_hqRes, true);

	//Set disable shutter to false
	EEPROM.write(eeprom_noShutter, false);

	//Battery gauge standard compensation values
	//DIY-Thermocam V1
	if ((teensyVersion == teensyVersion_old) && (mlx90614Version == mlx90614Version_new))
		EEPROM.write(eeprom_batComp, 0);
	//Thermocam V4
	else if ((teensyVersion == teensyVersion_old) && (mlx90614Version == mlx90614Version_old))
		EEPROM.write(eeprom_batComp, 0);
	//DIY-Thermocam V2
	else
		EEPROM.write(eeprom_batComp, 20);

	//Set current firmware version
	EEPROM.write(eeprom_fwVersion, fwVersion);

	//Set first start marker to true
	EEPROM.write(eeprom_firstStart, eeprom_setValue);

	//Set live helper to false to show it the next time
	EEPROM.write(eeprom_liveHelper, false);
}

/* First start setup*/
void firstStart() {
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

	//Hint screen for the visual image settings
	visualImageScreen();

	//Format SD card for the first time
	firstFormat();

	//Set EEPROM values
	stdEEPROMSet();

	//Show completion message
	firstStartComplete();
}
