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
* https://github.com/maxritter/diy-thermocam
*
*/

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <gui.h>
#include <hardware.h>
#include <mainmenu.h>
#include <TimeLib.h>
#include <fonts.h>
#include <lepton.h>
#include <touchscreen.h>
#include <EEPROM.h>
#include <battery.h>
#include <sdcard.h>
#include <create.h>
#include <buttons.h>
#include <massstorage.h>
#include <connection.h>
#include <settingsmenu.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Second Menu */
void secondMenu(bool firstStart)
{
	drawTitle((char *)"Second", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 54, 72, 76, (char *)"-");
	buttons_addButton(236, 54, 72, 76, (char *)"+");
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 190, 142, 36, (char *)"< Back");
	buttons_drawButtons();
	drawCenterElement(second());
	//Touch handler
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (second() > 0)
					setTime(hour(), minute(), second() - 1, day(), month(), year());
				else
					setTime(hour(), minute(), 59, day(), month(), year());
				drawCenterElement(second());
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (second() < 59)
					setTime(hour(), minute(), second() + 1, day(), month(), year());
				else
					setTime(hour(), minute(), 0, day(), month(), year());
				drawCenterElement(second());
			}
			//Back
			else if (pressedButton == 2)
			{
				Teensy3Clock.set(now());
				timeMenu(firstStart);
				break;
			}
		}
		delay(5);
	}
}

/* Minute Menu */
void minuteMenu(bool firstStart)
{
	drawTitle((char *)"Minute", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 54, 72, 76, (char *)"-");
	buttons_addButton(236, 54, 72, 76, (char *)"+");
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 190, 142, 36, (char *)"< Back");
	buttons_drawButtons();
	drawCenterElement(minute());
	//Touch handler
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (minute() > 0)
					setTime(hour(), minute() - 1, second(), day(), month(), year());
				else
					setTime(hour(), 59, second(), day(), month(), year());
				drawCenterElement(minute());
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (minute() < 59)
					setTime(hour(), minute() + 1, second(), day(), month(), year());
				else
					setTime(hour(), 0, second(), day(), month(), year());
				drawCenterElement(minute());
			}
			//Back
			else if (pressedButton == 2)
			{
				Teensy3Clock.set(now());
				timeMenu(firstStart);
				break;
			}
		}
		delay(5);
	}
}

/* Hour menu */
void hourMenu(bool firstStart)
{
	drawTitle((char *)"Hour", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 54, 72, 76, (char *)"-");
	buttons_addButton(236, 54, 72, 76, (char *)"+");
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 190, 142, 36, (char *)"< Back");
	buttons_drawButtons();
	drawCenterElement(hour());
	//Touch handler
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (hour() > 0)
					setTime(hour() - 1, minute(), second(), day(), month(), year());
				else
					setTime(23, minute(), second(), day(), month(), year());
				drawCenterElement(hour());
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (hour() < 23)
					setTime(hour() + 1, minute(), second(), day(), month(), year());
				else
					setTime(0, minute(), second(), day(), month(), year());
				drawCenterElement(hour());
			}
			//Back
			else if (pressedButton == 2)
			{
				Teensy3Clock.set(now());
				timeMenu(firstStart);
				break;
			}
		}
		delay(5);
	}
}

/* Day Menu */
void dayMenu(bool firstStart)
{
	drawTitle((char *)"Day", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 54, 72, 76, (char *)"-");
	buttons_addButton(236, 54, 72, 76, (char *)"+");
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 190, 142, 36, (char *)"< Back");
	buttons_drawButtons();
	drawCenterElement(day());
	//Touch handler
	while (true)
	{
		checkSerial();
		//touch press
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (day() > 1)
					setTime(hour(), minute(), second(), day() - 1, month(), year());
				else
					setTime(hour(), minute(), second(), 31, month(), year());
				drawCenterElement(day());
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (day() < 31)
					setTime(hour(), minute(), second(), day() + 1, month(), year());
				else
					setTime(hour(), minute(), second(), 1, month(), year());
				drawCenterElement(day());
			}
			//Back
			else if (pressedButton == 2)
			{
				Teensy3Clock.set(now());
				dateMenu(firstStart);
				break;
			}
		}
		delay(5);
	}
}

