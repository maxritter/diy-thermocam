/*
 *
 * MAIN MENU - Display the main menu with icons
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
#include <display.h>
#include <buttons.h>
#include <EEPROM.h>
#include <lepton.h>
#include <fonts.h>
#include <temperature.h>
#include <thermal.h>
#include <hardware.h>
#include <create.h>
#include <settingsmenu.h>
#include <gui.h>
#include <load.h>
#include <bitmaps.h>
#include <colorschemes.h>
#include <connection.h>
#include <massstorage.h>
#include <touchscreen.h>
#include <videomenu.h>
#include <mainmenu.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Draws the full-bleed modern 2026 submenu canvas */
void mainMenuBackground()
{
	display_fillScr(UI_BG_DARK);
	display_setColor(UI_SURFACE);
	display_fillRect(0, 0, 319, 34);
	display_setColor(UI_ACCENT);
	display_drawHLine(0, 34, 320);
	display_setColor(UI_BORDER);
	display_drawHLine(12, 184, 296);
}

/* Draws the content of the selection menu (bezel-safe >=12px margins) */
void drawSelectionMenu()
{
	//Buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 46, 42, 74, (char *)"<", 0, true);
	buttons_addButton(266, 46, 42, 74, (char *)">", 0, true);
	buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_addButton(95, 134, 130, 36, (char *)"OK");
	buttons_drawButtons();
	//Selection card border
	display_setColor(UI_ACCENT);
	display_drawRoundRect(64, 54, 256, 112);
}

/* Draws the title in the main menu */
void mainMenuTitle(char *title)
{
	display_setFont(bigFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print(title, CENTER, 10);
}

/* Draws the current selection in the menu */
void mainMenuSelection(char *selection)
{
	display_setColor(UI_SURFACE);
	display_fillRect(66, 58, 256, 110);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_ACCENT);
	display_print(selection, CENTER, 77);
}

/* Asks the user if he really wants to enter mass storage mode */
bool massStoragePrompt()
{
	//Title & Background
	drawTitle((char *)"USB File Transfer");
	display_setColor(UI_TEXT);
	display_setFont(smallFont);
	display_setBackColor(UI_BG_DARK);
	display_print((char *)"Do you want to enter mass storage", CENTER, 65);
	display_print((char *)"to transfer files to the PC?", CENTER, 85);
	display_print((char *)"Do not use this for FW updates", CENTER, 105);
	display_print((char *)"or for the USB serial connection.", CENTER, 125);
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(165, 160, 140, 55, (char *)"Yes");
	buttons_addButton(15, 160, 140, 55, (char *)"No");
	buttons_drawButtons();
	buttons_setTextFont(smallFont);
	//Wait for touch release
	while (touch_touched())
		;
	//Touch handler
	while (true)
	{
		//If touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//YES
			if (pressedButton == 0)
			{
				return true;
			}
			//NO
			if (pressedButton == 1)
			{
				return false;
			}
		}
	}
}

/* Calibration Repeat Choose */
bool calibrationRepeat()
{
	//Title & Background
	mainMenuBackground();
	mainMenuTitle((char *)"Bad Calibration");
	display_setColor(UI_TEXT);
	display_setFont(bigFont);
	display_setBackColor(UI_BG_DARK);
	display_print((char *)"Try again?", CENTER, 66);
	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT_MUTED);
	display_print((char *)"Use different calibration objects", CENTER, 201);
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(165, 106, 140, 55, (char *)"Yes");
	buttons_addButton(15, 106, 140, 55, (char *)"No");
	buttons_drawButtons();
	//Touch handler
	while (true)
	{
		//If touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//YES
			if (pressedButton == 0)
			{
				return true;
			}
			//NO
			if (pressedButton == 1)
			{
				return false;
			}
		}
	}
}

/* Calibration*/
void calibrationScreen(bool firstStart)
{
	//Normal mode
	if (firstStart == false)
	{
		mainMenuBackground();
		mainMenuTitle((char *)"Calibrating..");
		display_setColor(UI_TEXT);
		display_setBackColor(UI_BG_DARK);
		display_setFont(smallFont);
		display_print((char *)"Point the camera to different", CENTER, 63);
		display_print((char *)"hot and cold objects in the area.", CENTER, 96);
		buttons_deleteAllButtons();
		buttons_setTextFont(bigFont);
		buttons_addButton(90, 188, 140, 40, (char *)"Abort");
		buttons_drawButtons();
		display_setFont(bigFont);
		display_print((char *)"Status:  0%", CENTER, 140);
	}
	//First start
	else
	{
		display_fillScr(UI_BG_DARK);
		display_setFont(bigFont);
		display_setBackColor(UI_BG_DARK);
		display_setColor(UI_TEXT);
		display_print((char *)"Calibrating..", CENTER, 100);
		display_print((char *)"Status:  0%", CENTER, 140);
	}
}

/* Menu to add or remove temperature points to the thermal image */
bool tempPointsMenu()
{
redraw:
	//Background
	mainMenuBackground();
	//Title
	mainMenuTitle((char *)"Temp. points");
	//Draw the selection menu
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 92, 122, (char *)"Add");
	buttons_addButton(114, 46, 92, 122, (char *)"Remove");
	buttons_addButton(216, 46, 92, 122, (char *)"Clear");
	buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
	//Save the current position inside the menu
	while (true)
	{
		//Touch screen pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Add
			if (pressedButton == 0)
			{
				tempPointFunction(false);
				goto redraw;
			}
			//Remove
			if (pressedButton == 1)
			{
				tempPointFunction(true);
				goto redraw;
			}
			//Clear
			if (pressedButton == 2)
			{
				clearTempPoints();
				showFullMessage((char *)"All points cleared", true);
				delay(1000);
				goto redraw;
			}
			//BACK
			if (pressedButton == 3)
				return false;
		}
	}
}

/* Select the color for the live mode string */
void hotColdColorMenuString(int pos)
{
	char *text = (char *)"";
	switch (pos)
	{
	//White
	case 0:
		text = (char *)"White";
		break;
		//Black
	case 1:
		text = (char *)"Black";
		break;
		//Red
	case 2:
		text = (char *)"Red";
		break;
		//Green
	case 3:
		text = (char *)"Green";
		break;
		//Blue
	case 4:
		text = (char *)"Blue";
		break;
	}
	mainMenuSelection(text);
}

