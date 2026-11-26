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

static float smooth_box(float x, float y, float x0, float y0, float x1, float y1, float edge) {
	float dx = std::max(std::max(x0 - x, x - x1), 0.0f);
	float dy = std::max(std::max(y0 - y, y - y1), 0.0f);
	float d = std::hypot(dx, dy);
	return std::exp(-(d * d) / (edge * edge));
}

static float dist_to_segment(float px, float py, float ax, float ay, float bx, float by) {
	float vx = bx - ax, vy = by - ay;
	float wx = px - ax, wy = py - ay;
	float len2 = vx * vx + vy * vy;
	float t = (len2 > 1e-6f) ? std::clamp((wx * vx + wy * vy) / len2, 0.0f, 1.0f) : 0.0f;
	return std::hypot(px - (ax + t * vx), py - (ay + t * vy));
}

static void finalize_thermal_frame(byte scheme, const byte* cmap, int16_t clen,
                                   byte hcMode = hotColdMode_disabled, int16_t hcLevel = 35,
                                   byte hcColor = hotColdColor_red) {
	uint16_t mn = 65535, mx = 0;
	uint16_t mnPos = 0, mxPos = 0;
	for (int i = 0; i < 160 * 120; ++i) {
		uint16_t raw = smallBuffer[i];
		if (raw < mn) { mn = raw; mnPos = static_cast<uint16_t>(i); }
		if (raw > mx) { mx = raw; mxPos = static_cast<uint16_t>(i); }
	}
	minValue = mn;
	maxValue = mx;
	minTempVal = mn;
	maxTempVal = mx;
	minTempPos = mnPos;
	maxTempPos = mxPos;
	spotTemp = rawToTemp(smallBuffer[60 * 160 + 80]);

	colorScheme = scheme;
	colorMap = cmap;
	colorElements = clen;
	hotColdMode = hcMode;
	hotColdLevel = hcLevel;
	hotColdColor = hcColor;

	uint16_t hcRaw = (hcMode != hotColdMode_disabled) ? tempToRaw(static_cast<float>(hcLevel)) : 0;
	uint16_t hcRgb = tc_core::pack_rgb565(255, 48, 32);

	tc_core::build_rgb565_lut(colorMap, static_cast<uint16_t>(clen), colorLut);
	tc_core::filter_gaussian_3x3(smallBuffer, tc_core::SMALL_W, tc_core::SMALL_H);
	tc_core::upscale_sharp_2x(smallBuffer, bigBuffer);
	tc_core::convert_colors_u16(bigBuffer, tc_core::BIG_PIXELS, minValue, maxValue, colorLut,
		static_cast<uint16_t>(clen), hcMode, hcRaw, hcRgb);
}

// Scenario 1: Detailed Electronics PCB Thermal Inspection (QFN regulator, copper traces, MOSFETs, aluminum finned heatsink)
static void build_scene_pcb() {
	for (int y = 0; y < 120; ++y) {
		for (int x = 0; x < 160; ++x) {
			float fx = static_cast<float>(x), fy = static_cast<float>(y);
			float tempC = 22.8f + 0.008f * fx; // workbench ambient
			// FR-4 PCB outline (x=18..134, y=16..104)
			tempC += 5.5f * smooth_box(fx, fy, 18, 16, 134, 104, 2.5f);
			// Warm power delivery plane
			tempC += 7.5f * smooth_box(fx, fy, 68, 34, 122, 86, 14.0f);
			// Hot QFN buck converter IC in upper-right quadrant (98, 42) -> clears center spot reticle
			tempC += 28.0f * smooth_box(fx, fy, 92, 36, 106, 48, 5.5f);
			// Glowing copper power traces feeding MOSFETs and inductor
			float dTrace1 = dist_to_segment(fx, fy, 99, 42, 118, 42);
			float dTrace2 = dist_to_segment(fx, fy, 99, 42, 99, 76);
			float dTrace3 = dist_to_segment(fx, fy, 99, 76, 62, 76);
			tempC += 10.5f * std::exp(-(dTrace1 * dTrace1) / 8.0f);
			tempC += 9.0f * std::exp(-(dTrace2 * dTrace2) / 8.0f);
			tempC += 8.0f * std::exp(-(dTrace3 * dTrace3) / 9.0f);
			// Dual MOSFETs (114, 42) and (114, 68)
			tempC += 17.0f * smooth_box(fx, fy, 110, 38, 120, 46, 4.0f);
			tempC += 15.5f * smooth_box(fx, fy, 110, 64, 120, 72, 4.0f);
			// Warm shielded power inductor (60, 78)
			tempC += 14.0f * smooth_box(fx, fy, 54, 72, 68, 86, 5.0f);
			// Cool finned aluminum heatsink on left (x=24..46, y=28..76)
			if (fx >= 23 && fx <= 47 && fy >= 27 && fy <= 77) {
				float fin = 0.5f + 0.5f * std::sin((fx - 24.0f) * 1.65f);
				tempC -= (4.8f + 2.2f * fin) * smooth_box(fx, fy, 24, 28, 46, 76, 2.0f);
			}
			// Subtle sensor grain (~40 mK)
			float grain = std::sin(fx * 12.9898f + fy * 78.233f) * 0.08f;
			smallBuffer[y * 160 + x] = static_cast<uint16_t>(std::round((tempC + grain + 273.15f) * 100.0f));
		}
	}
	finalize_thermal_frame(colorScheme_glowBow, colorMap_glowBow, 224);
}

