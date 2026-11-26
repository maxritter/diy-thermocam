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
	ui_beginScreen(spec.title, spec.firstStart ? "SETUP" : nullptr, spec.firstStart);
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12,  66,  72,  80, (char *)"-");
	buttons_addButton(236, 66,  72,  80, (char *)"+");
	buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010); // 2: < Back
	buttons_drawButton(0);
	buttons_drawButton(1);
	buttons_setTextFont(smallFont);
	ui_drawPill(12, 192, 142, 34, "< Back", uiPill_secondary);
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

void drawThreeTileSubmenu(const char* title, const char* b0, const char* v0,
                          const char* b1, const char* v1,
                          const char* b2, const char* v2, bool firstStart)
{
	ui_beginScreen(title, firstStart ? "SETUP" : nullptr, firstStart);
	buttons_deleteAllButtons();
	buttons_addButton(12,  46,  142, 62, (char *)"", 0x0010);
	buttons_addButton(166, 46,  142, 62, (char *)"", 0x0010);
	buttons_addButton(12,  118, 142, 62, (char *)"", 0x0010);
	ui_drawCard(12,  46,  142, 62, b0, v0, false);
	ui_drawCard(166, 46,  142, 62, b1, v1, false);
	ui_drawCard(12,  118, 142, 62, b2, v2, false);
	if (firstStart)
	{
		buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010);
		ui_drawPill(166, 192, 142, 34, "Save", uiPill_primary);
	}
	else
	{
		buttons_addButton(12, 192, 142, 34, (char *)"", 0x0010);
		ui_drawPill(12, 192, 142, 34, "< Back", uiPill_secondary);
	}
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
			int pressed = buttons_checkButtons(true);
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

/* Shared two-option binary toggle menu (Disabled/Enabled, C/F, High/Low Gain).
   Unified bottom bar: < Back (left, cancels) + Save (right, commits) when !firstStart. */
bool runBinaryOptionMenu(const char* title, const char* opt0, const char* sub0,
                         const char* opt1, const char* sub1,
                         bool initialState, bool firstStart)
{
	bool state = initialState;
	auto redraw = [&]() {
		ui_beginScreen(title, firstStart ? "SETUP" : nullptr, firstStart);
		buttons_deleteAllButtons();
		buttons_addButton(12,  46,  142, 120, (char *)"", 0x0010); // 0: opt0
		buttons_addButton(166, 46,  142, 120, (char *)"", 0x0010); // 1: opt1
		ui_drawCard(12,  46, 142, 120, opt0, sub0, !state);
		ui_drawCard(166, 46, 142, 120, opt1, sub1,  state);
		if (firstStart)
		{
			buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010); // 2: Set
			ui_drawPill(166, 192, 142, 34, "Set", uiPill_primary);
		}
		else
		{
			buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010); // 2: < Back (cancel)
			buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010); // 3: Save (commit)
			ui_drawPill(12,  192, 142, 34, "< Back", uiPill_secondary);
			ui_drawPill(166, 192, 142, 34, "Save",   uiPill_primary);
		}
	};

	redraw();
	ui_waitTouchRelease();

	while (true)
	{
		if (checkSerial())
			redraw();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 0 && state)
			{
				state = false;
				ui_drawCard(12,  46, 142, 120, opt0, sub0, true);
				ui_drawCard(166, 46, 142, 120, opt1, sub1, false);
			}
			else if (pressed == 1 && !state)
			{
				state = true;
				ui_drawCard(12,  46, 142, 120, opt0, sub0, false);
				ui_drawCard(166, 46, 142, 120, opt1, sub1, true);
			}
			else if (pressed == 2)
			{
				return firstStart ? state : initialState;
			}
			else if (!firstStart && pressed == 3)
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
void secondMenu(bool firstStart) { runNumericStepper({"SET SECOND", 0, 59, true, firstStart, []() { return second(); }, setSec, timeMenu}); }
void minuteMenu(bool firstStart) { runNumericStepper({"SET MINUTE", 0, 59, true, firstStart, []() { return minute(); }, setMin, timeMenu}); }
void hourMenu(bool firstStart)   { runNumericStepper({"SET HOUR",   0, 23, true, firstStart, []() { return hour(); },   setHr,  timeMenu}); }
void dayMenu(bool firstStart)    { runNumericStepper({"SET DAY",    1, 31, true, firstStart, []() { return day(); },    setDy,  dateMenu}); }
void monthMenu(bool firstStart)  { runNumericStepper({"SET MONTH",  1, 12, true, firstStart, []() { return month(); },  setMon, dateMenu}); }
void yearMenu(bool firstStart)   { runNumericStepper({"SET YEAR", 2020, 2099, false, firstStart, []() { return year(); }, setYr, dateMenu}); }