/* Month Menu */
void monthMenu(bool firstStart)
{
	drawTitle((char *)"Month", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 54, 72, 76, (char *)"-");
	buttons_addButton(236, 54, 72, 76, (char *)"+");
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 190, 142, 36, (char *)"< Back");
	buttons_drawButtons();
	drawCenterElement(month());
	//Touch handler
	while (true)
	{
		checkSerial();
		//touch press
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (month() > 1)
					setTime(hour(), minute(), second(), day(), month() - 1, year());
				else
					setTime(hour(), minute(), second(), day(), 12, year());
				drawCenterElement(month());
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (month() < 12)
					setTime(hour(), minute(), second(), day(), month() + 1, year());
				else
					setTime(hour(), minute(), second(), day(), 1, year());
				drawCenterElement(month());
			}
			//Back
			else if (pressedButton == 2)
			{
				Teensy3Clock.set(now());
				dateMenu(firstStart);
				break;
			}
		}
		delay(5);
	}
}

/* Year Menu */
void yearMenu(bool firstStart)
{
	drawTitle((char *)"Year", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 54, 72, 76, (char *)"-");
	buttons_addButton(236, 54, 72, 76, (char *)"+");
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 190, 142, 36, (char *)"< Back");
	buttons_drawButtons();
	drawCenterElement(year());
	//Touch handler
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (year() > 2020)
				{
					setTime(hour(), minute(), second(), day(), month(), year() - 1);
					drawCenterElement(year());
				}
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (year() < 2099)
				{
					setTime(hour(), minute(), second(), day(), month(), year() + 1);
					drawCenterElement(year());
				}
			}
			//Back
			else if (pressedButton == 2)
			{
				Teensy3Clock.set(now());
				dateMenu(firstStart);
				break;
			}
		}
		delay(5);
	}
}

/* Calibrate the battery gauge */
void batteryGauge()
{
	//Title & 2026 Surface Card
	drawTitle((char *)"Battery Gauge", true);
	display_setColor(UI_SURFACE);
	display_fillRoundRect(12, 46, 308, 162);
	display_setColor(UI_BORDER);
	display_drawRoundRect(12, 46, 308, 162);
	display_setColor(UI_TEXT);
	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_print((char *)"Calibrate battery gauge now?", CENTER, 68);
	display_setColor(UI_TEXT_MUTED);
	display_print((char *)"Fully charge the battery first", CENTER, 102);
	display_print((char *)"until LED turns green / blue.", CENTER, 122);
	//Draw bezel-safe buttons (No left, Yes right)
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(166, 178, 142, 46, (char *)"Yes");
	buttons_addButton(12, 178, 142, 46, (char *)"No");
	buttons_drawButtons();
	buttons_setTextFont(smallFont);
	//Touch handler
	while (true)
	{
		checkSerial();
		//If touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//YES
			if (pressedButton == 0)
			{
				//Calc the compensation value
				checkBattery(false, true);

				//Show Message
				showFullMessage((char *)"Battery gauge calibrated", true);
				delay(1000);
				break;
			}

			//NO
			else if (pressedButton == 1)
			{
				break;
			}
		}
		delay(5);
	}

	hardwareMenu();
}

/* Date Menu */
void dateMenu(bool firstStart)
{
	drawTitle((char *)"Date", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 62, (char *)"Day");
	buttons_addButton(166, 46, 142, 62, (char *)"Month");
	buttons_addButton(12, 118, 142, 62, (char *)"Year");
	if (firstStart)
		buttons_addButton(166, 192, 142, 34, (char *)"Save");
	else
		buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
	if (firstStart)
		buttons_setActive(3);
}

