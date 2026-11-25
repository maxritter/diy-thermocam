#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <fonts.h>
#include <colorschemes.h>
#include <gui.h>
#include <hardware.h>
#include <livemode.h>
#include <mainmenu.h>
#include <settingsmenu.h>
#include <videomenu.h>
#include <loadmenu.h>
#include <buttons.h>
#include <temperature.h>
#include <perf.h>
#include <tc_thermal_core.h>

#include <zlib.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <sys/stat.h>

extern uint16_t sim_fb[320 * 240];

// Self-contained minimal PNG writer (2x nearest-neighbor upscale: 320x240 -> 640x480 RGB888)
static void write_be32(std::vector<uint8_t>& out, uint32_t v) {
	out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
	out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
	out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
	out.push_back(static_cast<uint8_t>(v & 0xFF));
}

static void write_chunk(std::vector<uint8_t>& out, const char type[4], const uint8_t* data, size_t len) {
	write_be32(out, static_cast<uint32_t>(len));
	size_t type_pos = out.size();
	out.insert(out.end(), type, type + 4);
	if (len > 0) out.insert(out.end(), data, data + len);
	uLong crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, &out[type_pos], static_cast<uInt>(4 + len));
	write_be32(out, static_cast<uint32_t>(crc));
}

static bool save_png_2x(const std::string& path, const uint16_t* fb) {
	const int sw = 320, sh = 240;
	const int w = sw * 2, h = sh * 2;
	std::vector<uint8_t> raw(static_cast<size_t>(h) * (1 + w * 3));

	for (int sy = 0; sy < sh; ++sy) {
		for (int dy = 0; dy < 2; ++dy) {
			uint8_t* row = &raw[static_cast<size_t>(sy * 2 + dy) * (1 + w * 3)];
			row[0] = 0; // filter None
			uint8_t* p = row + 1;
			for (int sx = 0; sx < sw; ++sx) {
				uint16_t c = fb[sy * sw + sx];
				uint8_t r = static_cast<uint8_t>(((c >> 11) & 0x1Fu) * 255u / 31u);
				uint8_t g = static_cast<uint8_t>(((c >> 5) & 0x3Fu) * 255u / 63u);
				uint8_t b = static_cast<uint8_t>((c & 0x1Fu) * 255u / 31u);
				p[0] = r; p[1] = g; p[2] = b;
				p[3] = r; p[4] = g; p[5] = b;
				p += 6;
			}
		}
	}

	uLong comp_cap = compressBound(static_cast<uLong>(raw.size()));
	std::vector<uint8_t> comp(comp_cap);
	if (compress2(comp.data(), &comp_cap, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK) {
		return false;
	}
	comp.resize(comp_cap);

	std::vector<uint8_t> png;
	static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
	png.insert(png.end(), sig, sig + 8);

	uint8_t ihdr[13] = {};
	ihdr[0] = (w >> 24) & 0xFF; ihdr[1] = (w >> 16) & 0xFF; ihdr[2] = (w >> 8) & 0xFF; ihdr[3] = w & 0xFF;
	ihdr[4] = (h >> 24) & 0xFF; ihdr[5] = (h >> 16) & 0xFF; ihdr[6] = (h >> 8) & 0xFF; ihdr[7] = h & 0xFF;
	ihdr[8] = 8; // 8-bit
	ihdr[9] = 2; // RGB
	write_chunk(png, "IHDR", ihdr, 13);
	write_chunk(png, "IDAT", comp.data(), comp.size());
	write_chunk(png, "IEND", nullptr, 0);

	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return false;
	bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
	std::fclose(f);
	return ok;
}

// Generate a realistic radiometric 160x120 thermal scene (warm electronics board + hot IC + warm mug)
static void build_synthetic_thermal_frame() {
	uint16_t mn = 65535, mx = 0;
	uint16_t mnPos = 0, mxPos = 0;
	for (int y = 0; y < 120; ++y) {
		for (int x = 0; x < 160; ++x) {
			float fx = static_cast<float>(x);
			float fy = static_cast<float>(y);
			// Ambient background 22.8..24.2 C
			float tempC = 23.2f + 0.006f * fx + 0.004f * (120.0f - fy);
			// Warm PCB region
			float d_pcb = std::hypot((fx - 78.0f) * 0.7f, (fy - 62.0f) * 0.9f);
			tempC += 9.5f * std::exp(-(d_pcb * d_pcb) / 1400.0f);
			// Hot MCU / VRM hotspot around (104, 54) -> ~50.5 C
			float d_ic = std::hypot(fx - 104.0f, fy - 54.0f);
			tempC += 19.5f * std::exp(-(d_ic * d_ic) / 190.0f);
			// Secondary warm inductor around (58, 72) -> ~38 C
			float d_ind = std::hypot(fx - 58.0f, fy - 72.0f);
			tempC += 10.0f * std::exp(-(d_ind * d_ind) / 140.0f);
			// Cool aluminum heatsink fin around (34, 34) -> ~19.8 C
			float d_cold = std::hypot(fx - 34.0f, fy - 34.0f);
			tempC -= 4.2f * std::exp(-(d_cold * d_cold) / 220.0f);

			uint16_t raw = static_cast<uint16_t>(std::round((tempC + 273.15f) * 100.0f));
			smallBuffer[y * 160 + x] = raw;
			if (raw < mn) { mn = raw; mnPos = static_cast<uint16_t>(y * 160 + x); }
			if (raw > mx) { mx = raw; mxPos = static_cast<uint16_t>(y * 160 + x); }
		}
	}
	minValue = mn;
	maxValue = mx;
	minTempVal = mn;
	maxTempVal = mx;
	minTempPos = mnPos;
	maxTempPos = mxPos;
	spotTemp = rawToTemp(smallBuffer[60 * 160 + 80]);

	// Use real firmware colorMap_glowBow (classic dark-to-bright Ironbow) + tc_core bilinear upscale & LUT colorization!
	colorScheme = colorScheme_glowBow;
	colorMap = colorMap_glowBow;
	colorElements = 224;
	tc_core::build_rgb565_lut(colorMap, 224, colorLut);
	tc_core::upscale_bilinear_2x(smallBuffer, bigBuffer);
	tc_core::convert_colors_u16(bigBuffer, tc_core::BIG_PIXELS, minValue, maxValue, colorLut, 224, hotColdMode_disabled, 0, 0);
}

static void render_font_sheet() {
	// Split screen: top half Dark theme, bottom half Daylight Light theme
	ui_setTheme(false);
	display_fillScr(UI_BG_DARK);

	// Header strip
	display_setColor(UI_SURFACE);
	display_fillRect(0, 0, 319, 26);
	display_setColor(UI_ACCENT);
	display_drawHLine(0, 26, 320);
	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print((char*)"FONT SHEET  |  BIG 16x16 + SMALL 8x12", 10, 7);

	// Dark section (y = 27..136)
	display_setBackColor(UI_BG_DARK);
	display_setFont(bigFont);
	display_setColor(UI_TEXT);
	display_print((char*)"DIY-THERMOCAM 4.0", 12, 34);
	display_setColor(UI_ACCENT);
	display_print((char*)"MWMW 0123456789", 12, 56);

	display_setFont(smallFont);
	display_setColor(UI_TEXT);
	display_print((char*)"ABCDEFGHIJKLMNOPQRSTUVWXYZ  0123456789", 8, 82);
	display_print((char*)"abcdefghijklmnopqrstuvwxyz  MWMW 0O1lI", 8, 98);
	display_setColor(UI_TEXT_MUTED);
	display_print((char*)"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ 24.8C", 8, 114);

	// Light section (y = 138..239)
	ui_setTheme(true);
	display_setColor(UI_BG_DARK);
	display_fillRect(0, 138, 320, 240);
	display_setColor(UI_ACCENT);
	display_drawHLine(0, 138, 320);

	display_setBackColor(UI_BG_DARK);
	display_setFont(bigFont);
	display_setColor(UI_TEXT);
	display_print((char*)"DAYLIGHT LIGHT UI", 12, 148);
	display_setColor(UI_ACCENT);
	display_print((char*)"W & M FULL WIDTH!", 12, 170);

	display_setFont(smallFont);
	display_setColor(UI_TEXT);
	display_print((char*)"ABCDEFGHIJKLMNOPQRSTUVWXYZ  0123456789", 8, 196);
	display_setColor(UI_TEXT_MUTED);
	display_print((char*)"abcdefghijklmnopqrstuvwxyz  REC 00:16", 8, 212);

	ui_setTheme(false);
}

int main(int argc, char** argv) {
	std::string outDir = "out";
	bool screenshotAll = false;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--screenshot-all") == 0) {
			screenshotAll = true;
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				outDir = argv[++i];
			}
		} else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
			outDir = argv[++i];
		}
	}
	if (!screenshotAll) {
		std::printf("Usage: %s --screenshot-all [out_dir]\n", argv[0]);
		return 0;
	}

	mkdir(outDir.c_str(), 0755);

	// Initialize perf values for realistic telemetry & video overlay
	g_perf.live_fps_x100 = 876;
	g_perf.video_fps_x100 = 865;
	g_perf.vospi_resyncs = 0;
	g_perf.vospi_crc_errors = 0;

	auto emit = [&](const char* name) {
		std::string p = outDir + "/" + name;
		if (!save_png_2x(p, sim_fb)) {
			std::fprintf(stderr, "Failed to write %s\n", p.c_str());
			std::exit(1);
		}
		std::printf("Wrote %s (640x480)\n", p.c_str());
	};

	// 1. Boot splash (Dark default, Version 1 Ironbow Thermal Lens)
	ui_setTheme(false);
	bootScreen();
	emit("01-boot-splash.png");

	// 2. Live HUD (RAM-composited zero-flicker over synthetic thermal frame)
	videoSave = videoSave_disabled;
	imgSave = imgSave_disabled;
	build_synthetic_thermal_frame();
	displayInfos();
	display_writeScreen(bigBuffer);
	emit("02-live-hud.png");

	// 3. Live Recording HUD (REC 00:16 + frame/FPS pill)
	videoSave = videoSave_recording;
	build_synthetic_thermal_frame();
	displayInfos();
	drawVideoRecordingOverlay(142, 16);
	display_writeScreen(bigBuffer);
	emit("03-live-recording.png");
	videoSave = videoSave_disabled;

	// 4. Main Menu Dark (with bezel-safe bottom < Live & More > pills)
	ui_setThemeMode(uiTheme_dark);
	buttons_init();
	drawMainMenu(0);
	emit("04-main-menu-dark.png");

	// 4b. Main Menu Daylight Light
	ui_setThemeMode(uiTheme_light);
	drawMainMenu(0);
	emit("04b-main-menu-light.png");

	// 4c. Settings Menu Dark (with Theme selector & inset < Back)
	ui_setThemeMode(uiTheme_dark);
	settingsMenu();
	emit("04c-settings-dark.png");

	// 4d. Theme Selector Light (Dark / Light / High-Contrast persisted in EEPROM)
	ui_setThemeMode(uiTheme_light);
	drawThemeMenu();
	emit("04d-theme-light.png");

	// 5. Visual Palette Picker with real gradient bars & bezel-safe footer (Dark)
	ui_setThemeMode(uiTheme_dark);
	drawColorMenuPage(2, colorScheme_ironblack);
	emit("05-palette-picker.png");

	// 6. Crisp Monospace Pixel Font Sheet (Dark + Daylight Light)
	render_font_sheet();
	emit("06-font-sheet.png");

	// 7. Modern 2026 Video Mode Screen (Dark, bezel-safe >=12px)
	ui_setThemeMode(uiTheme_dark);
	drawVideoModeMenu();
	emit("07-video-mode.png");

	// 8. Modern 2026 Gallery / Load Screen (Dark, bezel-safe >=12px)
	ui_setThemeMode(uiTheme_dark);
	drawLoadMenu();
	emit("08-gallery.png");

	return 0;
}
