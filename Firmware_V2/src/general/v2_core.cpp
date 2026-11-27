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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <v2_core.h>

namespace v2_core {

static const uint16_t kCrc16CcittLut[256] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
	0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
	0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
	0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
	0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
	0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
	0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
	0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
	0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
	0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
	0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
	0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
	0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
	0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
	0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
	0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
	0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
	0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
	0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
	0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
	0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
	0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
	0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

float celsius_to_fahrenheit(float tc) {
	return (9.0f / 5.0f) * tc + 32.0f;
}

float fahrenheit_to_celsius(float tf) {
	return (tf - 32.0f) * (5.0f / 9.0f);
}

bool cal_slope_valid(float slope) {
	return isfinite(slope) && (slope >= kCalSlopeMin) && (slope <= kCalSlopeMax);
}

bool cal_comp_valid(float comp) {
	return isfinite(comp) && (comp >= kCalCompMinC) && (comp <= kCalCompMaxC);
}

float clamp_cal_comp(float comp) {
	if (!isfinite(comp)) return 0.0f;
	if (comp < kCalCompMinC) return kCalCompMinC;
	if (comp > kCalCompMaxC) return kCalCompMaxC;
	return comp;
}

float cal_offset_celsius(bool radiometric, float ambC, float slope, float comp) {
	if (radiometric) {
		return -273.15f;
	}
	float s = cal_slope_valid(slope) ? slope : kCalStdSlope;
	float a = (isfinite(ambC) && ambC >= -40.0f && ambC <= 125.0f) ? ambC : 25.0f;
	float c = clamp_cal_comp(comp);
	return a - (s * 8192.0f) + c;
}

float raw_to_celsius(uint16_t rawValue, bool radiometric, float ambC, float slope, float comp) {
	float s = cal_slope_valid(slope) ? slope : (radiometric ? 0.01f : kCalStdSlope);
	float offset = cal_offset_celsius(radiometric, ambC, s, comp);
	return (s * static_cast<float>(rawValue)) + offset;
}

uint16_t celsius_to_raw(float tempC, bool radiometric, float ambC, float slope, float comp) {
	float s = cal_slope_valid(slope) ? slope : (radiometric ? 0.01f : kCalStdSlope);
	float offset = cal_offset_celsius(radiometric, ambC, s, comp);
	float raw = (tempC - offset) / s;
	if (!isfinite(raw) || raw <= 0.0f) return 0u;
	if (raw >= 65535.0f) return 65535u;
	return static_cast<uint16_t>(lroundf(raw));
}

float compensate_calib_step(
	float spotC,
	uint16_t minRaw,
	uint16_t maxRaw,
	float ambC,
	float slope,
	float currentComp) {
	float comp = clamp_cal_comp(currentComp);
	if (!isfinite(spotC) || spotC < -70.0f || spotC > 380.0f) {
		return comp;
	}
	if (minRaw >= maxRaw) {
		return comp;
	}

	float minC = raw_to_celsius(minRaw, false, ambC, slope, comp);
	float maxC = raw_to_celsius(maxRaw, false, ambC, slope, comp);

	// Additive adjustment in Celsius so the thermal span brackets spotC without oscillating.
	if (spotC < (minC - 1.0f)) {
		comp += (spotC - minC);
	} else if (spotC > (maxC + 1.0f)) {
		comp += (spotC - maxC);
	}

	return clamp_cal_comp(comp);
}

bool preset_valid(uint16_t minRaw, uint16_t maxRaw, float comp, bool radiometric) {
	if (minRaw == 0u || maxRaw == 0u || minRaw >= maxRaw) {
		return false;
	}
	if (!radiometric && !cal_comp_valid(comp)) {
		return false;
	}
	return true;
}

bool vospi_packet_crc_ok(const uint8_t packet[164]) {
	uint16_t expected = (static_cast<uint16_t>(packet[2]) << 8) | static_cast<uint16_t>(packet[3]);
	uint16_t crc = 0x0000u;

	uint8_t b0 = packet[0] & 0x0Fu;
	crc = (crc << 8) ^ kCrc16CcittLut[((crc >> 8) ^ b0) & 0xFFu];
	crc = (crc << 8) ^ kCrc16CcittLut[((crc >> 8) ^ packet[1]) & 0xFFu];
	// Bytes 2 and 3 (the CRC field itself) are treated as zero per FLIR Lepton IDD.
	crc = (crc << 8) ^ kCrc16CcittLut[(crc >> 8) & 0xFFu];
	crc = (crc << 8) ^ kCrc16CcittLut[(crc >> 8) & 0xFFu];

	for (int i = 4; i < 164; i++) {
		crc = (crc << 8) ^ kCrc16CcittLut[((crc >> 8) ^ packet[i]) & 0xFFu];
	}
	return crc == expected;
}

void gaussian_filter_160x120(uint16_t* buf) {
	uint16_t prevRow[160];
	uint16_t curRow[160];
	memcpy(prevRow, &buf[0], sizeof(prevRow));
	memcpy(curRow, &buf[160], sizeof(curRow));

	for (int y = 1; y < 119; y++) {
		const uint16_t* nextRow = &buf[(y + 1) * 160];
		uint16_t* dstRow = &buf[y * 160];

		for (int x = 1; x < 159; x++) {
			uint32_t sum =
				static_cast<uint32_t>(prevRow[x - 1]) +
				(static_cast<uint32_t>(prevRow[x]) << 1) +
				static_cast<uint32_t>(prevRow[x + 1]) +
				(static_cast<uint32_t>(curRow[x - 1]) << 1) +
				(static_cast<uint32_t>(curRow[x]) << 2) +
				(static_cast<uint32_t>(curRow[x + 1]) << 1) +
				static_cast<uint32_t>(nextRow[x - 1]) +
				(static_cast<uint32_t>(nextRow[x]) << 1) +
				static_cast<uint32_t>(nextRow[x + 1]);
			dstRow[x] = static_cast<uint16_t>((sum + 8u) >> 4);
		}

		memcpy(prevRow, curRow, sizeof(prevRow));
		if (y < 118) {
			memcpy(curRow, nextRow, sizeof(curRow));
		}
	}
}

void box_filter_160x120(uint16_t* buf) {
	uint16_t prevRow[160];
	uint16_t curRow[160];
	memcpy(prevRow, &buf[0], sizeof(prevRow));
	memcpy(curRow, &buf[160], sizeof(curRow));

	for (int y = 1; y < 119; y++) {
		const uint16_t* nextRow = &buf[(y + 1) * 160];
		uint16_t* dstRow = &buf[y * 160];

		for (int x = 1; x < 159; x++) {
			uint32_t sum =
				static_cast<uint32_t>(prevRow[x - 1]) +
				static_cast<uint32_t>(prevRow[x]) +
				static_cast<uint32_t>(prevRow[x + 1]) +
				static_cast<uint32_t>(curRow[x - 1]) +
				static_cast<uint32_t>(curRow[x]) +
				static_cast<uint32_t>(curRow[x + 1]) +
				static_cast<uint32_t>(nextRow[x - 1]) +
				static_cast<uint32_t>(nextRow[x]) +
				static_cast<uint32_t>(nextRow[x + 1]);
			dstRow[x] = static_cast<uint16_t>((sum + 4u) / 9u);
		}

		memcpy(prevRow, curRow, sizeof(prevRow));
		if (y < 118) {
			memcpy(curRow, nextRow, sizeof(curRow));
		}
	}
}

void build_rgb565_lut(const uint8_t* rgbMap, int16_t elements, uint16_t outLut[256]) {
	if (!rgbMap || elements <= 0) {
		memset(outLut, 0, 256u * sizeof(uint16_t));
		return;
	}
	int16_t n = (elements > 256) ? 256 : elements;
	for (int16_t i = 0; i < n; i++) {
		uint8_t r = rgbMap[3 * i];
		uint8_t g = rgbMap[3 * i + 1];
		uint8_t b = rgbMap[3 * i + 2];
		outLut[i] = static_cast<uint16_t>((((r & 0xF8u) | (g >> 5)) << 8) | (((g & 0x1Cu) << 3) | (b >> 3)));
	}
	uint16_t last = outLut[n - 1];
	for (int16_t i = n; i < 256; i++) {
		outLut[i] = last;
	}
}

uint8_t map_raw_to_lut_index(uint16_t raw, uint16_t minVal, uint16_t maxVal, int16_t colorElements) {
	if (colorElements <= 1) return 0u;
	uint32_t maxIdx = static_cast<uint32_t>(colorElements - 1);
	if (raw <= minVal) return 0u;
	if (raw >= maxVal) return static_cast<uint8_t>(maxIdx);
	uint32_t span = static_cast<uint32_t>(maxVal - minVal);
	uint32_t idx = (static_cast<uint32_t>(raw - minVal) * maxIdx) / span;
	if (idx > maxIdx) idx = maxIdx;
	return static_cast<uint8_t>(idx);
}

void format_float_2dp(char* out, size_t outSize, float val) {
	if (!out || outSize == 0u) return;
	if (!isfinite(val)) {
		snprintf(out, outSize, " -- ");
		return;
	}
	if (val > 9999.99f) val = 9999.99f;
	if (val < -999.99f) val = -999.99f;

	int32_t scaled = static_cast<int32_t>(lroundf(val * 100.0f));
	bool neg = (scaled < 0);
	uint32_t mag = static_cast<uint32_t>(neg ? -scaled : scaled);
	uint32_t whole = mag / 100u;
	uint32_t frac = mag % 100u;
	if (neg) {
		snprintf(out, outSize, "-%lu.%02lu", static_cast<unsigned long>(whole), static_cast<unsigned long>(frac));
	} else {
		snprintf(out, outSize, "%lu.%02lu", static_cast<unsigned long>(whole), static_cast<unsigned long>(frac));
	}
}

void format_temp_int(char* out, size_t outSize, float val) {
	if (!out || outSize == 0u) return;
	if (!isfinite(val)) {
		snprintf(out, outSize, "---");
		return;
	}
	if (val > 9999.0f) val = 9999.0f;
	if (val < -999.0f) val = -999.0f;
	int32_t rounded = static_cast<int32_t>(lroundf(val));
	snprintf(out, outSize, "%ld", static_cast<long>(rounded));
}

}  // namespace v2_core
