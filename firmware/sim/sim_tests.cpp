#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <touchscreen.h>
#include <fonts.h>
#include <gui.h>
#include <buttons.h>
#include <mainmenu.h>
#include <settingsmenu.h>
#include <load.h>
#include <loadmenu.h>
#include <videomenu.h>
#include <perf.h>
#include "sim_harness.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern uint16_t sim_fb[320 * 240];

static int g_passed = 0;
static int g_failed = 0;

static void check(bool cond, const char* test_name, const std::string& detail) {
	if (!cond) {
		std::fprintf(stderr, "  FAIL [%s]: %s\n", test_name, detail.c_str());
		throw std::runtime_error(detail);
	}
}

static void run_test(const char* name, void (*fn)()) {
	std::printf("[RUN ] %s\n", name);
	try {
		fn();
		g_passed++;
		std::printf("[PASS] %s\n", name);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "[FAIL] %s -> %s\n", name, e.what());
		g_failed++;
	}
}

static void reset_ui_state() {
	g_sim.reset(1000u);
	sim_sd_clear();
	ui_setThemeMode(uiTheme_dark);
	buttons_init();
	buttons_setTextFont(smallFont);
	display_setFont(smallFont);
	display_writeToImage = false;
	touch_capacitive = false;
	diagnostic = diag_ok;
	batPercentage = 84;
	batMillivolts = 3980;
	g_perf = {};
	g_perf.live_fps_x100 = 876;
	loadTouch = loadTouch_none;
	showMenu = showMenu_opened;
	spotEnabled = true;
	colorbarEnabled = true;
	minMaxPoints = minMaxPoints_disabled;
}

// 1. Regression test for V4 Gallery infinite refresh loop on XPT2046 resistive touchscreen:
//    User enters Gallery -> taps Images -> sits idle for 3 seconds while XPT2046 SPI polling
//    pulses PENIRQ and returns stale "Images" tap coordinates with z=0 -> then taps < Back.
static void test_gallery_images_no_infinite_refresh_xpt2046() {
	reset_ui_state();
	sim_sd_add_dat_image("20261125120000.DAT", 30000u);
	sim_sd_add_dat_image("20261125120005.DAT", 31000u);

	// Enable real XPT2046 hardware behavior: SPI poll pulses PENIRQ + stale coords when z==0
	g_sim.xpt2046_penirq_glitch_on_poll = true;
	g_sim.xpt2046_stale_coords = true;
	g_sim.setWatchdog(8000u);

	// t=1100: tap "Images" card center (83, 114) — which also overlaps loadTouch_previous (10..140, 90..150)!
	g_sim.scheduleTap(1100u, 83, 114, 50u);
	// Sit completely idle from t=1150 to t=4200 (3+ seconds!)
	// t=4200: tap "< Back" HUD pill (268, 217) to exit Gallery viewer
	g_sim.scheduleTap(4200u, 268, 217, 50u);

	loadFiles();

	// Should have loaded & flushed the latest image ONCE (NOT looped dozens of times during the 3s idle!)
	check(g_sim.stats.atomic_flushes == 1u,
		"gallery_images_idle_stability",
		"expected exactly 1 atomic image render while idle, got " + std::to_string(g_sim.stats.atomic_flushes));
	// Make sure we actually waited until the t=4200 Back tap (did not exit prematurely)
	check(g_sim.now_ms >= 4200u,
		"gallery_images_stayed_open",
		"exited prematurely at " + std::to_string(g_sim.now_ms) + " ms");
}

// 2. Gallery navigation test: enter Images with 2 images, tap center (should NOT reload),
//    tap Previous (<), tap Next (>), then exit cleanly.
static void test_gallery_images_navigation_and_center_tap() {
	reset_ui_state();
	sim_sd_add_dat_image("20261125120000.DAT", 30000u);
	sim_sd_add_dat_image("20261125120005.DAT", 31000u);

	g_sim.xpt2046_penirq_glitch_on_poll = true;
	g_sim.xpt2046_stale_coords = true;
	g_sim.setWatchdog(10000u);

	g_sim.scheduleTap(1100u, 83, 114, 40u);   // Images card -> render #1
	g_sim.scheduleTap(2000u, 160, 120, 40u);  // Center tap on still image -> MUST NOT reload!
	g_sim.scheduleTap(3000u, 25, 120, 40u);   // "<" Previous -> render #2
	g_sim.scheduleTap(4500u, 295, 120, 40u);  // ">" Next -> render #3
	g_sim.scheduleTap(6000u, 268, 217, 40u);  // "< Back" -> exit

	loadFiles();

	check(g_sim.stats.atomic_flushes == 3u,
		"gallery_nav_flush_count",
		"expected 3 image renders (open + prev + next; center tap ignored), got " +
			std::to_string(g_sim.stats.atomic_flushes));
}

