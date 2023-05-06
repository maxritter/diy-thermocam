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
#include <settingsmenu.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Second Menu */
void secondMenu(bool firstStart)
{
	drawTitle((char *)"Second", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char *)"-");
	buttons_addButton(230, 60, 70, 70, (char *)"+");
	buttons_addButton(20, 150, 280, 70, (char *)"Back");
	buttons_drawButtons();
	drawCenterElement(second());
	//Touch handler
	while (true)
	{
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (second() >= 0)
				{
					if (second() > 0)
						setTime(hour(), minute(), second() - 1, day(), month(),
								year());
					else if (second() == 0)
						setTime(hour(), minute(), 59, day(), month(), year());
					drawCenterElement(second());
				}
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (second() <= 59)
				{
					if (second() < 59)
						setTime(hour(), minute(), second() + 1, day(), month(),
								year());
					else if (second() == 59)
						setTime(hour(), minute(), 0, day(), month(), year());
					drawCenterElement(second());
				}
			}
			//Back
			else if (pressedButton == 2)
			{
				Teensy3Clock.set(now());
				timeMenu(firstStart);
				break;
			}
		}
	}
}

/* Minute Menu */
void minuteMenu(bool firstStart)
{
	drawTitle((char *)"Minute", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char *)"-");
	buttons_addButton(230, 60, 70, 70, (char *)"+");
	buttons_addButton(20, 150, 280, 70, (char *)"Back");
	buttons_drawButtons();
	drawCenterElement(minute());
	//Touch handler
	while (true)
	{
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (minute() >= 0)
				{
					if (minute() > 0)
						setTime(hour(), minute() - 1, second(), day(), month(),
								year());
					else if (minute() == 0)
						setTime(hour(), 59, second(), day(), month(), year());
					drawCenterElement(minute());
				}
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (minute() <= 59)
				{
					if (minute() < 59)
						setTime(hour(), minute() + 1, second(), day(), month(),
								year());
					else if (minute() == 59)
						setTime(hour(), 0, second(), day(), month(), year());
					drawCenterElement(minute());
				}
			}
			//Back
			else if (pressedButton == 2)
			{
				Teensy3Clock.set(now());
				timeMenu(firstStart);
				break;
			}
		}
	}
}

/* Hour menu */
void hourMenu(bool firstStart)
{
	drawTitle((char *)"Hour", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char *)"-");
	buttons_addButton(230, 60, 70, 70, (char *)"+");
	buttons_addButton(20, 150, 280, 70, (char *)"Back");
	buttons_drawButtons();
	drawCenterElement(hour());
	//Touch handler
	while (true)
	{
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (hour() >= 0)
				{
					if (hour() > 0)
						setTime(hour() - 1, minute(), second(), day(), month(),
								year());
					else if (hour() == 0)
						setTime(23, minute(), second(), day(), month(), year());
					drawCenterElement(hour());
				}
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (hour() <= 23)
				{
					if (hour() < 23)
						setTime(hour() + 1, minute(), second(), day(), month(),
								year());
					else if (hour() == 23)
						setTime(0, minute(), second(), day(), month(), year());
					drawCenterElement(hour());
				}
			}
			//Back
			else if (pressedButton == 2)
			{
				Teensy3Clock.set(now());
				timeMenu(firstStart);
				break;
			}
		}
	}
}

