#include <Arduino.h>
#include <EEPROM.h>
#include <TimeLib.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <touchscreen.h>
#include <colorschemes.h>
#include <perf.h>
#include <gui.h>
#include <temperature.h>
#include <thermal.h>
#include <hardware.h>
#include <lepton.h>
#include <sdcard.h>
#include <battery.h>
#include <connection.h>
#include <create.h>
#include <save.h>
#include <load.h>
#include <loadmenu.h>
#include <settingsmenu.h>
#include <massstorage.h>
#include <tc_thermal_core.h>
#include "sim_harness.h"

#include <map>
#include <set>

SimRuntime g_sim;
SimEEPROM EEPROM;
SimTeensyClock Teensy3Clock;

// Globals matching src/general/globalvariables.cpp
char versionString[] = "4.0.0 (2026-11-25)";
uint16_t fwVersion = 400;

static uint16_t s_bigA[320 * 240];
static uint16_t s_bigB[320 * 240];
static uint16_t s_smallA[160 * 120];
static uint16_t s_smallB[160 * 120];

unsigned short* bigBuffer = s_bigA;
unsigned short* bigBufferBack = s_bigB;
unsigned short* smallBuffer = s_smallA;
unsigned short* smallBufferBack = s_smallB;

Metro screenOff(0, false);
boolean screenPressed = false;
byte screenOffTime = screenOffTime_disabled;
Bounce buttonDebouncer(pin_button, 200);
Bounce touchDebouncer(pin_touch_irq, 200);
SdFs sd;
FsFile sdFile;
String sdInfo("14.2GB");
FsFile dir;
uint32_t sdTotalKB = 15204352u;
uint32_t sdFreeKB = 14889779u;
uint8_t sdFsType = 3;
char saveFilename[20] = "20261125143208.DAT";
ADC* batMeasure = nullptr;
int8_t batPercentage = 84;
uint16_t batMillivolts = 3980;
long batTimer = 1000;
int8_t batComp = 6;
bool usbConnected = false;
bool convertEnabled = false;
bool autoMode = true;
bool limitsLocked = false;
bool rotationVert = false;
bool rotationHorizont = false;
bool batteryEnabled = true;
bool timeEnabled = true;
bool dateEnabled = true;
bool spotEnabled = true;
bool colorbarEnabled = true;
bool storageEnabled = true;
byte filterType = filterType_gaussian;
byte minMaxPoints = minMaxPoints_both;
bool tempFormat = tempFormat_celcius;
byte textColor = textColor_white;
bool leptonGainMode = lepton_gain_high;
float leptonCalSlope = 0.01f;
byte leptonVersion = leptonVersion_3_5_shutter;
byte diagnostic = diag_ok;
byte colorScheme = colorScheme_ironblack;
const byte* colorMap = colorMap_ironblack;
int16_t colorElements = 256;
uint16_t colorLut[256] = {};
bool shutterModeAuto = true;
byte minMaxPresetCache = minMax_temporary;
char leptonPartNum[32] = "05-070170";
float leptonFpaTempC = 32.4f;
volatile uint32_t leptonFrameSeq = 0;
volatile uint64_t leptonFrameTimestampUs = 0;
uint16_t maxValue = 32095; // ~47.8 C
uint16_t minValue = 29455; // ~21.4 C
float spotTemp = 34.6f;
float ambTemp = 22.5f;
uint16_t minTempPos = 22 * 160 + 28;
uint16_t minTempVal = 29455;
uint16_t maxTempPos = 54 * 160 + 102;
uint16_t maxTempVal = 32095;
byte hotColdMode = hotColdMode_disabled;
int16_t hotColdLevel = 35;
byte hotColdColor = hotColdColor_red;
uint16_t tempPoints[96][2] = {};
volatile byte imgSave = imgSave_disabled;
volatile byte videoSave = videoSave_disabled;
volatile byte showMenu = showMenu_disabled;
volatile bool longTouch = false;
volatile bool serialMode = false;
volatile byte loadTouch = loadTouch_none;
volatile bool leptonBufferValid = false;
volatile uint32_t leptonFrameReadyUs = 0;
volatile bool disableSPIIRQ = false;
volatile bool touchIrqFlag = false;
volatile bool buttonIrqFlag = false;
volatile uint32_t touchLockoutUntilMs = 0;

volatile bool touch_capacitive = false;

