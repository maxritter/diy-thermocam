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
* https://github.com/maxritter/diy-thermocam
*
*/

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <touchscreen.h>
#include <hardware.h>
#include <gui.h>
#include <lepton.h>
#include <EEPROM.h>
#include <massstorage.h>
#include <create.h>
#include <temperature.h>
#include <display.h>
#include <fonts.h>
#include <colorschemes.h>
#include <connection.h>
#include <mainmenu.h>
#include <save.h>
#include <livemode.h>
#include <videomenu.h>
#include <globalvariables.h>
#include <perf.h>
#include <tc_thermal_core.h>
#include <thermal.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Non-blocking Touch ISR: only latches edge flag & honors post-menu lockout */
void touchIRQ()
{
	if (touchLockoutUntilMs != 0 && static_cast<int32_t>(millis() - touchLockoutUntilMs) < 0)
	{
		return;
	}
	if ((!showMenu) && (!videoSave) && (!longTouch) && (!imgSave) && (!serialMode))
	{
		touchIrqFlag = true;
	}
}

/* Non-blocking Button ISR: only latches edge flag (no busy-wait in ISR!) */
void buttonIRQ()
{
	if (videoSave == videoSave_recording)
	{
		videoSave = videoSave_processing;
		return;
	}
	if (videoSave == videoSave_processing)
	{
		videoSave = videoSave_menu;
		return;
	}
	if ((!showMenu) && (!videoSave) && (!longTouch) && (!imgSave) && (!serialMode))
	{
		buttonIrqFlag = true;
	}
}

/* Poll deferred touch / button presses in main loop without blocking IRQs */
static void pollLiveInputs()
{
	if (touchLockoutUntilMs != 0)
	{
		if (static_cast<int32_t>(millis() - touchLockoutUntilMs) < 0)
		{
			touchIrqFlag = false;
		}
		else
		{
			touchLockoutUntilMs = 0;
		}
	}

	if (touchIrqFlag)
	{
		touchIrqFlag = false;
		uint32_t startTime = millis();
		while ((millis() - startTime) < 1000u)
		{
			bool stillPressed = touch_capacitive ? touch_touched() : (!digitalRead(pin_touch_irq));
			if (!stillPressed) break;
			if (Serial.available() > 0) return; // yield immediately to USB host
			delay(2);
		}
		if ((millis() - startTime) < 1000u)
		{
			if (showMenu == showMenu_disabled)
				showMenu = showMenu_desired;
		}
		else
		{
			longTouch = true;
		}
	}

	if (buttonIrqFlag)
	{
		buttonIrqFlag = false;
		uint32_t startTime = millis();
		while (extButtonPressed() && ((millis() - startTime) < 1000u))
		{
			if (Serial.available() > 0) return;
			delay(2);
		}
		if ((millis() - startTime) < 1000u)
			imgSave = imgSave_set;
		else
			videoSave = videoSave_menu;
	}
}

/* Handler for a long touch press */
void longTouchHandler()
{
	//When in auto mode, toggle between locked & unlocked
	if (autoMode)
	{
		//Unlock limits and enable auto FFC
		if (limitsLocked)
		{
			showTransMessage((char *)"Limits unlocked");
			limitsLocked = false;
			lepton_ffcMode(true);
		}

		//Lock limits and disable auto FFC
		else
		{
			lepton_ffcMode(false);
			showTransMessage((char *)"Limits locked");
			limitsLocked = true;
		}
	}

	//When in manual mode, toggle between presets
	else
	{
		//Read preset from EEPROM
		byte minMaxPreset = EEPROM.read(eeprom_minMaxPreset);

		//When in temporary limits
		if (minMaxPreset == minMax_temporary)
		{
			if (EEPROM.read(eeprom_minMax1Set) == eeprom_setValue)
			{
				showTransMessage((char *)"Switch to Preset 1");
				minMaxPreset = minMax_preset1;
			}
			else if (EEPROM.read(eeprom_minMax2Set) == eeprom_setValue)
			{
				showTransMessage((char *)"Switch to Preset 2");
				minMaxPreset = minMax_preset2;
			}
			else if (EEPROM.read(eeprom_minMax3Set) == eeprom_setValue)
			{
				showTransMessage((char *)"Switch to Preset 3");
				minMaxPreset = minMax_preset3;
			}
			else
				showTransMessage((char *)"No other Preset");
		}

		//When in preset 1
		else if (minMaxPreset == minMax_preset1)
		{
			if (EEPROM.read(eeprom_minMax2Set) == eeprom_setValue)
			{
				showTransMessage((char *)"Switch to Preset 2");
				minMaxPreset = minMax_preset2;
			}
			else if (EEPROM.read(eeprom_minMax3Set) == eeprom_setValue)
			{
				showTransMessage((char *)"Switch to Preset 3");
				minMaxPreset = minMax_preset3;
			}
			else
				showTransMessage((char *)"No other Preset");
		}

		//When in preset 2
		else if (minMaxPreset == minMax_preset2)
		{
			if (EEPROM.read(eeprom_minMax3Set) == eeprom_setValue)
			{
				showTransMessage((char *)"Switch to Preset 3");
				minMaxPreset = minMax_preset3;
			}
			else if (EEPROM.read(eeprom_minMax1Set) == eeprom_setValue)
			{
				showTransMessage((char *)"Switch to Preset 1");
				minMaxPreset = minMax_preset1;
			}
			else
				showTransMessage((char *)"No other Preset");
		}

		//When in preset 3
		else if (minMaxPreset == minMax_preset3)
		{
			if (EEPROM.read(eeprom_minMax1Set) == eeprom_setValue)
			{
				showTransMessage((char *)"Switch to Preset 1");
				minMaxPreset = minMax_preset1;
			}
			else if (EEPROM.read(eeprom_minMax2Set) == eeprom_setValue)
			{
				showTransMessage((char *)"Switch to Preset 2");
				minMaxPreset = minMax_preset2;
			}
			else
				showTransMessage((char *)"No other Preset");
		}

		//When not using temporary preset
		if (minMaxPreset != minMax_temporary)
		{
			//Save new preset to EEPROM
			EEPROM.write(eeprom_minMaxPreset, minMaxPreset);

			//Load the new limits
			readTempLimits();
		}
	}

	//Disable lock limits menu & prevent finger-lift bounce into main menu
	longTouch = false;
	ui_waitTouchReleaseAndLockout(250u);
}