/* Menu to display the color in hot/cold color mode */
bool hotColdColorMenu()
{
	//Save the current position inside the menu
	byte hotColdColorMenuPos;
	if (hotColdMode == hotColdMode_hot)
		hotColdColorMenuPos = 2;
	else if (hotColdMode == hotColdMode_cold)
		hotColdColorMenuPos = 4;
	else
		hotColdColorMenuPos = 0;
	//Background
	mainMenuBackground();
	//Title
	mainMenuTitle((char *)"Select color");
	//Draw the selection menu
	drawSelectionMenu();
	//Draw the current item
	hotColdColorMenuString(hotColdColorMenuPos);
	//Save the current position inside the menu
	while (true)
	{
		//Touch screen pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//SELECT
			if (pressedButton == 3)
			{
				//Save
				hotColdColor = hotColdColorMenuPos;
				//Write to EEPROM
				EEPROM.write(eeprom_hotColdColor, hotColdColor);
				return true;
			}
			//BACK
			if (pressedButton == 2)
				return false;
			//BACKWARD
			else if (pressedButton == 0)
			{
				if (hotColdColorMenuPos > 0)
					hotColdColorMenuPos--;
				else if (hotColdColorMenuPos == 0)
					hotColdColorMenuPos = 4;
			}
			//FORWARD
			else if (pressedButton == 1)
			{
				if (hotColdColorMenuPos < 4)
					hotColdColorMenuPos++;
				else if (hotColdColorMenuPos == 4)
					hotColdColorMenuPos = 0;
			}
			//Change the menu name
			hotColdColorMenuString(hotColdColorMenuPos);
		}
	}
}

/* Touch handler for the hot & cold limit changer menu */
void hotColdChooserHandler()
{
	//Help variables
	char margin[20];

	//Display level as temperature
	if (!tempFormat)
	{
		sprintf(margin, "Limit: %dC", hotColdLevel);
	}
	else
	{
		sprintf(margin, "Limit: %dF", hotColdLevel);
	}
	display_print(margin, CENTER, 153);

	//Touch handler
	while (true)
	{
	waitTouch:

		//If touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//RESET
			if (pressedButton == 0)
			{
				if (hotColdMode == hotColdMode_cold)
					hotColdLevel = (int16_t)round(
						rawToTemp(
							0.2 * (maxValue - minValue) + minValue));
				if (hotColdMode == hotColdMode_hot)
					hotColdLevel = (int16_t)round(
						rawToTemp(
							0.8 * (maxValue - minValue) + minValue));
			}
			//SELECT
			else if (pressedButton == 1)
			{
				//Save to EEPROM
				EEPROM.write(eeprom_hotColdLevelHigh,
							 (hotColdLevel & 0xFF00) >> 8);
				EEPROM.write(eeprom_hotColdLevelLow, hotColdLevel & 0x00FF);
				break;
			}
			//MINUS
			else if (pressedButton == 2)
			{
				if (hotColdLevel > round(rawToTemp(minValue)))
					hotColdLevel--;
				else
					goto waitTouch;
			}
			//PLUS
			else if (pressedButton == 3)
			{
				if (hotColdLevel < round(rawToTemp(maxValue)))
					hotColdLevel++;
				else
					goto waitTouch;
			}

			createThermalImg(true);
			display_drawBitmap(80, 48, 160, 120, smallBuffer);

			//Display level as temperature
			if (!tempFormat)
			{
				sprintf(margin, "Limit: %dC", hotColdLevel);
			}
			else
			{
				sprintf(margin, "Limit: %dF", hotColdLevel);
			}
			display_print(margin, CENTER, 153);
		}
	}
}

/* Select the limit in hot/cold mode */
void hotColdChooser()
{
	//Background & title
	mainMenuBackground();
	mainMenuTitle((char *)"Set Limit");

	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 192, 142, 34, (char *)"Reset");
	buttons_addButton(166, 192, 142, 34, (char *)"OK");
	buttons_addButton(12, 48, 56, 120, (char *)"-");
	buttons_addButton(252, 48, 56, 120, (char *)"+");
	buttons_drawButtons();

	//Draw the border for the preview image
	display_setColor(UI_BORDER);
	display_drawRect(79, 47, 241, 169);

	//Set text color
	display_setFont(smallFont);
	display_setBackColor(VGA_TRANSPARENT);
	changeTextColor();

	//Find min and max values
	if ((autoMode) && (!limitsLocked))
	{
		autoMode = true;
		createThermalImg(true);
		autoMode = false;
	}

	//Calculate initial level
	if (hotColdMode == hotColdMode_cold)
		hotColdLevel = (int16_t)round(
			rawToTemp(0.2 * (maxValue - minValue) + minValue));
	if (hotColdMode == hotColdMode_hot)
		hotColdLevel = (int16_t)round(
			rawToTemp(0.8 * (maxValue - minValue) + minValue));

	createThermalImg(true);
	display_drawBitmap(80, 48, 160, 120, smallBuffer);

	//Go into the normal touch handler
	hotColdChooserHandler();
}

/* Menu to display hot or cold areas */
bool hotColdMenu()
{
redraw:
	//Background
	mainMenuBackground();

	//Title
	mainMenuTitle((char *)"Hot / Cold");

	//Draw the selection menu
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 92, 122, (char *)"Hot");
	buttons_addButton(114, 46, 92, 122, (char *)"Cold");
	buttons_addButton(216, 46, 92, 122, (char *)"Disabled");
	buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_drawButtons();

	//Save the current position inside the menu
	while (true)
	{
		//Touch screen pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Hot
			if (pressedButton == 0)
			{
				//Set marker to hot
				hotColdMode = hotColdMode_hot;

				//Choose the color
				if (hotColdColorMenu())
					//Set the limit
					hotColdChooser();

				//Go back
				else
				{
					hotColdMode = hotColdMode_disabled;
					goto redraw;
				}

				//Leave loop
				break;
			}
			//Cold
			if (pressedButton == 1)
			{
				//Set marker to cold
				hotColdMode = hotColdMode_cold;

				//Choose the color
				if (hotColdColorMenu())
					//Set the limit
					hotColdChooser();

				//Go back
				else
				{
					hotColdMode = hotColdMode_disabled;
					goto redraw;
				}

				//Leave loop
				break;
			}
			//Disabled
			if (pressedButton == 2)
			{
				//Set marker to disabled
				hotColdMode = hotColdMode_disabled;

				//Leave loop
				break;
			}
			//Back to main menu
			if (pressedButton == 3)
				return false;
		}
	}

	//Write to EEPROM
	EEPROM.write(eeprom_hotColdMode, hotColdMode);

	//Disable auto FFC for isotherm mode
	if (hotColdMode != hotColdMode_disabled)
		lepton_ffcMode(false);
	//Enable it when isotherm disabled
	else
		lepton_ffcMode(true);

	//Go back
	return true;
}