// Scenario 2: Home Insulation & Window Draft Inspection (recessed double-pane window + sill draft + radiator plume)
static void build_scene_building() {
	for (int y = 0; y < 120; ++y) {
		for (int x = 0; x < 160; ++x) {
			float fx = static_cast<float>(x), fy = static_cast<float>(y);
			// Interior plaster wall (~21.4 C with subtle vertical studs)
			float stud = 0.6f * std::exp(-std::pow(std::fmod(fx + 10.0f, 34.0f) - 17.0f, 2.0f) / 3.5f);
			float tempC = 21.4f - 0.02f * fy - stud;
			// Recessed window opening (x=36..118, y=14..70)
			float inWin = smooth_box(fx, fy, 38, 16, 116, 68, 2.2f);
			tempC -= 9.5f * inWin;
			// Warmer center mullion cross bars
			float vBar = smooth_box(fx, fy, 75, 16, 79, 68, 1.8f);
			float hBar = smooth_box(fx, fy, 38, 40, 116, 44, 1.8f);
			tempC += 3.2f * std::max(vBar, hBar);
			// Cold draft / air infiltration leaking along bottom-left window sill (7.2 C)
			float dDraft = dist_to_segment(fx, fy, 36, 68, 68, 72);
			tempC -= 5.8f * std::exp(-(dDraft * dDraft) / 22.0f);
			// Multi-rib wall radiator below window (x=42..112, y=82..108) -> ~43 C
			float radBox = smooth_box(fx, fy, 42, 82, 112, 108, 3.0f);
			float ribs = 0.72f + 0.28f * std::sin((fx - 42.0f) * 1.15f);
			tempC += 21.5f * radBox * ribs;
			// Rising warm convection plume above radiator
			float plume = smooth_box(fx, fy, 46, 68, 108, 84, 9.0f);
			tempC += 6.5f * plume * (1.0f - inWin * 0.6f);
			smallBuffer[y * 160 + x] = static_cast<uint16_t>(std::round((tempC + 273.15f) * 100.0f));
		}
	}
	finalize_thermal_frame(colorScheme_rainbow, colorMap_rainbow, 256);
}

// Scenario 3: Electrical Breaker Panel Hotspot (6 DIN-rail breakers, breaker #4 overloaded at ~78 C)
static void build_scene_breaker() {
	for (int y = 0; y < 120; ++y) {
		for (int x = 0; x < 160; ++x) {
			float fx = static_cast<float>(x), fy = static_cast<float>(y);
			float tempC = 23.2f; // metal cabinet backplate
			// DIN rail bar across y=56..64
			tempC += 2.0f * smooth_box(fx, fy, 16, 56, 144, 64, 2.0f);
			// 6 vertical circuit breakers across x=22..136
			const float temps[6] = {28.5f, 30.2f, 29.0f, 77.8f, 35.4f, 27.8f};
			for (int b = 0; b < 6; ++b) {
				float bx0 = 22.0f + b * 19.0f;
				float bx1 = bx0 + 15.0f;
				float bm = smooth_box(fx, fy, bx0, 32, bx1, 88, 2.2f);
				tempC += (temps[b] - 23.2f) * bm;
				// Feeder wires entering top & bottom terminals
				float cx = 0.5f * (bx0 + bx1);
				float dTop = dist_to_segment(fx, fy, cx, 10, cx, 32);
				float dBot = dist_to_segment(fx, fy, cx, 88, cx, 112);
				float wireBoost = (b == 3) ? 26.0f : 4.5f;
				tempC += wireBoost * std::exp(-(dTop * dTop) / 5.5f);
				tempC += (wireBoost * 0.85f) * std::exp(-(dBot * dBot) / 5.5f);
			}
			// Hot terminal lug bloom on breaker #4
			float dHot = std::hypot(fx - 86.5f, fy - 42.0f);
			tempC += 12.0f * std::exp(-(dHot * dHot) / 36.0f);
			smallBuffer[y * 160 + x] = static_cast<uint16_t>(std::round((tempC + 273.15f) * 100.0f));
		}
	}
	finalize_thermal_frame(colorScheme_ironblack, colorMap_ironblack, 256);
}

