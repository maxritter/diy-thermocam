/*
 * MAIN MENU - 2-page modern tile launcher, palette picker, and live HUD toggles
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
#include <lepton.h>
#include <fonts.h>
#include <thermal.h>
#include <hardware.h>
#include <settingsmenu.h>
#include <gui.h>
#include <load.h>
#include <colorschemes.h>
#include <connection.h>
#include <massstorage.h>
#include <touchscreen.h>
#include <videomenu.h>
#include <mainmenu.h>

namespace {

struct PaletteMeta {
	const char* name;
	const byte* map;
	int length;
};

const PaletteMeta kPalettes[colorSchemeTotal] = {
	{"Arctic",    colorMap_arctic,        240},
	{"Black-Hot", colorMap_blackHot,      224},
	{"Blue-Red",  colorMap_blueRed,       192},
	{"Coldest",   colorMap_coldest,       224},
	{"Contrast",  colorMap_contrast,      224},
	{"Dbl-Rain",  colorMap_doubleRainbow, 256},
	{"Gray-Red",  colorMap_grayRed,       224},
	{"Glowbow",   colorMap_glowBow,       224},
	{"Grayscale", colorMap_grayscale,     256},
	{"Hottest",   colorMap_hottest,       224},
	{"Ironblack", colorMap_ironblack,     256},
	{"Lava",      colorMap_lava,          240},
	{"Medical",   colorMap_medical,       224},
	{"Rainbow",   colorMap_rainbow,       256},
	{"Wheel 1",   colorMap_wheel1,        256},
	{"Wheel 2",   colorMap_wheel2,        256},
	{"Wheel 3",   colorMap_wheel3,        256},
	{"White-Hot", colorMap_whiteHot,      224},
	{"Yellow",    colorMap_yellow,        224}
};

const PaletteMeta& paletteMeta(byte idx)
{
	return kPalettes[(idx < colorSchemeTotal) ? idx : (colorSchemeTotal - 1)];
}

void drawTileIcon(int cx, int cy, byte iconId)
{
	display_setColor(UI_TEXT);
	switch (iconId)
	{
	case 0: // Palette (4 Ironbow thermal gradient bars)
		display_setColor(UI_INDIGO);    display_fillRect(cx - 19, cy - 10, cx - 11, cy + 10);
		display_setColor(UI_MAGENTA);   display_fillRect(cx - 9,  cy - 10, cx - 1,  cy + 10);
		display_setColor(UI_ACCENT);    display_fillRect(cx + 1,  cy - 10, cx + 9,  cy + 10);
		display_setColor(UI_ACCENT_HI); display_fillRect(cx + 11, cy - 10, cx + 19, cy + 10);
		break;
	case 1: // Range / Limits (dual range sliders)
		display_setColor(UI_BORDER);
		display_fillRect(cx - 16, cy - 7, cx + 16, cy - 5);
		display_fillRect(cx - 16, cy + 5, cx + 16, cy + 7);
		display_setColor(UI_ACCENT); display_fillCircle(cx + 6, cy - 6, 4);
		display_setColor(UI_TEXT);   display_fillCircle(cx - 6, cy + 6, 4);
		break;
	case 2: // FFC (concentric optical shutter iris)
		display_drawCircle(cx, cy, 12);
		display_drawCircle(cx, cy, 11);
		display_setColor(UI_ACCENT);
		display_drawCircle(cx, cy, 7);
		display_fillCircle(cx, cy, 3);
		break;
	case 3: // Points (minimalist thermal map pin)
		display_drawCircle(cx, cy - 4, 8);
		display_setColor(UI_ACCENT); display_fillCircle(cx, cy - 4, 3);
		display_setColor(UI_TEXT);
		display_drawLine(cx - 5, cy + 2, cx, cy + 11);
		display_drawLine(cx + 5, cy + 2, cx, cy + 11);
		break;
	case 4: // Gallery (photo frame)
		display_drawRoundRect(cx - 16, cy - 10, cx + 16, cy + 10);
		display_setColor(UI_ACCENT); display_fillCircle(cx - 7, cy - 3, 3);
		display_setColor(UI_TEXT);
		display_drawLine(cx - 12, cy + 8, cx - 2, cy - 2);
		display_drawLine(cx - 2,  cy - 2, cx + 4, cy + 4);
		display_drawLine(cx + 4,  cy + 4, cx + 11, cy - 3);
		break;
	case 6: // Overlays (HUD frame + colorbar)
		display_drawRoundRect(cx - 16, cy - 10, cx + 16, cy + 10);
		display_setColor(UI_ACCENT); display_fillRect(cx + 9, cy - 7, cx + 12, cy + 7);
		display_setColor(UI_TEXT);
		display_fillRect(cx - 12, cy - 6, cx + 2, cy - 4);
		display_fillRect(cx - 12, cy + 4, cx - 2, cy + 6);
		break;
	case 7: // Settings (3 equalizer sliders)
		for (int i = -1; i <= 1; ++i)
		{
			int yy = cy + i * 8;
			display_setColor(UI_BORDER); display_fillRect(cx - 16, yy - 1, cx + 16, yy);
			display_setColor(i == 0 ? UI_ACCENT : UI_TEXT);
			display_fillCircle(cx + i * 8, yy, 3);
		}
		break;
	case 8: // Isotherm / Hot-Cold (split cyan / amber)
		display_setColor(UI_CYAN);   display_fillRoundRect(cx - 15, cy - 10, cx - 2, cy + 10);
		display_setColor(UI_ACCENT); display_fillRoundRect(cx + 2,  cy - 10, cx + 15, cy + 10);
		break;
	case 9: // Status / Telemetry (live bar meter)
		display_setColor(UI_BORDER); display_fillRect(cx - 14, cy + 2,  cx - 8,  cy + 10);
		display_setColor(UI_TEXT);   display_fillRect(cx - 4,  cy - 4,  cx + 2,  cy + 10);
		display_setColor(UI_ACCENT); display_fillRect(cx + 6,  cy - 10, cx + 12, cy + 10);
		break;
	case 10: // USB MTP (connector icon)
		display_drawRoundRect(cx - 9, cy - 10, cx + 9, cy + 1);
		display_fillRect(cx - 4, cy - 7, cx - 2, cy - 3);
		display_fillRect(cx + 2, cy - 7, cx + 4, cy - 3);
		display_setColor(UI_ACCENT); display_fillRect(cx - 12, cy + 1, cx + 12, cy + 9);
		break;
	case 12: // Video (REC ring + dot)
		display_drawCircle(cx, cy, 11);
		display_setColor(UI_ERR); display_fillCircle(cx, cy, 6);
		break;
	case 13: // Theme (contrast disc)
		display_setColor(UI_ACCENT);  display_fillCircle(cx, cy, 10);
		display_setColor(UI_SURFACE); display_fillRect(cx, cy - 10, cx + 11, cy + 10);
		display_setColor(UI_TEXT);    display_drawCircle(cx, cy, 10);
		break;
	}
}

struct TileSpec {
	int x;
	int y;
	const char* label;
	byte iconId;
	bool accentBorder;
};

void drawModernMenuTile(const TileSpec& tile)
{
	const int w = 92, h = 68;
	display_setColor(UI_SURFACE);
	display_fillRoundRect(tile.x, tile.y, tile.x + w, tile.y + h);
	display_setColor(tile.accentBorder ? UI_ACCENT : UI_BORDER);
	display_drawRoundRect(tile.x, tile.y, tile.x + w, tile.y + h);

	drawTileIcon(tile.x + w / 2, tile.y + 25, tile.iconId);

	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(tile.accentBorder ? UI_TEXT : UI_TEXT_MUTED);
	int lx = tile.x + (w - static_cast<int>(strlen(tile.label)) * 8) / 2;
	display_print(const_cast<char *>(tile.label), lx, tile.y + h - 18);
}

const char* filterValueLabel()
{
	if (filterType == filterType_gaussian) return "GAUSS";
	if (filterType == filterType_box) return "BOX";
	return "OFF";
}

const char* minMaxValueLabel()
{
	if (minMaxPoints == minMaxPoints_both) return "BOTH";
	if (minMaxPoints == minMaxPoints_min) return "COLD";
	if (minMaxPoints == minMaxPoints_max) return "HOT";
	return "OFF";
}

void drawPaletteRow(byte slot, byte idx, bool isSel)
{
	const int y = 36 + slot * 37;
	buttons_addButton(12, y, 296, 32, (char *)"", 0x0010); // 4..7

	const PaletteMeta& meta = paletteMeta(idx);
	display_setColor(UI_SURFACE);
	display_fillRoundRect(12, y, 308, y + 32);
	display_setColor(isSel ? UI_ACCENT : UI_BORDER);
	display_drawRoundRect(12, y, 308, y + 32);

	if (isSel)
	{
		display_setColor(UI_ACCENT);
		display_fillCircle(24, y + 16, 4);
	}
	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(isSel ? UI_TEXT : UI_TEXT_MUTED);
	display_print(const_cast<char *>(meta.name), isSel ? 36 : 22, y + 10);

	// Live color gradient rendered directly from firmware colorMap_* LUTs
	const int gx0 = 166, gx1 = 296, gw = gx1 - gx0;
	display_setColor(UI_BORDER);
	display_drawRect(gx0 - 1, y + 7, gx1, y + 25);
	for (int dx = 0; dx < gw; ++dx)
	{
		int ci = (dx * (meta.length - 1)) / (gw - 1);
		display_setColor(meta.map[ci * 3], meta.map[ci * 3 + 1], meta.map[ci * 3 + 2]);
		display_drawVLine(gx0 + dx, y + 8, 17);
	}
}

} // namespace

/* Draw full-bleed modern submenu canvas (unified 28px header bar). */
void mainMenuBackground()
{
	ui_beginScreen("");
}