// Perf globals
uint32_t g_display_spi_hz = 40000000u;
uint32_t g_lepton_spi_hz = 20000000u;
uint8_t g_lepton_spi_mode = 1;
bool g_vospi_crc_enabled = true;
bool g_display_dma_active = false;
PerfRuntime g_perf = {};

// ---------------- Arduino time & GPIO shims backed by g_sim ----------------

uint32_t millis() { return g_sim.now_ms; }
uint32_t micros() { return g_sim.now_ms * 1000u; }
void delay(uint32_t ms) {
	if (ms == 0u) return;
	// Step in <=5ms increments so touch script transitions & ticks are never missed
	uint32_t rem = ms;
	while (rem > 0u) {
		uint32_t step = (rem > 5u) ? 5u : rem;
		g_sim.advanceMs(step);
		rem -= step;
	}
}
void delayMicroseconds(uint32_t) {}
int digitalRead(int pin) {
	if (pin == pin_touch_irq) {
		int16_t x = 0, y = 0;
		return g_sim.activeTouchAtNow(x, y) ? LOW : HIGH;
	}
	return HIGH;
}
void digitalWrite(int, int) {}
int analogRead(int) { return 0; }
void pinMode(int, int) {}
void noInterrupts() {}
void interrupts() {}
void attachInterrupt(int pin, void (*fn)(), int) {
	if (pin >= 0 && pin < 64) g_sim.irq_handlers[pin] = fn;
}
void detachInterrupt(int pin) {
	if (pin >= 0 && pin < 64) g_sim.irq_handlers[pin] = nullptr;
}
void yield() {}

// ---------------- Virtual In-Memory SD Filesystem ----------------

struct SimFsEntry {
	bool is_dir = false;
	std::vector<uint8_t> data;
};

static std::map<std::string, SimFsEntry> s_fs;
static std::string s_cwd = "/";

static std::string norm_path(const char* raw) {
	if (!raw || raw[0] == '\0') return s_cwd;
	std::string p(raw);
	if (p[0] != '/') {
		if (s_cwd == "/") p = "/" + p;
		else p = s_cwd + "/" + p;
	}
	while (p.size() > 1 && p.back() == '/') p.pop_back();
	return p;
}

static std::string basename_of(const std::string& p) {
	if (p == "/") return "/";
	size_t slash = p.rfind('/');
	return (slash == std::string::npos) ? p : p.substr(slash + 1);
}

static std::string parent_of(const std::string& p) {
	if (p == "/") return "";
	size_t slash = p.rfind('/');
	if (slash == std::string::npos || slash == 0) return "/";
	return p.substr(0, slash);
}

void sim_sd_clear() {
	s_fs.clear();
	s_fs["/"] = SimFsEntry{true, {}};
	s_cwd = "/";
}

static std::vector<uint8_t> make_lepton3_dat_bytes(uint16_t fill_raw) {
	// lepton3_small = 38421 bytes (19200 * 2 + 21 metadata bytes)
	std::vector<uint8_t> buf(38421u, 0u);
	for (size_t i = 0; i < 19200u; ++i) {
		uint16_t v = static_cast<uint16_t>(fill_raw + (i % 64u) * 4u);
		buf[i * 2] = static_cast<uint8_t>((v >> 8) & 0xFFu);
		buf[i * 2 + 1] = static_cast<uint8_t>(v & 0xFFu);
	}
	uint16_t mn = fill_raw;
	uint16_t mx = static_cast<uint16_t>(fill_raw + 63u * 4u);
	size_t off = 38400u;
	buf[off + 0] = static_cast<uint8_t>((mn >> 8) & 0xFFu);
	buf[off + 1] = static_cast<uint8_t>(mn & 0xFFu);
	buf[off + 2] = static_cast<uint8_t>((mx >> 8) & 0xFFu);
	buf[off + 3] = static_cast<uint8_t>(mx & 0xFFu);
	float spot = 32.5f;
	std::memcpy(&buf[off + 4], &spot, 4);
	buf[off + 8] = colorScheme_ironblack;
	buf[off + 9] = tempFormat_celcius;
	buf[off + 10] = 1; // spotEnabled
	buf[off + 11] = 1; // colorbarEnabled
	buf[off + 12] = minMaxPoints_disabled;
	return buf;
}

void sim_sd_add_dat_image(const std::string& filename, uint16_t fill_raw) {
	if (s_fs.find("/") == s_fs.end()) s_fs["/"] = SimFsEntry{true, {}};
	std::string p = (filename.empty() || filename[0] == '/') ? filename : ("/" + filename);
	s_fs[p] = SimFsEntry{false, make_lepton3_dat_bytes(fill_raw)};
}

