/*
 * GUI - Modern 2026 Dark-Slate + Thermal Amber On-Device UI
 */

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <touchscreen.h>
#include <fonts.h>
#include <bitmaps.h>
#include <buttons.h>
#include <hardware.h>
#include <tc_thermal_core.h>
#include <gui.h>

// 2026 Theme extra accents (base UI_* palette macros live in display.h)
static constexpr uint16_t UI_BG        = UI_BG_DARK;
static constexpr uint16_t UI_MUTED     = UI_TEXT_MUTED;
static constexpr uint16_t UI_AMBER     = UI_ACCENT;
static constexpr uint16_t UI_AMBER_HI  = 0xFD20; // #FFA500 hot amber
static constexpr uint16_t UI_CRIMSON   = 0xE8C4; // #E81A24 thermal crimson

/* Converts a given float to char array (fixes -1 < x < 0 minus sign bug) */
void floatToChar(char *buffer, float val)
{
	tc_core::format_float_2dp(buffer, 16, val);
}

/* Sets the text color to the right one */
void changeTextColor()
{
	if (textColor == textColor_red)
		display_setColor(VGA_RED);
	else if (textColor == textColor_black)
		display_setColor(VGA_BLACK);
	else if (textColor == textColor_green)
		display_setColor(VGA_GREEN);
	else if (textColor == textColor_blue)
		display_setColor(VGA_BLUE);
	else
		display_setColor(VGA_WHITE);
}

/* Shows a full screen or modal card message */
void showFullMessage(char *message, bool small)
{
	if (!small)
	{
		display_fillScr(UI_BG);
		display_setColor(UI_SURFACE);
		display_fillRoundRect(16, 82, 304, 158);
		display_setColor(UI_BORDER);
		display_drawRoundRect(16, 82, 304, 158);
		display_setColor(UI_AMBER);
		display_fillRect(18, 84, 302, 87);
	}
	else
	{
		display_setColor(UI_SURFACE);
		display_fillRoundRect(6, 6, 314, 234);
		display_setColor(UI_BORDER);
		display_drawRoundRect(6, 6, 314, 234);
	}

	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print(message, CENTER, 114);
}

/* Shows a transient status toast in live mode */
void showTransMessage(char *msg)
{
	display_setColor(UI_SURFACE);
	display_fillRoundRect(34, 88, 286, 146);
	display_setColor(UI_AMBER);
	display_drawRoundRect(34, 88, 286, 146);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_setFont(bigFont);
	display_print(msg, CENTER, 109);
	display_setFont(smallFont);
	delay(800);
}

/* Draw a BigFont value in the center of a stepper menu */
void drawCenterElement(int element)
{
	display_setColor(UI_BG);
	display_fillRoundRect(102, 64, 218, 126);
	display_setColor(UI_BORDER);
	display_drawRoundRect(102, 64, 218, 126);
	display_setFont(bigFont);
	display_setColor(UI_AMBER_HI);
	display_setBackColor(UI_BG);
	display_printNumI(element, CENTER, 87, 2, '0');
	display_setFont(smallFont);
}

/* Draws the outer frame for the main menu */
void drawMainMenuBorder()
{
	display_setColor(UI_BG);
	display_fillRoundRect(4, 4, 316, 236);
	display_setColor(UI_BORDER);
	display_drawRoundRect(5, 5, 315, 235);
}

