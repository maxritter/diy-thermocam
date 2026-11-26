// Film frames from the real Firmware 4.0 UI.
//
// Compiles the firmware's GUI, fonts, HUD, menus and colour maps against the host simulator in
// firmware/sim (display, stubs) and renders animated sequences for the launch film: the boot
// splash, the live HUD over animated thermal scenes (sensor rate, ~9 fps), every palette, the
// menus in all three themes and the recording overlay. Frames are 320x240 PNGs (the display's
// native resolution); the film scales them up with nearest-neighbour sampling.
//
//   make -C video/sim frames      -> video/public/sim/<sequence>/NNNN.png + index.json + palettes.json

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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <sys/stat.h>

extern uint16_t sim_fb[320 * 240];
long g_film_clock_s = 14 * 3600 + 32 * 60 + 8;

static const float kSensorFps = 9.0f;  // The Lepton delivers just under 9 frames per second
static std::string g_out = "out";
static std::string g_index = "{\n";

// ------------------------------------------------------------------------------------------
// PNG (320x240 RGB888, no scaling)
// ------------------------------------------------------------------------------------------

static void be32(std::vector<uint8_t>& o, uint32_t v) {
	for (int s = 24; s >= 0; s -= 8) o.push_back(static_cast<uint8_t>((v >> s) & 0xFF));
}

static void chunk(std::vector<uint8_t>& o, const char t[4], const uint8_t* d, size_t n) {
	be32(o, static_cast<uint32_t>(n));
	size_t at = o.size();
	o.insert(o.end(), t, t + 4);
	if (n) o.insert(o.end(), d, d + n);
	uLong crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, &o[at], static_cast<uInt>(4 + n));
	be32(o, static_cast<uint32_t>(crc));
}

static void save_png(const std::string& path) {
	const int w = 320, h = 240;
	std::vector<uint8_t> raw(static_cast<size_t>(h) * (1 + w * 3));
	for (int y = 0; y < h; ++y) {
		uint8_t* row = &raw[static_cast<size_t>(y) * (1 + w * 3)];
		row[0] = 0;
		for (int x = 0; x < w; ++x) {
			uint16_t c = sim_fb[y * w + x];
			row[1 + x * 3 + 0] = static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31);
			row[1 + x * 3 + 1] = static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63);
			row[1 + x * 3 + 2] = static_cast<uint8_t>((c & 0x1F) * 255 / 31);
		}
	}
	uLong cap = compressBound(static_cast<uLong>(raw.size()));
	std::vector<uint8_t> z(cap);
	compress2(z.data(), &cap, raw.data(), static_cast<uLong>(raw.size()), 6);
	z.resize(cap);
	std::vector<uint8_t> png = {137, 80, 78, 71, 13, 10, 26, 10};
	uint8_t ihdr[13] = {0, 0, 1, 64, 0, 0, 0, 240, 8, 2, 0, 0, 0};
	chunk(png, "IHDR", ihdr, 13);
	chunk(png, "IDAT", z.data(), z.size());
	chunk(png, "IEND", nullptr, 0);
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f || std::fwrite(png.data(), 1, png.size(), f) != png.size()) {
		std::fprintf(stderr, "cannot write %s\n", path.c_str());
		std::exit(1);
	}
	std::fclose(f);
}

struct Seq {
	std::string name;
	int n = 0;
	explicit Seq(const std::string& nm) : name(nm) { mkdir((g_out + "/" + nm).c_str(), 0755); }
	void emit() {
		char file[32];
		std::snprintf(file, sizeof(file), "/%04d.png", n++);
		save_png(g_out + "/" + name + file);
	}
	~Seq() {
		char line[160];
		std::snprintf(line, sizeof(line), "%s  \"%s\": %d", g_index.size() > 2 ? ",\n" : "", name.c_str(), n);
		g_index += line;
		std::printf("%-16s %3d frames\n", name.c_str(), n);
	}
};

// ------------------------------------------------------------------------------------------
// Thermal scenes (temperatures in degC on the Lepton's 160x120 grid, animated by time t)
// ------------------------------------------------------------------------------------------