/* Draw selection carousel chrome (< / > + bottom-left < Back / bottom-right OK). */
void drawSelectionMenu()
{
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12,  66,  46,  80, (char *)"<", 0, true);
	buttons_addButton(262, 66,  46,  80, (char *)">", 0, true);
	buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010); // 2: < Back
	buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010); // 3: OK
	buttons_drawButton(0);
	buttons_drawButton(1);
	ui_drawPill(12,  192, 142, 34, "< Back", uiPill_secondary);
	ui_drawPill(166, 192, 142, 34, "OK",     uiPill_primary);
	display_setColor(UI_SURFACE);
	display_fillRoundRect(68, 66, 252, 146);
	display_setColor(UI_ACCENT);
	display_drawRoundRect(68, 66, 252, 146);
}

/* Draw the submenu header title inside the unified 28px top bar. */
void mainMenuTitle(char *title)
{
	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print(title, 12, 8);
}

/* Draw the centered value inside a selection carousel card. */
void mainMenuSelection(char *selection)
{
	display_setColor(UI_SURFACE);
	display_fillRect(72, 70, 248, 142);
	display_setFont(bigFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_ACCENT);
	display_print(selection, CENTER, 98);
	display_setFont(smallFont);
}

/* Confirm entering USB MTP file transfer mode. */
bool massStoragePrompt()
{
	return ui_confirmDialog(
		"USB FILE TRANSFER",
		"Enter USB MTP file transfer mode?",
		"Windows / Linux / macOS MTP",
		"Not for FW updates or live stream.",
		true,
		false);
}