/* Switch the current preset menu item */
void tempLimitsPresetSaveString(int pos)
{
	char *text = (char *)"";
	switch (pos)
	{
	case 0:
		text = (char *)"Temporary";
		break;
	case 1:
		text = (char *)"Preset 1";
		break;
	case 2:
		text = (char *)"Preset 2";
		break;
	case 3:
		text = (char *)"Preset 3";
		break;
	}
	mainMenuSelection(text);
}

/* Menu to save the temperature limits to a preset */
bool tempLimitsPresetSaveMenu()
{
	//Save the current position inside the menu
	byte menuPos = 1;
	//Background
	mainMenuBackground();
	//Title
	mainMenuTitle((char *)"Select Preset");
	//Draw the selection menu
	drawSelectionMenu();
	//Draw the current item
	tempLimitsPresetSaveString(menuPos);
	//Save the current position inside the menu
	while (true)
	{
		//Touch screen pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//SELECT
			if (pressedButton == 3)
			{
				uint8_t farray[4];
				float calComp = -273.15;

				switch (menuPos)
				{
				//Temporary
				case 0:
					EEPROM.write(eeprom_minMaxPreset, minMax_temporary);
					break;
					//Preset 1
				case 1:
					EEPROM.write(eeprom_minValue1High,
								 (minValue & 0xFF00) >> 8);
					EEPROM.write(eeprom_minValue1Low, minValue & 0x00FF);
					EEPROM.write(eeprom_maxValue1High,
								 (maxValue & 0xFF00) >> 8);
					EEPROM.write(eeprom_maxValue1Low, maxValue & 0x00FF);
					floatToBytes(farray, (float)calComp);
					for (int i = 0; i < 4; i++)
						EEPROM.write(eeprom_minMax1Comp + i, (farray[i]));
					EEPROM.write(eeprom_minMax1Set, eeprom_setValue);
					EEPROM.write(eeprom_minMaxPreset, minMax_preset1);
					break;
					//Preset 2
				case 2:
					EEPROM.write(eeprom_minValue2High,
								 (minValue & 0xFF00) >> 8);
					EEPROM.write(eeprom_minValue2Low, minValue & 0x00FF);
					EEPROM.write(eeprom_maxValue2High,
								 (maxValue & 0xFF00) >> 8);
					EEPROM.write(eeprom_maxValue2Low, maxValue & 0x00FF);
					floatToBytes(farray, (float)calComp);
					for (int i = 0; i < 4; i++)
						EEPROM.write(eeprom_minMax2Comp + i, (farray[i]));
					EEPROM.write(eeprom_minMax2Set, eeprom_setValue);
					EEPROM.write(eeprom_minMaxPreset, minMax_preset2);
					break;
					//Preset 3
				case 3:
					EEPROM.write(eeprom_minValue3High,
								 (minValue & 0xFF00) >> 8);
					EEPROM.write(eeprom_minValue3Low, minValue & 0x00FF);
					EEPROM.write(eeprom_maxValue3High,
								 (maxValue & 0xFF00) >> 8);
					EEPROM.write(eeprom_maxValue3Low, maxValue & 0x00FF);
					floatToBytes(farray, (float)calComp);
					for (int i = 0; i < 4; i++)
						EEPROM.write(eeprom_minMax3Comp + i, (farray[i]));
					EEPROM.write(eeprom_minMax3Set, eeprom_setValue);
					EEPROM.write(eeprom_minMaxPreset, minMax_preset3);
					break;
				}
				return true;
			}
			//BACKWARD
			else if (pressedButton == 0)
			{
				if (menuPos > 0)
					menuPos--;
				else if (menuPos == 0)
					menuPos = 3;
			}
			//FORWARD
			else if (pressedButton == 1)
			{
				if (menuPos < 3)
					menuPos++;
				else if (menuPos == 3)
					menuPos = 0;
			}
			//BACK
			else if (pressedButton == 2)
				return false;
			//Change the menu name
			tempLimitsPresetSaveString(menuPos);
		}
	}
}

/* Touch Handler for the limit chooser menu */
bool tempLimitsManualHandler()
{

	//Set both modes to false for the first time
	bool minChange = false;
	bool maxChange = false;
	//Buffer
	int min, max;
	char minC[10];
	char maxC[10];

	//Touch handler
	while (true)
	{
		//Set font & text color
		display_setFont(smallFont);
		display_setBackColor(VGA_TRANSPARENT);
		changeTextColor();

		//Update minimum & maximum
		min = (int)round(rawToTemp(minValue));
		max = (int)round(rawToTemp(maxValue));
		if (tempFormat == tempFormat_celcius)
		{
			sprintf(minC, "Min:%dC", min);
			sprintf(maxC, "Max:%dC", max);
		}
		else
		{
			sprintf(minC, "Min:%dF", min);
			sprintf(maxC, "Max:%dF", max);
		}
		display_print(maxC, 180, 153);
		display_print(minC, 85, 153);
		display_setFont(bigFont);

		//If touch pressed
		if (touch_touched() == true)
		{
			int pressedButton;
			//Change values continously and fast when the user holds the plus or minus button
			if (minChange || maxChange)
				pressedButton = buttons_checkButtons(true, true);
			//Normal check when not in minChange or maxChange mode
			else
				pressedButton = buttons_checkButtons();
			//RESET
			if (pressedButton == 0)
			{
				autoMode = true;
				createThermalImg(true);
				autoMode = false;
			}
			//SELECT
			else if (pressedButton == 1)
			{
				//Leave the minimum or maximum change mode
				if (minChange || maxChange)
				{
					buttons_relabelButton(1, (char *)"OK", true);
					buttons_relabelButton(2, (char *)"Min", true);
					buttons_relabelButton(3, (char *)"Max", true);
					if (minChange == true)
						minChange = false;
					if (maxChange == true)
						maxChange = false;
				}
				//Go back
				else
				{
					if (tempLimitsPresetSaveMenu())
						return true;
					else
						return false;
				}
			}
			//DECREASE
			else if (pressedButton == 2)
			{
				//In minimum change mode - decrease minimum temp
				if ((minChange == true) && (maxChange == false))
				{
					//Check if minimum is in range
					if (min > -70)
					{
						min--;
						minValue = tempToRaw(min);
					}
				}
				//Enter minimum change mode
				else if ((minChange == false) && (maxChange == false))
				{
					buttons_relabelButton(1, (char *)"Back", true);
					buttons_relabelButton(2, (char *)"-", true);
					buttons_relabelButton(3, (char *)"+", true);
					minChange = true;
				}
				//In maximum change mode - decrease maximum temp
				else if ((minChange == false) && (maxChange == true))
				{
					//Check if maximum is bigger than minimum
					if (max > (min + 1))
					{
						max--;
						maxValue = tempToRaw(max);
					}
				}
			}
			//INCREASE
			else if (pressedButton == 3)
			{
				//In maximum change mode - increase maximum temp
				if ((minChange == false) && (maxChange == true))
				{
					//Check if maximum is in range
					if (max < 380)
					{
						max++;
						maxValue = tempToRaw(max);
					}
				}
				//Enter maximum change mode
				else if ((minChange == false) && (maxChange == false))
				{
					buttons_relabelButton(1, (char *)"Back", true);
					buttons_relabelButton(2, (char *)"-", true);
					buttons_relabelButton(3, (char *)"+", true);
					maxChange = true;
				}
				//In minimum change mode - increase minimum temp
				else if ((minChange == true) && (maxChange == false))
				{
					//Check if minimum is smaller than maximum
					if (min < (max - 1))
					{
						min++;
						minValue = tempToRaw(min);
					}
				}
			}

			createThermalImg(true);
			display_drawBitmap(80, 48, 160, 120, smallBuffer);
		}
	}
}