// 3. Regression test: empty video folder must NOT cause an infinite loop in loadFiles().
static void test_gallery_empty_video_folder_no_infinite_loop() {
	reset_ui_state();
	sim_sd_add_video_folder("20261125120000", 0u); // empty video folder!
	g_sim.setWatchdog(5000u);

	// Tap "Videos" card center (237, 114)
	g_sim.scheduleTap(1100u, 237, 114, 40u);

	loadFiles();

	// Should show "Empty video folder" once and exit cleanly well before watchdog
	check(g_sim.now_ms < 4000u,
		"empty_video_folder_exits",
		"expected clean exit after empty video folder, elapsed to " + std::to_string(g_sim.now_ms) + " ms");
}

// 4. Regression test: valid video folder stays paused cleanly on first frame, then exits on < Back.
static void test_gallery_video_paused_idle_and_exit() {
	reset_ui_state();
	sim_sd_add_video_folder("20261125120000", 3u);
	g_sim.xpt2046_penirq_glitch_on_poll = true;
	g_sim.xpt2046_stale_coords = true;
	g_sim.setWatchdog(8000u);

	// Tap "Videos" card (237, 114) — overlaps loadTouch_next (180..310, 90..150)!
	g_sim.scheduleTap(1100u, 237, 114, 50u);
	// Stay idle for 2.5 seconds on paused frame 1/3, then tap "< Back" (268, 217)
	g_sim.scheduleTap(3800u, 268, 217, 50u);

	loadFiles();

	check(g_sim.stats.atomic_flushes == 1u,
		"video_paused_single_render",
		"expected 1 frame render while paused, got " + std::to_string(g_sim.stats.atomic_flushes));
	check(g_sim.now_ms >= 3800u,
		"video_stayed_open",
		"video exited prematurely at " + std::to_string(g_sim.now_ms) + " ms");
}

// 5. Regression test for Status screen weird/flickering refresh:
//    Sit on Status screen for 3.2 seconds with STATIC telemetry -> must NOT wipe LCD directly
//    and must NOT redraw redundantly!
static void test_status_screen_zero_flicker_idle() {
	reset_ui_state();
	g_sim.setWatchdog(6000u);

	// Tap "< Back" pill (83, 209) after 3.2s idle
	g_sim.scheduleTap(4200u, 83, 209, 50u);

	showStatusScreen();

	check(g_sim.stats.direct_fill_scr == 0u,
		"status_zero_direct_fill_scr",
		"Status screen called direct-to-LCD display_fillScr() " +
			std::to_string(g_sim.stats.direct_fill_scr) + " times (causes visible full-screen wipe/flicker!)");
	check(g_sim.stats.atomic_flushes == 1u,
		"status_static_single_atomic_flush",
		"expected 1 atomic flush when telemetry is unchanged over 3.2s, got " +
			std::to_string(g_sim.stats.atomic_flushes));
}

// 6. Status screen live update test: when MCU temp actually changes during idle,
//    Status screen MUST refresh atomically (via bigBuffer, 0 direct_fill_scr) and update framebuffer pixels!
static void test_status_screen_updates_atomically_on_change() {
	reset_ui_state();
	g_sim.setWatchdog(6000u);

	std::vector<uint16_t> first_fb;
	bool changed_temp = false;
	g_sim.on_tick = [&](uint32_t now) {
		if (first_fb.empty() && g_sim.stats.atomic_flushes >= 1u) {
			first_fb.assign(sim_fb, sim_fb + 320 * 240);
		}
		if (!changed_temp && now >= 2100u) {
			changed_temp = true;
			g_sim.sim_mcu_temp_c = 47.8f; // visible change in THERMAL card
		}
	};

	g_sim.scheduleTap(3800u, 83, 209, 50u); // "< Back"

	showStatusScreen();

	check(g_sim.stats.direct_fill_scr == 0u,
		"status_update_zero_direct_fill_scr",
		"Status screen update used direct display_fillScr() (" +
			std::to_string(g_sim.stats.direct_fill_scr) + ")");
	check(g_sim.stats.atomic_flushes == 2u,
		"status_update_exact_two_flushes",
		"expected 2 atomic flushes (initial + 1 telemetry change), got " +
			std::to_string(g_sim.stats.atomic_flushes));
	bool pixels_changed = (first_fb.size() == 320u * 240u) &&
		(std::memcmp(first_fb.data(), sim_fb, sizeof(sim_fb)) != 0);
	check(pixels_changed,
		"status_update_pixels_changed",
		"Status screen framebuffer did not reflect updated MCU temperature");
}

