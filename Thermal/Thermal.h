/*
*
* THERMAL - Main functions in the live mode
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

/* Includes */

#include "Calibration.h"
#include "Create.h"
#include "Save.h"
#include "Load.h"

/* Methods*/

/* Touch interrupt handler */
void touchIRQ() {
	//When not in menu, video save, image save, serial mode or lock/release limits
	if ((!showMenu) && (!videoSave) && (!longTouch) && (!imgSave) && (!serialMode)) {
		//Count the time to choose selection
		long startTime = millis();
		delay(10);
		long endTime = millis() - startTime;

		//Wait for touch release, but not longer than a second
		if (touch_capacitive) {
			while ((touch_touched()) && (endTime <= 1000))
				endTime = millis() - startTime;
		}
		else {
			while ((!digitalRead(pin_touch_irq)) && (endTime <= 1000))
				endTime = millis() - startTime;
		}
		endTime = millis() - startTime;

		//Short press - show menu
		if (endTime < 1000)
			showMenu = showMenu_desired;
		//Long press not in visual - lock or release limits
		else if (displayMode != displayMode_visual)
			longTouch = true;
	}
}

/* Button interrupt handler */
void buttonIRQ() {
	//When not in menu, video save, image save, serial mode or lock/release limits
	if ((!showMenu) && (!videoSave) && (!longTouch) && (!imgSave) && (!serialMode)) {
		//Count the time to choose selection
		long startTime = millis();
		delay(10);
		long endTime = millis() - startTime;

		//As long as the button is pressed
		while (extButtonPressed() && (endTime <= 1000))
			endTime = millis() - startTime;

		//Short press - save image to SD Card
		if (endTime < 1000)
			//Prepare image save but let screen refresh first
			imgSave = imgSave_set;

		//Enable video mode
		else
			videoSave = videoSave_menu;
	}

	//When in video save recording mode, go to processing
	if (videoSave == videoSave_recording) {
		videoSave = videoSave_processing;
		while (extButtonPressed());
	}

	//When in video save processing, end it
	else if (videoSave == videoSave_processing) {
		videoSave = videoSave_menu;
		while (extButtonPressed());
	}
}

/* Handler for a long touch press */
void longTouchHandler() {
	//When in auto mode, toggle between locked & unlocked
	if (autoMode) {
		//Unlock limits and enable auto FFC
		if (limitsLocked) {
			showTransMessage((char*) "Limits unlocked");
			limitsLocked = false;
			lepton_ffcMode(true);
		}

		//Lock limits and disable auto FFC
		else {
			lepton_ffcMode(false);
			showTransMessage((char*) "Limits locked");
			limitsLocked = true;
		}
	}

	//When in manual mode, toggle between presets
	else {
		//Read preset from EEPROM
		byte minMaxPreset = EEPROM.read(eeprom_minMaxPreset);

		//When in temporary limits, go to preset 1
		if (minMaxPreset == minMax_temporary) {
			showTransMessage((char*) "Switch to Preset 1");
			EEPROM.write(eeprom_minMaxPreset, minMax_preset1);
		}

		//When in preset 1, go to preset 2
		if (minMaxPreset == minMax_preset1) {
			showTransMessage((char*) "Switch to Preset 2");
			EEPROM.write(eeprom_minMaxPreset, minMax_preset2);
		}

		//When in preset 2, go to preset 3
		if (minMaxPreset == minMax_preset2) {
			showTransMessage((char*) "Switch to Preset 3");
			EEPROM.write(eeprom_minMaxPreset, minMax_preset3);
		}

		//When in preset 3, go back to preset 1
		if (minMaxPreset == minMax_preset3) {
			showTransMessage((char*) "Switch to Preset 1");
			EEPROM.write(eeprom_minMaxPreset, minMax_preset1);
		}

		//Load the new limits
		readTempLimits();
	}

	//Disable lock limits menu
	longTouch = false;
}

