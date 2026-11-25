/*
 * TC_THERMAL_CORE - Pure C++17 thermal pipeline, upscaling, filters, VoSPI & .DAT helpers
 * Zero <Arduino.h> dependencies; compiles identically on Teensy 4.1 and host CI.
 */

#ifndef TC_THERMAL_CORE_H
#define TC_THERMAL_CORE_H

#include <cstddef>
#include <cstdint>

namespace tc_core {

static constexpr int SMALL_W = 160;
static constexpr int SMALL_H = 120;
static constexpr int SMALL_PIXELS = SMALL_W * SMALL_H; // 19200
static constexpr int BIG_W = 320;
static constexpr int BIG_H = 240;
static constexpr int BIG_PIXELS = BIG_W * BIG_H; // 76800

// Canonical .DAT file sizes (V1/V2/V3 compatible)
static constexpr size_t DAT_SIZE_LEPTON2_SMALL = 9621;
static constexpr size_t DAT_SIZE_LEPTON2_BIG   = 10005;
static constexpr size_t DAT_SIZE_LEPTON3_SMALL = 38421;
static constexpr size_t DAT_SIZE_LEPTON3_BIG   = 38805;

// Temperature conversions
float celsius_to_fahrenheit(float tc);
float fahrenheit_to_celsius(float tf);
float raw_to_temp(uint16_t raw, float slope, bool fahrenheit);
uint16_t temp_to_raw(float temp, float slope, bool fahrenheit);

// Fixed float formatting (fixes -1 < x < 0 dropped minus sign bug)
void format_float_2dp(char* buf, size_t buf_size, float val);

// RGB565 packing & 256-entry LUT
inline uint16_t pack_rgb565(uint8_t r, uint8_t g, uint8_t b) {
	return static_cast<uint16_t>((((r & 248u) | (g >> 5)) << 8) | (((g & 28u) << 3) | (b >> 3)));
}

void build_rgb565_lut(const uint8_t* color_map, uint16_t color_elements, uint16_t lut[256]);

// Fused single-pass min/max scan
struct MinMaxStats {
	uint16_t min_val = 65535;
	uint16_t max_val = 0;
	uint16_t min_pos = 0;
	uint16_t max_pos = 0;
};
void scan_min_max(const uint16_t* buf, size_t count, MinMaxStats& out);

// Fast integer colorization with min==max divide-by-zero guard + hot/cold isotherm
void convert_colors_u16(
	uint16_t* frame_buf,
	size_t count,
	uint16_t min_val,
	uint16_t max_val,
	const uint16_t lut[256],
	uint16_t color_elements,
	uint8_t hot_cold_mode,
	uint16_t hot_cold_raw_level,
	uint16_t hot_cold_rgb565);

// Fast integer 3x3 FIR filters (zero float, scratch-row safe)
void filter_gaussian_3x3(uint16_t* buf, int w, int h);
void filter_box_3x3(uint16_t* buf, int w, int h);

// Fast integer 160x120 -> 320x240 upscalers (no float)
void upscale_bilinear_2x(const uint16_t* src_160x120, uint16_t* dst_320x240);
void upscale_sharp_2x(const uint16_t* src_160x120, uint16_t* dst_320x240);

// Lepton VoSPI packet index mapping (all 8 combos: 2.5/3.5 x rotVert x rotHoriz)
void lepton_map_packet(
	const uint8_t packet[164],
	uint8_t line,
	uint8_t segment,
	bool is_lepton3,
	bool rot_vert,
	bool rot_horiz,
	uint16_t* small_buf);

// Lepton VoSPI CRC-16/CCITT (poly 0x1021, init 0x0000; masks top 4 bits of byte 0 and zeros bytes 2..3)
uint16_t vospi_crc16(const uint8_t packet[164]);
bool vospi_packet_crc_ok(const uint8_t packet[164]);

// Radiometric .DAT serialization/deserialization (100% V1/V2/V3 compatible)
struct DatImageMeta {
	bool is_lepton3 = true;
	uint16_t min_val = 0;
	uint16_t max_val = 0;
	float spot_temp = 0.0f;
	uint8_t color_scheme = 13;
	uint8_t temp_format = 0;
	uint8_t spot_enabled = 0;
	uint8_t colorbar_enabled = 1;
	uint8_t min_max_points = 0;
	float cal_offset = -273.15f;
	float cal_slope = 0.01f;
	bool has_temp_points = true;
	uint16_t temp_points[96][2] = {};
};

size_t dat_encode(
	uint8_t* out,
	size_t out_cap,
	const uint16_t* small_buf_160x120,
	const DatImageMeta& meta);

bool dat_decode(
	const uint8_t* data,
	size_t len,
	uint16_t* small_buf_160x120,
	DatImageMeta& meta);

} // namespace tc_core

#endif /* TC_THERMAL_CORE_H */