/* Day Menu */
void dayMenu(bool firstStart)
{
	drawTitle((char *)"Day", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char *)"-");
	buttons_addButton(230, 60, 70, 70, (char *)"+");
	buttons_addButton(20, 150, 280, 70, (char *)"Back");
	buttons_drawButtons();
	drawCenterElement(day());
	//Touch handler
	while (true)
	{
		//touch press
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (day() >= 1)
				{
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
			else if (pressedButton == 1)
			{
				if (day() <= 31)
				{
					if (day() < 31)
						setTime(hour(), minute(), second(), day() + 1, month(),
								year());
					else if (day() == 31)
						setTime(hour(), minute(), second(), 1, month(), year());
					drawCenterElement(day());
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
	}
}

/* Month Menu */
void monthMenu(bool firstStart)
{
	drawTitle((char *)"Month", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char *)"-");
	buttons_addButton(230, 60, 70, 70, (char *)"+");
	buttons_addButton(20, 150, 280, 70, (char *)"Back");
	buttons_drawButtons();
	drawCenterElement(month());
	//Touch handler
	while (true)
	{
		//touch press
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (month() >= 1)
				{
					if (month() > 1)
						setTime(hour(), minute(), second(), day(), month() - 1,
								year());
					else if (month() == 1)
						setTime(hour(), minute(), second(), day(), 12, year());
					drawCenterElement(month());
				}
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (month() <= 12)
				{
					if (month() < 12)
						setTime(hour(), minute(), second(), day(), month() + 1,
								year());
					else if (month() == 12)
						setTime(hour(), minute(), second(), day(), 1, year());
					drawCenterElement(month());
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
	}
}

/* Year Menu */
void yearMenu(bool firstStart)
{
	drawTitle((char *)"Year", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char *)"-");
	buttons_addButton(230, 60, 70, 70, (char *)"+");
	buttons_addButton(20, 150, 280, 70, (char *)"Back");
	buttons_drawButtons();
	drawCenterElement(year());
	//Touch handler
	while (true)
	{
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons(true);
			//Minus
			if (pressedButton == 0)
			{
				if (year() > 2021)
				{
					setTime(hour(), minute(), second(), day(), month(),
							year() - 1);
					drawCenterElement(year());
				}
			}
			//Plus
			else if (pressedButton == 1)
			{
				if (year() < 2099)
				{
					setTime(hour(), minute(), second(), day(), month(),
							year() + 1);
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
	}
}

/* Calibrate the battery gauge */
void batteryGauge()
{
	//Title & Background
	drawTitle((char *)"Battery Gauge", true);
	display_setColor(VGA_BLACK);
	display_setFont(smallFont);
	display_setBackColor(200, 200, 200);
	display_print((char *)"Do you want to calibrate the", CENTER, 75);
	display_print((char *)"battery gauge? Fully charge the", CENTER, 95);
	display_print((char *)"battery first (LED green/blue).", CENTER, 115);
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(165, 160, 140, 55, (char *)"Yes");
	buttons_addButton(15, 160, 140, 55, (char *)"No");
	buttons_drawButtons();
	buttons_setTextFont(smallFont);
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
	}

	hardwareMenu();
}

/* Date Menu */
void dateMenu(bool firstStart)
{
	drawTitle((char *)"Date", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char *)"Day");
	buttons_addButton(170, 60, 130, 70, (char *)"Month");
	buttons_addButton(20, 150, 130, 70, (char *)"Year");
	if (firstStart)
		buttons_addButton(170, 150, 130, 70, (char *)"Save");
	else
		buttons_addButton(170, 150, 130, 70, (char *)"Back");
	buttons_drawButtons();
}

/* Date Menu Handler */
void dateMenuHandler(bool firstStart)
{
	while (true)
	{
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
	}
}

/* Time Menu */
void timeMenu(bool firstStart)
{
	drawTitle((char *)"Time", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char *)"Hour");
	buttons_addButton(170, 60, 130, 70, (char *)"Minute");
	buttons_addButton(20, 150, 130, 70, (char *)"Second");
	if (firstStart)
		buttons_addButton(170, 150, 130, 70, (char *)"Save");
	else
		buttons_addButton(170, 150, 130, 70, (char *)"Back");
	buttons_drawButtons();
}

/* Time Menu Handler */
void timeMenuHandler(bool firstStart)
{
	while (true)
	{
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
	}
}

/* General Menu */
void generalMenu()
{
	drawTitle((char *)"Other Settings");
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char *)"Time");
	buttons_addButton(170, 60, 130, 70, (char *)"Date");
	buttons_addButton(20, 150, 130, 70, (char *)"BMP Conversion");
	buttons_addButton(170, 150, 130, 70, (char *)"Back");
	buttons_drawButtons();
}

/* General Menu Handler */
void generalMenuHandler()
{
	while (true)
	{
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
	}
}

/* Convert image selection menu */
void convertImageMenu(bool firstStart)
{
	drawTitle((char *)"BMP Conversion", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char *)"Disabled");
	buttons_addButton(170, 60, 130, 70, (char *)"Enabled");
	if (firstStart)
	{
		buttons_addButton(20, 150, 280, 70, (char *)"Set");
		convertEnabled = false;
	}
	else
		buttons_addButton(20, 150, 280, 70, (char *)"Back");
	buttons_drawButtons();
	if (!convertEnabled)
		buttons_setActive(0);
	else
		buttons_setActive(1);

	//Touch handler
	while (true)
	{
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Diabled
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
	}
}