static float box(float x, float y, float x0, float y0, float x1, float y1, float edge) {
	float dx = std::max(std::max(x0 - x, x - x1), 0.0f);
	float dy = std::max(std::max(y0 - y, y - y1), 0.0f);
	float d2 = dx * dx + dy * dy;
	return std::exp(-d2 / (edge * edge));
}

static float seg(float px, float py, float ax, float ay, float bx, float by, float* along = nullptr) {
	float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
	float l2 = vx * vx + vy * vy;
	float t = l2 > 1e-6f ? std::clamp((wx * vx + wy * vy) / l2, 0.0f, 1.0f) : 0.0f;
	if (along) *along = t * std::sqrt(l2);
	return std::hypot(px - (ax + t * vx), py - (ay + t * vy));
}

static float smooth(float e0, float e1, float x) {
	float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
	return t * t * (3 - 2 * t);
}

static float hash(int x, int y, int f) {
	uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u +
	             static_cast<uint32_t>(f) * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return static_cast<float>((h ^ (h >> 16)) & 0xFFFF) / 65535.0f - 0.5f;
}

using Scene = std::function<float(float x, float y, float t)>;

// A coffee mug on a desk: hot liquid at the rim, a warm handle, steam rising, a warm patch under it.
static float mug(float x, float y, float t) {
	float c = 21.6f + 0.9f * (1.0f - y / 120.0f);                       // wall
	c -= 0.6f * box(x, y, 0, 102, 160, 120, 1.5f);                    // desk edge, slightly cooler
	c += 6.5f * std::exp(-std::pow((x - 80) / 26.0f, 2) - std::pow((y - 104) / 4.0f, 2));  // warm desk under the mug
	// Body, slightly tapered toward the bottom
	float half = 22.0f - (y - 40) * 0.05f;
	float body = box(x, y, 80 - half, 42, 80 + half, 100, 1.4f);
	float glaze = 59.0f - 0.08f * (y - 42) - 3.0f * std::pow((x - 80) / half, 2);
	c = c + (glaze - c) * body;
	// Liquid surface at the rim
	float rim = std::exp(-std::pow((x - 80) / 21.0f, 8) - std::pow((y - 41) / 3.2f, 2));
	c = std::max(c, c + (66.0f - c) * rim);
	// Handle ring
	float r = std::hypot((x - 105) * 0.95f, y - 68);
	float handle = std::exp(-std::pow((r - 11.0f) / 2.4f, 2)) * (x > 100 ? 1.0f : 0.0f);
	c = std::max(c, c + (44.0f - c) * handle);
	// Steam: three plumes that sway and rise
	for (int i = 0; i < 3; ++i) {
		float base = 66.0f + i * 14.0f;
		float rise = std::clamp((40.0f - y) / 38.0f, 0.0f, 1.0f);
		float sway = 4.5f * std::sin(0.13f * y + 1.7f * t + i * 2.1f) * rise;
		float d = x - base - sway - (i - 1) * 6.0f * rise;
		float w = 2.2f + 5.0f * rise;
		float puff = 0.55f + 0.45f * std::sin(0.35f * y - 3.2f * t + i);
		c += (7.5f - 5.5f * rise) * std::exp(-d * d / (w * w)) * (y < 41 ? 1.0f : 0.0f) * puff * (rise > 0.98f ? 0.0f : 1.0f);
	}
	return c;
}