void sim_sd_add_video_folder(const std::string& dirname, uint32_t num_frames) {
	if (s_fs.find("/") == s_fs.end()) s_fs["/"] = SimFsEntry{true, {}};
	std::string d = (dirname.empty() || dirname[0] == '/') ? dirname : ("/" + dirname);
	s_fs[d] = SimFsEntry{true, {}};
	for (uint32_t i = 0; i < num_frames; ++i) {
		char fname[16];
		std::snprintf(fname, sizeof(fname), "%06lu.DAT", static_cast<unsigned long>(i));
		s_fs[d + "/" + fname] = SimFsEntry{false, make_lepton3_dat_bytes(static_cast<uint16_t>(30000u + i * 20u))};
	}
}

void sim_sd_add_raw_file(const std::string& path, const std::vector<uint8_t>& bytes) {
	if (s_fs.find("/") == s_fs.end()) s_fs["/"] = SimFsEntry{true, {}};
	std::string p = (path.empty() || path[0] == '/') ? path : ("/" + path);
	s_fs[p] = SimFsEntry{false, bytes};
}

std::string sim_sd_cwd() { return s_cwd; }
bool sim_sd_exists(const std::string& path) { return s_fs.find(path) != s_fs.end(); }

bool FsFile::open(const char* path, int) {
	close();
	std::string p = norm_path(path);
	auto it = s_fs.find(p);
	if (it == s_fs.end()) return false;
	open_ = true;
	is_dir_ = it->second.is_dir;
	path_ = p;
	name_ = basename_of(p);
	data_ = it->second.data;
	pos_ = 0;
	dir_children_.clear();
	dir_idx_ = 0;
	if (is_dir_) {
		for (const auto& kv : s_fs) {
			if (kv.first == p) continue;
			if (parent_of(kv.first) == p) {
				dir_children_.push_back(kv.first);
			}
		}
	}
	return true;
}

bool FsFile::openNext(FsFile* parent, int oflag) {
	close();
	if (!parent || !parent->open_ || !parent->is_dir_) return false;
	if (parent->dir_idx_ >= parent->dir_children_.size()) return false;
	std::string child = parent->dir_children_[parent->dir_idx_++];
	return open(child.c_str(), oflag);
}

void FsFile::close() {
	open_ = false;
	is_dir_ = false;
	path_.clear();
	name_.clear();
	data_.clear();
	pos_ = 0;
	dir_children_.clear();
	dir_idx_ = 0;
}

int FsFile::read() {
	if (!open_ || is_dir_ || pos_ >= data_.size()) return -1;
	return static_cast<int>(data_[pos_++]);
}

size_t FsFile::getName(char* buf, size_t len) const {
	if (!buf || len == 0) return 0;
	std::strncpy(buf, name_.c_str(), len - 1);
	buf[len - 1] = '\0';
	return std::strlen(buf);
}

bool SdFs::chdir(const char* path) {
	std::string p = norm_path(path);
	auto it = s_fs.find(p);
	if (it == s_fs.end() || !it->second.is_dir) return false;
	s_cwd = p;
	return true;
}

bool SdFs::mkdir(const char* path) {
	std::string p = norm_path(path);
	s_fs[p] = SimFsEntry{true, {}};
	return true;
}

bool SdFs::exists(const char* path) {
	return s_fs.find(norm_path(path)) != s_fs.end();
}

bool SdFs::remove(const char* path) {
	std::string p = norm_path(path);
	auto it = s_fs.find(p);
	if (it == s_fs.end() || it->second.is_dir) return false;
	s_fs.erase(it);
	return true;
}

bool SdFs::rmdir(const char* path) {
	std::string p = norm_path(path);
	auto it = s_fs.find(p);
	if (it == s_fs.end() || !it->second.is_dir) return false;
	s_fs.erase(it);
	return true;
}

// ---------------- Perf & Hardware / Driver stubs ----------------

float perf_mcu_temp_c() { return g_sim.sim_mcu_temp_c; }
void perf_record_frame_period(uint8_t, uint32_t) {}
void perf_save_video_snapshot() {}

