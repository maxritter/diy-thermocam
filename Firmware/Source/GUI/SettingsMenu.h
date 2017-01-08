/*
*
* SETTINGS MENU - Adjust different on-device settings
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

/* Methods */

/* Draw the GUI elements for the adjust combined menu */
void adjustCombinedGUI() {
	//Color and font
	changeTextColor();
	display_setFont(bigFont);
	display_setBackColor(VGA_TRANSPARENT);

	//Down arrow
	display_drawLine(149, 225, 159, 235);
	display_drawLine(150, 225, 160, 235);
	display_drawLine(151, 225, 161, 235);
	display_drawLine(159, 235, 169, 225);
	display_drawLine(160, 235, 170, 225);
	display_drawLine(161, 235, 171, 225);
	//Left arrow
	display_drawLine(15, 109, 5, 119);
	display_drawLine(15, 110, 5, 120);
	display_drawLine(15, 111, 5, 121);
	display_drawLine(5, 119, 15, 129);
	display_drawLine(5, 120, 15, 130);
	display_drawLine(5, 121, 15, 131);
	//Up arrow
	display_drawLine(149, 15, 159, 5);
	display_drawLine(150, 15, 160, 5);
	display_drawLine(151, 15, 161, 5);
	display_drawLine(159, 5, 169, 15);
	display_drawLine(160, 5, 170, 15);
	display_drawLine(161, 5, 171, 15);
	//Right arrow
	display_drawLine(305, 109, 315, 119);
	display_drawLine(305, 110, 315, 120);
	display_drawLine(305, 111, 315, 121);
	display_drawLine(315, 119, 305, 129);
	display_drawLine(315, 120, 305, 130);
	display_drawLine(315, 121, 305, 131);
	//Decrease
	display_drawLine(5, 224, 25, 224);
	display_drawLine(5, 225, 25, 225);
	display_drawLine(5, 226, 25, 226);
	//Increase
	display_drawLine(5, 14, 25, 14);
	display_drawLine(5, 15, 25, 15);
	display_drawLine(5, 16, 25, 16);
	display_drawLine(14, 5, 14, 25);
	display_drawLine(15, 5, 15, 25);
	display_drawLine(16, 5, 16, 25);

	//Alpha level
	display_print('A', 300, 5);

	//Color for confirm
	display_setColor(0, 255, 0);
	//Confirm button
	display_drawLine(294, 225, 304, 235);
	display_drawLine(295, 225, 305, 235);
	display_drawLine(296, 225, 306, 235);
	display_drawLine(304, 235, 314, 215);
	display_drawLine(305, 235, 315, 215);
	display_drawLine(306, 235, 316, 215);

	//Restore old font
	display_setFont(smallFont);
}

/* Refresh the screen content in adjust combined menu */
void adjustCombinedRefresh() {
	//Create the combined image
	createVisCombImg();

	//Display it on the screen
	displayBuffer();

	//Display the GUI
	adjustCombinedGUI();
}

/* Shows on the screen that is refreshes */
void adjustCombinedLoading() {
	//Set Text Color
	changeTextColor();
	//set Background transparent
	display_setBackColor(VGA_TRANSPARENT);
	//Give the user a hint that it tries to save
	display_setFont(bigFont);
	//Show text
	display_print((char*) "Please wait..", CENTER, 110);
	//Return to small font
	display_setFont(smallFont);
}

/* Switch the current preset menu item */
void adjustCombinedPresetSaveString(int pos) {
	char* text = (char*) "";
	switch (pos) {
	case 0:
		text = (char*) "Temporary";
		break;
	case 1:
		text = (char*) "Preset 1";
		break;
	case 2:
		text = (char*) "Preset 2";
		break;
	case 3:
		text = (char*) "Preset 3";
		break;
	}
	mainMenuSelection(text);
}