// The opening: a cold mug, barely there, then hot coffee pours in. The level rises, the wall
// warms behind it, then the handle and the desk; it ends where mug() begins.
static float pour(float x, float y, float t) {
	float c = 21.6f + 0.9f * (1.0f - y / 120.0f);
	c -= 0.6f * box(x, y, 0, 102, 160, 120, 1.5f);
	c += 6.5f * smooth(3.0f, 6.4f, t) * std::exp(-std::pow((x - 80) / 26.0f, 2) - std::pow((y - 104) / 4.0f, 2));
	float fill = std::clamp((t - 0.8f) / 3.6f, 0.0f, 1.0f);   // 0..1, linear
	float level = 100.0f - 58.0f * fill;                        // liquid surface (y)
	float half = 22.0f - (y - 40) * 0.05f;
	float body = box(x, y, 80 - half, 42, 80 + half, 100, 1.4f);
	float passed = 0.8f + 3.6f * (100.0f - y) / 58.0f;          // when the level reached this row
	float warm = std::clamp((t - passed) / 1.0f, 0.0f, 1.0f) * (fill > 0 ? 1.0f : 0.0f);
	float hot = 59.0f - 0.08f * (y - 42) - 3.0f * std::pow((x - 80) / half, 2);
	float glaze = 21.1f + (hot - 21.1f) * warm;
	c = c + (glaze - c) * body;
	if (fill > 0.0f && fill < 1.0f) {  // the surface while it rises, seen through the warming wall
		float surf = std::exp(-std::pow((x - 80) / 20.0f, 8) - std::pow((y - level) / 2.0f, 2)) * body;
		c = std::max(c, c + (48.0f - c) * surf * 0.6f);
	}
	float rim = std::exp(-std::pow((x - 80) / 21.0f, 8) - std::pow((y - 41) / 3.2f, 2));
	c = std::max(c, c + (66.0f - c) * rim * smooth(4.2f, 5.0f, t));
	// The stream from above, thinning out at the end
	float on = smooth(0.45f, 0.75f, t) * (1.0f - smooth(4.4f, 4.9f, t));
	float sx = 80.0f + 1.2f * std::sin(0.4f * y + 9.0f * t);
	float d = x - sx;
	float w = 2.0f + 0.6f * std::sin(0.7f * y - 14.0f * t);
	c = std::max(c, c + (82.0f - c) * on * std::exp(-d * d / (w * w)) * (y < 42 ? 1.0f : 0.0f) * smooth(-1.0f, 4.0f, y));
	// Handle warms last
	float r = std::hypot((x - 105) * 0.95f, y - 68);
	float handle = std::exp(-std::pow((r - 11.0f) / 2.4f, 2)) * (x > 100 ? 1.0f : 0.0f);
	c = std::max(c, c + (44.0f - c) * handle * smooth(3.4f, 6.4f, t));
	// Steam once it is full
	float st = smooth(3.8f, 6.0f, t);
	for (int i = 0; i < 3; ++i) {
		float base = 66.0f + i * 14.0f;
		float rise = std::clamp((40.0f - y) / 38.0f, 0.0f, 1.0f);
		float sway = 4.5f * std::sin(0.13f * y + 1.7f * t + i * 2.1f) * rise;
		float dd = x - base - sway - (i - 1) * 6.0f * rise;
		float ww = 2.2f + 5.0f * rise;
		float puff = 0.55f + 0.45f * std::sin(0.35f * y - 3.2f * t + i);
		c += st * (7.5f - 5.5f * rise) * std::exp(-dd * dd / (ww * ww)) * (y < 41 ? 1.0f : 0.0f) * puff * (rise > 0.98f ? 0.0f : 1.0f);
	}
	return c;
}

// Circuit board: a buck converter that warms up from cold (heat 0..1), traces, MOSFETs, inductor, heatsink.
static Scene pcb(float warmup_s) {
	return [warmup_s](float x, float y, float t) {
		float heat = warmup_s > 0 ? 0.35f + 0.65f * smooth(0.0f, warmup_s, t) : 1.0f;
		float c = 22.8f + 0.008f * x;
		c += 5.5f * box(x, y, 18, 16, 134, 104, 2.5f);
		c += 7.5f * heat * box(x, y, 68, 34, 122, 86, 14.0f);
		c += 28.0f * heat * box(x, y, 92, 36, 106, 48, 5.5f) * (1.0f + 0.03f * std::sin(t * 5.0f));
		float d1 = seg(x, y, 99, 42, 118, 42), d2 = seg(x, y, 99, 42, 99, 76), d3 = seg(x, y, 99, 76, 62, 76);
		c += heat * (10.5f * std::exp(-d1 * d1 / 8) + 9.0f * std::exp(-d2 * d2 / 8) + 8.0f * std::exp(-d3 * d3 / 9));
		c += heat * (17.0f * box(x, y, 110, 38, 120, 46, 4) + 15.5f * box(x, y, 110, 64, 120, 72, 4));
		c += 14.0f * heat * box(x, y, 54, 72, 68, 86, 5);
		if (x >= 23 && x <= 47 && y >= 27 && y <= 77) {
			float fin = 0.5f + 0.5f * std::sin((x - 24) * 1.65f);
			c -= (4.8f + 2.2f * fin) * box(x, y, 24, 28, 46, 76, 2);
		}
		return c;
	};
}