/* Calibrate the battery voltage gauge against a full charge. */
void batteryGauge()
{
	if (ui_confirmDialog("BATTERY GAUGE", "Calibrate battery gauge now?",
		"Fully charge the battery first", "until LED turns green / blue.", true, false))
	{
		checkBattery(false, true);
		showFullMessage((char *)"Battery gauge calibrated", true);
		delay(1000);
	}
	hardwareMenu();
}

/* Date & Time hub menus */
void dateMenu(bool firstStart)
{
	char d[8], m[8], y[8];
	snprintf(d, sizeof(d), "%02d", day());
	snprintf(m, sizeof(m), "%02d", month());
	snprintf(y, sizeof(y), "%04d", year());
	drawThreeTileSubmenu("DATE", "Day", d, "Month", m, "Year", y, firstStart);
}
void dateMenuHandler(bool firstStart) { runThreeTileHandler(firstStart, dateMenu, dayMenu, monthMenu, yearMenu); }

void timeMenu(bool firstStart)
{
	char h[8], m[8], s[8];
	snprintf(h, sizeof(h), "%02d", hour());
	snprintf(m, sizeof(m), "%02d", minute());
	snprintf(s, sizeof(s), "%02d", second());
	drawThreeTileSubmenu("TIME", "Hour", h, "Minute", m, "Second", s, firstStart);
}
void timeMenuHandler(bool firstStart) { runThreeTileHandler(firstStart, timeMenu, hourMenu, minuteMenu, secondMenu); }

/* General Settings menu */
void generalMenu()
{
	char tBuf[12], dBuf[12];
	snprintf(tBuf, sizeof(tBuf), "%02d:%02d", hour(), minute());
	snprintf(dBuf, sizeof(dBuf), "%02d.%02d.%02d", day(), month(), year() % 100);
	ui_beginScreen("GENERAL SETTINGS");
	buttons_deleteAllButtons();
	buttons_addButton(12,  46,  142, 62, (char *)"", 0x0010);
	buttons_addButton(166, 46,  142, 62, (char *)"", 0x0010);
	buttons_addButton(12,  118, 296, 62, (char *)"", 0x0010);
	buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010);
	ui_drawCard(12,  46,  142, 62, "Time", tBuf, false);
	ui_drawCard(166, 46,  142, 62, "Date", dBuf, false);
	ui_drawWideCard(12, 118, 296, 62, "BMP Conversion", convertEnabled ? "ON" : "OFF", convertEnabled);
	ui_drawPill(12, 192, 142, 34, "< Back", uiPill_secondary);
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
			int pressed = buttons_checkButtons(true);
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
		"BMP CONVERSION", "Disabled", "RAW .DAT only", "Enabled", ".DAT + .BMP",
		convertEnabled, firstStart);
	EEPROM.write(eeprom_convertEnabled, convertEnabled);
	if (!firstStart) generalMenu();
}

/* Confirm and format the internal SD card (FAT32 / exFAT). */
void formatStorage()
{
	if (ui_confirmDialog("FORMAT SD CARD", "Format the SD card now?",
		"This deletes ALL images and", "videos on the internal storage.", false, true))
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
	bool prevLow = (leptonGainMode == lepton_gain_low);
	bool isLow = runBinaryOptionMenu(
		"LEPTON GAIN", "High Gain", "-10 to +140C", "Low Gain", "-10 to +450C",
		prevLow, false);
	if (isLow != prevLow)
	{
		leptonGainMode = isLow ? lepton_gain_low : lepton_gain_high;
		if (leptonGainMode == lepton_gain_low) lepton_setLowGain();
		else lepton_setHighGain();
		EEPROM.write(eeprom_lepton_gain, leptonGainMode);
		lepton_ffc(true, true);
	}
	hardwareMenu();
}