/* Menu to save the adjust combined settings to a preset */
bool adjustCombinedPresetSaveMenu() {
	//Save the current position inside the menu
	byte menuPos = 1;
	//Border
	drawMainMenuBorder();
	//Background
	mainMenuBackground();
	//Title
	mainMenuTitle((char*) "Select Preset");
	//Draw the selection menu
	drawSelectionMenu();
	//Draw the current item
	adjustCombinedPresetSaveString(menuPos);
	//Save the current position inside the menu
	while (true) {
		//Touch screen pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//SELECT
			if (pressedButton == 3) {
				switch (menuPos) {
					//Temporary
				case 0:
					EEPROM.write(eeprom_adjCombPreset, adjComb_temporary);
					break;
					//Preset 1
				case 1:
					EEPROM.write(eeprom_adjComb1Left, adjCombLeft);
					EEPROM.write(eeprom_adjComb1Right, adjCombRight);
					EEPROM.write(eeprom_adjComb1Up, adjCombUp);
					EEPROM.write(eeprom_adjComb1Down, adjCombDown);
					EEPROM.write(eeprom_adjComb1Alpha, round(adjCombAlpha * 100.0));
					EEPROM.write(eeprom_adjComb1Factor, round(adjCombFactor * 100.0));
					EEPROM.write(eeprom_adjComb1Set, eeprom_setValue);
					EEPROM.write(eeprom_adjCombPreset, adjComb_preset1);
					break;
					//Preset 2
				case 2:
					EEPROM.write(eeprom_adjComb2Left, adjCombLeft);
					EEPROM.write(eeprom_adjComb2Right, adjCombRight);
					EEPROM.write(eeprom_adjComb2Up, adjCombUp);
					EEPROM.write(eeprom_adjComb2Down, adjCombDown);
					EEPROM.write(eeprom_adjComb2Alpha, round(adjCombAlpha * 100.0));
					EEPROM.write(eeprom_adjComb2Factor, round(adjCombFactor * 100.0));
					EEPROM.write(eeprom_adjComb2Set, eeprom_setValue);
					EEPROM.write(eeprom_adjCombPreset, adjComb_preset2);
					break;
					//Preset 3
				case 3:
					EEPROM.write(eeprom_adjComb3Left, adjCombLeft);
					EEPROM.write(eeprom_adjComb3Right, adjCombRight);
					EEPROM.write(eeprom_adjComb3Up, adjCombUp);
					EEPROM.write(eeprom_adjComb3Down, adjCombDown);
					EEPROM.write(eeprom_adjComb3Alpha, round(adjCombAlpha * 100.0));
					EEPROM.write(eeprom_adjComb3Factor, round(adjCombFactor * 100.0));
					EEPROM.write(eeprom_adjComb3Set, eeprom_setValue);
					EEPROM.write(eeprom_adjCombPreset, adjComb_preset3);
					break;
				}
				return true;
			}
			//BACKWARD
			else if (pressedButton == 0) {
				if (menuPos > 0)
					menuPos--;
				else if (menuPos == 0)
					menuPos = 3;
			}
			//FORWARD
			else if (pressedButton == 1) {
				if (menuPos < 3)
					menuPos++;
				else if (menuPos == 3)
					menuPos = 0;
			}
			//BACK
			else if (pressedButton == 2)
				return false;
			//Change the menu name
			adjustCombinedPresetSaveString(menuPos);
		}
	}
}