// Breaker panel: six breakers, number four overloaded and still climbing.
static float breaker(float x, float y, float t) {
	float c = 23.2f + 2.0f * box(x, y, 16, 56, 144, 64, 2);
	const float temps[6] = {28.5f, 30.2f, 29.0f, 70.0f + 1.6f * t, 35.4f, 27.8f};
	for (int b = 0; b < 6; ++b) {
		float x0 = 22.0f + b * 19.0f, x1 = x0 + 15.0f, cx = (x0 + x1) / 2;
		c += (temps[b] - 23.2f) * box(x, y, x0, 32, x1, 88, 2.2f);
		float dt = seg(x, y, cx, 10, cx, 32), db = seg(x, y, cx, 88, cx, 112);
		float wire = b == 3 ? 24.0f + 1.0f * t : 4.5f;
		c += wire * std::exp(-dt * dt / 5.5f) + 0.85f * wire * std::exp(-db * db / 5.5f);
	}
	float dh = std::hypot(x - 86.5f, y - 42.0f);
	return c + (11.0f + 0.8f * t) * std::exp(-dh * dh / 36.0f) * (1.0f + 0.04f * std::sin(t * 11.0f));
}

// Window and radiator: cold glass, a draft along the sill that breathes, a warm plume.
static float window_(float x, float y, float t) {
	float stud = 0.6f * std::exp(-std::pow(std::fmod(x + 10.0f, 34.0f) - 17.0f, 2) / 3.5f);
	float c = 21.4f - 0.02f * y - stud;
	float win = box(x, y, 38, 16, 116, 68, 2.2f);
	c -= 9.5f * win;
	c += 3.2f * std::max(box(x, y, 75, 16, 79, 68, 1.8f), box(x, y, 38, 40, 116, 44, 1.8f));
	float breathe = 0.75f + 0.25f * std::sin(t * 1.6f);
	float dd = seg(x, y, 36, 68, 68 + 6 * std::sin(t * 0.9f), 72);
	c -= 6.5f * breathe * std::exp(-dd * dd / 22.0f);
	float ribs = 0.72f + 0.28f * std::sin((x - 42) * 1.15f);
	c += 21.5f * box(x, y, 42, 82, 112, 108, 3) * ribs;
	float wob = 1.5f * std::sin(0.2f * x + t * 1.3f);
	c += 6.5f * box(x, y, 46, 68 + wob, 108, 84, 9) * (1.0f - win * 0.6f);
	return c;
}

// Underfloor heating: warm water enters the serpentine and the heat front travels along it.
static float floor_(float x, float y, float t) {
	struct S { float x0, y0, x1, y1; };
	static const S pipes[] = {{18, 24, 132, 24}, {132, 24, 132, 42}, {132, 42, 26, 42}, {26, 42, 26, 60},
	                          {26, 60, 132, 60}, {132, 60, 132, 78}, {132, 78, 26, 78}, {26, 78, 26, 96},
	                          {26, 96, 138, 96}};
	float tile = (static_cast<int>(x) % 28 == 0 || static_cast<int>(y) % 28 == 0) ? -0.35f : 0.0f;
	float c = 20.2f + tile, best = 0.0f, start = 0.0f;
	float front = 120.0f + 230.0f * t;  // pipe length reached by the warm water (pixels)
	for (const auto& s : pipes) {
		float along = 0.0f;
		float d = seg(x, y, s.x0, s.y0, s.x1, s.y1, &along);
		float pos = start + along;
		float warm = smooth(front, front - 60.0f, pos);
		float heat = (17.5f - pos * 0.0068f) * warm;
		best = std::max(best, heat * std::exp(-d * d / 26.0f));
		start += std::hypot(s.x1 - s.x0, s.y1 - s.y0);
	}
	return c + best;
}

