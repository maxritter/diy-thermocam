/*
 * SETTINGS MENU - On-device General, Hardware, Display, and Theme settings
 *
 * DIY-Thermocam Firmware — GNU GPL v3.0 — Copyright by Max Ritter
 * http://www.diy-thermocam.net | https://github.com/maxritter/diy-thermocam
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <TimeLib.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <gui.h>
#include <hardware.h>
#include <mainmenu.h>
#include <fonts.h>
#include <lepton.h>
#include <touchscreen.h>
#include <battery.h>
#include <sdcard.h>
#include <create.h>
#include <buttons.h>
#include <massstorage.h>
#include <connection.h>
#include <settingsmenu.h>

namespace {

/* Shared numeric stepper (- / + / < Back) for Time & Date fields. */
struct StepperSpec {
	const char* title;
	int minVal;
	int maxVal;
	bool wrap;
	bool firstStart;
	int (*readVal)();
	void (*writeVal)(int);
	void (*onReturn)(bool);
};

void runNumericStepper(const StepperSpec& spec)
{
	drawTitle(const_cast<char *>(spec.title), spec.firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 54, 72, 76, (char *)"-");
	buttons_addButton(236, 54, 72, 76, (char *)"+");
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 190, 142, 36, (char *)"< Back");
	buttons_drawButtons();
	drawCenterElement(spec.readVal());
	ui_waitTouchRelease();

	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			int cur = spec.readVal();
			if (pressed == 0)
			{
				int next = (cur > spec.minVal) ? (cur - 1) : (spec.wrap ? spec.maxVal : cur);
				if (next != cur) { spec.writeVal(next); drawCenterElement(spec.readVal()); }
			}
			else if (pressed == 1)
			{
				int next = (cur < spec.maxVal) ? (cur + 1) : (spec.wrap ? spec.minVal : cur);
				if (next != cur) { spec.writeVal(next); drawCenterElement(spec.readVal()); }
			}
			else if (pressed == 2)
			{
				Teensy3Clock.set(now());
				spec.onReturn(spec.firstStart);
				return;
			}
		}
		delay(5);
	}
}

void drawThreeTileSubmenu(const char* title, const char* b0, const char* b1, const char* b2, bool firstStart)
{
	drawTitle(const_cast<char *>(title), firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 62, const_cast<char *>(b0));
	buttons_addButton(166, 46, 142, 62, const_cast<char *>(b1));
	buttons_addButton(12, 118, 142, 62, const_cast<char *>(b2));
	if (firstStart)
		buttons_addButton(166, 192, 142, 34, (char *)"Save");
	else
		buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
	if (firstStart)
		buttons_setActive(3);
}

void runThreeTileHandler(bool firstStart, void (*redraw)(bool), void (*act0)(bool), void (*act1)(bool), void (*act2)(bool))
{
	ui_waitTouchRelease();
	while (true)
	{
		if (checkSerial())
			redraw(firstStart);
		if (touch_touched())
		{
			int pressed = buttons_checkButtons();
			if (pressed == 0) act0(firstStart);
			else if (pressed == 1) act1(firstStart);
			else if (pressed == 2) act2(firstStart);
			else if (pressed == 3)
			{
				if (!firstStart) generalMenu();
				return;
			}
		}
		delay(5);
	}
}

/* Shared two-option binary toggle menu (Disabled/Enabled, C/F, High/Low Gain). */
bool runBinaryOptionMenu(const char* title, const char* opt0, const char* opt1, const char* doneLbl,
                         bool initialState, bool doneOnRight, bool firstStart)
{
	drawTitle(const_cast<char *>(title), firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 72, const_cast<char *>(opt0));
	buttons_addButton(166, 46, 142, 72, const_cast<char *>(opt1));
	buttons_addButton(doneOnRight ? 166 : 12, 192, 142, 34, const_cast<char *>(doneLbl));
	buttons_drawButtons();

	bool state = initialState;
	buttons_setActive(state ? 1 : 0);
	ui_waitTouchRelease();

	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons();
			if (pressed == 0 && state)
			{
				state = false;
				buttons_setActive(0);
				buttons_setInactive(1);
			}
			else if (pressed == 1 && !state)
			{
				state = true;
				buttons_setActive(1);
				buttons_setInactive(0);
			}
			else if (pressed == 2)
			{
				return state;
			}
		}
		delay(5);
	}
}