/* Select the limits in Manual Mode*/
void tempLimitsManual()
{
redraw:
	//Background & title
	mainMenuBackground();
	mainMenuTitle((char *)"Temp. Limits");

	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 192, 142, 34, (char *)"Reset");
	buttons_addButton(166, 192, 142, 34, (char *)"OK");
	buttons_addButton(12, 48, 56, 120, (char *)"Min");
	buttons_addButton(252, 48, 56, 120, (char *)"Max");
	buttons_drawButtons();

	//Prepare the preview image
	autoMode = true;
	createThermalImg(true);
	autoMode = false;

	//Display the preview image
	display_drawBitmap(80, 48, 160, 120, smallBuffer);

	//Draw the border for the preview image
	display_setColor(UI_BORDER);
	display_drawRect(79, 47, 241, 169);

	//Go into the normal touch handler
	if (!tempLimitsManualHandler())
		goto redraw;
}

/* Switch the temperature limits preset string */
void tempLimitsPresetsString(int pos)
{
	char *text = (char *)"";
	switch (pos)
	{
	case 0:
		text = (char *)"New";
		break;
	case 1:
		text = (char *)"Preset 1";
		break;
	case 2:
		text = (char *)"Preset 2";
		break;
	case 3:
		text = (char *)"Preset 3";
		break;
	}
	mainMenuSelection(text);
}

/* Menu to save the temperature limits to a preset */
bool tempLimitsPresets()
{
	//Save the current position inside the menu
	byte tempLimitsMenuPos = 0;
	//Background
	mainMenuBackground();
	//Title
	mainMenuTitle((char *)"Choose Preset");
	//Draw the selection menu
	drawSelectionMenu();
	//Draw the current item
	tempLimitsPresetsString(tempLimitsMenuPos);
	//Save the current position inside the menu
	while (true)
	{
		//Touch screen pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//SELECT
			if (pressedButton == 3)
			{
				switch (tempLimitsMenuPos)
				{
				//New
				case 0:
					tempLimitsManual();
					return true;
					//Load Preset 1
				case 1:
					if (EEPROM.read(eeprom_minMax1Set) == eeprom_setValue)
						EEPROM.write(eeprom_minMaxPreset, minMax_preset1);
					else
					{
						showFullMessage((char *)"Preset 1 not saved", true);
						delay(1000);
						return false;
					}
					break;
					//Load Preset 2
				case 2:
					if (EEPROM.read(eeprom_minMax2Set) == eeprom_setValue)
						EEPROM.write(eeprom_minMaxPreset, minMax_preset2);
					else
					{
						showFullMessage((char *)"Preset 2 not saved", true);
						delay(1000);
						return false;
					}
					break;
					//Load Preset 3
				case 3:
					if (EEPROM.read(eeprom_minMax3Set) == eeprom_setValue)
						EEPROM.write(eeprom_minMaxPreset, minMax_preset3);
					else
					{
						showFullMessage((char *)"Preset 3 not saved", true);
						delay(1000);
						return false;
					}
					break;
				}
				//Read temperature limits from EEPROM
				readTempLimits();
				return true;
			}
			//BACKWARD
			else if (pressedButton == 0)
			{
				if (tempLimitsMenuPos > 0)
					tempLimitsMenuPos--;
				else if (tempLimitsMenuPos == 0)
					tempLimitsMenuPos = 3;
			}
			//FORWARD
			else if (pressedButton == 1)
			{
				if (tempLimitsMenuPos < 3)
					tempLimitsMenuPos++;
				else if (tempLimitsMenuPos == 3)
					tempLimitsMenuPos = 0;
			}
			//BACK
			else if (pressedButton == 2)
				return false;
			//Change the menu name
			tempLimitsPresetsString(tempLimitsMenuPos);
		}
	}
}

/* Temperature Limit Mode Selection */
bool tempLimits()
{
	//Title & Background
	mainMenuBackground();
	mainMenuTitle((char *)"Temp. Limits");

	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 46, 142, 120, (char *)"Auto");
	buttons_addButton(166, 46, 142, 120, (char *)"Manual");
	buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_drawButtons();

	//Touch handler
	while (true)
	{
		//If touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//AUTO
			if (pressedButton == 0)
			{
				//Show message
				showFullMessage((char *)"Please wait..", true);

				//Enable auto mode again and disable limits locked
				autoMode = true;
				limitsLocked = false;

				//Set temperature presets to temporary, so it does not load
				EEPROM.write(eeprom_minMaxPreset, minMax_temporary);

				//Enable auto FFC
				lepton_ffcMode(true);

				//Go back
				return true;
			}

			//MANUAL
			else if (pressedButton == 1)
			{
				//Disable auto mode and limits locked
				autoMode = false;
				limitsLocked = false;

				//Let the user choose the new limits
				return tempLimitsPresets();
			}

			//BACK
			else if (pressedButton == 2)
				return false;
		}
	}
}

