/*
 * LIVE MODE - Zero-flicker HUD composited directly into bigBuffer (320x240 RGB565)
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

static constexpr uint16_t HUD_PILL_BG = 0x08A5; // deep slate translucent pill

/* Display battery status in percentage */
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
		display_blendRect(222, 0, 96, 14, HUD_PILL_BG, 5);
		display_print((char *)"USB Power", 225, 1);
	}
	else if (batPercentage == 0)
	{
		display_blendRect(267, 0, 50, 14, 0x8000, 6);
		display_print((char *)"LOW", 270, 1);
	}
	else
	{
		display_blendRect(267, 0, 52, 14, HUD_PILL_BG, 5);
		if (isUSBConnected() && (batPercentage != 100))
		{
			display_printNumI(batPercentage, 270, 1, 3, ' ');
			display_print((char *)"%", 300, 1);
			display_print((char *)"+", 310, 1);
		}
		else
		{
			display_printNumI(batPercentage, 280, 1, 3, ' ');
			display_print((char *)"%", 310, 1);
		}
	}
}

/* Display the current time on the screen */
void displayTime()
{
	display_blendRect(2, 226, 70, 14, HUD_PILL_BG, 5);
	display_printNumI(hour(), 5, 228, 2, '0');
	display_print((char *)":", 20, 228);
	display_printNumI(minute(), 27, 228, 2, '0');
	display_print((char *)":", 42, 228);
	display_printNumI(second(), 49, 228, 2, '0');
}

/* Display the date on screen */
void displayDate()
{
	display_blendRect(2, 0, 86, 14, HUD_PILL_BG, 5);
	display_printNumI(day(), 5, 1, 2, '0');
	display_print((char *)".", 20, 1);
	display_printNumI(month(), 27, 1, 2, '0');
	display_print((char *)".", 42, 1);
	display_printNumI(year(), 49, 1, 4);
}

/* Display the current temperature mode on top (zero EEPROM reads per frame!) */
void displayTempMode()
{
	const char* label;
	if (limitsLocked)
		label = " LOCKED ";
	else if (autoMode)
		label = "  AUTO  ";
	else if (minMaxPresetCache == minMax_preset1)
		label = "PRESET 1";
	else if (minMaxPresetCache == minMax_preset2)
		label = "PRESET 2";
	else if (minMaxPresetCache == minMax_preset3)
		label = "PRESET 3";
	else
		label = " MANUAL ";

	display_blendRect(128, 0, 68, 14, HUD_PILL_BG, 5);
	display_print((char *)label, 130, 1);
}

/* Display the minimum and maximum point on the screen */
void displayMinMaxPoint(bool isMin)
{
	int16_t xpos, ypos;
	calculatePointPos(&xpos, &ypos, isMin ? minTempPos : maxTempPos);

	// Crisp 5px crosshair marker
	display_drawHLine(xpos - 3, ypos, 7);
	display_drawVLine(xpos, ypos - 3, 7);

	int16_t tx = xpos - 20;
	if (tx < 2) tx = 2;
	if (tx > 274) tx = 274;
	int16_t ty = ypos + 8;
	if (ty > 224) ty = ypos - 18;

	display_blendRect(tx - 2, ty - 1, 48, 14, HUD_PILL_BG, 5);
	display_printNumF(rawToTemp(isMin ? minTempVal : maxTempVal), 2, tx, ty);
}

/* Display free space on screen */
void displayFreeSpace()
{
	display_blendRect(202, 226, 116, 14, HUD_PILL_BG, 5);
	display_print(sdInfo, 205, 228);
}

/* Show the current spot temperature crosshair & readout pill */
void showSpot()
{
	char buffer[16];
	display_drawCircle(160, 120, 11);
	display_drawHLine(138, 120, 11);
	display_drawHLine(172, 120, 11);
	display_drawVLine(160, 98, 11);
	display_drawVLine(160, 132, 11);

	floatToChar(buffer, spotTemp);
	int w = display_getStringWidth(buffer);
	int tx = 160 - (w / 2);
	display_blendRect(tx - 4, 146, w + 8, 15, HUD_PILL_BG, 5);
	display_print(buffer, tx, 148);
}

/* Composite ALL HUD overlays into bigBuffer in RAM before ONE atomic DMA screen write */
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

	// Flicker-free SAVING badge composited in RAM
	if (imgSave == imgSave_create && !serialMode)
	{
		int by = spotEnabled ? 66 : 106;
		display_blendRect(102, by - 4, 116, 24, HUD_PILL_BG, 6);
		display_setFont(bigFont);
		display_setColor(0xFB82); // thermal amber
		display_print((char *)"SAVING", CENTER, by);
		display_setFont((uint8_t *)smallFont);
		changeTextColor();
	}

	display_writeToImage = false;
	g_perf.overlay_us = micros() - t0;
}