void setSec(int v)  { setTime(hour(), minute(), v, day(), month(), year()); }
void setMin(int v)  { setTime(hour(), v, second(), day(), month(), year()); }
void setHr(int v)   { setTime(v, minute(), second(), day(), month(), year()); }
void setDy(int v)   { setTime(hour(), minute(), second(), v, month(), year()); }
void setMon(int v)  { setTime(hour(), minute(), second(), day(), v, year()); }
void setYr(int v)   { setTime(hour(), minute(), second(), day(), month(), v); }

} // namespace

/* Time & Date field steppers */
void secondMenu(bool firstStart) { runNumericStepper({"Second", 0, 59, true, firstStart, []() { return second(); }, setSec, timeMenu}); }
void minuteMenu(bool firstStart) { runNumericStepper({"Minute", 0, 59, true, firstStart, []() { return minute(); }, setMin, timeMenu}); }
void hourMenu(bool firstStart)   { runNumericStepper({"Hour",   0, 23, true, firstStart, []() { return hour(); },   setHr,  timeMenu}); }
void dayMenu(bool firstStart)    { runNumericStepper({"Day",    1, 31, true, firstStart, []() { return day(); },    setDy,  dateMenu}); }
void monthMenu(bool firstStart)  { runNumericStepper({"Month",  1, 12, true, firstStart, []() { return month(); },  setMon, dateMenu}); }
void yearMenu(bool firstStart)   { runNumericStepper({"Year", 2020, 2099, false, firstStart, []() { return year(); }, setYr, dateMenu}); }

/* Calibrate the battery voltage gauge against a full charge. */
void batteryGauge()
{
	if (ui_confirmDialog("Battery Gauge", "Calibrate battery gauge now?",
		"Fully charge the battery first", "until LED turns green / blue."))
	{
		checkBattery(false, true);
		showFullMessage((char *)"Battery gauge calibrated", true);
		delay(1000);
	}
	hardwareMenu();
}

/* Date & Time hub menus */
void dateMenu(bool firstStart)        { drawThreeTileSubmenu("Date", "Day", "Month", "Year", firstStart); }
void dateMenuHandler(bool firstStart) { runThreeTileHandler(firstStart, dateMenu, dayMenu, monthMenu, yearMenu); }
void timeMenu(bool firstStart)        { drawThreeTileSubmenu("Time", "Hour", "Minute", "Second", firstStart); }
void timeMenuHandler(bool firstStart) { runThreeTileHandler(firstStart, timeMenu, hourMenu, minuteMenu, secondMenu); }

/* General Settings menu */
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

void generalMenuHandler()
{
	ui_waitTouchRelease();
	while (true)
	{
		if (checkSerial())
			generalMenu();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons();
			if (pressed == 0) { timeMenu(); timeMenuHandler(); }
			else if (pressed == 1) { dateMenu(); dateMenuHandler(); }
			else if (pressed == 2) convertImageMenu();
			else if (pressed == 3) { settingsMenu(); return; }
		}
		delay(5);
	}
}

/* Toggle automatic 640x480 BMP companion export alongside raw .DAT captures. */
void convertImageMenu(bool firstStart)
{
	if (firstStart) convertEnabled = false;
	convertEnabled = runBinaryOptionMenu(
		"BMP Conversion", "Disabled", "Enabled",
		firstStart ? "Set" : "< Back",
		convertEnabled, firstStart, firstStart);
	EEPROM.write(eeprom_convertEnabled, convertEnabled);
	if (!firstStart) generalMenu();
}

/* Confirm and format the internal SD card (FAT32 / exFAT). */
void formatStorage()
{
	if (ui_confirmDialog("Format SD Card", "Format the SD card now?",
		"This deletes ALL images and", "videos on the internal storage."))
	{
		showFullMessage((char *)"Format storage..", true);
		if (!formatCard())
		{
			showFullMessage((char *)"Error during formatting!", true);
			delay(1000);
		}
		else
		{
			refreshFreeSpace();
			showFullMessage((char *)"Formatting finished", true);
			delay(1000);
		}
	}
	hardwareMenu();
}

/* Switch FLIR Lepton high-gain (-10..+140 C) vs low-gain (-10..+450 C) radiometric mode. */
void changeLeptonGain()
{
	bool isLow = runBinaryOptionMenu(
		"Lepton Gain", "-10C to +140C", "-10C to +450C", "Save",
		leptonGainMode == lepton_gain_low, true, false);
	leptonGainMode = isLow ? lepton_gain_low : lepton_gain_high;
	if (leptonGainMode == lepton_gain_low) lepton_setLowGain();
	else lepton_setHighGain();
	EEPROM.write(eeprom_lepton_gain, leptonGainMode);
	lepton_ffc(true, true);
	hardwareMenu();
}

