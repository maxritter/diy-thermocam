/*
 * TEMP LIMITS, ISOTHERM & TEMP POINTS MENUS
 *
 * DIY-Thermocam Firmware — GNU GPL v3.0 — Copyright by Max Ritter
 * http://www.diy-thermocam.net | https://github.com/maxritter/diy-thermocam
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <buttons.h>
#include <fonts.h>
#include <lepton.h>
#include <temperature.h>
#include <thermal.h>
#include <hardware.h>
#include <create.h>
#include <gui.h>
#include <connection.h>
#include <touchscreen.h>
#include <mainmenu.h>

namespace {

/* Shared carousel selector (< / > / < Back / OK) used by color and preset pickers. */
int runCarouselSelector(const char* title, int initialPos, int itemCount, void (*drawItem)(int))
{
	int pos = initialPos;
	mainMenuBackground();
	mainMenuTitle(const_cast<char *>(title));
	drawSelectionMenu();
	drawItem(pos);
	ui_waitTouchRelease();

	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 3) return pos;
			if (pressed == 2) return -1;
			if (pressed == 0) pos = (pos > 0) ? (pos - 1) : (itemCount - 1);
			else if (pressed == 1) pos = (pos + 1 < itemCount) ? (pos + 1) : 0;
			else { delay(5); continue; }
			drawItem(pos);
		}
		delay(5);
	}
}

struct PresetSlotAddr {
	uint16_t minHigh;
	uint16_t minLow;
	uint16_t maxHigh;
	uint16_t maxLow;
	uint16_t compBase;
	uint16_t setFlag;
	byte presetId;
};

static const PresetSlotAddr kPresetSlots[3] = {
	{eeprom_minValue1High, eeprom_minValue1Low, eeprom_maxValue1High, eeprom_maxValue1Low,
	 eeprom_minMax1Comp, eeprom_minMax1Set, minMax_preset1},
	{eeprom_minValue2High, eeprom_minValue2Low, eeprom_maxValue2High, eeprom_maxValue2Low,
	 eeprom_minMax2Comp, eeprom_minMax2Set, minMax_preset2},
	{eeprom_minValue3High, eeprom_minValue3Low, eeprom_maxValue3High, eeprom_maxValue3Low,
	 eeprom_minMax3Comp, eeprom_minMax3Set, minMax_preset3}
};

void savePresetSlot(byte slotIdx)
{
	if (slotIdx == 0)
	{
		EEPROM.write(eeprom_minMaxPreset, minMax_temporary);
		return;
	}
	const PresetSlotAddr& s = kPresetSlots[slotIdx - 1];
	uint8_t farray[4];
	floatToBytes(farray, -273.15f);
	EEPROM.write(s.minHigh, (minValue >> 8) & 0xFFu);
	EEPROM.write(s.minLow, minValue & 0xFFu);
	EEPROM.write(s.maxHigh, (maxValue >> 8) & 0xFFu);
	EEPROM.write(s.maxLow, maxValue & 0xFFu);
	for (int i = 0; i < 4; i++)
		EEPROM.write(s.compBase + i, farray[i]);
	EEPROM.write(s.setFlag, eeprom_setValue);
	EEPROM.write(eeprom_minMaxPreset, s.presetId);
}

bool loadPresetSlot(byte slotIdx)
{
	const PresetSlotAddr& s = kPresetSlots[slotIdx - 1];
	if (EEPROM.read(s.setFlag) != eeprom_setValue)
	{
		char msg[24];
		snprintf(msg, sizeof(msg), "Preset %u not saved", static_cast<unsigned>(slotIdx));
		showFullMessage(msg, true);
		delay(1000);
		return false;
	}
	EEPROM.write(eeprom_minMaxPreset, s.presetId);
	readTempLimits();
	return true;
}

