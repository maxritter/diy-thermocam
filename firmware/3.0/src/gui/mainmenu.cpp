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
#include <massstorage.h>
#include <touchscreen.h>
#include <mainmenu.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Draws the background in the main menu */
void mainMenuBackground()
{
	display_setColor(120, 120, 120);
	display_fillRoundRect(6, 6, 314, 234);
	display_setColor(200, 200, 200);
	display_fillRect(6, 36, 314, 180);
	display_setColor(VGA_BLACK);
	display_drawHLine(6, 36, 314);
	display_drawHLine(6, 180, 314);
}

/* Draws the content of the selection menu*/
void drawSelectionMenu()
{
	//Buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(15, 45, 38, 77, (char *)"<", 0, true);
	buttons_addButton(267, 45, 38, 77, (char *)">", 0, true);
	buttons_addButton(15, 188, 120, 40, (char *)"Back");
	buttons_addButton(95, 132, 130, 35, (char *)"OK");
	buttons_drawButtons();
	//Border
	display_setColor(VGA_BLUE);
	display_drawRect(65, 57, 257, 111);
}

/* Draws the title in the main menu */
void mainMenuTitle(char *title)
{
	display_setFont(bigFont);
	display_setBackColor(120, 120, 120);
	display_setColor(VGA_WHITE);
	display_print(title, CENTER, 14);
}

/* Draws the current selection in the menu */
void mainMenuSelection(char *selection)
{
	//Clear the old content
	display_setColor(VGA_WHITE);
	display_fillRect(66, 58, 257, 111);
	//Print the text
	display_setBackColor(VGA_WHITE);
	display_setColor(VGA_BLUE);
	display_print(selection, CENTER, 77);
}

/* Asks the user if he really wants to enter mass storage mode */
bool massStoragePrompt()
{
	//Title & Background
	drawTitle((char *)"USB File Transfer");
	display_setColor(VGA_BLACK);
	display_setFont(smallFont);
	display_setBackColor(200, 200, 200);
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
	display_setColor(VGA_BLACK);
	display_setFont(bigFont);
	display_setBackColor(200, 200, 200);
	display_print((char *)"Try again?", CENTER, 66);
	display_setFont(smallFont);
	display_setBackColor(120, 120, 120);
	display_setColor(VGA_WHITE);
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
		display_setColor(VGA_BLACK);
		display_setBackColor(200, 200, 200);
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
		display_fillScr(200, 200, 200);
		display_setFont(bigFont);
		display_setBackColor(200, 200, 200);
		display_setColor(VGA_BLACK);
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
	buttons_addButton(15, 45, 90, 122, (char *)"Add");
	buttons_addButton(115, 45, 90, 122, (char *)"Remove");
	buttons_addButton(215, 45, 90, 122, (char *)"Clear");
	buttons_addButton(15, 188, 120, 40, (char *)"Back");
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
	char margin[14];

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
	buttons_addButton(15, 188, 140, 40, (char *)"Reset");
	buttons_addButton(165, 188, 140, 40, (char *)"OK");
	buttons_addButton(15, 48, 55, 120, (char *)"-");
	buttons_addButton(250, 48, 55, 120, (char *)"+");
	buttons_drawButtons();

	//Draw the border for the preview image
	display_setColor(VGA_BLACK);
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
	buttons_addButton(15, 45, 90, 122, (char *)"Hot");
	buttons_addButton(115, 45, 90, 122, (char *)"Cold");
	buttons_addButton(215, 45, 90, 122, (char *)"Disabled");
	buttons_addButton(15, 188, 120, 40, (char *)"Back");
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
	buttons_addButton(15, 188, 140, 40, (char *)"Reset");
	buttons_addButton(165, 188, 140, 40, (char *)"OK");
	buttons_addButton(15, 48, 55, 120, (char *)"Min");
	buttons_addButton(250, 48, 55, 120, (char *)"Max");
	buttons_drawButtons();

	//Prepare the preview image
	autoMode = true;
	createThermalImg(true);
	autoMode = false;

	//Display the preview image
	display_drawBitmap(80, 48, 160, 120, smallBuffer);

	//Draw the border for the preview image
	display_setColor(VGA_BLACK);
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
					if (EEPROM.read(eeprom_minMax2Set) == eeprom_setValue)
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
	buttons_addButton(15, 47, 140, 120, (char *)"Auto");
	buttons_addButton(165, 47, 140, 120, (char *)"Manual");
	buttons_addButton(15, 188, 140, 40, (char *)"Back");
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

/* Change the live display options */
bool liveDispMenu()
{
	//Save the current position inside the menu
	static byte displayOptionsPos = 0;
	//Background
	mainMenuBackground();
	//Title
	mainMenuTitle((char *)"Live Disp. Options");
	//Draw the selection menu
	drawSelectionMenu();
	//Rename OK button
	buttons_relabelButton(3, (char *)"Switch", true);
	//Draw the current item
	liveDispMenuString(displayOptionsPos);
	while (true)
	{
		//Touch screen pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//SELECT
			if (pressedButton == 3)
			{
				changeDisplayOptions(&displayOptionsPos);
			}
			//BACK
			if (pressedButton == 2)
			{
				return false;
			}
			//BACKWARD
			else if (pressedButton == 0)
			{
				if (displayOptionsPos > 0)
					displayOptionsPos--;
				else if (displayOptionsPos == 0)
					displayOptionsPos = 8;
			}
			//FORWARD
			else if (pressedButton == 1)
			{
				if (displayOptionsPos < 8)
					displayOptionsPos++;
				else if (displayOptionsPos == 8)
					displayOptionsPos = 0;
			}
			//Change the menu name
			liveDispMenuString(displayOptionsPos);
		}
	}
}