/* Draw 1-tap 3x3 Display & Theme Toggle Grid with bezel-safe bottom Back / Live pills. */
void drawLiveDispGrid()
{
	ui_beginScreen("DISPLAY & HUD", ui_themeName(uiThemeMode));

	buttons_deleteAllButtons();
	buttons_addButton(12, 192, 142, 34, (char *)"", 0x0010); // 0: < Back

	static const char* const kLabels[9] = {
		"Battery", "Time", "Date", "Spot", "Scale", "Storage", "Filter", "Theme", "Min/Max"
	};
	const char* values[9] = {
		batteryEnabled ? "ON" : "OFF",
		timeEnabled ? "ON" : "OFF",
		dateEnabled ? "ON" : "OFF",
		spotEnabled ? "ON" : "OFF",
		colorbarEnabled ? "ON" : "OFF",
		storageEnabled ? "ON" : "OFF",
		filterValueLabel(),
		ui_themeName(uiThemeMode),
		minMaxValueLabel()
	};
	const bool active[9] = {
		batteryEnabled, timeEnabled, dateEnabled, spotEnabled, colorbarEnabled, storageEnabled,
		filterType != filterType_none, true, minMaxPoints != minMaxPoints_disabled
	};

	for (int i = 0; i < 9; ++i)
	{
		const int x = 12 + (i % 3) * 102;
		const int y = 35 + (i / 3) * 51;
		const int w = 92, h = 45;
		buttons_addButton(x, y, w, h, (char *)"", 0x0010); // 1..9

		display_setColor(UI_SURFACE);
		display_fillRoundRect(x, y, x + w, y + h);
		display_setColor(active[i] ? UI_ACCENT : UI_BORDER);
		display_drawRoundRect(x, y, x + w, y + h);

		display_setFont(smallFont);
		display_setBackColor(UI_SURFACE);
		display_setColor(UI_TEXT);
		int lx = x + (w - static_cast<int>(strlen(kLabels[i])) * 8) / 2;
		display_print(const_cast<char *>(kLabels[i]), lx, y + 6);

		display_setColor(active[i] ? UI_ACCENT : UI_ELEVATED);
		display_fillRoundRect(x + 12, y + 22, x + w - 12, y + 39);
		display_setBackColor(active[i] ? UI_ACCENT : UI_ELEVATED);
		display_setColor(active[i] ? UI_ACCENT_FG : UI_TEXT_MUTED);
		int vx = x + (w - static_cast<int>(strlen(values[i])) * 8) / 2;
		display_print(const_cast<char *>(values[i]), vx, y + 25);
	}

	buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010); // 10: < Live
	ui_drawPill(12,  192, 142, 34, "< Back", uiPill_secondary);
	ui_drawPill(166, 192, 142, 34, "< Live", uiPill_primary);
}