/* Date Menu Handler */
void dateMenuHandler(bool firstStart)
{
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Day
			if (pressedButton == 0)
			{
				dayMenu(firstStart);
			}
			//Month
			else if (pressedButton == 1)
			{
				monthMenu(firstStart);
			}
			//Year
			else if (pressedButton == 2)
			{
				yearMenu(firstStart);
			}
			//Back
			else if (pressedButton == 3)
			{
				if (!firstStart)
					generalMenu();
				break;
			}
		}
		delay(5);
	}
}

/* Time Menu */
void timeMenu(bool firstStart)
{
	drawTitle((char *)"Time", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 62, (char *)"Hour");
	buttons_addButton(166, 46, 142, 62, (char *)"Minute");
	buttons_addButton(12, 118, 142, 62, (char *)"Second");
	if (firstStart)
		buttons_addButton(166, 192, 142, 34, (char *)"Save");
	else
		buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
	if (firstStart)
		buttons_setActive(3);
}

/* Time Menu Handler */
void timeMenuHandler(bool firstStart)
{
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Hours
			if (pressedButton == 0)
			{
				hourMenu(firstStart);
			}
			//Minutes
			else if (pressedButton == 1)
			{
				minuteMenu(firstStart);
			}
			//Seconds
			else if (pressedButton == 2)
			{
				secondMenu(firstStart);
			}
			//Back
			else if (pressedButton == 3)
			{
				if (!firstStart)
					generalMenu();
				break;
			}
		}
		delay(5);
	}
}

/* General Menu */
void generalMenu()
{
	drawTitle((char *)"General Settings");
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 62, (char *)"Time");
	buttons_addButton(166, 46, 142, 62, (char *)"Date");
	buttons_addButton(12, 118, 296, 62, (char *)"BMP Conversion");
	buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
}

/* General Menu Handler */
void generalMenuHandler()
{
	while (true)
	{
		checkSerial();
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();

			//Time
			if (pressedButton == 0)
			{
				timeMenu();
				timeMenuHandler();
			}

			//Date
			else if (pressedButton == 1)
			{
				dateMenu();
				dateMenuHandler();
			}

			//BMP Conversion
			else if (pressedButton == 2)
			{
				convertImageMenu();
			}

			//Back
			else if (pressedButton == 3)
			{
				settingsMenu();
				break;
			}
		}
		delay(5);
	}
}

/* Convert image selection menu */
void convertImageMenu(bool firstStart)
{
	drawTitle((char *)"BMP Conversion", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 72, (char *)"Disabled");
	buttons_addButton(166, 46, 142, 72, (char *)"Enabled");
	if (firstStart)
	{
		buttons_addButton(166, 192, 142, 34, (char *)"Set");
		convertEnabled = false;
	}
	else
		buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
	if (!convertEnabled)
		buttons_setActive(0);
	else
		buttons_setActive(1);

	//Touch handler
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Disabled
			if (pressedButton == 0)
			{
				if (convertEnabled)
				{
					convertEnabled = false;
					buttons_setActive(0);
					buttons_setInactive(1);
				}
			}
			//Enabled
			else if (pressedButton == 1)
			{
				if (!convertEnabled)
				{
					convertEnabled = true;
					buttons_setActive(1);
					buttons_setInactive(0);
				}
			}

			//Save
			else if (pressedButton == 2)
			{
				//Write new settings to EEPROM
				EEPROM.write(eeprom_convertEnabled, convertEnabled);

				if (!firstStart)
					generalMenu();
				return;
			}
		}
		delay(5);
	}
}