// 7. Regression test: tapping "< Back" in Gallery (12, 190, 142, 36) with a slightly long hold (220 ms)
//    must return to Main Menu and MUST NOT bleed through onto "< Live" (same rect 12, 190, 142, 36)!
static void test_back_button_no_bleed_through_to_live() {
	reset_ui_state();
	g_sim.setWatchdog(8000u);

	// Start in mainMenuHandler on page 0
	byte pos = 0;
	drawMainMenu(pos);
	// t=1100: tap "Gallery" tile (160, 146)
	g_sim.scheduleTap(1100u, 160, 146, 50u);
	// t=1500: tap "< Back" in Gallery (83, 208) and HOLD for 240 ms (> buttons_checkButtons 150ms timeout!)
	g_sim.scheduleTap(1500u, 83, 208, 240u);
	// Stay in Main Menu until t=3500, THEN tap "< Live" explicitly (83, 208)
	g_sim.scheduleTap(3500u, 83, 208, 40u);

	mainMenuHandler(&pos);

	check(g_sim.now_ms >= 3500u,
		"back_no_bleed_through",
		"Main Menu exited early at " + std::to_string(g_sim.now_ms) +
			" ms — Gallery < Back bled through into Main Menu < Live!");
}

// 8. Gallery Back button works cleanly directly from Main Menu -> Gallery -> < Back
static void test_gallery_menu_idle_and_back() {
	reset_ui_state();
	g_sim.xpt2046_penirq_glitch_on_poll = true;
	g_sim.setWatchdog(6000u);

	// Stay idle on Gallery screen for 2.5s, then tap < Back (83, 208)
	g_sim.scheduleTap(3500u, 83, 208, 40u);

	loadFiles();

	// drawLoadMenu() should run exactly ONCE
	check(g_sim.stats.direct_fill_scr == 1u,
		"gallery_menu_single_draw",
		"expected 1 drawLoadMenu() call, got " + std::to_string(g_sim.stats.direct_fill_scr));
	check(g_sim.now_ms >= 3500u,
		"gallery_menu_stayed_open",
		"exited prematurely at " + std::to_string(g_sim.now_ms) + " ms");
}

// 9. Unit check: loadTouchProcess() ignores stale coordinates when touch pressure z == 0
static void test_load_touch_ignores_zero_pressure() {
	reset_ui_state();
	g_sim.xpt2046_stale_coords = true;
	g_sim.last_x = 83;
	g_sim.last_y = 114; // squarely inside loadTouch_previous
	loadTouch = loadTouch_none;
	loadTouchProcess();
	check(loadTouch == loadTouch_none,
		"load_touch_zero_pressure",
		"loadTouchProcess() decoded stale coordinates with z==0 into action " + std::to_string(loadTouch));
}

// 10. Gallery video playback: tap Play pill (with spotEnabled=true -> py=66),
//     play multiple frames atomically with zero direct_fill_scr flicker, pause, then < Back.
static void test_gallery_video_play_pause_and_exit() {
	reset_ui_state();
	spotEnabled = true;
	sim_sd_add_video_folder("20261125120000", 4u);
	g_sim.xpt2046_penirq_glitch_on_poll = true;
	g_sim.xpt2046_stale_coords = true;
	g_sim.setWatchdog(10000u);

	g_sim.scheduleTap(1100u, 237, 114, 40u); // Videos card
	// Play pill with spotEnabled=true is at x=108..212, y=66..98 -> tap (160, 75)
	g_sim.scheduleTap(1600u, 160, 75, 40u);  // Play!
	// Wait while frames play, then tap center (160, 120) to pause
	g_sim.scheduleTap(1800u, 160, 120, 40u); // Pause!
	// Then tap < Back (268, 217) to exit
	g_sim.scheduleTap(2500u, 268, 217, 40u); // Exit!

	loadFiles();

	check(g_sim.stats.atomic_flushes >= 3u,
		"video_played_frames",
		"expected >= 3 atomic video frame flushes (paused + playback + re-paused), got " +
			std::to_string(g_sim.stats.atomic_flushes));
}