/* Hardware Settings menu */
void hardwareMenu()
{
	ui_beginScreen("HARDWARE SETTINGS");
	buttons_deleteAllButtons();
	buttons_addButton(12,  46,  142, 62, (char *)"", 0x0010);
	buttons_addButton(166, 46,  142, 62, (char *)"", 0x0010);
	buttons_addButton(12,  118, 142, 62, (char *)"", 0x0010);
	buttons_addButton(166, 118, 142, 62, (char *)"", 0x0010);
	buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010);
	ui_drawCard(12,  46,  142, 62, "Battery Gauge", "Calibrate", false);
	ui_drawCard(166, 46,  142, 62, "Lepton Gain", (leptonGainMode == lepton_gain_low) ? "Low 450C" : "High 140C", false);
	ui_drawCard(12,  118, 142, 62, "Format SD", "Erase All", false);
	ui_drawCard(166, 118, 142, 62, "USB MTP", "File Xfer", false);
	ui_drawPill(12,  192, 142, 34, "< Back", uiPill_secondary);
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
			int pressed = buttons_checkButtons(true);
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
		"TEMP. FORMAT", "Celsius", "Metric (C)", "Fahrenheit", "Imperial (F)",
		tempFormat == tempFormat_fahrenheit, firstStart);
	tempFormat = isF ? tempFormat_fahrenheit : tempFormat_celcius;
	EEPROM.write(eeprom_tempFormat, tempFormat);
	if (!firstStart) displayMenu();
}