/* Asks the user if he really wants to format */
void formatStorage()
{
	//Title & 2026 Surface Card
	drawTitle((char *)"Format SD Card");
	display_setColor(UI_SURFACE);
	display_fillRoundRect(12, 46, 308, 162);
	display_setColor(UI_BORDER);
	display_drawRoundRect(12, 46, 308, 162);
	display_setColor(UI_TEXT);
	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_print((char *)"Format the SD card now?", CENTER, 68);
	display_setColor(UI_TEXT_MUTED);
	display_print((char *)"This deletes ALL images and", CENTER, 102);
	display_print((char *)"videos on the internal storage.", CENTER, 122);
	//Draw bezel-safe buttons (No left, Yes right)
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(166, 178, 142, 46, (char *)"Yes");
	buttons_addButton(12, 178, 142, 46, (char *)"No");
	buttons_drawButtons();
	buttons_setTextFont(smallFont);
	//Touch handler
	while (true)
	{
		checkSerial();
		//If touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//YES
			if (pressedButton == 0)
			{
				showFullMessage((char *)"Format storage..", true);
				if (!formatCard())
				{
					showFullMessage((char *)"Error during formatting!", true);
					delay(1000);
					break;
				}
				refreshFreeSpace();
				showFullMessage((char *)"Formatting finished", true);
				delay(1000);
				break;
			}
			//NO
			if (pressedButton == 1)
			{
				break;
			}
		}
		delay(5);
	}

	hardwareMenu();
}

void changeLeptonGain()
{
	/* Generate menu */
	drawTitle((char *)"Lepton Gain");
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 72, (char *)"-10C to +140C");
	buttons_addButton(166, 46, 142, 72, (char *)"-10C to +450C");
	buttons_addButton(166, 192, 142, 34, (char *)"Save");
	buttons_drawButtons();
	if (leptonGainMode == lepton_gain_high)
		buttons_setActive(0);
	else
		buttons_setActive(1);
	//Touch handler
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//High gain
			if (pressedButton == 0)
			{
				if (leptonGainMode == lepton_gain_low)
				{
					leptonGainMode = lepton_gain_high;
					buttons_setActive(0);
					buttons_setInactive(1);
				}
			}
			//Low gain
			else if (pressedButton == 1)
			{
				if (leptonGainMode == lepton_gain_high)
				{
					leptonGainMode = lepton_gain_low;
					buttons_setActive(1);
					buttons_setInactive(0);
				}
			}
			//Save
			else if (pressedButton == 2)
			{
				//Change gain mode
				if (leptonGainMode == lepton_gain_low)
				{
					lepton_setLowGain();
				}
				else
				{
					lepton_setHighGain();
				}

				//Write new settings to EEPROM
				EEPROM.write(eeprom_lepton_gain, leptonGainMode);

				//Trigger shutter
				lepton_ffc(true, true);

				hardwareMenu();
				return;
			}
		}
		delay(5);
	}
}

/* Hardware menu handler*/
void hardwareMenuHandler()
{
	while (true)
	{
		checkSerial();
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Battery Gauge
			if (pressedButton == 0)
			{
				batteryGauge();
			}
			//Lepton Gain
			else if (pressedButton == 1)
			{
				changeLeptonGain();
			}
			//Format
			else if (pressedButton == 2)
			{
				formatStorage();
			}
			//USB MTP (moved out of automatic plug-in popup into Settings)
			else if (pressedButton == 3)
			{
				if (massStoragePrompt())
					setMassStorage();
				hardwareMenu();
			}
			//Back
			else if (pressedButton == 4)
			{
				settingsMenu();
				break;
			}
		}
		delay(5);
	}
}

/* Hardware menu */
void hardwareMenu()
{
	drawTitle((char *)"Hardware Settings");
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12,  46,  142, 62, (char *)"Battery Gauge");
	buttons_addButton(166, 46,  142, 62, (char *)"Lepton Gain");
	buttons_addButton(12,  118, 142, 62, (char *)"Format SD");
	buttons_addButton(166, 118, 142, 62, (char *)"USB MTP");
	buttons_addButton(12,  192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
}