/* Draw a menu title header */
void drawTitle(char *name, bool firstStart)
{
	if (firstStart)
		display_fillScr(UI_BG);
	else
	{
		display_setColor(UI_BG);
		display_fillRoundRect(6, 6, 314, 234);
		display_setColor(UI_BORDER);
		display_drawRoundRect(6, 6, 314, 234);
	}
	display_setColor(UI_SURFACE);
	display_fillRect(7, 7, 313, 46);
	display_setColor(UI_AMBER);
	display_drawHLine(7, 46, 306);

	display_setFont(bigFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print(name, CENTER, 18);
	display_setFont(smallFont);
}

/* Shows the hardware self-diagnostics screen */
void showDiagnostic()
{
	display_fillScr(UI_BG);
	display_setColor(UI_SURFACE);
	display_fillRect(0, 0, 319, 40);
	display_setColor(UI_AMBER);
	display_drawHLine(0, 40, 320);

	display_setFont(bigFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print((char *)"Self-Diagnostic", CENTER, 12);

	display_setFont(smallFont);
	display_setBackColor(UI_BG);

	const char* names[5] = {"Display", "Touchscreen", "Battery Gauge", "FLIR Lepton", "SD Card"};
	const byte bits[5]   = {diag_display, diag_touch, diag_bat, diag_lepton, diag_sd};
	for (int i = 0; i < 5; ++i)
	{
		int y = 56 + i * 30;
		display_setColor(UI_TEXT);
		display_print((char *)names[i], 42, y);
		if (checkDiagnostic(bits[i]))
		{
			display_setColor(UI_OK);
			display_print((char *)"OK    ", 224, y);
		}
		else
		{
			display_setColor(UI_ERR);
			display_print((char *)"FAILED", 224, y);
		}
	}

	display_setColor(UI_MUTED);
	display_print((char *)"Touch screen to continue", CENTER, 216);

	while (!touch_touched()) {}
	showFullMessage((char *)"Continuing boot...");
}

/* Draw the modern 2026 Boot splash with geometric thermal-reticle emblem */
void bootScreen()
{
	setDisplayRotation();
	buttons_init();
	buttons_setTextFont((uint8_t *)smallFont);
	display_setFont(smallFont);

	// Deep slate backdrop
	display_fillScr(UI_BG);

	// Subtle top & bottom thermal-amber accent bars
	display_setColor(UI_AMBER);
	display_fillRect(0, 0, 319, 2);
	display_fillRect(0, 237, 319, 239);

	// Geometric 2026 Thermal-Reticle Emblem centered at (160, 86)
	const int cx = 160, cy = 86;
	// Corner viewfinder brackets
	display_setColor(UI_BORDER);
	const int bx = 46, by = 40, bl = 12;
	display_drawHLine(cx - bx, cy - by, bl); display_drawVLine(cx - bx, cy - by, bl);
	display_drawHLine(cx + bx - bl + 1, cy - by, bl); display_drawVLine(cx + bx, cy - by, bl);
	display_drawHLine(cx - bx, cy + by, bl); display_drawVLine(cx - bx, cy + by - bl + 1, bl);
	display_drawHLine(cx + bx - bl + 1, cy + by, bl); display_drawVLine(cx + bx, cy + by - bl + 1, bl);

	// Concentric thermal rings (outer muted, mid amber, inner crimson)
	display_setColor(UI_BORDER);
	display_drawCircle(cx, cy, 34);
	display_setColor(UI_AMBER);
	display_drawCircle(cx, cy, 24);
	display_drawCircle(cx, cy, 23);
	display_setColor(UI_CRIMSON);
	display_drawCircle(cx, cy, 13);

	// Precision crosshair ticks
	display_setColor(UI_AMBER_HI);
	display_drawHLine(cx - 42, cy, 14);
	display_drawHLine(cx + 29, cy, 14);
	display_drawVLine(cx, cy - 36, 10);
	display_drawVLine(cx, cy + 27, 10);

	// Glowing thermal core
	display_setColor(UI_AMBER_HI);
	display_fillCircle(cx, cy, 5);
	display_setColor(UI_TEXT);
	display_fillCircle(cx, cy, 2);

	// Brand typography
	display_setFont(bigFont);
	display_setBackColor(UI_BG);
	display_setColor(UI_TEXT);
	display_print((char *)"DIY-THERMOCAM", CENTER, 142);

	// V3 badge + version line
	display_setFont(smallFont);
	display_setColor(UI_AMBER);
	display_print((char *)"V3  |  RADIOMETRIC LWIR", CENTER, 170);

	// Subtle divider & version string
	display_setColor(UI_BORDER);
	display_drawHLine(70, 192, 180);
	display_setColor(UI_MUTED);
	display_print(versionString, CENTER, 204);

	delay(500);
}