/* Touch handler for the new adjust combined menu */
void adjustCombinedNewMenuHandler(bool firstStart = false) {
	//Touch handler
	while (true) {
		//Touch pressed
		if (touch_touched() == true) {
			//Get coordinates
			TS_Point p = touch_getPoint();
			uint16_t x = p.x;
			uint16_t y = p.y;
			//Increase
			if ((x >= 0) && (x <= 50) && (y >= 0) && (y <= 50)) {
				if (adjCombFactor < 1.0) {
					adjustCombinedLoading();
					adjCombFactor += 0.02;
					adjustCombinedRefresh();
				}
			}
			//Decrease
			else if ((x >= 0) && (x <= 50) && (y >= 190) && (y <= 240)) {
				if (adjCombFactor > 0.70) {
					adjustCombinedLoading();
					adjCombFactor -= 0.02;
					adjustCombinedRefresh();
				}
			}
			//Left
			else if ((x >= 0) && (x <= 50) && (y >= 95) && (y <= 145)) {
				if (adjCombLeft < 10) {
					adjustCombinedLoading();
					if (adjCombRight > 0)
						adjCombRight -= 1;
					else
						adjCombLeft += 1;
					adjustCombinedRefresh();
				}
			}
			//Right
			else if ((x >= 270) && (x <= 320) && (y >= 95) && (y <= 145)) {
				if (adjCombRight < 10) {
					adjustCombinedLoading();
					if (adjCombLeft > 0)
						adjCombLeft -= 1;
					else
						adjCombRight += 1;
					adjustCombinedRefresh();
				}
			}
			//Up
			else if ((x >= 135) && (x <= 185) && (y >= 0) && (y <= 50)) {
				if (adjCombUp < 10) {
					adjustCombinedLoading();
					if (adjCombDown > 0)
						adjCombDown -= 1;
					else
						adjCombUp += 1;
					adjustCombinedRefresh();
				}
			}
			//Down
			else if ((x >= 135) && (x <= 185) && (y >= 190) && (y <= 240)) {
				if (adjCombDown < 10) {
					adjustCombinedLoading();
					if (adjCombUp > 0)
						adjCombUp -= 1;
					else
						adjCombDown += 1;
					adjustCombinedRefresh();
				}
			}
			//OK
			else if ((x >= 270) && (x <= 320) && (y >= 190) && (y <= 240)) {
				//Preset chooser
				if (!firstStart) {
					if (adjustCombinedPresetSaveMenu())
						return;
					else
						adjustCombinedRefresh();
				}
				//First start, save as preset 1
				else {
					EEPROM.write(eeprom_adjComb1Left, adjCombLeft);
					EEPROM.write(eeprom_adjComb1Right, adjCombRight);
					EEPROM.write(eeprom_adjComb1Up, adjCombUp);
					EEPROM.write(eeprom_adjComb1Down, adjCombDown);
					EEPROM.write(eeprom_adjComb1Alpha, round(adjCombAlpha * 100.0));
					EEPROM.write(eeprom_adjComb1Factor, round(adjCombFactor * 100.0));
					EEPROM.write(eeprom_adjComb1Set, eeprom_setValue);
					EEPROM.write(eeprom_adjCombPreset, adjComb_preset1);
					return;
				}
			}
			//Refresh
			else if ((x >= 50) && (x <= 270) && (y >= 50) && (y <= 210)) {
				adjustCombinedLoading();
				adjustCombinedRefresh();
			}
			//Change alpha
			else if ((x >= 270) && (x <= 320) && (y >= 0) && (y <= 50)) {
				char buffer[20];
				if (adjCombAlpha < 0.7)
					adjCombAlpha += 0.1;
				else
					adjCombAlpha = 0.3;
				sprintf(buffer, "Alpha set to %.1f", adjCombAlpha);
				display_setFont(bigFont);
				changeTextColor();
				display_print(buffer, CENTER, 110);
				display_setFont(smallFont);
				adjustCombinedRefresh();
			}
		}
	}
}

/* Adjust combined new menu */
void adjustCombinedNewMenu(bool firstStart = false) {
	//Show loading message
	showFullMessage((char*)"Please wait..");
	//Prepare the preview image
	byte displayMode_old = displayMode;
	displayMode = displayMode_combined;
	//Load the defaults
	adjCombDown = 0;
	adjCombUp = 0;
	adjCombLeft = 0;
	adjCombRight = 0;
	adjCombAlpha = 0.5;
	adjCombFactor = 1.0;
	//Show the preview image
	adjustCombinedRefresh();
	//Run the handler and 
	adjustCombinedNewMenuHandler(firstStart);
	//Show message
	showFullMessage((char*) "Please wait..");
	//Restore the old mode
	displayMode = displayMode_old;
}

/* Switch the current temperature menu item */
void adjustCombinedString(int pos) {
	char* text = (char*) "";
	switch (pos) {
	case 0:
		text = (char*) "New";
		break;
	case 1:
		text = (char*) "Preset 1";
		break;
	case 2:
		text = (char*) "Preset 2";
		break;
	case 3:
		text = (char*) "Preset 3";
		break;
	}
	mainMenuSelection(text);
}