/* Temperature format menu */
void tempFormatMenu(bool firstStart)
{
	drawTitle((char *)"Temp. Format", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 72, (char *)"Celsius (C)");
	buttons_addButton(166, 46, 142, 72, (char *)"Fahrenheit (F)");
	buttons_addButton(166, 192, 142, 34, (char *)"Save");
	if (firstStart)
	{
		buttons_relabelButton(2, (char *)"Set", false);
		tempFormat = tempFormat_celcius;
	}
	buttons_drawButtons();
	if (tempFormat == tempFormat_celcius)
		buttons_setActive(tempFormat_celcius);
	else
		buttons_setActive(tempFormat_fahrenheit);
	//Touch handler
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Celsius
			if (pressedButton == 0)
			{
				if (tempFormat == tempFormat_fahrenheit)
				{
					tempFormat = tempFormat_celcius;
					buttons_setActive(0);
					buttons_setInactive(1);
				}
			}
			//Fahrenheit
			else if (pressedButton == 1)
			{
				if (tempFormat == tempFormat_celcius)
				{
					tempFormat = tempFormat_fahrenheit;
					buttons_setActive(1);
					buttons_setInactive(0);
				}
			}
			//Save
			else if (pressedButton == 2)
			{
				//Write new settings to EEPROM
				EEPROM.write(eeprom_tempFormat, tempFormat);
				if (firstStart)
					return;
				else
				{
					displayMenu();
				}
				break;
			}
		}
		delay(5);
	}
}

/* Rotate display menu */
void rotateDisplayMenu(bool firstStart)
{
	drawTitle((char *)"Disp. Rotation", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 62, (char *)"Rotate 180");
	buttons_addButton(166, 46, 142, 62, (char *)"Hor. Flip");
	buttons_addButton(12, 118, 142, 62, (char *)"Normal");
	buttons_addButton(166, 192, 142, 34, (char *)"Save");
	if (firstStart)
		buttons_relabelButton(3, (char *)"Set", false);
	buttons_drawButtons();
	if (rotationVert)
		buttons_setActive(0);
	else if (rotationHorizont)
		buttons_setActive(1);
	else
		buttons_setActive(2);
	//Touch handler
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Rotate 180°
			if (pressedButton == 0)
			{
				if (!rotationVert)
				{
					rotationVert = true;
					rotationHorizont = false;
					buttons_setActive(0);
					buttons_setInactive(1);
					buttons_setInactive(2);
				}
			}
			//Mirror horizontally
			else if (pressedButton == 1)
			{
				if (!rotationHorizont)
				{
					rotationHorizont = true;
					rotationVert = false;
					buttons_setActive(1);
					buttons_setInactive(0);
					buttons_setInactive(2);
				}
			}
			//Disable
			else if (pressedButton == 2)
			{
				rotationVert = false;
				rotationHorizont = false;
				buttons_setActive(2);
				buttons_setInactive(0);
				buttons_setInactive(1);
			}
			//Save
			else if (pressedButton == 3)
			{
				//Write new settings to EEPROM
				EEPROM.write(eeprom_rotationVert, rotationVert);
				EEPROM.write(eeprom_rotationHorizont, rotationHorizont);
				if (firstStart)
					return;
				//Set the rotation
				setDisplayRotation();
				//Show the display menu
				displayMenu();
				break;
			}
		}
		delay(5);
	}
}

/* Screen timeout menu */
void screenTimeoutMenu()
{
	drawTitle((char *)"Screen Timeout");
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 62, (char *)"Disabled");
	buttons_addButton(166, 46, 142, 62, (char *)"5 Min.");
	buttons_addButton(12, 118, 142, 62, (char *)"20 Min.");
	buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
	//Set current one active
	buttons_setActive(screenOffTime);
	//Touch handler
	while (true)
	{
		checkSerial();
		//Touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Set to new timeout
			if ((pressedButton == 0) || (pressedButton == 1) || (pressedButton == 2))
			{
				buttons_setInactive(screenOffTime);
				screenOffTime = pressedButton;
				buttons_setActive(screenOffTime);
			}
			//Save
			else if (pressedButton == 3)
			{
				//Write new settings to EEPROM
				EEPROM.write(eeprom_screenOffTime, screenOffTime);
				//Init timer
				initScreenOffTimer();
				//Return to display menu
				displayMenu();
				break;
			}
		}
		delay(5);
	}
}

