/*
 * GUI - Modern 2026 Dark-Slate + Thermal Amber On-Device UI
 */

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <touchscreen.h>
#include <fonts.h>
#include <buttons.h>
#include <hardware.h>
#include <battery.h>
#include <connection.h>
#include <perf.h>
#include <sdcard.h>
#include <tc_thermal_core.h>
#include <gui.h>

// Runtime Dark (default) & Daylight Light theme palettes (#145)
static const UiThemePalette kDarkTheme = {
	0x0862, // bg:          #0B0D12 obsidian
	0x10C4, // surface:     #161A23 dark card
	0x1926, // elevated:    #1E2430 elevated
	0x2967, // border:      #282E3D 1px border
	0xF7DF, // text:        #F4F6FA crisp white
	0x8CB5, // muted:       #8B94A8 muted slate
	0xF364, // accent:      #F36D21 Ironbow amber
	0xFF96, // accent_hi:   #FFF2B2 ivory core
	0xC241, // accent_dark: #C24908 deep amber
	0x0862, // accent_fg:   #0B0D12 dark ink on amber
	0x18AC, // indigo:      #1B1464
	0xA12D, // magenta:     #A4246E
	0x3DFF, // cyan:        #38BDF8
	0x262B, // ok:          #22C55E
	0xEA28  // err:         #EF4444
};

static const UiThemePalette kLightTheme = {
	0xF7DF, // bg:          #F8FAFC crisp daylight canvas
	0xFFFF, // surface:     #FFFFFF pure white card
	0xE73C, // elevated:    #E2E8F0 subtle pill
	0xCEBC, // border:      #CBD5E1 clean slate border
	0x08A5, // text:        #0F172A deep ink
	0x4AA9, // muted:       #475569 legible slate
	0xE2C2, // accent:      #E05A14 high-contrast daylight orange
	0xFF96, // accent_hi:   #FFF2B2 ivory core
	0xB201, // accent_dark: #B44108 deep orange
	0xFFFF, // accent_fg:   #FFFFFF white ink on daylight orange
	0x18AC, // indigo:      #1B1464
	0xA12D, // magenta:     #A4246E
	0x0479, // cyan:        #0284C7 daylight cyan
	0x1527, // ok:          #16A34A daylight green
	0xD945  // err:         #DC2626 daylight red
};

static const UiThemePalette kHighContrastTheme = {
	0x0000, // bg:          #000000 pure black
	0x0841, // surface:     #080808 near-black card
	0x18E3, // elevated:    #181818 high-contrast pill
	0xFFFF, // border:      #FFFFFF crisp white border
	0xFFFF, // text:        #FFFFFF pure white ink
	0xD69A, // muted:       #D0D0D0 bright gray
	0xFD40, // accent:      #FFAA00 vivid high-vis amber
	0xFFE0, // accent_hi:   #FFFF00 pure yellow
	0xD380, // accent_dark: #D07000 deep amber
	0x0000, // accent_fg:   #000000 pure black ink on amber
	0x18AC, // indigo:      #1B1464
	0xA12D, // magenta:     #A4246E
	0x07FF, // cyan:        #00FFFF pure cyan
	0x07E0, // ok:          #00FF00 pure green
	0xF800  // err:         #FF0000 pure red
};

UiThemePalette g_uiTheme = kDarkTheme;
bool uiLightTheme = false;
uint8_t uiThemeMode = uiTheme_dark;

void ui_setThemeMode(uint8_t mode)
{
	if (mode > uiTheme_highContrast) mode = uiTheme_dark;
	uiThemeMode = mode;
	uiLightTheme = (mode == uiTheme_light);
	if (mode == uiTheme_light)
		g_uiTheme = kLightTheme;
	else if (mode == uiTheme_highContrast)
		g_uiTheme = kHighContrastTheme;
	else
		g_uiTheme = kDarkTheme;
	buttons_setButtonColors(UI_TEXT, UI_TEXT_MUTED, UI_BORDER, UI_ACCENT, UI_SURFACE);
}

