/*
 * LIVE MODE - 2026 Zero-Flicker HUD composited directly into bigBuffer (320x240 RGB565)
 */

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <battery.h>
#include <temperature.h>
#include <TimeLib.h>
#include <hardware.h>
#include <thermal.h>
#include <gui.h>
#include <create.h>
#include <fonts.h>
#include <perf.h>
#include <livemode.h>

static constexpr uint16_t HUD_PILL_BG = UI_HUD_BG;

/* Display battery status pill with crisp mini gauge icon */
void displayBatteryStatus()
{
	if (batTimer == 0)
		batTimer = millis();
	if ((millis() - batTimer) > 60000)
	{
		checkBattery();
		batTimer = millis();
	}

	if (batPercentage == -1)
	{
		display_blendRect(264, 4, 52, 16, HUD_PILL_BG, 5);
		display_setColor(UI_HUD_ACCENT);
		display_fillCircle(272, 12, 3);
		display_setColor(UI_HUD_TEXT);
		display_print((char *)"USB", 282, 6);
	}
	else if (batPercentage == 0)
	{
		display_blendRect(266, 4, 50, 16, UI_HUD_ERR, 6);
		display_setColor(UI_HUD_TEXT);
		display_print((char *)"LOW", 278, 6);
	}
	else
	{
		bool charging = isUSBConnected() && (batPercentage != 100);
		int px = charging ? 246 : 254;
		int pw = 316 - px;
		display_blendRect(px, 4, pw, 16, HUD_PILL_BG, 5);

		// Crisp mini battery outline + proportional fill bar
		int bx = px + 5;
		display_setColor(UI_HUD_TEXT);
		display_drawRect(bx, 8, bx + 13, 15);
		display_drawVLine(bx + 14, 10, 4);
		int fillW = (batPercentage * 10 + 50) / 100;
		if (fillW < 1) fillW = 1;
		if (fillW > 10) fillW = 10;
		word barCol = (batPercentage <= 20) ? UI_HUD_ERR : (charging ? UI_HUD_ACCENT : UI_HUD_OK);
		display_setColor(barCol);
		display_fillRect(bx + 2, 10, bx + 1 + fillW, 13);

		display_setColor(UI_HUD_TEXT);
		char buf[8];
		if (charging)
			snprintf(buf, sizeof(buf), "%d%%+", batPercentage);
		else
			snprintf(buf, sizeof(buf), "%d%%", batPercentage);
		display_print(buf, bx + 18, 6);
	}
	changeTextColor();
}

/* Display current time pill in bottom-left */
void displayTime()
{
	display_blendRect(4, 220, 72, 16, HUD_PILL_BG, 5);
	char buf[12];
	snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour(), minute(), second());
	display_print(buf, 8, 222);
}

/* Display current date pill in top-left */
void displayDate()
{
	display_blendRect(4, 4, 88, 16, HUD_PILL_BG, 5);
	char buf[14];
	snprintf(buf, sizeof(buf), "%02d.%02d.%04d", day(), month(), year());
	display_print(buf, 8, 6);
}

/* Display current temperature range mode pill in top-center (zero EEPROM reads!) */
void displayTempMode()
{
	const char* label;
	word dotCol = UI_HUD_ACCENT;
	if (limitsLocked) {
		label = "LOCK";
		dotCol = UI_HUD_ERR;
	} else if (autoMode) {
		label = "AUTO";
		dotCol = UI_HUD_OK;
	} else if (minMaxPresetCache == minMax_preset1) {
		label = "PRESET 1";
	} else if (minMaxPresetCache == minMax_preset2) {
		label = "PRESET 2";
	} else if (minMaxPresetCache == minMax_preset3) {
		label = "PRESET 3";
	} else {
		label = "MANUAL";
	}

	int tw = static_cast<int>(strlen(label)) * 8;
	int pw = tw + 22;
	int px = (320 - pw) / 2;
	display_blendRect(px, 4, pw, 16, HUD_PILL_BG, 5);
	display_setColor(dotCol);
	display_fillCircle(px + 8, 12, 3);
	changeTextColor();
	display_print((char *)label, px + 16, 6);
}

/* Display minimum (cyan) or maximum (amber) point marker & readout pill */
void displayMinMaxPoint(bool isMin)
{
	int16_t xpos, ypos;
	calculatePointPos(&xpos, &ypos, isMin ? minTempPos : maxTempPos);

	word col = isMin ? UI_HUD_CYAN : UI_HUD_ACCENT;
	display_setColor(col);
	display_fillCircle(xpos, ypos, 3);

	char buf[12];
	floatToChar(buf, rawToTemp(isMin ? minTempVal : maxTempVal));
	int tw = static_cast<int>(strlen(buf)) * 8;
	int pw = tw + 8;
	int16_t tx = xpos - pw / 2;
	if (tx < 4) tx = 4;
	if (tx + pw > 262) tx = 262 - pw;
	int16_t ty = ypos + 7;
	if (ty > 200) ty = ypos - 22;
	if (ty > 200) ty = 200;
	if (ty < 24) ty = 24;

	display_blendRect(tx, ty, pw, 15, HUD_PILL_BG, 5);
	display_setColor(col);
	display_print(buf, tx + 4, ty + 2);
	changeTextColor();
}

/* Display free SD space pill in bottom-right */
void displayFreeSpace()
{
	int len = sdInfo.length();
	int pw = (len * 8) + 10;
	if (pw < 60) pw = 60;
	if (pw > 140) pw = 140;
	int px = 316 - pw;
	display_blendRect(px, 220, pw, 16, HUD_PILL_BG, 5);
	display_print(sdInfo, px + 5, 222);
}

/* Show crisp open-ring center spot sensor & translucent temperature pill */
void showSpot()
{
	char buffer[16];
	display_drawCircle(160, 120, 9);
	display_drawHLine(145, 120, 5);
	display_drawHLine(171, 120, 5);
	display_drawVLine(160, 105, 5);
	display_drawVLine(160, 131, 5);

	floatToChar(buffer, spotTemp);
	size_t n = strlen(buffer);
	if (n + 1 < sizeof(buffer)) {
		buffer[n] = (tempFormat == tempFormat_fahrenheit) ? 'F' : 'C';
		buffer[n + 1] = '\0';
	}
	int w = static_cast<int>(strlen(buffer)) * 8;
	int tx = 160 - (w / 2);
	display_blendRect(tx - 5, 140, w + 10, 16, HUD_PILL_BG, 6);
	display_print(buffer, tx, 142);
}

/* Composite ALL HUD overlays into bigBuffer in RAM before ONE atomic LCD write */
void displayInfos()
{
	uint32_t t0 = micros();
	changeTextColor();
	display_setBackColor(VGA_TRANSPARENT);
	display_setFont((uint8_t *)smallFont);

	display_writeToImage = true;

	if ((imgSave != imgSave_create) && (!videoSave))
	{
		if (batteryEnabled)
			displayBatteryStatus();
		if (timeEnabled)
			displayTime();
		if (dateEnabled)
			displayDate();
		if (storageEnabled)
			displayFreeSpace();
		if (!showMenu)
			displayTempMode();
	}

	if (minMaxPoints & minMaxPoints_min)
		displayMinMaxPoint(true);
	if (minMaxPoints & minMaxPoints_max)
		displayMinMaxPoint(false);

	if (spotEnabled)
		showSpot();

	if (colorbarEnabled)
		showColorBar();

	showTemperatures();

	display_writeToImage = false;
	g_perf.overlay_us = micros() - t0;
}
