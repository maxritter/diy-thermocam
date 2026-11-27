/*
 *
 * V2_CORE - Hardware-independent thermal math, filters, VoSPI CRC & formatting
 *
 * DIY-Thermocam Firmware (V1 / V2 - Teensy 3.2 / 3.6)
 *
 * GNU General Public License v3.0
 *
 * Copyright by Max Ritter
 *
 */

#ifndef V2_CORE_H
#define V2_CORE_H

#include <stddef.h>
#include <stdint.h>

namespace v2_core {

static constexpr float kCalStdSlope = 0.0300f;
static constexpr float kCalCompMinC = -80.0f;
static constexpr float kCalCompMaxC = 80.0f;
static constexpr float kCalSlopeMin = 0.001f;
static constexpr float kCalSlopeMax = 1.0f;

float celsius_to_fahrenheit(float tc);
float fahrenheit_to_celsius(float tf);

bool cal_slope_valid(float slope);
bool cal_comp_valid(float comp);
float clamp_cal_comp(float comp);

// Computes offset in Celsius and converts raw 16-bit Lepton value to Celsius.
float cal_offset_celsius(bool radiometric, float ambC, float slope, float comp);
float raw_to_celsius(uint16_t rawValue, bool radiometric, float ambC, float slope, float comp);
uint16_t celsius_to_raw(float tempC, bool radiometric, float ambC, float slope, float comp);

// Stable additive calibration compensation in Celsius (eliminates Fahrenheit oscillation).
// Returns the updated, clamped calComp in Celsius.
float compensate_calib_step(
	float spotC,
	uint16_t minRaw,
	uint16_t maxRaw,
	float ambC,
	float slope,
	float currentComp);

// Validate an EEPROM preset's raw limits & calComp before loading.
bool preset_valid(uint16_t minRaw, uint16_t maxRaw, float comp, bool radiometric);

// FLIR Lepton VoSPI CCITT CRC-16 verification (poly 0x1021, init 0x0000).
bool vospi_packet_crc_ok(const uint8_t packet[164]);

// True symmetric 3x3 integer filters using a 2-row rolling line buffer (zero float).
void gaussian_filter_160x120(uint16_t* buf);
void box_filter_160x120(uint16_t* buf);

// Precompute RGB565 LUT from 3-byte RGB888 colormap.
void build_rgb565_lut(const uint8_t* rgbMap, int16_t elements, uint16_t outLut[256]);

// Map a raw 16-bit Lepton pixel to [0, colorElements - 1] safely even when maxVal <= minVal.
uint8_t map_raw_to_lut_index(uint16_t raw, uint16_t minVal, uint16_t maxVal, int16_t colorElements);

// Format float with 2 decimal places safely (handles -0.50, NaN, Inf, bounds).
void format_float_2dp(char* out, size_t outSize, float val);

// Clamp an integer temperature display string to safe bounds for colorbar/labels.
void format_temp_int(char* out, size_t outSize, float val);

}  // namespace v2_core

#endif /* V2_CORE_H */