void ui_setTheme(bool light)
{
	ui_setThemeMode(light ? uiTheme_light : uiTheme_dark);
}

const char* ui_themeName(uint8_t mode)
{
	if (mode == uiTheme_light) return "Light";
	if (mode == uiTheme_highContrast) return "Hi-Con";
	return "Dark";
}

#define UI_BG        UI_BG_DARK
#define UI_MUTED     UI_TEXT_MUTED
#define UI_AMBER     UI_ACCENT
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
		display_fillRoundRect(12, 12, 308, 228);
		display_setColor(UI_BORDER);
		display_drawRoundRect(12, 12, 308, 228);
		display_setColor(UI_AMBER);
		display_fillRect(14, 14, 306, 17);
	}

	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print(message, CENTER, 114);
}

/* Shows a unified 2026 multi-line modal card (title + body + footer hint) */
void showModalCard(const char *title, const char *line1, const char *line2, const char *hint)
{
	display_fillScr(UI_BG);
	display_setColor(UI_SURFACE);
	display_fillRoundRect(12, 24, 308, 216);
	display_setColor(UI_BORDER);
	display_drawRoundRect(12, 24, 308, 216);
	display_setColor(UI_AMBER);
	display_fillRect(14, 26, 306, 30);

	display_setFont(bigFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_ACCENT);
	display_print((char *)title, CENTER, 46);

	display_setFont(smallFont);
	display_setColor(UI_TEXT);
	if (line1 && line1[0])
		display_print((char *)line1, CENTER, 92);
	if (line2 && line2[0])
		display_print((char *)line2, CENTER, 116);

	if (hint && hint[0])
	{
		display_setColor(UI_BORDER);
		display_drawHLine(28, 160, 264);
		display_setColor(UI_MUTED);
		display_print((char *)hint, CENTER, 178);
	}
}

/* Bounded wait for finger release so a button tap never bleeds into the next screen */
void ui_waitTouchRelease(uint32_t maxMs)
{
	uint32_t t0 = millis();
	while (touch_touched() && ((millis() - t0) < maxMs))
	{
		delay(5);
	}
}

/* Unified 28px non-touch top header bar shared by every menu, settings screen, and modal */
void ui_beginScreen(const char* title, const char* rightBadge, bool accentBadge)
{
	display_fillScr(UI_BG);
	display_setColor(UI_SURFACE);
	display_fillRect(0, 0, 319, 28);
	display_setColor(UI_AMBER);
	display_drawHLine(0, 28, 320);

	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	if (title && title[0])
		display_print(const_cast<char *>(title), 12, 8);

	if (rightBadge && rightBadge[0])
	{
		int rx = 308 - static_cast<int>(strlen(rightBadge)) * 8;
		display_setColor(accentBadge ? UI_AMBER : UI_MUTED);
		display_print(const_cast<char *>(rightBadge), rx, 8);
	}
}

/* Unified bezel-safe bottom action pill (standard geometry: y=192, h=34) */
void ui_drawPill(int x, int y, int w, int h, const char* label, UiPillStyle style)
{
	uint16_t fill = UI_SURFACE;
	uint16_t border = UI_BORDER;
	uint16_t ink = UI_TEXT;
	if (style == uiPill_primary)
	{
		fill = UI_ACCENT;
		border = UI_ACCENT;
		ink = UI_ACCENT_FG;
	}
	else if (style == uiPill_danger)
	{
		fill = UI_ERR;
		border = UI_ERR;
		ink = 0xFFFF;
	}
	display_setColor(fill);
	display_fillRoundRect(x, y, x + w, y + h);
	display_setColor(border);
	display_drawRoundRect(x, y, x + w, y + h);
	display_setFont(smallFont);
	display_setBackColor(fill);
	display_setColor(ink);
	if (label && label[0])
	{
		int tx = x + (w - static_cast<int>(strlen(label)) * 8) / 2;
		int ty = y + (h - 12) / 2;
		display_print(const_cast<char *>(label), tx, ty);
	}
}