/* Switch the current display option item */
void liveDispMenuString(int pos)
{
	char *text = (char *)"";
	switch (pos)
	{
	//Battery
	case 0:
		if (batteryEnabled)
			text = (char *)"Battery On";
		else
			text = (char *)"Battery Off";
		break;
		//Time
	case 1:
		if (timeEnabled)
			text = (char *)"Time On";
		else
			text = (char *)"Time Off";
		break;
		//Date
	case 2:
		if (dateEnabled)
			text = (char *)"Date On";
		else
			text = (char *)"Date Off";
		break;
		//Spot
	case 3:
		if (spotEnabled)
			text = (char *)"Spot On";
		else
			text = (char *)"Spot Off";
		break;
		//Colorbar
	case 4:
		if (colorbarEnabled)
			text = (char *)"Bar On";
		else
			text = (char *)"Bar Off";
		break;
		//Storage
	case 5:
		if (storageEnabled)
			text = (char *)"Storage On";
		else
			text = (char *)"Storage Off";
		break;
		//Filter
	case 6:
		if (filterType == filterType_box)
			text = (char *)"Box-Filter";
		else if (filterType == filterType_gaussian)
			text = (char *)"Gaus-Filter";
		else
			text = (char *)"No Filter";
		break;
		//Text Color
	case 7:
		if (textColor == textColor_black)
			text = (char *)"Black Text";
		else if (textColor == textColor_red)
			text = (char *)"Red Text";
		else if (textColor == textColor_green)
			text = (char *)"Green Text";
		else if (textColor == textColor_blue)
			text = (char *)"Blue Text";
		else
			text = (char *)"White Text";
		break;
		//Hottest or coldest
	case 8:
		if (minMaxPoints == minMaxPoints_disabled)
			text = (char *)"No Cold/Hot";
		else if (minMaxPoints == minMaxPoints_min)
			text = (char *)"Coldest";
		else if (minMaxPoints == minMaxPoints_max)
			text = (char *)"Hottest";
		else
			text = (char *)"Both C/H";
		break;
	}
	mainMenuSelection(text);
}

/* Helper: draw sleek 28px non-touch top header bar (never puts buttons in the bezel corner!) */
static void drawModernHeader(const char* title, const char* rightBadge = nullptr)
{
	display_setColor(UI_SURFACE);
	display_fillRect(0, 0, 319, 28);
	display_setColor(UI_ACCENT);
	display_drawHLine(0, 28, 320);

	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print((char *)title, 12, 8);

	if (rightBadge && rightBadge[0]) {
		int rx = 308 - static_cast<int>(strlen(rightBadge)) * 8;
		display_setColor(UI_TEXT_MUTED);
		display_print((char *)rightBadge, rx, 8);
	}
}

/* Helper: draw bezel-safe bottom pill button (>=12px side margin, >=14px bottom margin) */
static void drawBottomPill(int x, int y, int w, int h, const char* lbl, bool accent = false)
{
	display_setColor(accent ? UI_ACCENT : UI_SURFACE);
	display_fillRoundRect(x, y, x + w, y + h);
	display_setColor(accent ? UI_ACCENT : UI_BORDER);
	display_drawRoundRect(x, y, x + w, y + h);
	display_setFont(smallFont);
	display_setBackColor(accent ? UI_ACCENT : UI_SURFACE);
	display_setColor(accent ? UI_ACCENT_FG : UI_TEXT);
	int tx = x + (w - static_cast<int>(strlen(lbl)) * 8) / 2;
	int ty = y + (h - 12) / 2;
	display_print((char *)lbl, tx, ty);
}

/* Helper: lookup palette name, pointer and length directly from firmware single-source colorschemes */
static void getPaletteMeta(byte idx, const char** nameOut, const byte** mapOut, int* lenOut)
{
	switch (idx) {
	case colorScheme_arctic:        *nameOut = "Arctic";      *mapOut = colorMap_arctic;        *lenOut = 240; break;
	case colorScheme_blackHot:      *nameOut = "Black-Hot";   *mapOut = colorMap_blackHot;      *lenOut = 224; break;
	case colorScheme_blueRed:       *nameOut = "Blue-Red";    *mapOut = colorMap_blueRed;       *lenOut = 192; break;
	case colorScheme_coldest:       *nameOut = "Coldest";     *mapOut = colorMap_coldest;       *lenOut = 224; break;
	case colorScheme_contrast:      *nameOut = "Contrast";    *mapOut = colorMap_contrast;      *lenOut = 224; break;
	case colorScheme_doubleRainbow: *nameOut = "Dbl-Rain";    *mapOut = colorMap_doubleRainbow; *lenOut = 256; break;
	case colorScheme_grayRed:       *nameOut = "Gray-Red";    *mapOut = colorMap_grayRed;       *lenOut = 224; break;
	case colorScheme_glowBow:       *nameOut = "Glowbow";     *mapOut = colorMap_glowBow;       *lenOut = 224; break;
	case colorScheme_grayscale:     *nameOut = "Grayscale";   *mapOut = colorMap_grayscale;     *lenOut = 256; break;
	case colorScheme_hottest:       *nameOut = "Hottest";     *mapOut = colorMap_hottest;       *lenOut = 224; break;
	case colorScheme_ironblack:     *nameOut = "Ironblack";   *mapOut = colorMap_ironblack;     *lenOut = 256; break;
	case colorScheme_lava:          *nameOut = "Lava";        *mapOut = colorMap_lava;          *lenOut = 240; break;
	case colorScheme_medical:       *nameOut = "Medical";     *mapOut = colorMap_medical;       *lenOut = 224; break;
	case colorScheme_rainbow:       *nameOut = "Rainbow";     *mapOut = colorMap_rainbow;       *lenOut = 256; break;
	case colorScheme_wheel1:        *nameOut = "Wheel 1";     *mapOut = colorMap_wheel1;        *lenOut = 256; break;
	case colorScheme_wheel2:        *nameOut = "Wheel 2";     *mapOut = colorMap_wheel2;        *lenOut = 256; break;
	case colorScheme_wheel3:        *nameOut = "Wheel 3";     *mapOut = colorMap_wheel3;        *lenOut = 256; break;
	case colorScheme_whiteHot:      *nameOut = "White-Hot";   *mapOut = colorMap_whiteHot;      *lenOut = 224; break;
	default:                        *nameOut = "Yellow";      *mapOut = colorMap_yellow;        *lenOut = 224; break;
	}
}