/* Configure 180-degree display rotation or horizontal mirror flip. */
void rotateDisplayMenu(bool firstStart)
{
	const bool origVert = rotationVert;
	const bool origHorz = rotationHorizont;
	int activeIdx = rotationVert ? 0 : (rotationHorizont ? 1 : 2);

	auto drawCards = [&]() {
		ui_drawCard(12,  46,  142, 62, "Rotate 180", "Upside Down", activeIdx == 0);
		ui_drawCard(166, 46,  142, 62, "Hor. Flip",  "Mirror",      activeIdx == 1);
		ui_drawCard(12,  118, 142, 62, "Normal",     "Standard",    activeIdx == 2);
	};
	auto redraw = [&]() {
		ui_beginScreen("DISP. ROTATION", firstStart ? "SETUP" : nullptr, firstStart);
		buttons_deleteAllButtons();
		buttons_addButton(12,  46,  142, 62, (char *)"", 0x0010); // 0: Rotate 180
		buttons_addButton(166, 46,  142, 62, (char *)"", 0x0010); // 1: Hor. Flip
		buttons_addButton(12,  118, 142, 62, (char *)"", 0x0010); // 2: Normal
		drawCards();
		if (firstStart)
		{
			buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010); // 3: Set
			ui_drawPill(166, 192, 142, 34, "Set", uiPill_primary);
		}
		else
		{
			buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010); // 3: < Back (cancel)
			buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010); // 4: Save
			ui_drawPill(12,  192, 142, 34, "< Back", uiPill_secondary);
			ui_drawPill(166, 192, 142, 34, "Save",   uiPill_primary);
		}
	};

	redraw();
	ui_waitTouchRelease();

	while (true)
	{
		if (checkSerial())
			redraw();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed >= 0 && pressed <= 2 && pressed != activeIdx)
			{
				activeIdx = pressed;
				rotationVert = (activeIdx == 0);
				rotationHorizont = (activeIdx == 1);
				drawCards();
			}
			else if (!firstStart && pressed == 3)
			{
				rotationVert = origVert;
				rotationHorizont = origHorz;
				displayMenu();
				return;
			}
			else if ((firstStart && pressed == 3) || (!firstStart && pressed == 4))
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
	byte sel = screenOffTime;
	auto drawCards = [&]() {
		ui_drawCard(12,  46,  142, 62, "Disabled", "Always On",  sel == screenOffTime_disabled);
		ui_drawCard(166, 46,  142, 62, "5 Min.",   "Power Save", sel == screenOffTime_5min);
		ui_drawCard(12,  118, 142, 62, "20 Min.",  "Long Idle",  sel == screenOffTime_20min);
	};
	auto redraw = [&]() {
		ui_beginScreen("SCREEN TIMEOUT");
		buttons_deleteAllButtons();
		buttons_addButton(12,  46,  142, 62, (char *)"", 0x0010); // 0: Disabled
		buttons_addButton(166, 46,  142, 62, (char *)"", 0x0010); // 1: 5 Min
		buttons_addButton(12,  118, 142, 62, (char *)"", 0x0010); // 2: 20 Min
		buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010); // 3: < Back
		buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010); // 4: Save
		drawCards();
		ui_drawPill(12,  192, 142, 34, "< Back", uiPill_secondary);
		ui_drawPill(166, 192, 142, 34, "Save",   uiPill_primary);
	};

	redraw();
	ui_waitTouchRelease();

	while (true)
	{
		if (checkSerial())
			redraw();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed >= 0 && pressed <= 2 && sel != static_cast<byte>(pressed))
			{
				sel = static_cast<byte>(pressed);
				drawCards();
			}
			else if (pressed == 3)
			{
				displayMenu();
				return;
			}
			else if (pressed == 4)
			{
				screenOffTime = sel;
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
	ui_beginScreen("UI THEME", ui_themeName(uiThemeMode));
	buttons_deleteAllButtons();
	buttons_addButton(12, 44,  296, 42, (char *)"", 0x0010);
	buttons_addButton(12, 92,  296, 42, (char *)"", 0x0010);
	buttons_addButton(12, 140, 296, 42, (char *)"", 0x0010);
	buttons_addButton(12, 192, 142, 34, (char *)"", 0x0010);
	ui_drawWideCard(12, 44,  296, 42, "Dark",          "Obsidian",  uiThemeMode == uiTheme_dark);
	ui_drawWideCard(12, 92,  296, 42, "Light",         "Daylight",  uiThemeMode == uiTheme_light);
	ui_drawWideCard(12, 140, 296, 42, "High-Contrast", "Glove/Sun", uiThemeMode == uiTheme_highContrast);
	ui_drawPill(12, 192, 142, 34, "< Back", uiPill_secondary);
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
	const char* rotSub = rotationVert ? "180 deg" : (rotationHorizont ? "H-Flip" : "Normal");
	const char* tmoSub = (screenOffTime == screenOffTime_5min) ? "5 Min."
	                   : ((screenOffTime == screenOffTime_20min) ? "20 Min." : "Off");
	ui_beginScreen("DISPLAY SETTINGS");
	buttons_deleteAllButtons();
	buttons_addButton(12,  46,  142, 62, (char *)"", 0x0010);
	buttons_addButton(166, 46,  142, 62, (char *)"", 0x0010);
	buttons_addButton(12,  118, 142, 62, (char *)"", 0x0010);
	buttons_addButton(166, 118, 142, 62, (char *)"", 0x0010);
	buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010);
	ui_drawCard(12,  46,  142, 62, "Temp. Format", (tempFormat == tempFormat_fahrenheit) ? "Fahrenheit" : "Celsius", false);
	ui_drawCard(166, 46,  142, 62, "Disp. Rotation", rotSub, false);
	ui_drawCard(12,  118, 142, 62, "Screen Timeout", tmoSub, false);
	ui_drawCard(166, 118, 142, 62, "UI Theme", ui_themeName(uiThemeMode), false);
	ui_drawPill(12,  192, 142, 34, "< Back", uiPill_secondary);
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
			int pressed = buttons_checkButtons(true);
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
	ui_beginScreen("SETTINGS", ui_themeName(uiThemeMode));
	buttons_deleteAllButtons();
	buttons_addButton(12,  46,  142, 62, (char *)"", 0x0010);
	buttons_addButton(166, 46,  142, 62, (char *)"", 0x0010);
	buttons_addButton(12,  118, 142, 62, (char *)"", 0x0010);
	buttons_addButton(166, 118, 142, 62, (char *)"", 0x0010);
	buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010);
	ui_drawCard(12,  46,  142, 62, "General",  "Time / BMP",  false);
	ui_drawCard(166, 46,  142, 62, "Hardware", "Gain / MTP",  false);
	ui_drawCard(12,  118, 142, 62, "Display",  "Units / Rot", false);
	ui_drawCard(166, 118, 142, 62, "UI Theme", ui_themeName(uiThemeMode), false);
	ui_drawPill(12,  192, 142, 34, "< Back", uiPill_secondary);
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
			int pressed = buttons_checkButtons(true);
			if (pressed == 0) { generalMenu(); generalMenuHandler(); }
			else if (pressed == 1) { hardwareMenu(); hardwareMenuHandler(); }
			else if (pressed == 2) { displayMenu(); displayMenuHandler(); }
			else if (pressed == 3) { themeMenu(); settingsMenu(); }
			else if (pressed == 4) return;
		}
		delay(5);
	}
}