/* Show the color bar on screen */
void showColorBar()
{
	//Help variables
	char buffer[6];
	byte red, green, blue, fac;
	byte count = 0;

	//Calculate color bar height corresponding on color elements
	byte height = 240 - ((240 - (colorElements / 2)) / 2);

	//Calculate color level for hot and cold (divide-by-zero guarded)
	float colorLevel = 0;
	if ((hotColdMode != hotColdMode_disabled) && (maxValue > minValue))
		colorLevel = (tempToRaw(hotColdLevel) * 1.0f - minValue) / (maxValue * 1.0f - minValue);

	//Subtle 1px dark slate frame OUTSIDE x=298..314 so x=300..312 remains pixel-exact!
	byte barRows = static_cast<byte>((colorElements) / 2);
	int yTop = static_cast<int>(height) - static_cast<int>(barRows) + 1;
	display_setColor(15, 23, 42);
	display_drawHLine(297, yTop - 1, 19);
	display_drawHLine(297, height + 1, 19);
	display_drawVLine(297, yTop, barRows);
	display_drawVLine(315, yTop, barRows);

	//Display color bar (pixel-exact rows at x=298..314)
	for (int i = 0; i < (colorElements - 1); i++)
	{
		fac = 2;

		if ((i % fac) == 0)
		{
			//Hot
			if ((hotColdMode == hotColdMode_hot) && (i >= (colorLevel * colorElements)))
				getHotColdColors(&red, &green, &blue);
			
			//Cold
			else if ((hotColdMode == hotColdMode_cold) && (i <= (colorLevel * colorElements)))
				getHotColdColors(&red, &green, &blue);
			
			//Other
			else
			{
				red = colorMap[i * 3];
				green = colorMap[(i * 3) + 1];
				blue = colorMap[(i * 3) + 2];
			}

			//Draw the line
			display_setColor(red, green, blue);
			display_drawHLine(298, height - count, 17);

			//Raise counter
			count++;
		}
	}

	//Translucent dark backdrop behind tick labels for razor-sharp contrast
	display_blendRect(264, yTop - 6, 31, barRows + 12, UI_HUD_BG, 4);

	//Set text color
	changeTextColor();

	//Calculate min and max temp in celcius/fahrenheit
	float min = rawToTemp(minValue);
	float max = rawToTemp(maxValue);

	//Calculate step
	float step = (max - min) / 3.0f;
	fac = 1;

	//Draw min temp
	sprintf(buffer, "%d", (int)round(min));
	display_print(buffer, 267, (height * fac) - 5);

	//Draw temperatures after min before max
	for (int i = 2; i >= 1; i--)
	{
		float temp = min + (i * step);
		sprintf(buffer, "%d", (int)round(temp));
		display_print(buffer, 267, (height * fac) - 5 - (i * (colorElements / 6)));
	}

	//Draw max temp
	sprintf(buffer, "%d", (int)round(max));
	display_print(buffer, 267, (height * fac) - 5 - (3 * (colorElements / 6)));
}