struct Palette { byte id; const char* name; const byte* map; int16_t n; };
static const Palette kPalettes[19] = {
	{colorScheme_arctic, "Arctic", colorMap_arctic, 240},
	{colorScheme_blackHot, "Black-Hot", colorMap_blackHot, 224},
	{colorScheme_blueRed, "Blue-Red", colorMap_blueRed, 192},
	{colorScheme_coldest, "Coldest", colorMap_coldest, 224},
	{colorScheme_contrast, "Contrast", colorMap_contrast, 224},
	{colorScheme_doubleRainbow, "Double-Rainbow", colorMap_doubleRainbow, 256},
	{colorScheme_grayRed, "Gray-Red", colorMap_grayRed, 224},
	{colorScheme_glowBow, "Glowbow", colorMap_glowBow, 224},
	{colorScheme_grayscale, "Grayscale", colorMap_grayscale, 256},
	{colorScheme_hottest, "Hottest", colorMap_hottest, 224},
	{colorScheme_ironblack, "Ironblack", colorMap_ironblack, 256},
	{colorScheme_lava, "Lava", colorMap_lava, 240},
	{colorScheme_medical, "Medical", colorMap_medical, 224},
	{colorScheme_rainbow, "Rainbow", colorMap_rainbow, 256},
	{colorScheme_wheel1, "Wheel 1", colorMap_wheel1, 256},
	{colorScheme_wheel2, "Wheel 2", colorMap_wheel2, 256},
	{colorScheme_wheel3, "Wheel 3", colorMap_wheel3, 256},
	{colorScheme_whiteHot, "White-Hot", colorMap_whiteHot, 224},
	{colorScheme_yellow, "Yellow", colorMap_yellow, 224},
};

static const Palette& pal(byte id) {
	for (const auto& p : kPalettes) if (p.id == id) return p;
	return kPalettes[0];
}

static int g_frame = 0;

// Sample the scene with a slow handheld drift and sensor grain, then run the firmware pipeline.
static void render_scene(const Scene& scene, float t, const Palette& p, float drift = 1.0f) {
	float ox = drift * (1.6f * std::sin(t * 0.7f) + 0.5f * std::sin(t * 2.3f));
	float oy = drift * (1.1f * std::sin(t * 0.53f + 1.0f) + 0.4f * std::sin(t * 1.9f));
	++g_frame;
	for (int y = 0; y < 120; ++y)
		for (int x = 0; x < 160; ++x) {
			float c = scene(x + ox, y + oy, t) + 0.05f * hash(x, y, g_frame);
			smallBuffer[y * 160 + x] = static_cast<uint16_t>(std::lround((c + 273.15f) * 100.0f));
		}
	uint16_t mn = 65535, mx = 0, mnPos = 0, mxPos = 0;
	for (int i = 0; i < 160 * 120; ++i) {
		if (smallBuffer[i] < mn) { mn = smallBuffer[i]; mnPos = static_cast<uint16_t>(i); }
		if (smallBuffer[i] > mx) { mx = smallBuffer[i]; mxPos = static_cast<uint16_t>(i); }
	}
	minValue = minTempVal = mn;
	maxValue = maxTempVal = mx;
	minTempPos = mnPos;
	maxTempPos = mxPos;
	spotTemp = rawToTemp(smallBuffer[60 * 160 + 80]);
	colorScheme = p.id;
	colorMap = p.map;
	colorElements = p.n;
	hotColdMode = hotColdMode_disabled;
	tc_core::build_rgb565_lut(colorMap, static_cast<uint16_t>(colorElements), colorLut);
	tc_core::filter_gaussian_3x3(smallBuffer, tc_core::SMALL_W, tc_core::SMALL_H);
	tc_core::upscale_sharp_2x(smallBuffer, bigBuffer);
	tc_core::convert_colors_u16(bigBuffer, tc_core::BIG_PIXELS, minValue, maxValue, colorLut,
		static_cast<uint16_t>(colorElements), hotColdMode, 0, 0);
	displayInfos();
}