/* Show the color bar on screen */
void showColorBar() {
	//Help variables
	char buffer[6];
	byte red, green, blue, fac;
	byte count = 0;

	//Calculate color bar height corresponding on color elements
	byte height;
	//For 320x240
	if (teensyVersion == teensyVersion_new)
		height = 240 - ((240 - (colorElements / 2)) / 2);
	//For 160x120
	else
		height = 120 - ((120 - (colorElements / 4)) / 2);

	//Calculate color level for hot and cold
	float colorLevel = 0;
	if ((hotColdMode != hotColdMode_disabled) && (displayMode != displayMode_combined))
		colorLevel = (tempToRaw(hotColdLevel) * 1.0 - minValue) / (maxValue * 1.0 - minValue);

	//Display color bar
	for (int i = 0; i < (colorElements - 1); i++) {
		//For 320x240, use every second
		if (teensyVersion == teensyVersion_new)
			fac = 2;
		//For 160x120, use every forth
		else
			fac = 4;
		if ((i % fac) == 0) {
			//Hot
			if ((hotColdMode == hotColdMode_hot) && (i >= (colorLevel * colorElements)) && (calStatus != cal_warmup) && (displayMode != displayMode_combined))
				getHotColdColors(&red, &green, &blue);
			//Cold
			else if ((hotColdMode == hotColdMode_cold) && (i <= (colorLevel * colorElements)) && (calStatus != cal_warmup) && (displayMode != displayMode_combined))
				getHotColdColors(&red, &green, &blue);
			//Other
			else {
				red = colorMap[i * 3];
				green = colorMap[(i * 3) + 1];
				blue = colorMap[(i * 3) + 2];
			}
			//Draw the line
			display_setColor(red, green, blue);
			//For 320x240
			if (teensyVersion == teensyVersion_new)
				display_drawLine(298, height - count, 314, height - count);
			//For 160x120
			else
				display_drawLine(298, (height - count) * 2, 314, (height - count) * 2);
			//Raise counter
			count++;
		}
	}

	//Set text color
	changeTextColor();

	//Calculate min and max temp in celcius/fahrenheit
	float min = calFunction(minValue);
	float max = calFunction(maxValue);
	//Calculate step
	float step = (max - min) / 3.0;

	//For 320x240, set fac to one
	if (teensyVersion == teensyVersion_new)
		fac = 1;
	//For 160x120, set fac to two
	else
		fac = 2;

	//Draw min temp
	sprintf(buffer, "%d", (int)round(min));
	display_print(buffer, 270, (height * fac) - 5);

	//Draw temperatures after min before max
	for (int i = 2; i >= 1; i--) {
		float temp = min + (i*step);
		sprintf(buffer, "%d", (int)round(temp));
		display_print(buffer, 270, (height * fac) - 5 - (i * (colorElements / 6)));
	}

	//Draw max temp
	sprintf(buffer, "%d", (int)round(max));
	display_print(buffer, 270, (height * fac) - 5 - (3 * (colorElements / 6)));
}

/* Change the display options */
void changeDisplayOptions(byte* pos) {
	switch (*pos) {
		//Battery
	case 0:
		batteryEnabled = !batteryEnabled;
		EEPROM.write(eeprom_batteryEnabled, batteryEnabled);
		break;

		//Time
	case 1:
		timeEnabled = !timeEnabled;
		EEPROM.write(eeprom_timeEnabled, timeEnabled);
		break;

		//Date
	case 2:
		dateEnabled = !dateEnabled;
		EEPROM.write(eeprom_dateEnabled, dateEnabled);
		break;

		//Spot
	case 3:
		spotEnabled = !spotEnabled;
		EEPROM.write(eeprom_spotEnabled, spotEnabled);
		break;

		//Colorbar
	case 4:
		colorbarEnabled = !colorbarEnabled;
		EEPROM.write(eeprom_colorbarEnabled, colorbarEnabled);
		break;

		//Storage
	case 5:
		storageEnabled = !storageEnabled;
		EEPROM.write(eeprom_storageEnabled, storageEnabled);
		break;

		//Filter
	case 6:
		if (filterType == filterType_box)
			filterType = filterType_gaussian;
		else if (filterType == filterType_gaussian)
			filterType = filterType_none;
		else
			filterType = filterType_box;
		EEPROM.write(eeprom_filterType, filterType);
		break;

		//Text color
	case 7:
		if (textColor == textColor_white)
			textColor = textColor_black;
		else if (textColor == textColor_black)
			textColor = textColor_red;
		else if (textColor == textColor_red)
			textColor = textColor_green;
		else if (textColor == textColor_green)
			textColor = textColor_blue;
		else
			textColor = textColor_white;
		EEPROM.write(eeprom_textColor, textColor);
		break;

		//Hottest or coldest display
	case 8:
		if (minMaxPoints == minMaxPoints_disabled)
			minMaxPoints = minMaxPoints_min;
		else if (minMaxPoints == minMaxPoints_min)
			minMaxPoints = minMaxPoints_max;
		else if (minMaxPoints == minMaxPoints_max)
			minMaxPoints = minMaxPoints_both;
		else
			minMaxPoints = minMaxPoints_disabled;
		EEPROM.write(eeprom_minMaxPoints, minMaxPoints);
		break;
	}
}