/* Hardware Settings menu */
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

void hardwareMenuHandler()
{
	ui_waitTouchRelease();
	while (true)
	{
		if (checkSerial())
			hardwareMenu();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons();
			if (pressed == 0) batteryGauge();
			else if (pressed == 1) changeLeptonGain();
			else if (pressed == 2) formatStorage();
			else if (pressed == 3) { if (massStoragePrompt()) setMassStorage(); hardwareMenu(); }
			else if (pressed == 4) { settingsMenu(); return; }
		}
		delay(5);
	}
}

/* Select Celsius vs Fahrenheit temperature display units. */
void tempFormatMenu(bool firstStart)
{
	if (firstStart) tempFormat = tempFormat_celcius;
	bool isF = runBinaryOptionMenu(
		"Temp. Format", "Celsius (C)", "Fahrenheit (F)",
		firstStart ? "Set" : "Save",
		tempFormat == tempFormat_fahrenheit, true, firstStart);
	tempFormat = isF ? tempFormat_fahrenheit : tempFormat_celcius;
	EEPROM.write(eeprom_tempFormat, tempFormat);
	if (!firstStart) displayMenu();
}

/* Configure 180-degree display rotation or horizontal mirror flip. */
void rotateDisplayMenu(bool firstStart)
{
	drawTitle((char *)"Disp. Rotation", firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 46, 142, 62, (char *)"Rotate 180");
	buttons_addButton(166, 46, 142, 62, (char *)"Hor. Flip");
	buttons_addButton(12, 118, 142, 62, (char *)"Normal");
	buttons_addButton(166, 192, 142, 34, firstStart ? (char *)"Set" : (char *)"Save");
	buttons_drawButtons();

	int activeIdx = rotationVert ? 0 : (rotationHorizont ? 1 : 2);
	buttons_setActive(activeIdx);
	ui_waitTouchRelease();

	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons();
			if (pressed >= 0 && pressed <= 2 && pressed != activeIdx)
			{
				buttons_setInactive(activeIdx);
				activeIdx = pressed;
				buttons_setActive(activeIdx);
				rotationVert = (activeIdx == 0);
				rotationHorizont = (activeIdx == 1);
			}
			else if (pressed == 3)
			{
				EEPROM.write(eeprom_rotationVert, rotationVert);
				EEPROM.write(eeprom_rotationHorizont, rotationHorizont);
				if (firstStart) return;
				setDisplayRotation();
				displayMenu();
				return;
			}
		}
		delay(5);
	}
}

/* Configure automatic display backlight sleep timeout. */
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
	buttons_setActive(screenOffTime);
	ui_waitTouchRelease();

	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons();
			if (pressed >= 0 && pressed <= 2)
			{
				buttons_setInactive(screenOffTime);
				screenOffTime = static_cast<byte>(pressed);
				buttons_setActive(screenOffTime);
			}
			else if (pressed == 3)
			{
				EEPROM.write(eeprom_screenOffTime, screenOffTime);
				initScreenOffTimer();
				displayMenu();
				return;
			}
		}
		delay(5);
	}
}

/* Draw dedicated 2026 Theme Selector screen (Dark / Daylight Light / High-Contrast). */
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

/* Interactive Theme Selector (persisted immediately to EEPROM). */
void themeMenu()
{
	drawThemeMenu();
	ui_waitTouchRelease();
	while (true)
	{
		if (checkSerial())
			drawThemeMenu();
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

/* Display Settings menu */
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

void displayMenuHandler()
{
	ui_waitTouchRelease();
	while (true)
	{
		if (checkSerial())
			displayMenu();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons();
			if (pressed == 0) tempFormatMenu();
			else if (pressed == 1) rotateDisplayMenu();
			else if (pressed == 2) screenTimeoutMenu();
			else if (pressed == 3) { themeMenu(); displayMenu(); }
			else if (pressed == 4) { settingsMenu(); return; }
		}
		delay(5);
	}
}

/* Top-level Settings menu */
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

void settingsMenuHandler()
{
	ui_waitTouchRelease();
	while (true)
	{
		if (checkSerial())
			settingsMenu();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons();
			if (pressed == 0) { generalMenu(); generalMenuHandler(); }
			else if (pressed == 1) { hardwareMenu(); hardwareMenuHandler(); }
			else if (pressed == 2) { displayMenu(); displayMenuHandler(); }
			else if (pressed == 3) { themeMenu(); settingsMenu(); }
			else if (pressed == 4) return;
		}
		delay(5);
	}
}