/* Interactive 1-tap Live Display & Theme toggle screen. */
bool liveDispMenu()
{
	drawLiveDispGrid();
	ui_waitTouchRelease();
	while (true)
	{
		if (checkSerial())
			drawLiveDispGrid();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 0) return false;
			if (pressed == 10) return true;
			if (pressed == 8)
			{
				uint8_t nextMode = (uiThemeMode + 1) % 3;
				ui_setThemeMode(nextMode);
				EEPROM.write(eeprom_uiTheme, nextMode);
				drawLiveDispGrid();
			}
			else if (pressed >= 1 && pressed <= 9)
			{
				byte pos = static_cast<byte>(pressed - 1);
				changeDisplayOptions(&pos);
				drawLiveDispGrid();
			}
		}
		delay(5);
	}
}

/* Draw one page (4 palettes) of the visual palette picker with bezel-safe footer. */
void drawColorMenuPage(byte page, byte selected)
{
	char hdr[32];
	const byte totalPages = (colorSchemeTotal + 3) / 4;
	snprintf(hdr, sizeof(hdr), "COLOR PALETTE  %u/%u", static_cast<unsigned>(page + 1), static_cast<unsigned>(totalPages));
	ui_beginScreen(hdr);

	buttons_deleteAllButtons();
	buttons_addButton(12,  192, 78,  34, (char *)"", 0x0010); // 0: < Back
	buttons_addButton(96,  192, 44,  34, (char *)"", 0x0010); // 1: Prev
	buttons_addButton(146, 192, 112, 34, (char *)"", 0x0010); // 2: Apply
	buttons_addButton(264, 192, 44,  34, (char *)"", 0x0010); // 3: Next

	for (byte slot = 0; slot < 4; ++slot)
	{
		byte idx = static_cast<byte>(page * 4 + slot);
		if (idx >= colorSchemeTotal) break;
		drawPaletteRow(slot, idx, idx == selected);
	}

	ui_drawPill(12,  192, 78,  34, "< Back", uiPill_secondary);
	ui_drawPill(96,  192, 44,  34, "<",      uiPill_secondary);
	ui_drawPill(146, 192, 112, 34, "Apply",  uiPill_primary);
	ui_drawPill(264, 192, 44,  34, ">",      uiPill_secondary);
}

/* Interactive visual palette picker with live firmware LUT gradient previews. */
bool colorMenu()
{
	byte selected = colorScheme;
	const byte totalPages = (colorSchemeTotal + 3) / 4;
	byte page = selected / 4;
	drawColorMenuPage(page, selected);
	ui_waitTouchRelease();

	while (true)
	{
		if (checkSerial())
			drawColorMenuPage(page, selected);
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 0) return false;
			if (pressed == 2) { changeColorScheme(&selected); return true; }
			if (pressed == 1)
			{
				page = (page == 0) ? (totalPages - 1) : (page - 1);
				drawColorMenuPage(page, selected);
			}
			else if (pressed == 3)
			{
				page = (page + 1 >= totalPages) ? 0 : (page + 1);
				drawColorMenuPage(page, selected);
			}
			else if (pressed >= 4 && pressed <= 7)
			{
				byte idx = static_cast<byte>(page * 4 + (pressed - 4));
				if (idx < colorSchemeTotal)
				{
					selected = idx;
					changeColorScheme(&selected);
					drawColorMenuPage(page, selected);
				}
			}
		}
		delay(5);
	}
}