/* Unified titled card with optional live status/value pill subtitle */
void ui_drawCard(int x, int y, int w, int h, const char* title, const char* subtitle, bool active)
{
	display_setColor(UI_SURFACE);
	display_fillRoundRect(x, y, x + w, y + h);
	display_setColor(active ? UI_ACCENT : UI_BORDER);
	display_drawRoundRect(x, y, x + w, y + h);

	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);

	if (subtitle && subtitle[0])
	{
		int titleY = (h >= 90) ? (y + h / 2 - 22) : (y + 11);
		int tx = x + (w - static_cast<int>(strlen(title)) * 8) / 2;
		display_print(const_cast<char *>(title), tx, titleY);

		int pad = (w >= 120) ? 14 : 6;
		int pillY0 = (h >= 90) ? (y + h / 2 + 2) : (y + h - 27);
		int pillY1 = pillY0 + 18;
		uint16_t pillBg = active ? UI_ACCENT : UI_ELEVATED;
		uint16_t pillFg = active ? UI_ACCENT_FG : UI_MUTED;
		display_setColor(pillBg);
		display_fillRoundRect(x + pad, pillY0, x + w - pad, pillY1);
		display_setBackColor(pillBg);
		display_setColor(pillFg);
		int sx = x + (w - static_cast<int>(strlen(subtitle)) * 8) / 2;
		display_print(const_cast<char *>(subtitle), sx, pillY0 + 3);
	}
	else if (title && title[0])
	{
		int tx = x + (w - static_cast<int>(strlen(title)) * 8) / 2;
		int ty = y + (h - 12) / 2;
		display_print(const_cast<char *>(title), tx, ty);
	}
}

/* Unified full-width list card (left title + active dot + right status badge pill) */
void ui_drawWideCard(int x, int y, int w, int h, const char* title, const char* badge, bool active)
{
	display_setColor(UI_SURFACE);
	display_fillRoundRect(x, y, x + w, y + h);
	display_setColor(active ? UI_ACCENT : UI_BORDER);
	display_drawRoundRect(x, y, x + w, y + h);

	int textY = y + (h - 12) / 2;
	if (active)
	{
		display_setColor(UI_ACCENT);
		display_fillCircle(x + 14, y + h / 2, 4);
	}
	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(active ? UI_TEXT : UI_TEXT);
	if (title && title[0])
		display_print(const_cast<char *>(title), active ? (x + 26) : (x + 14), textY);

	if (badge && badge[0])
	{
		int bw = static_cast<int>(strlen(badge)) * 8 + 16;
		int bx0 = x + w - 10 - bw;
		int bx1 = x + w - 10;
		int by0 = y + (h - 20) / 2;
		int by1 = by0 + 20;
		uint16_t pillBg = active ? UI_ACCENT : UI_ELEVATED;
		uint16_t pillFg = active ? UI_ACCENT_FG : UI_MUTED;
		display_setColor(pillBg);
		display_fillRoundRect(bx0, by0, bx1, by1);
		display_setBackColor(pillBg);
		display_setColor(pillFg);
		display_print(const_cast<char *>(badge), bx0 + 8, by0 + 4);
	}
}