// 11. Palette picker: navigate pages, pick a palette, stay idle (zero extra draws), then Apply.
static void test_palette_picker_flow() {
	reset_ui_state();
	g_sim.setWatchdog(8000u);
	colorScheme = colorScheme_ironblack;

	// Buttons: 0=<Back (12,192,78,34), 1=Prev (96,192,44,34), 2=Apply (146,192,112,34), 3=Next (264,192,44,34), 4..7=slots
	g_sim.scheduleTap(1500u, 286, 209, 40u); // Next page
	g_sim.scheduleTap(2000u, 118, 209, 40u); // Prev page
	g_sim.scheduleTap(2500u, 160, 52, 40u);  // Select slot 0 on current page
	g_sim.scheduleTap(4500u, 202, 209, 40u); // Idle 2s, then Apply

	bool applied = colorMenu();
	check(applied, "palette_applied", "expected colorMenu() to return true on Apply");
	// 1 initial draw + Next + Prev + Slot select = 4 total fillScr (0 spurious idle redraws!)
	check(g_sim.stats.direct_fill_scr == 4u,
		"palette_exact_draws",
		"expected 4 screen draws in palette picker, got " + std::to_string(g_sim.stats.direct_fill_scr));
}

// 12. Overlays / Live Display grid: toggle Spot, toggle Theme, stay idle 1.5s, return < Back.
static void test_overlays_grid_flow() {
	reset_ui_state();
	g_sim.setWatchdog(8000u);
	ui_setThemeMode(uiTheme_dark);

	// Tile 8 (Theme) is index 7 -> col=1, row=2 -> x=114..206, y=137..182
	g_sim.scheduleTap(1500u, 160, 160, 40u); // Cycle Dark -> Light
	g_sim.scheduleTap(2000u, 160, 160, 40u); // Cycle Light -> Hi-Con
	// Idle 1.5s, then tap < Back (12,192,142,34) -> (83, 209)
	g_sim.scheduleTap(3500u, 83, 209, 40u);

	bool toLive = liveDispMenu();
	check(!toLive, "overlays_back_returns_false", "expected < Back to return false");
	check(uiThemeMode == uiTheme_highContrast, "overlays_theme_cycled", "expected theme to cycle to High-Contrast");
	check(g_sim.stats.direct_fill_scr == 3u,
		"overlays_no_idle_redraw",
		"expected 3 draws (initial + 2 theme toggles), got " + std::to_string(g_sim.stats.direct_fill_scr));
}

// 13. Temp Limits (Range) -> Manual -> New -> Manual Limit Editor:
//     Verifies NO SPI print spam while idle (previously printed Min/Max every loop!), Min/Max adjust, Preset Save -> Temporary.
static void test_temp_limits_manual_no_idle_spam() {
	reset_ui_state();
	g_sim.setWatchdog(12000u);
	autoMode = true;

	uint32_t ops_at_manual_idle_start = 0;
	uint32_t ops_at_manual_idle_end = 0;
	g_sim.on_tick = [&](uint32_t now) {
		if (ops_at_manual_idle_start == 0 && now >= 2600u) ops_at_manual_idle_start = g_sim.stats.direct_draw_ops;
		if (ops_at_manual_idle_end == 0 && now >= 4600u) ops_at_manual_idle_end = g_sim.stats.direct_draw_ops;
	};

	// tempLimits(): Manual is (166, 46, 142, 120) -> (237, 106)
	g_sim.scheduleTap(1200u, 237, 106, 40u);
	// tempLimitsPresets(): default pos=0 ("New"), unified bottom-right OK is (166, 192, 142, 34) -> (237, 209)
	g_sim.scheduleTap(1700u, 237, 209, 40u);
	// Now inside tempLimitsManualHandler()! Sit idle from t=2600..4600 (2 seconds) -> must have ZERO draw ops!
	// t=4800: tap Min button (12, 48, 56, 120) -> (40, 108)
	g_sim.scheduleTap(4800u, 40, 108, 40u);
	// t=5200: tap "+" (252, 48, 56, 120) -> (280, 108) to raise min by 1C
	g_sim.scheduleTap(5200u, 280, 108, 40u);
	// t=5600: tap "Done" (166, 192, 142, 34) -> (237, 209) to leave minChange mode
	g_sim.scheduleTap(5600u, 237, 209, 40u);
	// t=6000: tap "OK" (237, 209) -> enters tempLimitsPresetSaveMenu (default pos=1 Preset 1)
	g_sim.scheduleTap(6000u, 237, 209, 40u);
	// t=6500: tap "<" (12, 66, 46, 80) -> (35, 106) to pick Temporary (pos=0)
	g_sim.scheduleTap(6500u, 35, 106, 40u);
	// t=7000: tap unified bottom-right "OK" (237, 209) -> saves and exits
	g_sim.scheduleTap(7000u, 237, 209, 40u);

	bool ok = tempLimits();
	check(ok, "temp_limits_manual_saved", "expected tempLimits() manual flow to return true");
	check(ops_at_manual_idle_end == ops_at_manual_idle_start,
		"temp_limits_manual_zero_idle_spi_spam",
		"tempLimitsManualHandler() executed " +
			std::to_string(ops_at_manual_idle_end - ops_at_manual_idle_start) +
			" direct draw ops during 2s idle!");
}