/* Draw the 2-page Main Menu with >=12px safe bezel margins and inset bottom bar. */
void drawMainMenu(byte pos)
{
	static const TileSpec kPage0[6] = {
		{12,  36,  "Palette",  0, true},
		{114, 36,  "Range",    1, false},
		{216, 36,  "FFC",      2, false},
		{12,  112, "Points",   3, false},
		{114, 112, "Gallery",  4, false},
		{216, 112, "Settings", 7, false}
	};
	static const TileSpec kPage1[6] = {
		{12,  36,  "Overlays", 6,  false},
		{114, 36,  "Isotherm", 8,  false},
		{216, 36,  "Status",   9,  false},
		{12,  112, "USB MTP",  10, false},
		{114, 112, "Video",    12, false},
		{216, 112, "Theme",    13, false}
	};

	ui_beginScreen((pos == 0) ? "MAIN MENU  1/2" : "MAIN MENU  2/2", ui_themeName(uiThemeMode));

	buttons_deleteAllButtons();
	const TileSpec* tiles = (pos == 0) ? kPage0 : kPage1;
	for (int i = 0; i < 6; ++i)
	{
		buttons_addButton(tiles[i].x, tiles[i].y, 92, 68, (char *)"", 0x0010);
		drawModernMenuTile(tiles[i]);
	}

	buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010); // 6: < Live
	buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010); // 7: More > / < Page 1
	ui_drawPill(12,  192, 142, 34, "< Live", uiPill_primary);
	ui_drawPill(166, 192, 142, 34, (pos == 0) ? "More >" : "< Page 1", uiPill_secondary);
}

/* Dispatch the selected tile on page 0 or page 1. Returns true to exit back to Live Mode. */
bool mainMenuSelect(byte pos, byte page)
{
	if (page == 0)
	{
		switch (pos)
		{
		case 0: return colorMenu();
		case 1: return tempLimits();
		case 2: lepton_ffc(true); return true;
		case 3: return tempPointsMenu();
		case 4: loadFiles(); return false;
		case 5: settingsMenu(); settingsMenuHandler(); return false;
		default: return false;
		}
	}
	switch (pos)
	{
	case 0: return liveDispMenu();
	case 1: return hotColdMenu();
	case 2: showStatusScreen(); return false;
	case 3: if (massStoragePrompt()) setMassStorage(); return false;
	case 4: videoMode(); return false;
	case 5: themeMenu(); return false;
	default: return false;
	}
}

/* Touch handler for the main menu (continuously polls checkSerial() so USB never locks out). */
void mainMenuHandler(byte *pos)
{
	if (*pos > 1) *pos = 0;
	ui_waitTouchRelease();

	while (true)
	{
		if (checkSerial() || screenOffCheck())
			drawMainMenu(*pos);

		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 6) return;
			if (pressed == 7)
			{
				*pos = (*pos == 0) ? 1 : 0;
				drawMainMenu(*pos);
				continue;
			}
			if (pressed >= 0 && pressed <= 5)
			{
				if (mainMenuSelect(static_cast<byte>(pressed), *pos))
					return;
				drawMainMenu(*pos);
				ui_waitTouchRelease();
			}
		}
		delay(5);
	}
}

/* Open the on-device main menu and cleanly re-arm Live Mode upon exit. */
void mainMenu()
{
	showMenu = showMenu_opened;
	static byte mainMenuPos = 0;

	drawMainMenu(mainMenuPos);
	mainMenuHandler(&mainMenuPos);

	display_setFont(smallFont);
	buttons_setTextFont(smallFont);
	buttons_deleteAllButtons();

	ui_waitTouchReleaseAndLockout(250u);
	Serial.clear();
	disableSPIIRQ = false;
	lepton_startFrame();
}