void drawManualLimitLabels()
{
	char minC[12];
	char maxC[12];
	const int minT = static_cast<int>(round(rawToTemp(minValue)));
	const int maxT = static_cast<int>(round(rawToTemp(maxValue)));
	const char unit = (tempFormat == tempFormat_celcius) ? 'C' : 'F';
	snprintf(minC, sizeof(minC), "Min:%d%c", minT, unit);
	snprintf(maxC, sizeof(maxC), "Max:%d%c", maxT, unit);

	display_setFont(smallFont);
	display_setBackColor(VGA_TRANSPARENT);
	changeTextColor();
	display_print(maxC, 180, 153);
	display_print(minC, 85, 153);
	display_setFont(bigFont);
}

void enterLimitEditMode(bool& editFlag)
{
	buttons_relabelButton(1, (char *)"Back", true);
	buttons_relabelButton(2, (char *)"-", true);
	buttons_relabelButton(3, (char *)"+", true);
	editFlag = true;
}

void leaveLimitEditMode(bool& minChange, bool& maxChange)
{
	buttons_relabelButton(1, (char *)"OK", true);
	buttons_relabelButton(2, (char *)"Min", true);
	buttons_relabelButton(3, (char *)"Max", true);
	minChange = false;
	maxChange = false;
}

void adjustManualMinMax(int pressedButton, bool& minChange, bool& maxChange)
{
	int minT = static_cast<int>(round(rawToTemp(minValue)));
	int maxT = static_cast<int>(round(rawToTemp(maxValue)));

	if (pressedButton == 2)
	{
		if (!minChange && !maxChange)
			enterLimitEditMode(minChange);
		else if (minChange && minT > -70)
			minValue = tempToRaw(static_cast<float>(minT - 1));
		else if (maxChange && maxT > (minT + 1))
			maxValue = tempToRaw(static_cast<float>(maxT - 1));
	}
	else if (pressedButton == 3)
	{
		if (!minChange && !maxChange)
			enterLimitEditMode(maxChange);
		else if (maxChange && maxT < 380)
			maxValue = tempToRaw(static_cast<float>(maxT + 1));
		else if (minChange && minT < (maxT - 1))
			minValue = tempToRaw(static_cast<float>(minT + 1));
	}
}

void drawHotColdLimitLabel()
{
	char margin[20];
	snprintf(margin, sizeof(margin), "Limit: %d%c", hotColdLevel, tempFormat ? 'F' : 'C');
	display_print(margin, CENTER, 153);
}

int16_t defaultHotColdLevel()
{
	const float ratio = (hotColdMode == hotColdMode_cold) ? 0.2f : 0.8f;
	return static_cast<int16_t>(round(rawToTemp(ratio * (maxValue - minValue) + minValue)));
}

} // namespace

/* Menu to add, remove, or clear on-screen temperature measurement points. */
bool tempPointsMenu()
{
	while (true)
	{
		mainMenuBackground();
		mainMenuTitle((char *)"Temp. points");
		buttons_deleteAllButtons();
		buttons_setTextFont(smallFont);
		buttons_addButton(12, 46, 92, 122, (char *)"Add");
		buttons_addButton(114, 46, 92, 122, (char *)"Remove");
		buttons_addButton(216, 46, 92, 122, (char *)"Clear");
		buttons_addButton(12, 192, 142, 34, (char *)"< Back");
		buttons_drawButtons();
		ui_waitTouchRelease();

		bool needRedraw = false;
		while (!needRedraw)
		{
			if (checkSerial()) { needRedraw = true; break; }
			if (touch_touched())
			{
				int pressed = buttons_checkButtons(true);
				if (pressed == 0) { tempPointFunction(false); needRedraw = true; }
				else if (pressed == 1) { tempPointFunction(true); needRedraw = true; }
				else if (pressed == 2)
				{
					clearTempPoints();
					showFullMessage((char *)"All points cleared", true);
					delay(1000);
					needRedraw = true;
				}
				else if (pressed == 3) return false;
			}
			delay(5);
		}
	}
}

/* Display the selected hot/cold isotherm color name. */
void hotColdColorMenuString(int pos)
{
	static const char* const kColors[5] = {"White", "Black", "Red", "Green", "Blue"};
	const char* text = (pos >= 0 && pos < 5) ? kColors[pos] : "";
	mainMenuSelection(const_cast<char *>(text));
}