/* Menu to save the adjust combined settings to a preset */
bool adjustCombinedMenu() {
	//Save the current position inside the menu
	byte adjCombMenuPos = 0;
	//Background
	mainMenuBackground();
	//Title
	mainMenuTitle((char*) "Adjust Combined");
	//Draw the selection menu
	drawSelectionMenu();
	//Draw the current item
	adjustCombinedString(adjCombMenuPos);
	//Save the current position inside the menu
	while (true) {
		//Touch screen pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//SELECT
			if (pressedButton == 3) {
				switch (adjCombMenuPos) {
					//New
				case 0:
					adjustCombinedNewMenu();
					return true;
					//Load Preset 1
				case 1:
					EEPROM.write(eeprom_adjCombPreset, adjComb_preset1);
					break;
					//Load Preset 2
				case 2:
					EEPROM.write(eeprom_adjCombPreset, adjComb_preset2);
					break;
					//Load Preset 3
				case 3:
					EEPROM.write(eeprom_adjCombPreset, adjComb_preset3);
					break;
				}
				//Read config from EEPROM
				readAdjustCombined();
				return true;
			}
			//BACKWARD
			else if (pressedButton == 0) {
				if (adjCombMenuPos > 0)
					adjCombMenuPos--;
				else if (adjCombMenuPos == 0)
					adjCombMenuPos = 3;
			}
			//FORWARD
			else if (pressedButton == 1) {
				if (adjCombMenuPos < 3)
					adjCombMenuPos++;
				else if (adjCombMenuPos == 3)
					adjCombMenuPos = 0;
			}
			//BACK
			else if (pressedButton == 2)
				return false;

			//Change the menu name
			adjustCombinedString(adjCombMenuPos);
		}
	}
}


/* Second Menu */
void secondMenu(bool firstStart) {
	drawTitle((char*) "Second", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char*) "-");
	buttons_addButton(230, 60, 70, 70, (char*) "+");
	buttons_addButton(20, 150, 280, 70, (char*) "Back");
	buttons_drawButtons();
	drawCenterElement(second());
	//Touch handler
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0) {
				if (second() >= 0) {
					if (second() > 0)
						setTime(hour(), minute(), second() - 1, day(), month(),
							year());
					else if (second() == 0)
						setTime(hour(), minute(), 59, day(), month(), year());
					drawCenterElement(second());
				}
			}
			//Plus
			else if (pressedButton == 1) {
				if (second() <= 59) {
					if (second() < 59)
						setTime(hour(), minute(), second() + 1, day(), month(),
							year());
					else if (second() == 59)
						setTime(hour(), minute(), 0, day(), month(), year());
					drawCenterElement(second());
				}
			}
			//Back
			else if (pressedButton == 2) {
				Teensy3Clock.set(now());
				timeMenu(firstStart);
				break;
			}
		}
	}
}

/* Minute Menu */
void minuteMenu(bool firstStart) {
	drawTitle((char*) "Minute", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char*) "-");
	buttons_addButton(230, 60, 70, 70, (char*) "+");
	buttons_addButton(20, 150, 280, 70, (char*) "Back");
	buttons_drawButtons();
	drawCenterElement(minute());
	//Touch handler
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0) {
				if (minute() >= 0) {
					if (minute() > 0)
						setTime(hour(), minute() - 1, second(), day(), month(),
							year());
					else if (minute() == 0)
						setTime(hour(), 59, second(), day(), month(), year());
					drawCenterElement(minute());
				}
			}
			//Plus
			else if (pressedButton == 1) {
				if (minute() <= 59) {
					if (minute() < 59)
						setTime(hour(), minute() + 1, second(), day(), month(),
							year());
					else if (minute() == 59)
						setTime(hour(), 0, second(), day(), month(), year());
					drawCenterElement(minute());
				}
			}
			//Back
			else if (pressedButton == 2) {
				Teensy3Clock.set(now());
				timeMenu(firstStart);
				break;
			}
		}
	}
}

/* Hour menu */
void hourMenu(bool firstStart) {
	drawTitle((char*) "Hour", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char*) "-");
	buttons_addButton(230, 60, 70, 70, (char*) "+");
	buttons_addButton(20, 150, 280, 70, (char*) "Back");
	buttons_drawButtons();
	drawCenterElement(hour());
	//Touch handler
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0) {
				if (hour() >= 0) {
					if (hour() > 0)
						setTime(hour() - 1, minute(), second(), day(), month(),
							year());
					else if (hour() == 0)
						setTime(23, minute(), second(), day(), month(), year());
					drawCenterElement(hour());
				}
			}
			//Plus
			else if (pressedButton == 1) {
				if (hour() <= 23) {
					if (hour() < 23)
						setTime(hour() + 1, minute(), second(), day(), month(),
							year());
					else if (hour() == 23)
						setTime(0, minute(), second(), day(), month(), year());
					drawCenterElement(hour());
				}
			}
			//Back
			else if (pressedButton == 2) {
				Teensy3Clock.set(now());
				timeMenu(firstStart);
				break;
			}
		}
	}
}