/* Shared 2026 Yes/No confirmation modal with unified 28px header and standard bottom bar */
bool ui_confirmDialog(const char* title, const char* headline, const char* line1, const char* line2, bool highlightYes, bool destructive)
{
	ui_beginScreen(title, destructive ? "CONFIRM" : nullptr, destructive);

	display_setColor(UI_SURFACE);
	display_fillRoundRect(12, 38, 308, 180);
	display_setColor(destructive ? UI_ERR : UI_BORDER);
	display_drawRoundRect(12, 38, 308, 180);
	display_setColor(destructive ? UI_ERR : UI_AMBER);
	display_fillRect(14, 40, 306, 43);

	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	if (headline && headline[0])
		display_print(const_cast<char *>(headline), CENTER, 68);
	display_setColor(UI_MUTED);
	if (line1 && line1[0])
		display_print(const_cast<char *>(line1), CENTER, 106);
	if (line2 && line2[0])
		display_print(const_cast<char *>(line2), CENTER, 128);

	buttons_deleteAllButtons();
	buttons_addButton(166, 192, 142, 34, (char *)"", 0x0010); // 0: Yes (always right)
	buttons_addButton(12,  192, 142, 34, (char *)"", 0x0010); // 1: No  (always left)

	UiPillStyle yesStyle = destructive ? uiPill_danger : uiPill_primary;
	ui_drawPill(12,  192, 142, 34, "No",  uiPill_secondary);
	ui_drawPill(166, 192, 142, 34, "Yes", yesStyle);
	(void)highlightYes;

	ui_waitTouchRelease(1000u);
	while (true)
	{
		checkSerial();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 0) return true;
			if (pressed == 1) return false;
		}
		delay(5);
	}
}

/* Wait for finger release (1500 ms max) + 120 ms settle + arm post-menu touch lockout */
void ui_waitTouchReleaseAndLockout(uint32_t lockoutMs)
{
	ui_waitTouchRelease(1500u);
	delay(120);
	ui_waitTouchRelease(300u);
	noInterrupts();
	touchIrqFlag = false;
	showMenu = showMenu_disabled;
	longTouch = false;
	touchLockoutUntilMs = millis() + lockoutMs;
	if (touchLockoutUntilMs == 0)
		touchLockoutUntilMs = 1;
	interrupts();
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
	display_setColor(UI_SURFACE);
	display_fillRoundRect(96, 66, 224, 146);
	display_setColor(UI_ACCENT);
	display_drawRoundRect(96, 66, 224, 146);
	display_setFont(bigFont);
	display_setColor(UI_ACCENT);
	display_setBackColor(UI_SURFACE);
	display_printNumI(element, CENTER, 98, 2, '0');
	display_setFont(smallFont);
}

/* Draws the clean full-bleed background for menus */
void drawMainMenuBorder()
{
	display_fillScr(UI_BG);
}

/* Draw a sleek full-bleed 2026 submenu title header (unified 28px bar) */
void drawTitle(char *name, bool firstStart)
{
	ui_beginScreen(name, firstStart ? "SETUP" : nullptr, firstStart);
}

/* Shows the hardware self-diagnostics screen */
void showDiagnostic()
{
	ui_beginScreen("SELF-DIAGNOSTIC", "POST");

	display_setFont(smallFont);
	display_setBackColor(UI_BG);

	const char* names[5] = {"Display", "Touchscreen", "Battery Gauge", "FLIR Lepton", "SD Card"};
	const byte bits[5]   = {diag_display, diag_touch, diag_bat, diag_lepton, diag_sd};
	for (int i = 0; i < 5; ++i)
	{
		int y = 44 + i * 28;
		ui_drawWideCard(12, y, 296, 24, names[i], checkDiagnostic(bits[i]) ? "OK" : "FAIL", checkDiagnostic(bits[i]));
	}

	display_setBackColor(UI_BG);
	display_setColor(UI_MUTED);
	display_print((char *)"Touch screen to continue", CENTER, 206);

	while (!touch_touched()) {
		checkSerial();
	}
	showFullMessage((char *)"Continuing boot...");
}