static void live(const char* name, const Scene& scene, float seconds, byte palette, float t0 = 0.0f,
                 bool recording = false) {
	Seq s(name);
	int frames = static_cast<int>(std::lround(seconds * kSensorFps));
	std::string hud = "[";  // Per frame: hottest x, y and coldest x, y on the 320x240 screen
	for (int i = 0; i < frames; ++i) {
		float t = t0 + i / kSensorFps;
		g_film_clock_s = 14 * 3600 + 32 * 60 + 8 + static_cast<long>(t);
		render_scene(scene, t, pal(palette));
		if (recording) drawVideoRecordingOverlay(static_cast<uint32_t>(i), static_cast<uint32_t>(t - t0));
		display_writeScreen(bigBuffer);
		s.emit();
		char pos[64];
		std::snprintf(pos, sizeof(pos), "%s[%d,%d,%d,%d]", i ? "," : "", (maxTempPos % 160) * 2, (maxTempPos / 160) * 2,
		              (minTempPos % 160) * 2, (minTempPos / 160) * 2);
		hud += pos;
	}
	FILE* f = std::fopen((g_out + "/" + name + "/hud.json").c_str(), "w");
	std::fputs((hud + "]\n").c_str(), f);
	std::fclose(f);
}

// One still frame's temperatures (degC, 160x120, no drift or grain) for the pixel-grid shot.
static void write_grid(const char* name, const Scene& scene, float t) {
	std::string json = "{\"w\": 160, \"h\": 120, \"t\": [";
	float mn = 1e9f, mx = -1e9f;
	for (int y = 0; y < 120; ++y)
		for (int x = 0; x < 160; ++x) {
			float c = scene(static_cast<float>(x), static_cast<float>(y), t);
			mn = std::min(mn, c);
			mx = std::max(mx, c);
			char v[16];
			std::snprintf(v, sizeof(v), "%s%.1f", x || y ? "," : "", c);
			json += v;
		}
	char tail[64];
	std::snprintf(tail, sizeof(tail), "], \"min\": %.1f, \"max\": %.1f}\n", mn, mx);
	json += tail;
	FILE* f = std::fopen((g_out + "/" + name + ".json").c_str(), "w");
	std::fputs(json.c_str(), f);
	std::fclose(f);
}

static void hud_on() {
	videoSave = videoSave_disabled;
	imgSave = imgSave_disabled;
	batteryEnabled = timeEnabled = dateEnabled = true;
	spotEnabled = colorbarEnabled = storageEnabled = true;
	minMaxPoints = minMaxPoints_both;
	showMenu = showMenu_disabled;
}

static void write_palettes() {
	std::string json = "[\n";
	for (int i = 0; i < 19; ++i) {
		const auto& p = kPalettes[i];
		json += "  {\"name\": \"" + std::string(p.name) + "\", \"stops\": [";
		for (int k = 0; k < 24; ++k) {
			int e = k * (p.n - 1) / 23;
			char hex[16];
			std::snprintf(hex, sizeof(hex), "%s\"#%02x%02x%02x\"", k ? ", " : "", p.map[e * 3], p.map[e * 3 + 1], p.map[e * 3 + 2]);
			json += hex;
		}
		json += i < 18 ? "]},\n" : "]}\n";
	}
	json += "]\n";
	FILE* f = std::fopen((g_out + "/palettes.json").c_str(), "w");
	std::fputs(json.c_str(), f);
	std::fclose(f);
}