/* Map to the right color scheme */
void selectColorScheme() {
	//Select the right color scheme
	switch (colorScheme) {
		//Arctic
	case colorScheme_arctic:
		colorMap = colorMap_arctic;
		colorElements = 240;
		break;

		//Black-Hot
	case colorScheme_blackHot:
		colorMap = colorMap_blackHot;
		colorElements = 224;
		break;

		//Blue-Red
	case colorScheme_blueRed:
		colorMap = colorMap_blueRed;
		colorElements = 192;
		break;

		//Coldest
	case colorScheme_coldest:
		colorMap = colorMap_coldest;
		colorElements = 224;
		break;

		//Contrast
	case colorScheme_contrast:
		colorMap = colorMap_contrast;
		colorElements = 224;
		break;

		//Double-Rainbow
	case colorScheme_doubleRainbow:
		colorMap = colorMap_doubleRainbow;
		colorElements = 256;
		break;

		//Gray-Red
	case colorScheme_grayRed:
		colorMap = colorMap_grayRed;
		colorElements = 224;
		break;

		//Glowbow
	case colorScheme_glowBow:
		colorMap = colorMap_glowBow;
		colorElements = 224;
		break;

		//Grayscale
	case colorScheme_grayscale:
		colorMap = colorMap_grayscale;
		colorElements = 256;
		break;

		//Hottest
	case colorScheme_hottest:
		colorMap = colorMap_hottest;
		colorElements = 224;
		break;

		//Ironblack
	case colorScheme_ironblack:
		colorMap = colorMap_ironblack;
		colorElements = 256;
		break;

		//Lava
	case colorScheme_lava:
		colorMap = colorMap_lava;
		colorElements = 240;
		break;

		//Medical
	case colorScheme_medical:
		colorMap = colorMap_medical;
		colorElements = 224;
		break;

		//Rainbow
	case colorScheme_rainbow:
		colorMap = colorMap_rainbow;
		colorElements = 256;
		break;

		//Wheel 1
	case colorScheme_wheel1:
		colorMap = colorMap_wheel1;
		colorElements = 256;
		break;

		//Wheel 2
	case colorScheme_wheel2:
		colorMap = colorMap_wheel2;
		colorElements = 256;
		break;

		//Wheel 3
	case colorScheme_wheel3:
		colorMap = colorMap_wheel3;
		colorElements = 256;
		break;

		//White-Hot
	case colorScheme_whiteHot:
		colorMap = colorMap_whiteHot;
		colorElements = 224;
		break;

		//Yellow
	case colorScheme_yellow:
		colorMap = colorMap_yellow;
		colorElements = 224;
		break;
	}
}

/* Change the color scheme for the thermal image */
void changeColorScheme(byte* pos) {
	//Align position to color scheme
	colorScheme = *pos;
	//Map to the right color scheme
	selectColorScheme();
	//Save to EEPROM
	EEPROM.write(eeprom_colorScheme, colorScheme);
}

/* Show the thermal/visual/combined image on the screen */
void showImage() {
	//Draw thermal image on screen if created previously and not in menu nor in video save
	if ((!imgSave) && (!showMenu) && (!videoSave))
		displayBuffer();

	//If the image has been created, set to save
	if (imgSave == imgSave_create)
		imgSave = imgSave_save;
}

/* Init procedure for the live mode */
void liveModeInit() {
	//Activate laser if enabled on old HW
	if (laserEnabled && (teensyVersion == teensyVersion_old))
		digitalWrite(pin_laser, HIGH);

	//Select color scheme
	selectColorScheme();

	//For visual / combined, change cam res and take firts shot
	if (displayMode != displayMode_thermal)
		camera_setDisplayRes();

	//Attach the Button interrupt
	attachInterrupt(pin_button, buttonIRQ, RISING);
	//Attach the Touch interrupt
	attachInterrupt(pin_touch_irq, touchIRQ, FALLING);

	//Clear temperature points array
	clearTempPoints();
}

/* Main entry point for the live mode */
void liveMode() {
	//Init
	liveModeInit();

	//Main Loop
	while (true) {
		//Check for serial connection
		checkSerial();

		//Check for screen sleep
		screenOffCheck();

		//If touch IRQ has been triggered, open menu
		if (showMenu)
			mainMenu();

		//Start the image save procedure
		if (imgSave == imgSave_set)
			imgSaveStart();

		//Create thermal image
		if (displayMode == displayMode_thermal)
			createThermalImg();
		//Create visual or combined image
		else
			createVisCombImg();

		//Display additional information
		displayInfos();

		//Show the content on the screen
		showImage();

		//Save the converted / visual image
		if (imgSave == imgSave_save)
			imgSaveEnd();

		//Go into video mode
		if (videoSave == videoSave_menu)
			videoMode();

		//Long touch handler
		if (longTouch)
			longTouchHandler();
	}
}