/* Day Menu */
void dayMenu(bool firstStart) {
	drawTitle((char*) "Day", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char*) "-");
	buttons_addButton(230, 60, 70, 70, (char*) "+");
	buttons_addButton(20, 150, 280, 70, (char*) "Back");
	buttons_drawButtons();
	drawCenterElement(day());
	//Touch handler
	while (true) {
		//touch press
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0) {
				if (day() >= 1) {
					if (day() > 1)
						setTime(hour(), minute(), second(), day() - 1, month(),
							year());
					else if (day() == 1)
						setTime(hour(), minute(), second(), 31, month(),
							year());
					drawCenterElement(day());
				}
			}
			//Plus
			else if (pressedButton == 1) {
				if (day() <= 31) {
					if (day() < 31)
						setTime(hour(), minute(), second(), day() + 1, month(),
							year());
					else if (day() == 31)
						setTime(hour(), minute(), second(), 1, month(), year());
					drawCenterElement(day());
				}
			}
			//Back
			else if (pressedButton == 2) {
				Teensy3Clock.set(now());
				dateMenu(firstStart);
				break;
			}
		}
	}
}

/* Month Menu */
void monthMenu(bool firstStart) {
	drawTitle((char*) "Month", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char*) "-");
	buttons_addButton(230, 60, 70, 70, (char*) "+");
	buttons_addButton(20, 150, 280, 70, (char*) "Back");
	buttons_drawButtons();
	drawCenterElement(month());
	//Touch handler
	while (true) {
		//touch press
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0) {
				if (month() >= 1) {
					if (month() > 1)
						setTime(hour(), minute(), second(), day(), month() - 1,
							year());
					else if (month() == 1)
						setTime(hour(), minute(), second(), day(), 12, year());
					drawCenterElement(month());
				}
			}
			//Plus
			else if (pressedButton == 1) {
				if (month() <= 12) {
					if (month() < 12)
						setTime(hour(), minute(), second(), day(), month() + 1,
							year());
					else if (month() == 12)
						setTime(hour(), minute(), second(), day(), 1, year());
					drawCenterElement(month());
				}
			}
			//Back
			else if (pressedButton == 2) {
				Teensy3Clock.set(now());
				dateMenu(firstStart);
				break;
			}
		}
	}
}

/* Year Menu */
void yearMenu(bool firstStart) {
	drawTitle((char*) "Year", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char*) "-");
	buttons_addButton(230, 60, 70, 70, (char*) "+");
	buttons_addButton(20, 150, 280, 70, (char*) "Back");
	buttons_drawButtons();
	drawCenterElement(year());
	//Touch handler
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0) {
				if (year() > 2017) {
					setTime(hour(), minute(), second(), day(), month(),
						year() - 1);
					drawCenterElement(year());
				}
			}
			//Plus
			else if (pressedButton == 1) {
				if (year() < 2099) {
					setTime(hour(), minute(), second(), day(), month(),
						year() + 1);
					drawCenterElement(year());
				}
			}
			//Back
			else if (pressedButton == 2) {
				Teensy3Clock.set(now());
				dateMenu(firstStart);
				break;
			}
		}
	}
}

/* Calibrate the battery gauge */
void batteryGauge()
{
	//Title & Background
	drawTitle((char*) "Battery Gauge", true);
	display_setColor(VGA_BLACK);
	display_setFont(smallFont);
	display_setBackColor(200, 200, 200);
	display_print((char*)"Do you want to calibrate the", CENTER, 75);
	display_print((char*)"battery gauge? Fully charge the", CENTER, 95);
	display_print((char*)"battery first (LED green/blue)!", CENTER, 115);
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(165, 160, 140, 55, (char*) "Yes");
	buttons_addButton(15, 160, 140, 55, (char*) "No");
	buttons_drawButtons();
	buttons_setTextFont(smallFont);;
	//Touch handler
	while (true) {
		//If touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//YES
			if (pressedButton == 0) {
				//Calc the compensation value
				checkBattery(false, true);

				//Show Message
				showFullMessage((char*) "Battery gauge calibrated!", true);
				delay(1000);

				//Return
				otherMenu();
				return;
			}
			//NO
			else if (pressedButton == 1) {
				otherMenu();
				return;
			}
		}
	}
}