/* Switch the current color scheme item */
void colorMenuString(int pos)
{
	char *text = (char *)"";
	switch (pos)
	{
	case colorScheme_arctic:
		text = (char *)"Arctic";
		break;
	case colorScheme_blackHot:
		text = (char *)"Black-Hot";
		break;
	case colorScheme_blueRed:
		text = (char *)"Blue-Red";
		break;
	case colorScheme_coldest:
		text = (char *)"Coldest";
		break;
	case colorScheme_contrast:
		text = (char *)"Contrast";
		break;
	case colorScheme_doubleRainbow:
		text = (char *)"Double-Rain";
		break;
	case colorScheme_grayRed:
		text = (char *)"Gray-Red";
		break;
	case colorScheme_glowBow:
		text = (char *)"Glowbow";
		break;
	case colorScheme_grayscale:
		text = (char *)"Grayscale";
		break;
	case colorScheme_hottest:
		text = (char *)"Hottest";
		break;
	case colorScheme_ironblack:
		text = (char *)"Ironblack";
		break;
	case colorScheme_lava:
		text = (char *)"Lava";
		break;
	case colorScheme_medical:
		text = (char *)"Medical";
		break;
	case colorScheme_rainbow:
		text = (char *)"Rainbow";
		break;
	case colorScheme_wheel1:
		text = (char *)"Wheel 1";
		break;
	case colorScheme_wheel2:
		text = (char *)"Wheel 2";
		break;
	case colorScheme_wheel3:
		text = (char *)"Wheel 3";
		break;
	case colorScheme_whiteHot:
		text = (char *)"White-Hot";
		break;
	case colorScheme_yellow:
		text = (char *)"Yellow";
		break;
	}
	mainMenuSelection(text);
}

/* Choose the applied color scale */
bool colorMenu()
{
	//Save the current position inside the menu
	byte changeColorPos = colorScheme;
	//Background
	mainMenuBackground();
	//Title
	mainMenuTitle((char *)"Change Color");
	//Draw the selection menu
	drawSelectionMenu();
	//Draw the current item
	colorMenuString(changeColorPos);
	while (true)
	{
		//Touch screen pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//SELECT
			if (pressedButton == 3)
			{
				changeColorScheme(&changeColorPos);
				return true;
			}
			//BACK
			if (pressedButton == 2)
				return false;
			//BACKWARD
			else if (pressedButton == 0)
			{
				if (changeColorPos > 0)
					changeColorPos--;
				else if (changeColorPos == 0)
					changeColorPos = colorSchemeTotal - 1;
			}
			//FORWARD
			else if (pressedButton == 1)
			{
				if (changeColorPos < (colorSchemeTotal - 1))
					changeColorPos++;
				else if (changeColorPos == (colorSchemeTotal - 1))
					changeColorPos = 0;
			}
			//Change the menu name
			colorMenuString(changeColorPos);
		}
	}
}

