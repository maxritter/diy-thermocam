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

volatile bool touch_capacitive = false;

// Perf globals
uint32_t g_display_spi_hz = 40000000u;
uint32_t g_lepton_spi_hz = 20000000u;
uint8_t g_lepton_spi_mode = 1;
bool g_vospi_crc_enabled = true;
bool g_display_dma_active = false;
PerfRuntime g_perf = {};

float perf_mcu_temp_c() { return 42.3f; }
void perf_record_frame_period(uint8_t, uint32_t) {}
void perf_save_video_snapshot() {}

// Hardware / Driver stubs
bool isUSBConnected() { return usbConnected; }
void setDisplayRotation() {}
void displayBuffer() { display_writeScreen(bigBuffer); }
bool checkDiagnostic(byte device) { return (diagnostic >> device) & 1; }
void setDiagnostic(byte device) { diagnostic &= ~(1 << device); }
void floatToBytes(uint8_t* farray, float val) { std::memcpy(farray, &val, 4); }
float bytesToFloat(uint8_t* farray) { float v = 0; std::memcpy(&v, farray, 4); return v; }
void readTempLimits() {}
bool screenOffCheck() { return false; }
bool checkScreenLight() { return true; }
void enableScreenLight() {}
void disableScreenLight() {}

TS_Point touch_getPoint() { return TS_Point(0, 0, 0); }
void touch_init() {}
void touch_setRotation(bool) {}
volatile bool touch_touched() { return false; }

void checkBattery(bool, bool) {}
void checkSerial() {}
bool lepton_ffc(bool, bool) { return false; }
void lepton_ffcMode(bool) {}
void lepton_setHighGain() {}
void lepton_setLowGain() {}
void lepton_startFrame() {}
void lepton_endFrame() {}
void setMassStorage() {}
void initScreenOffTimer() {}
bool formatCard() { return true; }
void loadFiles() {}
void clearTempPoints() { std::memset(tempPoints, 0, sizeof(tempPoints)); }
void tempPointFunction(bool) {}
void showTemperatures() {}
void createThermalImg(bool) {}
void convertColors(bool) {}
void saveRawData(bool, char*, uint32_t) {}
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
	colorMap = colorMap_ironblack;
	colorElements = 256;
	tc_core::build_rgb565_lut(colorMap, 256, colorLut);
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