/* Draw the modern 2026 Boot splash with Version 1 Ironbow Thermal Lens emblem (zero crosshairs!) */
void bootScreen()
{
	setDisplayRotation();
	buttons_init();
	buttons_setTextFont((uint8_t *)smallFont);
	display_setFont(smallFont);

	// Deep obsidian backdrop
	display_fillScr(UI_BG);

	// Subtle top & bottom Ironbow amber accent hairlines
	display_setColor(UI_AMBER);
	display_fillRect(0, 0, 319, 2);
	display_fillRect(0, 237, 319, 239);

	// Version 1 Ironbow Thermal Lens Mark centered at (160, 82) — strictly non-military!
	const int cx = 160, cy = 82;
	display_setColor(UI_SURFACE);
	display_fillRoundRect(cx - 44, cy - 36, cx + 44, cy + 36);
	display_setColor(UI_BORDER);
	display_drawRoundRect(cx - 44, cy - 36, cx + 44, cy + 36);
	display_setColor(UI_INDIGO);       // #1B1464 deep indigo
	display_fillCircle(cx, cy, 28);
	display_setColor(UI_MAGENTA);      // #A4246E magenta
	display_fillCircle(cx, cy, 22);
	display_setColor(UI_CRIMSON);      // crimson transition
	display_fillCircle(cx, cy, 16);
	display_setColor(UI_AMBER);        // #F36D21 Ironbow amber
	display_fillCircle(cx, cy, 11);
	display_setColor(UI_AMBER_HI);     // warm gold
	display_fillCircle(cx, cy, 6);
	display_setColor(UI_ACCENT_HI);    // #FFF2B2 ivory core
	display_fillCircle(cx, cy, 3);

	// Brand typography
	display_setFont(bigFont);
	display_setBackColor(UI_BG);
	display_setColor(UI_TEXT);
	display_print((char *)"DIY-THERMOCAM", CENTER, 138);

	// Subtitle + version line
	display_setFont(smallFont);
	display_setColor(UI_AMBER);
	display_print((char *)"RADIOMETRIC LWIR  |  V4", CENTER, 166);

	display_setColor(UI_BORDER);
	display_drawHLine(72, 188, 176);
	display_setColor(UI_MUTED);
	display_print(versionString, CENTER, 200);

	delay(500);
}

/* Draw 2026 live Status & Telemetry card (100% RAM-composited into bigBuffer -> single atomic LCD flush, zero flicker!) */
void drawStatusScreen()
{
	const bool prevWrite = display_writeToImage;
	display_writeToImage = true;

	ui_beginScreen("SYSTEM STATUS", "FW 4.0.0", true);

	auto drawMetricCard = [](int x, int y, int w, int h, const char* label, const char* val, uint16_t valColor) {
		display_setColor(UI_SURFACE);
		display_fillRoundRect(x, y, x + w, y + h);
		display_setColor(UI_BORDER);
		display_drawRoundRect(x, y, x + w, y + h);
		display_setFont(smallFont);
		display_setBackColor(UI_SURFACE);
		display_setColor(UI_MUTED);
		display_print((char *)label, x + 8, y + 5);
		display_setColor(valColor);
		display_print((char *)val, x + 8, y + 21);
	};

	char buf[48];
	// Row 1: Live FPS & VoSPI Link
	snprintf(buf, sizeof(buf), "%.2f FPS", g_perf.live_fps_x100 / 100.0f);
	drawMetricCard(12, 36, 142, 38, "LIVE RATE", buf, UI_OK);

	snprintf(buf, sizeof(buf), "RES:%lu CRC:%lu",
		static_cast<unsigned long>(g_perf.vospi_resyncs),
		static_cast<unsigned long>(g_perf.vospi_crc_errors));
	drawMetricCard(166, 36, 142, 38, "VOSPI LINK", buf,
		(g_perf.vospi_resyncs == 0 && g_perf.vospi_crc_errors == 0) ? UI_OK : UI_AMBER);

	// Row 2: Battery & SD Storage
	if (batPercentage < 0) {
		snprintf(buf, sizeof(buf), "USB POWER");
	} else {
		snprintf(buf, sizeof(buf), "%d%% (%u mV)", static_cast<int>(batPercentage), static_cast<unsigned>(batMillivolts));
	}
	drawMetricCard(12, 80, 142, 38, "POWER", buf, (batPercentage == 0) ? UI_ERR : UI_TEXT);

	snprintf(buf, sizeof(buf), "%lu/%lu MB",
		static_cast<unsigned long>(getSDFreeKB() / 1024u),
		static_cast<unsigned long>(getSDTotalKB() / 1024u));
	drawMetricCard(166, 80, 142, 38, "SD CARD", buf, checkDiagnostic(diag_sd) ? UI_TEXT : UI_ERR);

	// Row 3: SPI & Temperatures
	snprintf(buf, sizeof(buf), "LCD %uM LEP %uM",
		static_cast<unsigned>(g_display_spi_hz / 1000000u),
		static_cast<unsigned>(g_lepton_spi_hz / 1000000u));
	drawMetricCard(12, 124, 142, 38, "SPI BUS", buf, UI_CYAN);

	snprintf(buf, sizeof(buf), "MCU %.1fC", perf_mcu_temp_c());
	drawMetricCard(166, 124, 142, 38, "THERMAL", buf, UI_TEXT);

	// Compact version / diagnostic line
	display_setBackColor(UI_BG);
	display_setColor(UI_MUTED);
	snprintf(buf, sizeof(buf), "DIAG 0x%02X | CPU %u MHz | %s",
		static_cast<unsigned>(diagnostic),
		static_cast<unsigned>(F_CPU_ACTUAL / 1000000u),
		ui_themeName(uiThemeMode));
	display_print(buf, 12, 170);

	// Bezel-safe bottom inset < Back pill (142x34, 12px left / 14px bottom margin)
	ui_drawPill(12, 192, 142, 34, "< Back", uiPill_secondary);

	display_writeToImage = prevWrite;
	displayBuffer();
}