// 14. Hot/Cold (Isotherm) full flow: Hot -> select color -> adjust limit -> OK, then Disabled.
static void test_hot_cold_menu_flow() {
	reset_ui_state();
	g_sim.setWatchdog(10000u);

	// hotColdMenu(): Hot=(12,46,92,122)->(58,107)
	g_sim.scheduleTap(1200u, 58, 107, 40u);
	// hotColdColorMenu(): ">"=(262,66,46,80)->(285,106), unified bottom-right OK=(237,209)
	g_sim.scheduleTap(1700u, 285, 106, 40u);
	g_sim.scheduleTap(2200u, 237, 209, 40u);
	// hotColdChooserHandler(): "+"=(252,48,56,120)->(280,108), OK=(166,192,142,34)->(237,209)
	g_sim.scheduleTap(2700u, 280, 108, 40u);
	g_sim.scheduleTap(3200u, 237, 209, 40u);

	bool ok = hotColdMenu();
	check(ok, "hot_cold_hot_applied", "expected hotColdMenu() Hot flow to return true");
	check(hotColdMode == hotColdMode_hot, "hot_cold_mode_hot", "expected hotColdMode == hot");
}

// 15. Temp Points menu: Clear + < Back cleanly with zero idle redraws.
static void test_temp_points_menu_flow() {
	reset_ui_state();
	g_sim.setWatchdog(8000u);
	tempPoints[0][0] = 100;

	// Clear=(216,46,92,122)->(262,107), then < Back=(12,192,142,34)->(83,209)
	g_sim.scheduleTap(1200u, 262, 107, 40u);
	g_sim.scheduleTap(3000u, 83, 209, 40u);

	bool res = tempPointsMenu();
	check(!res, "temp_points_back", "expected tempPointsMenu() < Back to return false");
	check(tempPoints[0][0] == 0, "temp_points_cleared", "expected tempPoints to be cleared");
}