// Scenario 4: Underfloor Hydronic Radiant Heating Loops (serpentine warm water pipes in cool slab)
static void build_scene_underfloor() {
	struct Seg { float x0, y0, x1, y1, heat; };
	static const Seg kPipes[] = {
		{18, 24, 132, 24, 17.5f}, {132, 24, 132, 42, 16.8f},
		{132, 42, 26, 42, 16.0f}, {26, 42, 26, 60, 15.3f},
		{26, 60, 132, 60, 14.6f}, {132, 60, 132, 78, 13.9f},
		{132, 78, 26, 78, 13.2f}, {26, 78, 26, 96, 12.5f},
		{26, 96, 138, 96, 11.8f}
	};
	for (int y = 0; y < 120; ++y) {
		for (int x = 0; x < 160; ++x) {
			float fx = static_cast<float>(x), fy = static_cast<float>(y);
			// Cool tile floor slab with subtle tile grout grid (~20.2 C)
			float tile = ((static_cast<int>(fx) % 28 == 0) || (static_cast<int>(fy) % 28 == 0)) ? -0.35f : 0.0f;
			float tempC = 20.2f + tile;
			float pipeHeat = 0.0f;
			for (const auto& s : kPipes) {
				float d = dist_to_segment(fx, fy, s.x0, s.y0, s.x1, s.y1);
				pipeHeat = std::max(pipeHeat, s.heat * std::exp(-(d * d) / 26.0f));
			}
			tempC += pipeHeat;
			smallBuffer[y * 160 + x] = static_cast<uint16_t>(std::round((tempC + 273.15f) * 100.0f));
		}
	}
	finalize_thermal_frame(colorScheme_lava, colorMap_lava, 240);
}

static void render_font_sheet() {
	ui_setTheme(false);
	display_fillScr(UI_BG_DARK);

	display_setColor(UI_SURFACE);
	display_fillRect(0, 0, 319, 26);
	display_setColor(UI_ACCENT);
	display_drawHLine(0, 26, 320);
	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print((char*)"FONT SHEET  |  BIG 16x16 + SMALL 8x12", 10, 7);

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

	g_perf.live_fps_x100 = 877;
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

	// 1. Boot splash
	ui_setTheme(false);
	bootScreen();
	emit("01-boot-splash.png");

	// 2. Live HUD scenarios (PCB inspection, Building heat leak, Breaker panel, Underfloor heating)
	videoSave = videoSave_disabled;
	imgSave = imgSave_disabled;
	batteryEnabled = true; timeEnabled = true; dateEnabled = true;
	spotEnabled = true; colorbarEnabled = true; storageEnabled = true;
	minMaxPoints = minMaxPoints_both;

	build_scene_pcb();
	displayInfos();
	display_writeScreen(bigBuffer);
	emit("02-live-hud.png");

	build_scene_building();
	displayInfos();
	display_writeScreen(bigBuffer);
	emit("02b-live-building.png");

	build_scene_breaker();
	displayInfos();
	display_writeScreen(bigBuffer);
	emit("02c-live-breaker.png");

	build_scene_underfloor();
	displayInfos();
	display_writeScreen(bigBuffer);
	emit("02d-live-floor.png");

	// 3. Live Recording HUD (REC 00:16 + frame/FPS pill over PCB scene)
	videoSave = videoSave_recording;
	build_scene_pcb();
	displayInfos();
	drawVideoRecordingOverlay(142, 16);
	display_writeScreen(bigBuffer);
	emit("03-live-recording.png");
	videoSave = videoSave_disabled;

	// 4. Main Menu (Page 1 Dark, Page 2 Dark, Page 1 Light), Overlays, Settings, Theme
	ui_setThemeMode(uiTheme_dark);
	buttons_init();
	drawMainMenu(0);
	emit("04-main-menu-dark.png");

	ui_setThemeMode(uiTheme_light);
	drawMainMenu(0);
	emit("04b-main-menu-light.png");

	ui_setThemeMode(uiTheme_dark);
	settingsMenu();
	emit("04c-settings-dark.png");

	ui_setThemeMode(uiTheme_light);
	drawThemeMenu();
	emit("04d-theme-light.png");

	ui_setThemeMode(uiTheme_dark);
	drawMainMenu(1);
	emit("04e-main-menu-page2-dark.png");

	drawLiveDispGrid();
	emit("04f-overlays-grid-dark.png");

	// 5. Visual Palette Picker
	ui_setThemeMode(uiTheme_dark);
	drawColorMenuPage(2, colorScheme_ironblack);
	emit("05-palette-picker.png");

	// 6. Font Sheet
	render_font_sheet();
	emit("06-font-sheet.png");

	// 7. Video Recording chooser
	ui_setThemeMode(uiTheme_dark);
	drawVideoModeMenu();
	emit("07-video-mode.png");

	// 8. Gallery chooser + Gallery image viewer with HUD pills
	ui_setThemeMode(uiTheme_dark);
	drawLoadMenu();
	emit("08-gallery.png");

	batteryEnabled = false; timeEnabled = false; dateEnabled = false; storageEnabled = false;
	minMaxPoints = minMaxPoints_disabled;
	showMenu = showMenu_opened;
	build_scene_building();
	displayInfos();
	displayGUI(12, (char*)"26.11.26 04:42:57");
	display_writeScreen(bigBuffer);
	emit("08b-gallery-viewer.png");
	showMenu = showMenu_disabled;
	batteryEnabled = true; timeEnabled = true; dateEnabled = true; storageEnabled = true;

	// 9. System Status & Telemetry (RAM-composited zero-flicker)
	ui_setThemeMode(uiTheme_dark);
	drawStatusScreen();
	emit("09-status-screen.png");

	return 0;
}