/* Date Menu */
void dateMenu(bool firstStart) {
	drawTitle((char*) "Date", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char*) "Day");
	buttons_addButton(170, 60, 130, 70, (char*) "Month");
	buttons_addButton(20, 150, 130, 70, (char*) "Year");
	if (firstStart)
		buttons_addButton(170, 150, 130, 70, (char*) "Save");
	else
		buttons_addButton(170, 150, 130, 70, (char*) "Back");
	buttons_drawButtons();
}

/* Date Menu Handler */
void dateMenuHandler(bool firstStart = false) {
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Day
			if (pressedButton == 0) {
				dayMenu(firstStart);
			}
			//Month
			else if (pressedButton == 1) {
				monthMenu(firstStart);
			}
			//Year
			else if (pressedButton == 2) {
				yearMenu(firstStart);
			}
			//Back
			else if (pressedButton == 3) {
				if(!firstStart)
					otherMenu();
				break;
			}
		}
	}
}

/* Time Menu */
void timeMenu(bool firstStart) {
	drawTitle((char*) "Time", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char*) "Hour");
	buttons_addButton(170, 60, 130, 70, (char*) "Minute");
	buttons_addButton(20, 150, 130, 70, (char*) "Second");
	if (firstStart)
		buttons_addButton(170, 150, 130, 70, (char*) "Save");
	else
		buttons_addButton(170, 150, 130, 70, (char*) "Back");
	buttons_drawButtons();
}

/* Time Menu Handler */
void timeMenuHandler(bool firstStart = false) {
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Hours
			if (pressedButton == 0) {
				hourMenu(firstStart);
			}
			//Minutes
			else if (pressedButton == 1) {
				minuteMenu(firstStart);
			}
			//Seconds
			else if (pressedButton == 2) {
				secondMenu(firstStart);
			}
			//Back
			else if (pressedButton == 3) {
				if(!firstStart)
					otherMenu();
				break;
			}
		}
	}
}

/* Time & Date Menu */
void otherMenu() {
	drawTitle((char*) "Other Settings");
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char*) "Time");
	buttons_addButton(170, 60, 130, 70, (char*) "Date");
	buttons_addButton(20, 150, 130, 70, (char*) "Battery Gauge");
	buttons_addButton(170, 150, 130, 70, (char*) "Back");
	buttons_drawButtons();
}

/* Time & Date Menu Handler */
void otherMenuHandler() {
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Time
			if (pressedButton == 0) {
				timeMenu();
				timeMenuHandler();
			}
			//Date
			else if (pressedButton == 1) {
				dateMenu();
				dateMenuHandler();
			}
			//Battery Gauge
			else if (pressedButton == 2) {
				batteryGauge();
			}
			//Back
			else if (pressedButton == 3) {
				settingsMenu();
				break;
			}
		}
	}
}



/* Visual image selection menu */
void visualImageMenu(bool firstStart = false) {
	drawTitle((char*) "Visual image", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char*) "Disabled");
	buttons_addButton(170, 60, 130, 70, (char*) "Save JPEG");
	if (firstStart) {
		buttons_addButton(20, 150, 280, 70, (char*) "Set");
		visualEnabled = true;
	}
	else
		buttons_addButton(20, 150, 280, 70, (char*) "Back");
	buttons_drawButtons();
	if (!visualEnabled)
		buttons_setActive(0);
	else
		buttons_setActive(1);
	//Touch handler
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Disabled
			if (pressedButton == 0) {
				if (visualEnabled) {
					visualEnabled = false;
					buttons_setActive(0);
					buttons_setInactive(1);
				}
			}
			//Save JPEG
			else if (pressedButton == 1) {
				if (!visualEnabled) {
					visualEnabled = true;
					buttons_setActive(1);
					buttons_setInactive(0);
				}
			}
			//Save
			else if (pressedButton == 2) {
				//Write new settings to EEPROM
				EEPROM.write(eeprom_visualEnabled, visualEnabled);
				if (firstStart)
					return;
				else {
					storageMenu();
				}
				break;
			}
		}
	}
}