/* Draw 1-tap 3x3 Display & Theme Toggle Grid with bezel-safe bottom Back / Live pills */
void drawLiveDispGrid()
{
	display_fillScr(UI_BG_DARK);
	drawModernHeader("DISPLAY & HUD", ui_themeName(uiThemeMode));

	buttons_deleteAllButtons();
	buttons_addButton(12, 192, 142, 34, (char *)"", 0x0010);  // 0: < Back

	const char* labels[9] = {"Battery", "Time", "Date", "Spot", "Scale", "Storage", "Filter", "Theme", "Min/Max"};
	const char* values[9] = {
		batteryEnabled ? "ON" : "OFF",
		timeEnabled ? "ON" : "OFF",
		dateEnabled ? "ON" : "OFF",
		spotEnabled ? "ON" : "OFF",
		colorbarEnabled ? "ON" : "OFF",
		storageEnabled ? "ON" : "OFF",
		(filterType == filterType_gaussian) ? "GAUSS" : (filterType == filterType_box ? "BOX" : "OFF"),
		ui_themeName(uiThemeMode),
		(minMaxPoints == minMaxPoints_both) ? "BOTH" : (minMaxPoints == minMaxPoints_min ? "COLD" : (minMaxPoints == minMaxPoints_max ? "HOT" : "OFF"))
	};
	bool active[9] = {
		batteryEnabled, timeEnabled, dateEnabled, spotEnabled, colorbarEnabled, storageEnabled,
		filterType != filterType_none, true, minMaxPoints != minMaxPoints_disabled
	};

	for (int i = 0; i < 9; ++i) {
		int col = i % 3;
		int row = i / 3;
		int x = 12 + col * 102;
		int y = 35 + row * 51;
		int w = 92, h = 45;
		buttons_addButton(x, y, w, h, (char *)"", 0x0010); // 1..9

		display_setColor(UI_SURFACE);
		display_fillRoundRect(x, y, x + w, y + h);
		display_setColor(active[i] ? UI_ACCENT : UI_BORDER);
		display_drawRoundRect(x, y, x + w, y + h);

		display_setFont(smallFont);
		display_setBackColor(UI_SURFACE);
		display_setColor(UI_TEXT);
		int lx = x + (w - static_cast<int>(strlen(labels[i])) * 8) / 2;
		display_print((char *)labels[i], lx, y + 6);

		// Status chip pill
		display_setColor(active[i] ? UI_ACCENT : UI_ELEVATED);
		display_fillRoundRect(x + 12, y + 22, x + w - 12, y + 39);
		display_setBackColor(active[i] ? UI_ACCENT : UI_ELEVATED);
		display_setColor(active[i] ? UI_ACCENT_FG : UI_TEXT_MUTED);
		int vx = x + (w - static_cast<int>(strlen(values[i])) * 8) / 2;
		display_print((char *)values[i], vx, y + 25);
	}

	buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010); // 10: < Live
	drawBottomPill(12,  192, 142, 34, "< Back", false);
	drawBottomPill(166, 192, 142, 34, "< Live", true);
}

/* Change the live display options & Theme (1-tap toggle cards!) */
bool liveDispMenu()
{
	drawLiveDispGrid();
	while (touch_touched()) {}
	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 0) return false;
			if (pressed == 10) return true;
			if (pressed == 8) {
				// 1-tap Dark -> Light -> Hi-Con Theme cycle persisted in EEPROM
				uint8_t nextMode = (uiThemeMode + 1) % 3;
				ui_setThemeMode(nextMode);
				EEPROM.write(eeprom_uiTheme, nextMode);
				drawLiveDispGrid();
			} else if (pressed >= 1 && pressed <= 9) {
				byte pos = static_cast<byte>(pressed - 1);
				changeDisplayOptions(&pos);
				drawLiveDispGrid();
			}
		}
		delay(5);
	}
}

/* Switch the current color scheme item (kept for API compatibility) */
void colorMenuString(int pos)
{
	const char* name = "";
	const byte* cmap = nullptr;
	int clen = 0;
	getPaletteMeta(static_cast<byte>(pos), &name, &cmap, &clen);
	mainMenuSelection((char *)name);
}

/* Draw one page (4 palettes) of the Visual 2026 Palette Picker with bezel-safe bottom bar */
void drawColorMenuPage(byte page, byte selected)
{
	display_fillScr(UI_BG_DARK);
	char hdr[32];
	const byte totalPages = (colorSchemeTotal + 3) / 4;
	snprintf(hdr, sizeof(hdr), "COLOR PALETTE  %u/%u", (unsigned)(page + 1), (unsigned)totalPages);
	drawModernHeader(hdr);

	buttons_deleteAllButtons();
	buttons_addButton(12,  192, 78,  34, (char *)"", 0x0010); // 0: < Back (bezel-safe!)
	buttons_addButton(96,  192, 44,  34, (char *)"", 0x0010); // 1: Prev
	buttons_addButton(146, 192, 112, 34, (char *)"", 0x0010); // 2: Apply
	buttons_addButton(264, 192, 44,  34, (char *)"", 0x0010); // 3: Next

	for (byte slot = 0; slot < 4; ++slot) {
		byte idx = static_cast<byte>(page * 4 + slot);
		if (idx >= colorSchemeTotal) break;
		int y = 36 + slot * 37;
		buttons_addButton(12, y, 296, 32, (char *)"", 0x0010); // 4..7

		const char* name = "";
		const byte* cmap = nullptr;
		int clen = 0;
		getPaletteMeta(idx, &name, &cmap, &clen);
		bool isSel = (idx == selected);

		display_setColor(UI_SURFACE);
		display_fillRoundRect(12, y, 308, y + 32);
		display_setColor(isSel ? UI_ACCENT : UI_BORDER);
		display_drawRoundRect(12, y, 308, y + 32);

		if (isSel) {
			display_setColor(UI_ACCENT);
			display_fillCircle(24, y + 16, 4);
		}
		display_setFont(smallFont);
		display_setBackColor(UI_SURFACE);
		display_setColor(isSel ? UI_TEXT : UI_TEXT_MUTED);
		display_print((char *)name, isSel ? 36 : 22, y + 10);

		// Real live color gradient bar rendered straight from firmware colorMap_*!
		const int gx0 = 166, gx1 = 296;
		const int gw = gx1 - gx0;
		display_setColor(UI_BORDER);
		display_drawRect(gx0 - 1, y + 7, gx1, y + 25);
		for (int dx = 0; dx < gw; ++dx) {
			int ci = (dx * (clen - 1)) / (gw - 1);
			byte r = cmap[ci * 3];
			byte g = cmap[ci * 3 + 1];
			byte b = cmap[ci * 3 + 2];
			display_setColor(r, g, b);
			display_drawVLine(gx0 + dx, y + 8, 17);
		}
	}

	// Bezel-safe bottom navigation pills (12px side margin, 14px bottom margin)
	drawBottomPill(12,  192, 78,  34, "< Back", false);
	drawBottomPill(96,  192, 44,  34, "<",      false);
	drawBottomPill(146, 192, 112, 34, "Apply",  true);
	drawBottomPill(264, 192, 44,  34, ">",      false);
}