int main(int argc, char** argv) {
	if (argc > 1) g_out = argv[1];
	mkdir(g_out.c_str(), 0755);
	g_perf.live_fps_x100 = 877;
	g_perf.video_fps_x100 = 865;

	{ Seq s("boot"); ui_setThemeMode(uiTheme_dark); bootScreen(); s.emit(); }

	hud_on();
	ui_setThemeMode(uiTheme_dark);
	live("live-mug", mug, 12.0f, colorScheme_glowBow);
	live("live-pcb", pcb(5.0f), 7.0f, colorScheme_glowBow);

	{   // Every palette over the same mug, a little under half a second each
		Seq s("palettes");
		float t = 0.0f;
		for (const auto& p : kPalettes)
			for (int k = 0; k < 5; ++k, t += 1.0f / kSensorFps) {
				render_scene(mug, t, p);
				display_writeScreen(bigBuffer);
				s.emit();
			}
	}

	live("use-breaker", breaker, 5.0f, colorScheme_lava);
	live("use-window", window_, 5.0f, colorScheme_rainbow);
	live("use-pcb", pcb(0.0f), 5.0f, colorScheme_glowBow, 3.0f);
	live("use-floor", floor_, 5.0f, colorScheme_lava);
	videoSave = videoSave_recording;
	live("rec", mug, 6.0f, colorScheme_glowBow, 2.0f, true);
	videoSave = videoSave_disabled;

	// Menus: one still each (the film adds the taps)
	struct Still { const char* name; uint8_t theme; std::function<void()> draw; };
	const Still stills[] = {
		{"menu-dark", uiTheme_dark, [] { buttons_init(); drawMainMenu(0); }},
		{"menu-light", uiTheme_light, [] { buttons_init(); drawMainMenu(0); }},
		{"menu-contrast", uiTheme_highContrast, [] { buttons_init(); drawMainMenu(0); }},
		{"menu-page2", uiTheme_dark, [] { buttons_init(); drawMainMenu(1); }},
		{"palette-picker", uiTheme_dark, [] { drawColorMenuPage(2, colorScheme_ironblack); }},
		{"settings", uiTheme_dark, [] { settingsMenu(); }},
		{"theme-light", uiTheme_light, [] { drawThemeMenu(); }},
		{"video-mode", uiTheme_dark, [] { drawVideoModeMenu(); }},
		{"gallery", uiTheme_dark, [] { drawLoadMenu(); }},
		{"status", uiTheme_dark, [] { drawStatusScreen(); }},
	};
	for (const auto& st : stills) {
		Seq s(st.name);
		ui_setThemeMode(st.theme);
		st.draw();
		s.emit();
	}

	{   // Gallery viewer: a saved window image with the viewer's controls
		Seq s("gallery-viewer");
		ui_setThemeMode(uiTheme_dark);
		batteryEnabled = timeEnabled = dateEnabled = storageEnabled = false;
		minMaxPoints = minMaxPoints_disabled;
		showMenu = showMenu_opened;
		render_scene(window_, 1.0f, pal(colorScheme_rainbow), 0.0f);
		displayGUI(12, (char*)"26.11.26 14:31:02");
		display_writeScreen(bigBuffer);
		s.emit();
		hud_on();
	}

	// Added last so the frames above keep their sensor grain
	{   // The opening: hot coffee poured into a cold mug, heat appearing out of nothing
		hud_on();
		ui_setThemeMode(uiTheme_dark);
		Seq s("pour");
		const float seconds = 6.4f;
		for (int i = 0; i < static_cast<int>(std::lround(seconds * kSensorFps)); ++i) {
			float t = i / kSensorFps;
			g_film_clock_s = 14 * 3600 + 32 * 60 + 1 + static_cast<long>(t);
			render_scene(pour, t, pal(colorScheme_glowBow));
			display_writeScreen(bigBuffer);
			s.emit();
		}
	}
	write_grid("grid-mug", mug, 1.0f);
	{   // The same mug without the camera's overlay, for Studio's viewport
		batteryEnabled = timeEnabled = dateEnabled = false;
		spotEnabled = colorbarEnabled = storageEnabled = false;
		minMaxPoints = minMaxPoints_disabled;
		live("studio-mug", mug, 12.0f, colorScheme_glowBow);
		hud_on();
	}

	{   // Two submenus, drawn with the same calls as tempLimits() and tempPointsMenu()
		ui_setThemeMode(uiTheme_dark);
		{
			Seq s("range");
			ui_beginScreen("TEMP. RANGE", "AUTO");
			ui_drawCard(12, 46, 142, 120, "Auto", "Dynamic", true);
			ui_drawCard(166, 46, 142, 120, "Manual", "Locked", false);
			ui_drawPill(12, 192, 142, 34, "< Back", uiPill_secondary);
			s.emit();
		}
		{
			Seq s("points");
			ui_beginScreen("TEMP. POINTS", "3 ACTIVE");
			ui_drawCard(12, 46, 92, 122, "Add", "Tap Img", false);
			ui_drawCard(114, 46, 92, 122, "Remove", "Tap Pt", false);
			ui_drawCard(216, 46, 92, 122, "Clear", "3 Pts", true);
			ui_drawPill(12, 192, 142, 34, "< Back", uiPill_secondary);
			s.emit();
		}
	}

	write_palettes();
	g_index += "\n}\n";
	FILE* f = std::fopen((g_out + "/index.json").c_str(), "w");
	std::fputs(g_index.c_str(), f);
	std::fclose(f);
	return 0;
}