/* Convert image selection menu */
void convertImageMenu(bool firstStart = false) {
	drawTitle((char*) "Convert image", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char*) "DAT only");
	buttons_addButton(170, 60, 130, 70, (char*) "BMP & DAT");
	if (firstStart) {
		buttons_addButton(20, 150, 280, 70, (char*) "Set");
		convertEnabled = false;
	}
	else
		buttons_addButton(20, 150, 280, 70, (char*) "Back");
	buttons_drawButtons();
	if (!convertEnabled)
		buttons_setActive(0);
	else
		buttons_setActive(1);

	//Touch handler
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//DAT only
			if (pressedButton == 0) {
				if (convertEnabled) {
					convertEnabled = false;
					buttons_setActive(0);
					buttons_setInactive(1);
				}
			}
			//BMP & DAT
			else if (pressedButton == 1) {
				if (!convertEnabled) {
					convertEnabled = true;
					buttons_setActive(1);
					buttons_setInactive(0);
				}
			}

			//Save
			else if (pressedButton == 2) {
				//Write new settings to EEPROM
				EEPROM.write(eeprom_convertEnabled, convertEnabled);
				if (firstStart)
					return;
				else {
					storageMenu();
				}
				break;
			}
		}
	}
}

/* Asks the user if he really wants to format */
void formatStorage() {
	//ThermocamV4 or DIY-Thermocam V2, check SD card
	if ((mlx90614Version == mlx90614Version_old) ||
		(teensyVersion == teensyVersion_new)) {
		showFullMessage((char*) "Checking SD card..", true);
		if (!checkSDCard()) {
			showFullMessage((char*) "Insert SD card!", true);
			delay(1000);
			storageMenu();
			return;
		}
	}
	//Title & Background
	drawTitle((char*) "Storage");
	display_setColor(VGA_BLACK);
	display_setFont(smallFont);
	display_setBackColor(200, 200, 200);
	display_print((char*)"Do you really want to format?", CENTER, 66);
	display_print((char*)"This will delete all images", CENTER, 105);
	display_print((char*)"and videos on the internal storage.", CENTER, 125);
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(165, 160, 140, 55, (char*) "Yes");
	buttons_addButton(15, 160, 140, 55, (char*) "No");
	buttons_drawButtons();
	buttons_setTextFont(smallFont);
	//Touch handler
	while (true) {
		//If touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//YES
			if (pressedButton == 0) {
				showFullMessage((char*) "Format storage..", true);
				formatCard();
				refreshFreeSpace();
				showFullMessage((char*) "Formatting finished!", true);
				delay(1000);
				break;
			}
			//NO
			if (pressedButton == 1) {
				break;
			}
		}
	}
	//Go back to the storage menu
	storageMenu();
}

/* Storage menu handler*/
void storageMenuHandler() {
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Convert image
			if (pressedButton == 0) {
				convertImageMenu();
			}
			//Visual image
			else if (pressedButton == 1) {
				visualImageMenu();
			}
			//Format
			else if (pressedButton == 2) {
				formatStorage();
			}
			//Back
			else if (pressedButton == 3) {
				settingsMenu();
				break;
			}
		}
	}
}

/* Storage menu */
void storageMenu() {
	drawTitle((char*) "Storage Settings");
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char*) "Convert image");
	buttons_addButton(170, 60, 130, 70, (char*) "Visual image");
	buttons_addButton(20, 150, 130, 70, (char*) "Format");
	buttons_addButton(170, 150, 130, 70, (char*) "Back");
	buttons_drawButtons();
}

/* Temperature format menu */
void tempFormatMenu(bool firstStart = false) {
	drawTitle((char*) "Temp. Format", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char*) "Celcius");
	buttons_addButton(170, 60, 130, 70, (char*) "Fahrenheit");
	buttons_addButton(20, 150, 280, 70, (char*) "Save");
	if (firstStart) {
		buttons_relabelButton(2, (char*) "Set", false);
		tempFormat = tempFormat_celcius;
	}
	buttons_drawButtons();
	if (tempFormat == tempFormat_celcius)
		buttons_setActive(tempFormat_celcius);
	else
		buttons_setActive(tempFormat_fahrenheit);
	//Touch handler
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Celcius
			if (pressedButton == 0) {
				if (tempFormat == tempFormat_fahrenheit) {
					tempFormat = tempFormat_celcius;
					buttons_setActive(0);
					buttons_setInactive(1);
				}
			}
			//Fahrenheit
			else if (pressedButton == 1) {
				if (tempFormat == tempFormat_celcius) {
					tempFormat = tempFormat_fahrenheit;
					buttons_setActive(1);
					buttons_setInactive(0);
				}
			}
			//Save
			else if (pressedButton == 2) {
				//Write new settings to EEPROM
				EEPROM.write(eeprom_tempFormat, tempFormat);
				if (firstStart)
					return;
				else {
					displayMenu();
				}
				break;
			}
		}
	}
}