/* Draw dedicated 2026 Theme Selector screen (Dark / Daylight Light / High-Contrast) */
void drawThemeMenu()
{
	drawTitle((char *)"UI Theme");
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12,  46,  296, 40, (char *)"Dark  (Obsidian + Amber)");
	buttons_addButton(12,  94,  296, 40, (char *)"Light  (Daylight Outdoor)");
	buttons_addButton(12,  142, 296, 40, (char *)"High-Contrast  (Glove/Sun)");
	buttons_addButton(12,  192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
	buttons_setActive(uiThemeMode);
}

/* Interactive Theme Selector (persisted immediately to EEPROM) */
void themeMenu()
{
	drawThemeMenu();
	while (touch_touched()) {}
	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed >= 0 && pressed <= 2)
			{
				ui_setThemeMode(static_cast<uint8_t>(pressed));
				EEPROM.write(eeprom_uiTheme, uiThemeMode);
				drawThemeMenu();
			}
			else if (pressed == 3)
			{
				return;
			}
		}
		delay(5);
	}
}

/* Display menu handler*/
void displayMenuHandler()
{
	while (true)
	{
		checkSerial();
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Temp. format
			if (pressedButton == 0)
			{
				tempFormatMenu();
			}
			//Rotate display
			else if (pressedButton == 1)
			{
				rotateDisplayMenu();
			}
			//Screen timeout
			else if (pressedButton == 2)
			{
				screenTimeoutMenu();
			}
			//Theme selector (Dark / Light / High-Contrast)
			else if (pressedButton == 3)
			{
				themeMenu();
				displayMenu();
			}
			//Back
			else if (pressedButton == 4)
			{
				settingsMenu();
				break;
			}
		}
		delay(5);
	}
}

/* Display menu */
void displayMenu()
{
	drawTitle((char *)"Display Settings");
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	static char themeLbl[24];
	snprintf(themeLbl, sizeof(themeLbl), "Theme: %s", ui_themeName(uiThemeMode));
	buttons_addButton(12,  46,  142, 62, (char *)"Temp. format");
	buttons_addButton(166, 46,  142, 62, (char *)"Disp. rotation");
	buttons_addButton(12,  118, 142, 62, (char *)"Screen timeout");
	buttons_addButton(166, 118, 142, 62, themeLbl);
	buttons_addButton(12,  192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
}

/* Touch handler for the settings menu */
void settingsMenuHandler()
{
	while (true)
	{
		checkSerial();
		//touch press
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();

			//General
			if (pressedButton == 0)
			{
				generalMenu();
				generalMenuHandler();
			}
			//Hardware
			else if (pressedButton == 1)
			{
				hardwareMenu();
				hardwareMenuHandler();
			}
			//Display
			else if (pressedButton == 2)
			{
				displayMenu();
				displayMenuHandler();
			}
			//Theme selector
			else if (pressedButton == 3)
			{
				themeMenu();
				settingsMenu();
			}
			//Back
			else if (pressedButton == 4)
				break;
		}
		delay(5);
	}
}

/* Settings menu main screen (modern 2026 card grid + Theme selector + inset < Back) */
void settingsMenu()
{
	drawTitle((char *)"Settings");
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	static char themeLbl[24];
	snprintf(themeLbl, sizeof(themeLbl), "Theme: %s", ui_themeName(uiThemeMode));
	buttons_addButton(12,  46,  142, 62, (char *)"General");
	buttons_addButton(166, 46,  142, 62, (char *)"Hardware");
	buttons_addButton(12,  118, 142, 62, (char *)"Display");
	buttons_addButton(166, 118, 142, 62, themeLbl);
	buttons_addButton(12,  192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
}