/* Color selector for Hot/Cold isotherm highlight mode. */
bool hotColdColorMenu()
{
	int initial = (hotColdMode == hotColdMode_hot) ? 2 : ((hotColdMode == hotColdMode_cold) ? 4 : 0);
	int chosen = runCarouselSelector("Select color", initial, 5, hotColdColorMenuString);
	if (chosen < 0) return false;
	hotColdColor = static_cast<byte>(chosen);
	EEPROM.write(eeprom_hotColdColor, hotColdColor);
	return true;
}

/* Touch handler for adjusting the hot/cold isotherm temperature threshold. */
void hotColdChooserHandler()
{
	drawHotColdLimitLabel();
	ui_waitTouchRelease();

	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 1)
			{
				EEPROM.write(eeprom_hotColdLevelHigh, (hotColdLevel >> 8) & 0xFFu);
				EEPROM.write(eeprom_hotColdLevelLow, hotColdLevel & 0xFFu);
				return;
			}
			bool updated = false;
			if (pressed == 0) { hotColdLevel = defaultHotColdLevel(); updated = true; }
			else if (pressed == 2 && hotColdLevel > round(rawToTemp(minValue))) { hotColdLevel--; updated = true; }
			else if (pressed == 3 && hotColdLevel < round(rawToTemp(maxValue))) { hotColdLevel++; updated = true; }

			if (updated)
			{
				createThermalImg(true);
				display_drawBitmap(80, 48, 160, 120, smallBuffer);
				drawHotColdLimitLabel();
			}
		}
		delay(5);
	}
}

/* Select the temperature threshold in hot/cold isotherm mode. */
void hotColdChooser()
{
	mainMenuBackground();
	mainMenuTitle((char *)"Set Limit");
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 192, 142, 34, (char *)"Reset");
	buttons_addButton(166, 192, 142, 34, (char *)"OK");
	buttons_addButton(12, 48, 56, 120, (char *)"-");
	buttons_addButton(252, 48, 56, 120, (char *)"+");
	buttons_drawButtons();

	display_setColor(UI_BORDER);
	display_drawRect(79, 47, 241, 169);
	display_setFont(smallFont);
	display_setBackColor(VGA_TRANSPARENT);
	changeTextColor();

	if (autoMode && !limitsLocked)
	{
		autoMode = true;
		createThermalImg(true);
		autoMode = false;
	}
	hotColdLevel = defaultHotColdLevel();
	createThermalImg(true);
	display_drawBitmap(80, 48, 160, 120, smallBuffer);
	hotColdChooserHandler();
}

/* Menu to enable Hot, Cold, or Disabled isotherm highlighting. */
bool hotColdMenu()
{
	while (true)
	{
		mainMenuBackground();
		mainMenuTitle((char *)"Hot / Cold");
		buttons_deleteAllButtons();
		buttons_setTextFont(smallFont);
		buttons_addButton(12, 46, 92, 122, (char *)"Hot");
		buttons_addButton(114, 46, 92, 122, (char *)"Cold");
		buttons_addButton(216, 46, 92, 122, (char *)"Disabled");
		buttons_addButton(12, 192, 142, 34, (char *)"< Back");
		buttons_drawButtons();
		ui_waitTouchRelease();

		bool needRedraw = false;
		while (!needRedraw)
		{
			if (checkSerial()) { needRedraw = true; break; }
			if (touch_touched())
			{
				int pressed = buttons_checkButtons(true);
				if (pressed == 3) return false;
				if (pressed == 2)
				{
					hotColdMode = hotColdMode_disabled;
					goto apply_mode;
				}
				if (pressed == 0 || pressed == 1)
				{
					hotColdMode = (pressed == 0) ? hotColdMode_hot : hotColdMode_cold;
					if (hotColdColorMenu())
					{
						hotColdChooser();
						goto apply_mode;
					}
					hotColdMode = hotColdMode_disabled;
					needRedraw = true;
				}
			}
			delay(5);
		}
	}

apply_mode:
	EEPROM.write(eeprom_hotColdMode, hotColdMode);
	lepton_ffcMode(hotColdMode == hotColdMode_disabled);
	return true;
}