/* Rotate display menu */
void rotateDisplayMenu(bool firstStart = false) {
	drawTitle((char*) "Disp. rotation", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char*) "Enabled");
	buttons_addButton(170, 60, 130, 70, (char*) "Disabled");
	buttons_addButton(20, 150, 280, 70, (char*) "Save");
	if (firstStart)
		buttons_relabelButton(2, (char*) "Set", false);
	buttons_drawButtons();
	if (rotationEnabled)
		buttons_setActive(0);
	else
		buttons_setActive(1);
	//Touch handler
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Enabled
			if (pressedButton == 0) {
				if (!rotationEnabled) {
					rotationEnabled = true;
					buttons_setActive(0);
					buttons_setInactive(1);
				}
			}
			//Disabled
			else if (pressedButton == 1) {
				if (rotationEnabled) {
					rotationEnabled = false;
					buttons_setActive(1);
					buttons_setInactive(0);
				}
			}
			//Save
			else if (pressedButton == 2) {
				//Write new settings to EEPROM
				EEPROM.write(eeprom_rotationEnabled, rotationEnabled);
				if (firstStart)
					return;
				//Set the rotation
				setDisplayRotation();
				//Show the display menu
				displayMenu();
				break;
			}
		}
	}
}

/* Screen timeout menu */
void screenTimeoutMenu() {
	drawTitle((char*) "Screen timeout");
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char*) "Disabled");
	buttons_addButton(170, 60, 130, 70, (char*) "5 Min.");
	buttons_addButton(20, 150, 130, 70, (char*) "20 Min.");
	buttons_addButton(170, 150, 130, 70, (char*) "Back");
	buttons_drawButtons();
	//Set current one active
	buttons_setActive(screenOffTime);
	//Touch handler
	while (true) {
		//Touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Set to new color
			if ((pressedButton == 0) || (pressedButton == 1) || (pressedButton == 2)) {
				buttons_setInactive(screenOffTime);
				screenOffTime = pressedButton;
				buttons_setActive(screenOffTime);
			}
			//Save
			else if (pressedButton == 3) {
				//Write new settings to EEPROM
				EEPROM.write(eeprom_screenOffTime, screenOffTime);
				//Init timer
				initScreenOffTimer();
				//Return to display menu
				displayMenu();
				break;
			}
		}
	}
}


/* Display menu handler*/
void displayMenuHandler() {
	while (true) {
		//touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Temp. format
			if (pressedButton == 0) {
				tempFormatMenu();
			}
			//Rotate display
			else if (pressedButton == 1) {
				rotateDisplayMenu();
			}
			//Screen timeout
			else if (pressedButton == 2) {
				screenTimeoutMenu();
			}
			//Back
			else if (pressedButton == 3) {
				settingsMenu();
				break;
			}
		}
	}
}

/* Display menu */
void displayMenu() {
	drawTitle((char*) "Display Settings");
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char*) "Temp. format");
	buttons_addButton(170, 60, 130, 70, (char*) "Disp. rotation");
	buttons_addButton(20, 150, 130, 70, (char*) "Screen timeout");
	buttons_addButton(170, 150, 130, 70, (char*) "Back");
	buttons_drawButtons();

}

/* Touch handler for the settings menu */
void settingsMenuHandler() {
	while (true) {
		//touch press
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Display
			if (pressedButton == 0) {
				displayMenu();
				displayMenuHandler();
			}
			//Storage
			else if (pressedButton == 1) {
				storageMenu();
				storageMenuHandler();
			}
			//Other
			else if (pressedButton == 2) {
				otherMenu();
				otherMenuHandler();
			}
			//Back
			else if (pressedButton == 3)
				break;
		}
	}
}

/* Settings menu main screen */
void settingsMenu() {
	drawTitle((char*) "Settings");
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(20, 60, 130, 70, (char*) "Display");
	buttons_addButton(170, 60, 130, 70, (char*) "Storage");
	buttons_addButton(20, 150, 130, 70, (char*) "Other");
	buttons_addButton(170, 150, 130, 70, (char*) "Back");
	buttons_drawButtons();
}