// 16. Settings menu & ALL submenus (General/Time/Date/BMP, Hardware/Battery/Gain/Format/MTP, Display/Format/Rotation/Timeout/Theme)
//     + bleed-through test when holding "Settings" tile on Main Menu (overlaps "Theme" tile!).
static void test_all_settings_submenus_and_bleed_through() {
	reset_ui_state();
	g_sim.setWatchdog(25000u);

	byte pos = 0;
	drawMainMenu(pos);
	// Hold Settings tile (262, 146) for 230ms (>150ms buttons_checkButtons timeout) — MUST NOT trigger Theme tile!
	g_sim.scheduleTap(1100u, 262, 146, 230u);
	// Inside Settings: General=(83,77), Hardware=(237,77), Display=(83,149), Theme=(237,149), <Back=(83,209)
	// 1) General -> Time -> Hour -> + -> <Back -> <Back -> Date -> Year -> + -> <Back -> <Back -> BMP Conv -> Enabled -> <Back -> <Back
	g_sim.scheduleTap(2000u, 83, 77, 40u);   // General
	g_sim.scheduleTap(2400u, 83, 77, 40u);   // Time
	g_sim.scheduleTap(2800u, 83, 77, 40u);   // Hour
	g_sim.scheduleTap(3200u, 272, 92, 40u);  // +
	g_sim.scheduleTap(3600u, 83, 208, 40u);  // < Back to Time
	g_sim.scheduleTap(4000u, 83, 209, 40u);  // < Back to General
	g_sim.scheduleTap(4400u, 237, 77, 40u);  // Date
	g_sim.scheduleTap(4800u, 83, 149, 40u);  // Year
	g_sim.scheduleTap(5200u, 272, 92, 40u);  // +
	g_sim.scheduleTap(5600u, 83, 208, 40u);  // < Back to Date
	g_sim.scheduleTap(6000u, 83, 209, 40u);  // < Back to General
	g_sim.scheduleTap(6400u, 160, 149, 40u); // BMP Conversion
	g_sim.scheduleTap(6800u, 237, 106, 40u); // Enabled
	g_sim.scheduleTap(7200u, 237, 209, 40u); // Save -> General
	g_sim.scheduleTap(7600u, 83, 209, 40u);  // < Back to Settings
	// 2) Hardware -> Battery Gauge (No) -> Lepton Gain (Low + Save) -> Format SD (No) -> USB MTP (No) -> <Back
	g_sim.scheduleTap(8000u, 237, 77, 40u);  // Hardware
	g_sim.scheduleTap(8400u, 83, 77, 40u);   // Battery Gauge
	g_sim.scheduleTap(8800u, 83, 209, 40u);  // No (unified y=192..226)
	g_sim.scheduleTap(9200u, 237, 77, 40u);  // Lepton Gain
	g_sim.scheduleTap(9600u, 237, 106, 40u); // Low gain (-10C..+450C)
	g_sim.scheduleTap(10000u, 237, 209, 40u);// Save
	g_sim.scheduleTap(10400u, 83, 149, 40u); // Format SD
	g_sim.scheduleTap(10800u, 83, 209, 40u); // No
	g_sim.scheduleTap(11200u, 237, 149, 40u);// USB MTP
	g_sim.scheduleTap(11600u, 83, 209, 40u); // No
	g_sim.scheduleTap(12000u, 83, 209, 40u); // < Back to Settings
	// 3) Display -> Temp Format (F + Save) -> Disp Rotation (Rotate 180 + Save) -> Screen Timeout (5 Min + Save) -> Theme (Light + <Back) -> <Back
	g_sim.scheduleTap(12400u, 83, 149, 40u); // Display
	g_sim.scheduleTap(12800u, 83, 77, 40u);  // Temp Format
	g_sim.scheduleTap(13200u, 237, 106, 40u);// Fahrenheit
	g_sim.scheduleTap(13600u, 237, 209, 40u);// Save
	g_sim.scheduleTap(14000u, 237, 77, 40u); // Disp Rotation
	g_sim.scheduleTap(14400u, 83, 77, 40u);  // Rotate 180
	g_sim.scheduleTap(14800u, 237, 209, 40u);// Save
	g_sim.scheduleTap(15200u, 83, 149, 40u); // Screen Timeout
	g_sim.scheduleTap(15600u, 237, 77, 40u); // 5 Min
	g_sim.scheduleTap(16000u, 237, 209, 40u);// Save -> Display
	g_sim.scheduleTap(16400u, 237, 149, 40u);// Theme
	g_sim.scheduleTap(16800u, 160, 114, 40u);// Light
	g_sim.scheduleTap(17200u, 83, 209, 40u); // < Back to Display
	g_sim.scheduleTap(17600u, 83, 209, 40u); // < Back to Settings
	// Exit Settings -> back to Main Menu -> tap < Live
	g_sim.scheduleTap(18000u, 83, 209, 40u); // < Back to Main Menu
	g_sim.scheduleTap(18800u, 83, 208, 40u); // < Live

	mainMenuHandler(&pos);

	check(convertEnabled == true, "settings_bmp_enabled", "expected BMP conversion enabled");
	check(leptonGainMode == lepton_gain_low, "settings_gain_low", "expected Lepton low gain");
	check(tempFormat == tempFormat_fahrenheit, "settings_temp_f", "expected Fahrenheit format");
	check(rotationVert == true, "settings_rot_180", "expected 180 deg rotation");
	check(screenOffTime == screenOffTime_5min, "settings_timeout_5m", "expected 5 min screen timeout");
	check(uiThemeMode == uiTheme_light, "settings_theme_light", "expected Light theme");
}

// 17. Video Recording (Normal mode): records multiple frames atomically, stops cleanly on top button,
//     creates .DAT frames on SD, and ALWAYS restores SD CWD back to "/"!
static void test_video_recording_normal_and_cwd_restore() {
	reset_ui_state();
	g_sim.setWatchdog(10000u);
	convertEnabled = false;
	showMenu = showMenu_opened;

	uint32_t direct_fills_during_rec = 0;
	uint32_t direct_fills_before_rec = 0;
	bool rec_started = false;
	bool stopped = false;

	g_sim.on_tick = [&](uint32_t now) {
		if (!rec_started && videoSave == videoSave_recording) {
			rec_started = true;
			direct_fills_before_rec = g_sim.stats.direct_fill_scr;
		}
		if (rec_started && !stopped && now >= 3200u) {
			direct_fills_during_rec = g_sim.stats.direct_fill_scr - direct_fills_before_rec;
			// Simulate top push button ISR stopping recording
			videoSave = videoSave_processing;
			stopped = true;
		}
	};

	// Tap Normal card (12, 38, 142, 136) -> (83, 106)
	g_sim.scheduleTap(1200u, 83, 106, 40u);

	videoMode();

	check(rec_started && stopped, "video_rec_ran_and_stopped", "video recording did not start/stop cleanly");
	check(direct_fills_during_rec == 0u,
		"video_rec_zero_direct_fill_scr",
		"video recording loop used direct display_fillScr() (" + std::to_string(direct_fills_during_rec) + ")");
	check(g_sim.stats.atomic_flushes >= 5u,
		"video_rec_atomic_frames",
		"expected >= 5 atomic recorded preview frames, got " + std::to_string(g_sim.stats.atomic_flushes));
	check(sim_sd_exists("/20261125143208/000000.DAT"),
		"video_rec_saved_dat",
		"expected /20261125143208/000000.DAT to exist on SD");
	check(sim_sd_cwd() == "/",
		"video_rec_restores_root_cwd",
		"videoCapture() left SD CWD at '" + sim_sd_cwd() + "' instead of '/'!");
	check(videoSave == videoSave_disabled, "video_rec_state_disabled", "videoSave not reset to disabled");
}