/* Display target preset name when saving manual temperature limits. */
void tempLimitsPresetSaveString(int pos)
{
	static const char* const kNames[4] = {"Temporary", "Preset 1", "Preset 2", "Preset 3"};
	mainMenuSelection(const_cast<char *>((pos >= 0 && pos < 4) ? kNames[pos] : ""));
}

/* Menu to save the current manual temperature limits to a preset slot. */
bool tempLimitsPresetSaveMenu()
{
	int chosen = runCarouselSelector("Select Preset", 1, 4, tempLimitsPresetSaveString);
	if (chosen < 0) return false;
	savePresetSlot(static_cast<byte>(chosen));
	return true;
}

/* Touch handler for adjusting manual Min/Max temperature limits. */
bool tempLimitsManualHandler()
{
	bool minChange = false;
	bool maxChange = false;
	drawManualLimitLabels();
	ui_waitTouchRelease();

	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = (minChange || maxChange)
				? buttons_checkButtons(true, true)
				: buttons_checkButtons();
			if (pressed < 0) { delay(5); continue; }

			if (pressed == 0)
			{
				autoMode = true;
				createThermalImg(true);
				autoMode = false;
			}
			else if (pressed == 1)
			{
				if (minChange || maxChange) leaveLimitEditMode(minChange, maxChange);
				else return tempLimitsPresetSaveMenu();
			}
			else
			{
				adjustManualMinMax(pressed, minChange, maxChange);
			}

			createThermalImg(true);
			display_drawBitmap(80, 48, 160, 120, smallBuffer);
			drawManualLimitLabels();
		}
		delay(5);
	}
}

/* Select manual Min/Max temperature limits with live thermal preview. */
void tempLimitsManual()
{
	while (true)
	{
		mainMenuBackground();
		mainMenuTitle((char *)"Temp. Limits");
		buttons_deleteAllButtons();
		buttons_setTextFont(bigFont);
		buttons_addButton(12, 192, 142, 34, (char *)"Reset");
		buttons_addButton(166, 192, 142, 34, (char *)"OK");
		buttons_addButton(12, 48, 56, 120, (char *)"Min");
		buttons_addButton(252, 48, 56, 120, (char *)"Max");
		buttons_drawButtons();

		autoMode = true;
		createThermalImg(true);
		autoMode = false;

		display_drawBitmap(80, 48, 160, 120, smallBuffer);
		display_setColor(UI_BORDER);
		display_drawRect(79, 47, 241, 169);

		if (tempLimitsManualHandler())
			return;
	}
}

/* Display preset name when loading manual temperature limits. */
void tempLimitsPresetsString(int pos)
{
	static const char* const kNames[4] = {"New", "Preset 1", "Preset 2", "Preset 3"};
	mainMenuSelection(const_cast<char *>((pos >= 0 && pos < 4) ? kNames[pos] : ""));
}

/* Menu to create new manual limits or load a saved preset (1..3). */
bool tempLimitsPresets()
{
	int chosen = runCarouselSelector("Choose Preset", 0, 4, tempLimitsPresetsString);
	if (chosen < 0) return false;
	if (chosen == 0)
	{
		tempLimitsManual();
		return true;
	}
	return loadPresetSlot(static_cast<byte>(chosen));
}

/* Temperature Limit mode selector (Auto vs Manual). */
bool tempLimits()
{
	mainMenuBackground();
	mainMenuTitle((char *)"Temp. Limits");
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 46, 142, 120, (char *)"Auto");
	buttons_addButton(166, 46, 142, 120, (char *)"Manual");
	buttons_addButton(12, 192, 142, 34, (char *)"< Back");
	buttons_drawButtons();
	ui_waitTouchRelease();

	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 0)
			{
				showFullMessage((char *)"Please wait..", true);
				autoMode = true;
				limitsLocked = false;
				EEPROM.write(eeprom_minMaxPreset, minMax_temporary);
				lepton_ffcMode(true);
				return true;
			}
			if (pressed == 1)
			{
				autoMode = false;
				limitsLocked = false;
				return tempLimitsPresets();
			}
			if (pressed == 2) return false;
		}
		delay(5);
	}
}