/* Change the display options */
void changeDisplayOptions(byte *pos)
{
	switch (*pos)
	{
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
void selectColorScheme()
{
	//Select the right color scheme
	switch (colorScheme)
	{
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

	//Rebuild 256-entry RGB565 LUT for fast integer colorization
	tc_core::build_rgb565_lut(colorMap, static_cast<uint16_t>(colorElements), colorLut);
}

/* Change the color scheme for the thermal image */
void changeColorScheme(byte *pos)
{
	//Align position to color scheme
	colorScheme = *pos;
	//Map to the right color scheme
	selectColorScheme();
	//Save to EEPROM
	EEPROM.write(eeprom_colorScheme, colorScheme);
}

/* Show the thermal/visual/combined image on the screen */
void showImage()
{
	//Draw clean thermal image on screen if not in menu nor in video save
	if ((!showMenu) && (!videoSave))
		displayBuffer();

	//If an image is being saved, draw SAVING card directly to LCD (never into bigBuffer/.BMP!)
	if (imgSave == imgSave_create) {
		if (!serialMode) {
			display_writeToImage = false;
			int by = spotEnabled ? 62 : 104;
			display_setColor(UI_SURFACE);
			display_fillRoundRect(94, by - 6, 226, by + 22);
			display_setColor(UI_ACCENT);
			display_drawRoundRect(94, by - 6, 226, by + 22);
			display_setBackColor(UI_SURFACE);
			display_setColor(UI_TEXT);
			display_setFont(bigFont);
			display_print((char *)"SAVING", CENTER, by);
			display_setFont((uint8_t *)smallFont);
			changeTextColor();
		}
		imgSave = imgSave_save;
	}
}

/* Init procedure for the live mode */
void liveModeInit()
{
	selectColorScheme();

	attachInterrupt(pin_button, buttonIRQ, RISING);
	attachInterrupt(pin_touch_irq, touchIRQ, FALLING);

	showMenu = showMenu_disabled;
	usbConnected = true;
	longTouch = false;
	disableSPIIRQ = false;

	clearTempPoints();
	lepton_startFrame();
}

/* Main entry point for the live mode */
void liveMode()
{
	//Init
	liveModeInit();

	uint32_t lastFrameUs = micros();
	uint32_t lastFrameMs = millis();
	bool softResyncTried = false;

	auto resetLiveWatchdog = [&]() {
		disableSPIIRQ = false;
		lepton_startFrame();
		lastFrameMs = millis();
		softResyncTried = false;
	};

	//Main Loop
	while (true)
	{
		//Check for serial connection (non-blocking, <1 ms response!)
		if (Serial.available() > 0) {
			checkSerial();
			resetLiveWatchdog();
		}

		//Poll deferred touch/button edge flags
		pollLiveInputs();

		//Check for screen sleep
		if (screenOffCheck()) {
			resetLiveWatchdog();
		}

		//If touch IRQ has been triggered, open menu
		if (showMenu == showMenu_desired) {
			mainMenu();
			resetLiveWatchdog();
		}

		//Start the image save procedure
		if (imgSave == imgSave_set) {
			imgSaveStart();
			resetLiveWatchdog();
		}

		//Go into video mode
		if (videoSave == videoSave_menu) {
			videoMode();
			resetLiveWatchdog();
		}

		//Long touch handler
		if (longTouch) {
			longTouchHandler();
			resetLiveWatchdog();
		}

		//Wait for new double-buffered frame while continuously servicing USB + 500ms/2000ms VoSPI watchdog!
		if (!lepton_hasNewFrame()) {
			uint32_t stallMs = millis() - lastFrameMs;
			if (!softResyncTried && stallMs >= 500u) {
				// 500 ms live-view watchdog: FLIR datasheet VoSPI resync (CS high >=205 ms + re-arm)
				softResyncTried = true;
				lepton_vospiResync();
			} else if (stallMs >= 2000u) {
				// 2000 ms escalation: full Lepton VoSPI + VSYNC GPIO reset & re-arm
				lepton_reset();
				lepton_startFrame();
				lastFrameMs = millis();
				softResyncTried = false;
			}
			continue;
		}

		lastFrameMs = millis();
		softResyncTried = false;

		uint32_t loopStartUs = micros();

		//Create thermal image (swaps double buffer immediately; Lepton captures next frame in background)
		createThermalImg();

		//Composite ALL HUD overlays into bigBuffer in RAM (zero flicker!)
		displayInfos();

		//Push bigBuffer to ILI9341 via LPSPI4 burst (one atomic write!)
		showImage();

		//Save the converted BMP image if requested
		if (imgSave == imgSave_save) {
			imgSaveEnd();
			resetLiveWatchdog();
		}

		uint32_t nowUs = micros();
		(void)perf_uptime_us();
		g_perf.loop_total_us = nowUs - loopStartUs;
		perf_record_frame_period(PERF_MODE_LIVE, nowUs - lastFrameUs);
		lastFrameUs = nowUs;
	}
}