// 18. Video Recording (Interval / Time-Lapse mode): open Interval chooser, step interval, record, stop, CWD == "/".
static void test_video_recording_interval_flow() {
	reset_ui_state();
	g_sim.setWatchdog(12000u);
	convertEnabled = false;
	showMenu = showMenu_opened;

	bool stopped = false;
	g_sim.on_tick = [&](uint32_t now) {
		if (!stopped && videoSave == videoSave_recording && now >= 5200u) {
			videoSave = videoSave_processing;
			stopped = true;
		}
	};

	// Tap Interval card (166, 38, 142, 136) -> (237, 106)
	g_sim.scheduleTap(1200u, 237, 106, 40u);
	// Interval chooser: ">"=(285,106) -> 5s, "<"=(35,106) -> 1s, unified bottom-right OK=(237,209)
	g_sim.scheduleTap(1600u, 285, 106, 40u);
	g_sim.scheduleTap(2000u, 35, 106, 40u);
	g_sim.scheduleTap(2400u, 237, 209, 40u);

	videoMode();

	check(stopped, "interval_rec_stopped", "interval recording did not run & stop cleanly");
	check(sim_sd_exists("/20261125143208/000000.DAT"), "interval_frame_0_saved", "expected frame 0 saved");
	check(sim_sd_exists("/20261125143208/000001.DAT"), "interval_frame_1_saved", "expected frame 1 saved after 1s interval");
	check(sim_sd_cwd() == "/", "interval_cwd_root", "expected SD CWD restored to '/'");
}

// 19. Gallery Convert to BMP: must re-render the exact .DAT cleanly (ZERO Gallery HUD pills burned into BMP!),
//     write matching .BMP filename, and block duplicate conversion on a second tap.
static void test_gallery_convert_image_clean_bmp_no_hud_and_repeat() {
	reset_ui_state();
	sim_sd_add_dat_image("20261125120000.DAT", 28500u);
	sim_sd_add_dat_image("20261125120005.DAT", 31500u);
	g_sim.setWatchdog(12000u);

	// Precompute expected clean bigBuffer under loadSettings() (same state as inside loadFiles())
	loadSettings();
	loadRawData(const_cast<char*>("20261125120005.DAT"));
	renderRawDataToBuffer();
	std::vector<uint16_t> expected_clean(bigBuffer, bigBuffer + (320u * 240u));
	// Also verify displayGUI() actually modifies bigBuffer (so the comparison below is non-vacuous!)
	displayGUI(2, const_cast<char*>("25.11.26 12:00:05"));
	std::vector<uint16_t> with_gallery_hud(bigBuffer, bigBuffer + (320u * 240u));
	check(with_gallery_hud != expected_clean,
		"convert_hud_differs_from_clean",
		"sanity check failed: displayGUI() did not modify bigBuffer");

	// Reset stats and run full Gallery -> Images -> Convert -> Yes -> Convert again (already converted) -> < Back
	g_sim.stats = {};
	g_sim.scheduleTap(1100u, 83, 114, 40u);   // Images card (opens 20261125120005.DAT with Gallery HUD in bigBuffer)
	g_sim.scheduleTap(1800u, 50, 217, 40u);   // Convert pill (12..88, 206..228 -> inside loadTouch_convert 10..140, 160..230)
	g_sim.scheduleTap(2300u, 237, 201, 40u);  // Confirm dialog "Yes" button (166..308, 184..218)
	// After 1000ms "Image converted" banner, openImage reloads; tap Convert a second time -> "Image is already converted"
	g_sim.scheduleTap(4200u, 50, 217, 40u);   // Convert again -> must NOT call saveBuffer a second time!
	g_sim.scheduleTap(5500u, 268, 217, 40u);  // < Back -> exit Gallery

	loadFiles();

	check(g_sim.save_buffer_calls == 1u,
		"convert_called_once",
		"expected exactly 1 saveBuffer call (second Convert must detect existing BMP), got " +
			std::to_string(g_sim.save_buffer_calls));
	check(g_sim.last_saved_bmp_name == "20261125120005.BMP",
		"convert_matching_bmp_name",
		"expected saved BMP name 20261125120005.BMP, got " + g_sim.last_saved_bmp_name);
	check(sim_sd_exists("/20261125120005.BMP"),
		"convert_bmp_exists_on_sd",
		"expected /20261125120005.BMP to exist on virtual SD");
	check(g_sim.last_saved_bmp_pixels == expected_clean,
		"convert_bmp_clean_no_gallery_hud",
		"saved BMP pixels differed from clean renderRawDataToBuffer() (Gallery HUD buttons leaked into BMP!)");
	check(sim_sd_cwd() == "/",
		"convert_cwd_root",
		"expected SD CWD '/' after convertImage, got " + sim_sd_cwd());
}