/* Visual 2026 Palette Picker with live gradient bars */
bool colorMenu()
{
	byte selected = colorScheme;
	const byte totalPages = (colorSchemeTotal + 3) / 4;
	byte page = selected / 4;
	drawColorMenuPage(page, selected);
	while (touch_touched()) {}
	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 0) return false;
			if (pressed == 1) {
				page = (page == 0) ? (totalPages - 1) : (page - 1);
				drawColorMenuPage(page, selected);
			} else if (pressed == 2) {
				changeColorScheme(&selected);
				return true;
			} else if (pressed == 3) {
				page = (page + 1 >= totalPages) ? 0 : (page + 1);
				drawColorMenuPage(page, selected);
			} else if (pressed >= 4 && pressed <= 7) {
				byte idx = static_cast<byte>(page * 4 + (pressed - 4));
				if (idx < colorSchemeTotal) {
					selected = idx;
					changeColorScheme(&selected);
					drawColorMenuPage(page, selected);
				}
			}
		}
		delay(5);
	}
}

/* Draws a sleek 2026 minimalist vector tile (zero white-background icons, zero crosshairs!) */
static void drawModernMenuTile(int x, int y, int w, int h, const char* label, byte iconId, bool accentBorder = false)
{
	display_setColor(UI_SURFACE);
	display_fillRoundRect(x, y, x + w, y + h);
	display_setColor(accentBorder ? UI_ACCENT : UI_BORDER);
	display_drawRoundRect(x, y, x + w, y + h);

	int cx = x + w / 2;
	int cy = y + 25;
	display_setColor(UI_TEXT);

	switch (iconId) {
	case 0: // Palette (4 Ironbow thermal gradient bars)
		display_setColor(UI_INDIGO);    display_fillRect(cx - 19, cy - 10, cx - 11, cy + 10);
		display_setColor(UI_MAGENTA);   display_fillRect(cx - 9,  cy - 10, cx - 1,  cy + 10);
		display_setColor(UI_ACCENT);    display_fillRect(cx + 1,  cy - 10, cx + 9,  cy + 10);
		display_setColor(UI_ACCENT_HI); display_fillRect(cx + 11, cy - 10, cx + 19, cy + 10);
		break;
	case 1: // Range / Limits (dual range sliders)
		display_setColor(UI_BORDER);
		display_fillRect(cx - 16, cy - 7, cx + 16, cy - 5);
		display_fillRect(cx - 16, cy + 5, cx + 16, cy + 7);
		display_setColor(UI_ACCENT);
		display_fillCircle(cx + 6, cy - 6, 4);
		display_setColor(UI_TEXT);
		display_fillCircle(cx - 6, cy + 6, 4);
		break;
	case 2: // FFC (concentric optical shutter iris)
		display_setColor(UI_TEXT);
		display_drawCircle(cx, cy, 12);
		display_drawCircle(cx, cy, 11);
		display_setColor(UI_ACCENT);
		display_drawCircle(cx, cy, 7);
		display_fillCircle(cx, cy, 3);
		break;
	case 3: // Points (minimalist thermal map pin)
		display_setColor(UI_TEXT);
		display_drawCircle(cx, cy - 4, 8);
		display_setColor(UI_ACCENT);
		display_fillCircle(cx, cy - 4, 3);
		display_setColor(UI_TEXT);
		display_drawLine(cx - 5, cy + 2, cx, cy + 11);
		display_drawLine(cx + 5, cy + 2, cx, cy + 11);
		break;
	case 4: // Gallery (photo frame)
		display_drawRoundRect(cx - 16, cy - 10, cx + 16, cy + 10);
		display_setColor(UI_ACCENT);
		display_fillCircle(cx - 7, cy - 3, 3);
		display_setColor(UI_TEXT);
		display_drawLine(cx - 12, cy + 8, cx - 2, cy - 2);
		display_drawLine(cx - 2, cy - 2, cx + 4, cy + 4);
		display_drawLine(cx + 4, cy + 4, cx + 11, cy - 3);
		break;
	case 6: // Overlays (HUD frame + colorbar)
		display_drawRoundRect(cx - 16, cy - 10, cx + 16, cy + 10);
		display_setColor(UI_ACCENT);
		display_fillRect(cx + 9, cy - 7, cx + 12, cy + 7);
		display_setColor(UI_TEXT);
		display_fillRect(cx - 12, cy - 6, cx + 2, cy - 4);
		display_fillRect(cx - 12, cy + 4, cx - 2, cy + 6);
		break;
	case 7: // Settings (3 equalizer sliders)
		for (int i = -1; i <= 1; ++i) {
			int yy = cy + i * 8;
			display_setColor(UI_BORDER);
			display_fillRect(cx - 16, yy - 1, cx + 16, yy);
			display_setColor(i == 0 ? UI_ACCENT : UI_TEXT);
			display_fillCircle(cx + i * 8, yy, 3);
		}
		break;
	case 8: // Isotherm / Hot-Cold (split cyan / amber)
		display_setColor(UI_CYAN);
		display_fillRoundRect(cx - 15, cy - 10, cx - 2, cy + 10);
		display_setColor(UI_ACCENT);
		display_fillRoundRect(cx + 2, cy - 10, cx + 15, cy + 10);
		break;
	case 9: // Status / Telemetry (live bar meter)
		display_setColor(UI_BORDER);
		display_fillRect(cx - 14, cy + 2,  cx - 8,  cy + 10);
		display_setColor(UI_TEXT);
		display_fillRect(cx - 4,  cy - 4,  cx + 2,  cy + 10);
		display_setColor(UI_ACCENT);
		display_fillRect(cx + 6,  cy - 10, cx + 12, cy + 10);
		break;
	case 10: // USB MTP (USB connector icon)
		display_setColor(UI_TEXT);
		display_drawRoundRect(cx - 9, cy - 10, cx + 9, cy + 1);
		display_fillRect(cx - 4, cy - 7, cx - 2, cy - 3);
		display_fillRect(cx + 2, cy - 7, cx + 4, cy - 3);
		display_setColor(UI_ACCENT);
		display_fillRect(cx - 12, cy + 1, cx + 12, cy + 9);
		break;
	case 12: // Video (crisp REC ring + red/amber dot)
		display_setColor(UI_TEXT);
		display_drawCircle(cx, cy, 11);
		display_setColor(UI_ERR);
		display_fillCircle(cx, cy, 6);
		break;
	case 13: // Theme (half-filled contrast sun/moon disc)
		display_setColor(UI_ACCENT);
		display_fillCircle(cx, cy, 10);
		display_setColor(UI_SURFACE);
		display_fillRect(cx, cy - 10, cx + 11, cy + 10);
		display_setColor(UI_TEXT);
		display_drawCircle(cx, cy, 10);
		break;
	}

	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(accentBorder ? UI_TEXT : UI_TEXT_MUTED);
	int lx = x + (w - static_cast<int>(strlen(label)) * 8) / 2;
	display_print((char *)label, lx, y + h - 18);
}