/* Draws the content of the main menu*/
void drawMainMenu(byte pos)
{
	//Border
	drawMainMenuBorder();
	//Background
	display_setColor(200, 200, 200);
	display_fillRoundRect(6, 6, 314, 234);
	//Buttons
	buttons_deleteAllButtons();

	//Page 1
	if (pos == 0)
	{
		//1.1 Temperature points
		buttons_addButton(23, 28, 80, 80, iconTempPointsBMP, iconTempPointsPalette);
		//1.2 Temperature limits
		buttons_addButton(120, 28, 80, 80, iconTempLimitsBMP, iconTempLimitsPalette);
		//1.3 Hot / Cold
		buttons_addButton(217, 28, 80, 80, iconHotColdBMP, iconHotColdPalette);
	}

	//Page 2
	if (pos == 1)
	{
		//2.1 Load menu
		buttons_addButton(23, 28, 80, 80, iconLoadMenuBMP, iconLoadMenuPalette);
		//2.2 Settings menu
		buttons_addButton(120, 28, 80, 80, iconSettingsMenuBMP, iconSettingsMenuPalette);
		//2.3 Change color
		buttons_addButton(217, 28, 80, 80, iconChangeColorBMP, iconChangeColorPalette);
	}

	//Page 3
	if (pos == 2)
	{
		//3.1 Display settings
		buttons_addButton(23, 28, 80, 80, iconDisplaySettingsBMP, iconDisplaySettingsPalette);
		//3.2 Trigger shutter
		buttons_addButton(120, 28, 80, 80, iconShutterBMP, iconShutterPalette);
		//3.3 Display off for radiometric
		buttons_addButton(217, 28, 80, 80, iconDisplayOffBMP, iconDisplayOffPalette);
	}

	buttons_addButton(23, 132, 80, 80, iconBWBitmap, iconBWColors);
	buttons_addButton(120, 132, 80, 80, iconReturnBitmap, iconReturnColors);
	buttons_addButton(217, 132, 80, 80, iconFWBitmap, iconFWColors);
	buttons_drawButtons();
}

/* Select the action when the select button is pressed */
bool mainMenuSelect(byte pos, byte page)
{
	//Page 1
	if (page == 0)
	{
		//1.1 Temperature points
		if (pos == 0)
		{
			return tempPointsMenu();
		}
		//1.2 Temperature limits
		if (pos == 1)
		{
			return tempLimits();
		}
		//1.3 Hot / Cold
		if (pos == 2)
		{
			return hotColdMenu();
		}
	}

	//Page 2
	if (page == 1)
	{
		//2.1 Load menu
		if (pos == 0)
		{
			loadFiles();
		}
		//2.2 Settings menu
		if (pos == 1)
		{
			settingsMenu();
			settingsMenuHandler();
		}
		//2.3 Change color
		if (pos == 2)
		{
			return colorMenu();
		}
	}

	//Page 3
	if (page == 2)
	{
		//3.1 Display settings
		if (pos == 0)
		{
			return liveDispMenu();
		}
		//3.2 Trigger shutter
		if (pos == 1)
		{
			lepton_ffc(true);
		}
		//3.3 Display off
		if (pos == 2)
		{
			toggleDisplay();
		}
	}

	return false;
}

/* Touch Handler for the Live Menu */
void mainMenuHandler(byte *pos)
{
	int numPages = 3;

	//Main loop
	while (true)
	{
		//Enter mass storage on USB connect
		checkMassStorage();

		//Check for screen sleep
		if (screenOffCheck())
			drawMainMenu(*pos);

		//Touch screen pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//FIRST BUTTON
			if (pressedButton == 0)
			{
				//Leave menu
				if (mainMenuSelect(0, *pos))
					break;
			}
			//SECOND BUTTON
			if (pressedButton == 1)
			{
				//Leave menu
				if (mainMenuSelect(1, *pos))
					break;
			}
			//THIRD BUTTON
			if (pressedButton == 2)
			{
				//Leave menu
				if (mainMenuSelect(2, *pos))
					break;
			}
			//BACKWARD
			else if (pressedButton == 3)
			{
				if (*pos > 0)
					*pos = *pos - 1;
				else if (*pos == 0)
					*pos = numPages - 1;
			}
			//EXIT
			if (pressedButton == 4)
			{
				showFullMessage((char *)"Please wait..", true);
				return;
			}
			//FORWARD
			else if (pressedButton == 5)
			{
				if (*pos < numPages - 1)
					*pos = *pos + 1;
				else if (*pos == numPages - 1)
					*pos = 0;
			}
			drawMainMenu(*pos);
		}
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