// 20. Unified Settings bottom bar: tapping < Back on option pickers discards unsaved changes; Save commits.
static void test_settings_back_cancels_without_saving() {
	reset_ui_state();
	g_sim.setWatchdog(8000u);
	convertEnabled = false;
	tempFormat = tempFormat_celcius;

	// Enter BMP Conversion -> toggle Enabled (237, 106) -> tap < Back (83, 209) -> must remain false!
	g_sim.scheduleTap(1200u, 237, 106, 40u); // Enabled
	g_sim.scheduleTap(1700u, 83,  209, 40u); // < Back (cancel!)
	convertImageMenu(false);
	check(convertEnabled == false, "bmp_back_cancels", "expected < Back to discard unsaved BMP Conversion toggle");

	// Enter Temp Format -> toggle Fahrenheit (237, 106) -> tap < Back (83, 209) -> must remain Celsius!
	g_sim.reset(1000u);
	g_sim.setWatchdog(8000u);
	g_sim.scheduleTap(1200u, 237, 106, 40u); // Fahrenheit
	g_sim.scheduleTap(1700u, 83,  209, 40u); // < Back (cancel!)
	tempFormatMenu(false);
	check(tempFormat == tempFormat_celcius, "temp_format_back_cancels", "expected < Back to discard unsaved Temp Format toggle");
}

int main() {
	run_test("gallery_menu_idle_and_back", test_gallery_menu_idle_and_back);
	run_test("gallery_images_no_infinite_refresh_xpt2046", test_gallery_images_no_infinite_refresh_xpt2046);
	run_test("gallery_images_navigation_and_center_tap", test_gallery_images_navigation_and_center_tap);
	run_test("gallery_empty_video_folder_no_infinite_loop", test_gallery_empty_video_folder_no_infinite_loop);
	run_test("gallery_video_paused_idle_and_exit", test_gallery_video_paused_idle_and_exit);
	run_test("gallery_video_play_pause_and_exit", test_gallery_video_play_pause_and_exit);
	run_test("status_screen_zero_flicker_idle", test_status_screen_zero_flicker_idle);
	run_test("status_screen_updates_atomically_on_change", test_status_screen_updates_atomically_on_change);
	run_test("back_button_no_bleed_through_to_live", test_back_button_no_bleed_through_to_live);
	run_test("load_touch_ignores_zero_pressure", test_load_touch_ignores_zero_pressure);
	run_test("palette_picker_flow", test_palette_picker_flow);
	run_test("overlays_grid_flow", test_overlays_grid_flow);
	run_test("temp_limits_manual_no_idle_spam", test_temp_limits_manual_no_idle_spam);
	run_test("hot_cold_menu_flow", test_hot_cold_menu_flow);
	run_test("temp_points_menu_flow", test_temp_points_menu_flow);
	run_test("all_settings_submenus_and_bleed_through", test_all_settings_submenus_and_bleed_through);
	run_test("video_recording_normal_and_cwd_restore", test_video_recording_normal_and_cwd_restore);
	run_test("video_recording_interval_flow", test_video_recording_interval_flow);
	run_test("gallery_convert_image_clean_bmp_no_hud_and_repeat", test_gallery_convert_image_clean_bmp_no_hud_and_repeat);
	run_test("settings_back_cancels_without_saving", test_settings_back_cancels_without_saving);

	std::printf("\nSimulator UI test summary: %d passed, %d failed\n", g_passed, g_failed);
	return (g_failed == 0) ? 0 : 1;
}