bool isUSBConnected() { return usbConnected; }
void setDisplayRotation() {}
void displayBuffer() { display_writeScreen(bigBuffer); }
bool checkDiagnostic(byte device) { return (diagnostic >> device) & 1; }
void setDiagnostic(byte device) { diagnostic &= ~(1 << device); }
void floatToBytes(uint8_t* farray, float val) { std::memcpy(farray, &val, 4); }
float bytesToFloat(uint8_t* farray) { float v = 0; std::memcpy(&v, farray, 4); return v; }
void readTempLimits() {}
void readEEPROM() {}
bool screenOffCheck() { return false; }
bool checkScreenLight() { return true; }
void enableScreenLight() {}
void disableScreenLight() {}

TS_Point touch_getPoint() {
	int16_t x = 0, y = 0;
	if (g_sim.activeTouchAtNow(x, y)) {
		g_sim.last_x = x;
		g_sim.last_y = y;
		return TS_Point(x, y, 1200);
	}
	if (g_sim.xpt2046_stale_coords) {
		// Exact XPT2046 hardware behavior: xraw/yraw keep last touch coordinates, zraw == 0
		return TS_Point(g_sim.last_x, g_sim.last_y, 0);
	}
	return TS_Point(0, 0, 0);
}

void touch_init() {}
void touch_setRotation(bool) {}

volatile bool touch_touched() {
	// Advance virtual clock by 1ms on every poll so busy-wait loops never hang host tests
	g_sim.advanceMs(1u);
	int16_t x = 0, y = 0;
	bool active = g_sim.activeTouchAtNow(x, y);
	if (active) {
		g_sim.last_x = x;
		g_sim.last_y = y;
		return true;
	}
	// Real XPT2046 SPI polling briefly switches internal mux / PD bits and pulses PENIRQ low!
	if (!touch_capacitive && g_sim.xpt2046_penirq_glitch_on_poll) {
		g_sim.fireIrq(pin_touch_irq);
	}
	return false;
}

void touchIRQ() {}
void checkBattery(bool, bool) {}
bool checkSerial() { return false; }
bool lepton_ffc(bool, bool) { return false; }
void lepton_ffcMode(bool) {}
void lepton_setHighGain() {}
void lepton_setLowGain() {}
void lepton_startFrame() {}
void lepton_endFrame() {}
void setMassStorage() {}
void initScreenOffTimer() {}
bool formatCard() { return true; }
void clearTempPoints() { std::memset(tempPoints, 0, sizeof(tempPoints)); }
void tempPointFunction(bool) {}
void showTemperatures() {}
void createThermalImg(bool) {}
void createThermalImgVideo(bool saveRawNow, char* dirName, uint32_t folderFrames) {
	// Simulate 114 ms (~8.8 FPS) Lepton frame cadence + save frame file into active video folder
	g_sim.advanceMs(114u);
	if (saveRawNow && dirName && dirName[0]) {
		char fname[16];
		std::snprintf(fname, sizeof(fname), "%06lu.DAT", static_cast<unsigned long>(folderFrames));
		std::string p = std::string("/") + dirName + "/" + fname;
		s_fs[p] = SimFsEntry{false, make_lepton3_dat_bytes(static_cast<uint16_t>(30200u + folderFrames * 10u))};
	}
}
void boxFilter() { tc_core::filter_box_3x3(smallBuffer, tc_core::SMALL_W, tc_core::SMALL_H); }
void gaussianFilter() { tc_core::filter_gaussian_3x3(smallBuffer, tc_core::SMALL_W, tc_core::SMALL_H); }
void smallToBigBuffer() { tc_core::upscale_bilinear_2x(smallBuffer, bigBuffer); }
void refreshMinMax() {
	tc_core::MinMaxStats st{};
	tc_core::scan_min_max(smallBuffer, tc_core::SMALL_PIXELS, st);
	minTempPos = st.min_pos;
	minTempVal = st.min_val;
	maxTempPos = st.max_pos;
	maxTempVal = st.max_val;
}
void selectColorScheme() {
	colorMap = colorMap_ironblack;
	colorElements = 256;
	tc_core::build_rgb565_lut(colorMap, 256, colorLut);
}
void convertColors(bool small) {
	unsigned short* fb = small ? smallBuffer : bigBuffer;
	size_t cnt = small ? tc_core::SMALL_PIXELS : tc_core::BIG_PIXELS;
	tc_core::convert_colors_u16(fb, cnt, minValue, maxValue, colorLut,
		static_cast<uint16_t>(colorElements), hotColdMode_disabled, 0, 0);
}
void saveRawData(bool, char*, uint32_t) {}
void saveBuffer(char* filename) {
	if (!filename) return;
	sd.chdir("/");
	std::strcpy(&filename[14], ".BMP");
	std::string p = std::string("/") + filename;
	s_fs[p] = SimFsEntry{false, std::vector<uint8_t>(614466u, 0u)};
	g_sim.last_saved_bmp_name = filename;
	g_sim.last_saved_bmp_pixels.assign(bigBuffer, bigBuffer + (320u * 240u));
	g_sim.save_buffer_calls++;
}
void frameFilename(char* filename, uint32_t count) {
	std::snprintf(filename, 11, "%06lu.DAT", static_cast<unsigned long>(count));
}
void createSDName(char* filename, bool) { std::strcpy(filename, "20261125143208"); }
bool beginSD() { return true; }
uint32_t getSDSpace() { return sdFreeKB; }
uint32_t getSDTotalKB() { return sdTotalKB; }
uint32_t getSDFreeKB() { return sdFreeKB; }
void refreshFreeSpace() {}
void processVideoFrames(uint32_t, char*) {}