/* Draws the redesigned 2026 2-page Main Menu with >=12px safe bezel margins & inset bottom < Live bar */
void drawMainMenu(byte pos)
{
	display_fillScr(UI_BG_DARK);
	drawModernHeader((pos == 0) ? "MAIN MENU  1/2" : "MAIN MENU  2/2", ui_themeName(uiThemeMode));

	buttons_deleteAllButtons();
	// Buttons 0..5 = 3x2 grid cards (92x68, 12px left/right margins)
	buttons_addButton(12,  36,  92, 68, (char *)"", 0x0010);
	buttons_addButton(114, 36,  92, 68, (char *)"", 0x0010);
	buttons_addButton(216, 36,  92, 68, (char *)"", 0x0010);
	buttons_addButton(12,  112, 92, 68, (char *)"", 0x0010);
	buttons_addButton(114, 112, 92, 68, (char *)"", 0x0010);
	buttons_addButton(216, 112, 92, 68, (char *)"", 0x0010);
	// Buttons 6..7 = ergonomic bottom bar (142x36, 12px side margin, 14px bottom margin!)
	buttons_addButton(12,  190, 142, 36, (char *)"", 0x0010); // 6: < Live
	buttons_addButton(166, 190, 142, 36, (char *)"", 0x0010); // 7: More > / < Page 1

	if (pos == 0) {
		drawModernMenuTile(12,  36,  92, 68, "Palette",  0, true);
		drawModernMenuTile(114, 36,  92, 68, "Range",    1);
		drawModernMenuTile(216, 36,  92, 68, "FFC",      2);
		drawModernMenuTile(12,  112, 92, 68, "Points",   3);
		drawModernMenuTile(114, 112, 92, 68, "Gallery",  4);
		drawModernMenuTile(216, 112, 92, 68, "Settings", 7);
	} else {
		drawModernMenuTile(12,  36,  92, 68, "Overlays", 6);
		drawModernMenuTile(114, 36,  92, 68, "Isotherm", 8);
		drawModernMenuTile(216, 36,  92, 68, "Status",   9);
		drawModernMenuTile(12,  112, 92, 68, "USB MTP",  10);
		drawModernMenuTile(114, 112, 92, 68, "Video",    12);
		drawModernMenuTile(216, 112, 92, 68, "Theme",    13);
	}

	drawBottomPill(12,  190, 142, 36, "< Live", true);
	drawBottomPill(166, 190, 142, 36, (pos == 0) ? "More >" : "< Page 1", false);
}

/* Select the action when a card is pressed */
bool mainMenuSelect(byte pos, byte page)
{
	if (page == 0) {
		if (pos == 0) return colorMenu();
		if (pos == 1) return tempLimits();
		if (pos == 2) { lepton_ffc(true); return true; }
		if (pos == 3) return tempPointsMenu();
		if (pos == 4) { loadFiles(); return false; }
		if (pos == 5) { settingsMenu(); settingsMenuHandler(); return false; }
	} else {
		if (pos == 0) return liveDispMenu();
		if (pos == 1) return hotColdMenu();
		if (pos == 2) { showStatusScreen(); return false; }
		if (pos == 3) { if (massStoragePrompt()) setMassStorage(); return false; }
		if (pos == 4) { videoMode(); return false; }
		if (pos == 5) { themeMenu(); return false; }
	}
	return false;
}

/* Touch Handler for the Live Menu (always polls checkSerial() so USB never locks out!) */
void mainMenuHandler(byte *pos)
{
	if (*pos > 1) *pos = 0;
	while (touch_touched()) {}
	while (true)
	{
		checkSerial();
		if (screenOffCheck())
			drawMainMenu(*pos);

		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 6) { // Ergonomic bottom < Live pill
				return;
			}
			if (pressed == 7) { // More > / < Page 1 flip
				*pos = (*pos == 0) ? 1 : 0;
				drawMainMenu(*pos);
				continue;
			}
			if (pressed >= 0 && pressed <= 5) {
				if (mainMenuSelect(static_cast<byte>(pressed), *pos))
					return;
				drawMainMenu(*pos);
			}
		}
		delay(5);
	}
}

/* Start live menu */
void mainMenu()
{
	//Set show menu to opened
	showMenu = showMenu_opened;

	//Position in the main menu
	static byte mainMenuPos = 0;

	//Draw content
	drawMainMenu(mainMenuPos);

	//Touch handler - return true if exit to Main menu, otherwise false
	mainMenuHandler(&mainMenuPos);

	//Restore old fonts
	display_setFont(smallFont);
	buttons_setTextFont(smallFont);

	//Delete the old buttons
	buttons_deleteAllButtons();

	//Wait a short time
	delay(500);
	Serial.clear();
	showMenu = showMenu_disabled;
	lepton_startFrame();
}