namespace {
struct StatusSnapshot {
	uint16_t live_fps_x100;
	uint32_t vospi_resyncs;
	uint32_t vospi_crc_errors;
	int8_t bat_pct;
	uint16_t bat_mv;
	uint32_t sd_free_mb;
	uint32_t sd_total_mb;
	int16_t mcu_temp_x10;
	byte diag;
	uint8_t theme;

	bool operator!=(const StatusSnapshot& o) const {
		return std::memcmp(this, &o, sizeof(StatusSnapshot)) != 0;
	}
};

static StatusSnapshot captureStatusSnapshot() {
	StatusSnapshot s{};
	s.live_fps_x100 = g_perf.live_fps_x100;
	s.vospi_resyncs = g_perf.vospi_resyncs;
	s.vospi_crc_errors = g_perf.vospi_crc_errors;
	s.bat_pct = batPercentage;
	s.bat_mv = batMillivolts;
	s.sd_free_mb = getSDFreeKB() / 1024u;
	s.sd_total_mb = getSDTotalKB() / 1024u;
	s.mcu_temp_x10 = static_cast<int16_t>(roundf(perf_mcu_temp_c() * 10.0f));
	s.diag = diagnostic;
	s.theme = uiThemeMode;
	return s;
}
} // namespace

/* Interactive Status & Telemetry screen (atomic RAM-composited updates, refreshes only when values change) */
void showStatusScreen()
{
	buttons_deleteAllButtons();
	buttons_addButton(12, 192, 142, 34, (char *)"", 0x0010);
	checkBattery(false, false);
	StatusSnapshot prevSnap = captureStatusSnapshot();
	drawStatusScreen();
	uint32_t t0 = millis();
	while (touch_touched() && (millis() - t0) < 1000u) { delay(5); }
	uint32_t lastRefreshMs = millis();
	while (true) {
		if (checkSerial()) {
			prevSnap = captureStatusSnapshot();
			drawStatusScreen();
		}
		if ((millis() - lastRefreshMs) >= 1000u) {
			lastRefreshMs = millis();
			checkBattery(false, false);
			StatusSnapshot curSnap = captureStatusSnapshot();
			if (curSnap != prevSnap) {
				prevSnap = curSnap;
				drawStatusScreen();
			}
		}
		if (touch_touched()) {
			int btn = buttons_checkButtons(true);
			if (btn == 0) return;
		}
		delay(5);
	}
}