float rawToTemp(uint16_t rawValue) {
	float c = static_cast<float>(rawValue) * leptonCalSlope - 273.15f;
	return (tempFormat == tempFormat_fahrenheit) ? (c * 1.8f + 32.0f) : c;
}
uint16_t tempToRaw(float temp) {
	float c = (tempFormat == tempFormat_fahrenheit) ? ((temp - 32.0f) / 1.8f) : temp;
	return static_cast<uint16_t>(std::round((c + 273.15f) / leptonCalSlope));
}
void calculatePointPos(int16_t* xpos, int16_t* ypos, uint16_t pixelIndex) {
	*xpos = static_cast<int16_t>((pixelIndex % 160) * 2);
	*ypos = static_cast<int16_t>((pixelIndex / 160) * 2);
}
void getHotColdColors(byte* r, byte* g, byte* b) { *r = 255; *g = 0; *b = 0; }
void changeDisplayOptions(byte*) {}
void changeColorScheme(byte* pos) {
	colorScheme = *pos;
	selectColorScheme();
}

// Exact copy of thermal.cpp showColorBar() so simulator renders identical colorbar + tick labels
void showColorBar() {
	char buffer[8];
	byte red, green, blue, fac;
	byte count = 0;
	byte height = 240 - ((240 - (colorElements / 2)) / 2);
	float colorLevel = 0;
	if ((hotColdMode != hotColdMode_disabled) && (maxValue > minValue))
		colorLevel = (tempToRaw(hotColdLevel) * 1.0f - minValue) / (maxValue * 1.0f - minValue);

	byte barRows = static_cast<byte>((colorElements) / 2);
	int yTop = static_cast<int>(height) - static_cast<int>(barRows) + 1;
	display_setColor(15, 23, 42);
	display_drawHLine(297, yTop - 1, 19);
	display_drawHLine(297, height + 1, 19);
	display_drawVLine(297, yTop, barRows);
	display_drawVLine(315, yTop, barRows);

	for (int i = 0; i < (colorElements - 1); i++) {
		fac = 2;
		if ((i % fac) == 0) {
			if ((hotColdMode == hotColdMode_hot) && (i >= (colorLevel * colorElements)))
				getHotColdColors(&red, &green, &blue);
			else if ((hotColdMode == hotColdMode_cold) && (i <= (colorLevel * colorElements)))
				getHotColdColors(&red, &green, &blue);
			else {
				red = colorMap[i * 3];
				green = colorMap[(i * 3) + 1];
				blue = colorMap[(i * 3) + 2];
			}
			display_setColor(red, green, blue);
			display_drawHLine(298, height - count, 17);
			count++;
		}
	}

	display_blendRect(264, yTop - 6, 31, barRows + 12, 0x08A5, 4);
	changeTextColor();

	float minT = rawToTemp(minValue);
	float maxT = rawToTemp(maxValue);
	float step = (maxT - minT) / 3.0f;

	std::snprintf(buffer, sizeof(buffer), "%d", (int)std::round(minT));
	display_print(buffer, 267, height - 5);
	for (int i = 2; i >= 1; i--) {
		float temp = minT + (i * step);
		std::snprintf(buffer, sizeof(buffer), "%d", (int)std::round(temp));
		display_print(buffer, 267, height - 5 - (i * (colorElements / 6)));
	}
	std::snprintf(buffer, sizeof(buffer), "%d", (int)std::round(maxT));
	display_print(buffer, 267, height - 5 - (3 * (colorElements / 6)));
}