/* Asks the user if he really wants to format */
void formatStorage()
{
	//Title & Background
	drawTitle((char *)"Storage");
	display_setColor(VGA_BLACK);
	display_setFont(smallFont);
	display_setBackColor(200, 200, 200);
	display_print((char *)"Do you really want to format?", CENTER, 66);
	display_print((char *)"This will delete all images", CENTER, 105);
	display_print((char *)"and videos on the internal storage.", CENTER, 125);
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(165, 160, 140, 55, (char *)"Yes");
	buttons_addButton(15, 160, 140, 55, (char *)"No");
	buttons_drawButtons();
	buttons_setTextFont(smallFont);
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
	}

	hardwareMenu();
}

void changeLeptonGain()
{
	/* Generate menu */
	drawTitle((char *)"Lepton Gain");
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(20, 60, 130, 70, (char *)"-10C - +140C");
	buttons_addButton(170, 60, 130, 70, (char *)"-10C - +450C");
	buttons_addButton(20, 150, 280, 70, (char *)"Save");
	buttons_drawButtons();
	if (leptonGainMode == lepton_gain_high)
		buttons_setActive(0);
	else
		buttons_setActive(1);
	//Touch handler
	while (true)
	{
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
	}
}

/* Hardware menu handler*/
void hardwareMenuHandler()
{
	while (true)
	{
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
			//Back
			else if (pressedButton == 3)
			{
				settingsMenu();
				break;
			}
		}
	}
}

/* Hardware menu */
void hardwareMenu()
{
	drawTitle((char *)"Hardware Settings");
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char *)"Battery Gauge");
	buttons_addButton(170, 60, 130, 70, (char *)"Lepton Gain");
	buttons_addButton(20, 150, 130, 70, (char *)"Format");
	buttons_addButton(170, 150, 130, 70, (char *)"Back");
	buttons_drawButtons();
}

/* Temperature format menu */
void tempFormatMenu(bool firstStart)
{
	drawTitle((char *)"Temp. Format", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char *)"Celcius");
	buttons_addButton(170, 60, 130, 70, (char *)"Fahrenheit");
	buttons_addButton(20, 150, 280, 70, (char *)"Save");
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
		//touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Celcius
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
	}
}

/* Rotate display menu */
void rotateDisplayMenu(bool firstStart)
{
	drawTitle((char *)"Disp. rotation", firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char *)"Rotation");
	buttons_addButton(170, 60, 130, 70, (char *)"Hor. Flip");
	buttons_addButton(20, 150, 130, 70, (char *)"Disabled");
	buttons_addButton(170, 150, 130, 70, (char *)"Save");
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
	}
}

/* Screen timeout menu */
void screenTimeoutMenu()
{
	drawTitle((char *)"Screen timeout");
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char *)"Disabled");
	buttons_addButton(170, 60, 130, 70, (char *)"5 Min.");
	buttons_addButton(20, 150, 130, 70, (char *)"20 Min.");
	buttons_addButton(170, 150, 130, 70, (char *)"Back");
	buttons_drawButtons();
	//Set current one active
	buttons_setActive(screenOffTime);
	//Touch handler
	while (true)
	{
		//Touch pressed
		if (touch_touched() == true)
		{
			int pressedButton = buttons_checkButtons();
			//Set to new color
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
	}
}

/* Display menu handler*/
void displayMenuHandler()
{
	while (true)
	{
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
			//Back
			else if (pressedButton == 3)
			{
				settingsMenu();
				break;
			}
		}
	}
}

/* Display menu */
void displayMenu()
{
	drawTitle((char *)"Display Settings");
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 130, 70, (char *)"Temp. format");
	buttons_addButton(170, 60, 130, 70, (char *)"Disp. rotation");
	buttons_addButton(20, 150, 130, 70, (char *)"Screen timeout");
	buttons_addButton(170, 150, 130, 70, (char *)"Back");
	buttons_drawButtons();
}

/* Touch handler for the settings menu */
void settingsMenuHandler()
{
	while (true)
	{
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
			//Back
			else if (pressedButton == 3)
				break;
		}
	}
}

/* Settings menu main screen */
void settingsMenu()
{
	drawTitle((char *)"Settings");
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(20, 60, 130, 70, (char *)"General");
	buttons_addButton(170, 60, 130, 70, (char *)"Hardware");
	buttons_addButton(20, 150, 130, 70, (char *)"Display");
	buttons_addButton(170, 150, 130, 70, (char *)"Back");
	buttons_drawButtons();
}
