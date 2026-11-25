/*
 * TC_THERMAL_CORE - Pure C++17 thermal pipeline, upscaling, filters, VoSPI & .DAT helpers
 */

#include "tc_thermal_core.h"
#include "tc_proto_v2.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace tc_core {

float celsius_to_fahrenheit(float tc) {
	return (9.0f / 5.0f) * tc + 32.0f;
}

float fahrenheit_to_celsius(float tf) {
	return (tf - 32.0f) * (5.0f / 9.0f);
}

float raw_to_temp(uint16_t raw, float slope, bool fahrenheit) {
	float temp = (slope * static_cast<float>(raw)) - 273.15f;
	if (fahrenheit) {
		temp = celsius_to_fahrenheit(temp);
	}
	return temp;
}

uint16_t temp_to_raw(float temp, float slope, bool fahrenheit) {
	if (fahrenheit) {
		temp = fahrenheit_to_celsius(temp);
	}
	if (slope <= 0.0f) return 0;
	float r = (temp + 273.15f) / slope;
	if (r <= 0.0f) return 0;
	if (r >= 65535.0f) return 65535;
	return static_cast<uint16_t>(r);
}

void format_float_2dp(char* buf, size_t buf_size, float val) {
	if (!buf || buf_size == 0) return;
	// Round to nearest centi-unit first so -0.004 -> "0.00" and -0.45 -> "-0.45"
	long scaled = static_cast<long>(std::lround(static_cast<double>(val) * 100.0));
	bool neg = scaled < 0;
	unsigned long abs_scaled = neg ? static_cast<unsigned long>(-scaled)
	                               : static_cast<unsigned long>(scaled);
	unsigned long units = abs_scaled / 100ul;
	unsigned long frac = abs_scaled % 100ul;
	if (neg) {
		std::snprintf(buf, buf_size, "-%lu.%02lu", units, frac);
	} else {
		std::snprintf(buf, buf_size, "%lu.%02lu", units, frac);
	}
}

void build_rgb565_lut(const uint8_t* color_map, uint16_t color_elements, uint16_t lut[256]) {
	if (!color_map || color_elements == 0) {
		std::memset(lut, 0, 256 * sizeof(uint16_t));
		return;
	}
	uint16_t n = (color_elements > 256) ? 256 : color_elements;
	for (uint16_t i = 0; i < n; ++i) {
		lut[i] = pack_rgb565(color_map[i * 3], color_map[i * 3 + 1], color_map[i * 3 + 2]);
	}
	uint16_t last = lut[n - 1];
	for (uint16_t i = n; i < 256; ++i) {
		lut[i] = last;
	}
}

void scan_min_max(const uint16_t* buf, size_t count, MinMaxStats& out) {
	uint16_t mn = 65535;
	uint16_t mx = 0;
	uint16_t mn_pos = 0;
	uint16_t mx_pos = 0;
	for (size_t i = 0; i < count; ++i) {
		uint16_t v = buf[i];
		if (v < mn) {
			mn = v;
			mn_pos = static_cast<uint16_t>(i);
		}
		if (v > mx) {
			mx = v;
			mx_pos = static_cast<uint16_t>(i);
		}
	}
	out.min_val = mn;
	out.max_val = mx;
	out.min_pos = mn_pos;
	out.max_pos = mx_pos;
}

void convert_colors_u16(
	uint16_t* frame_buf,
	size_t count,
	uint16_t min_val,
	uint16_t max_val,
	const uint16_t lut[256],
	uint16_t color_elements,
	uint8_t hot_cold_mode,
	uint16_t hot_cold_raw_level,
	uint16_t hot_cold_rgb565)
{
	if (!frame_buf || count == 0) return;
	const uint32_t max_idx = (color_elements > 0) ? static_cast<uint32_t>(color_elements - 1) : 0u;
	// Divide-by-zero guard when min_val >= max_val
	if (min_val >= max_val || max_idx == 0) {
		const uint16_t fallback = lut[0];
		for (size_t i = 0; i < count; ++i) {
			uint16_t v = frame_buf[i];
			if ((hot_cold_mode == 2 && v >= hot_cold_raw_level) ||
			    (hot_cold_mode == 1 && v <= hot_cold_raw_level)) {
				frame_buf[i] = hot_cold_rgb565;
			} else {
				frame_buf[i] = fallback;
			}
		}
		return;
	}

	const uint32_t span = static_cast<uint32_t>(max_val - min_val);
	// Exact Q32 fixed-point reciprocal: floor(((v - min) * max_idx) / span)
	const uint64_t recip = ((static_cast<uint64_t>(max_idx) << 32) + (span - 1)) / span;

	if (hot_cold_mode == 0) {
		for (size_t i = 0; i < count; ++i) {
			uint16_t v = frame_buf[i];
			if (v <= min_val) {
				frame_buf[i] = lut[0];
			} else if (v >= max_val) {
				frame_buf[i] = lut[max_idx];
			} else {
				uint32_t idx = static_cast<uint32_t>((static_cast<uint64_t>(v - min_val) * recip) >> 32);
				if (idx > max_idx) idx = max_idx;
				frame_buf[i] = lut[idx];
			}
		}
	} else if (hot_cold_mode == 2) {
		for (size_t i = 0; i < count; ++i) {
			uint16_t v = frame_buf[i];
			if (v >= hot_cold_raw_level) {
				frame_buf[i] = hot_cold_rgb565;
			} else if (v <= min_val) {
				frame_buf[i] = lut[0];
			} else if (v >= max_val) {
				frame_buf[i] = lut[max_idx];
			} else {
				uint32_t idx = static_cast<uint32_t>((static_cast<uint64_t>(v - min_val) * recip) >> 32);
				if (idx > max_idx) idx = max_idx;
				frame_buf[i] = lut[idx];
			}
		}
	} else {
		for (size_t i = 0; i < count; ++i) {
			uint16_t v = frame_buf[i];
			if (v <= hot_cold_raw_level) {
				frame_buf[i] = hot_cold_rgb565;
			} else if (v <= min_val) {
				frame_buf[i] = lut[0];
			} else if (v >= max_val) {
				frame_buf[i] = lut[max_idx];
			} else {
				uint32_t idx = static_cast<uint32_t>((static_cast<uint64_t>(v - min_val) * recip) >> 32);
				if (idx > max_idx) idx = max_idx;
				frame_buf[i] = lut[idx];
			}
		}
	}
}

void filter_gaussian_3x3(uint16_t* buf, int w, int h) {
	if (!buf || w < 3 || h < 3 || w > SMALL_W) return;
	uint16_t prev_row[SMALL_W];
	uint16_t cur_row[SMALL_W];
	std::memcpy(prev_row, buf, static_cast<size_t>(w) * sizeof(uint16_t));
	for (int y = 1; y < h - 1; ++y) {
		uint16_t* row = buf + y * w;
		const uint16_t* next = buf + (y + 1) * w;
		std::memcpy(cur_row, row, static_cast<size_t>(w) * sizeof(uint16_t));
		for (int x = 1; x < w - 1; ++x) {
			uint32_t sum =
				static_cast<uint32_t>(prev_row[x - 1]) +
				(static_cast<uint32_t>(prev_row[x]) << 1) +
				static_cast<uint32_t>(prev_row[x + 1]) +
				(static_cast<uint32_t>(cur_row[x - 1]) << 1) +
				(static_cast<uint32_t>(cur_row[x]) << 2) +
				(static_cast<uint32_t>(cur_row[x + 1]) << 1) +
				static_cast<uint32_t>(next[x - 1]) +
				(static_cast<uint32_t>(next[x]) << 1) +
				static_cast<uint32_t>(next[x + 1]);
			row[x] = static_cast<uint16_t>((sum + 8u) >> 4);
		}
		std::memcpy(prev_row, cur_row, static_cast<size_t>(w) * sizeof(uint16_t));
	}
}

void filter_box_3x3(uint16_t* buf, int w, int h) {
	if (!buf || w < 3 || h < 3 || w > SMALL_W) return;
	uint16_t prev_row[SMALL_W];
	uint16_t cur_row[SMALL_W];
	std::memcpy(prev_row, buf, static_cast<size_t>(w) * sizeof(uint16_t));
	for (int y = 1; y < h - 1; ++y) {
		uint16_t* row = buf + y * w;
		const uint16_t* next = buf + (y + 1) * w;
		std::memcpy(cur_row, row, static_cast<size_t>(w) * sizeof(uint16_t));
		for (int x = 1; x < w - 1; ++x) {
			uint32_t sum =
				static_cast<uint32_t>(prev_row[x - 1]) + prev_row[x] + prev_row[x + 1] +
				cur_row[x - 1] + cur_row[x] + cur_row[x + 1] +
				next[x - 1] + next[x] + next[x + 1];
			// Exact rounded integer divide by 9 via (sum + 4) / 9
			row[x] = static_cast<uint16_t>((sum + 4u) / 9u);
		}
		std::memcpy(prev_row, cur_row, static_cast<size_t>(w) * sizeof(uint16_t));
	}
}

void upscale_bilinear_2x(const uint16_t* src, uint16_t* dst) {
	// Center-aligned separable 2x bilinear (weights 3/4 + 1/4, zero float, zero half-pixel shift)
	uint16_t row_buf[2][BIG_W];
	auto expand_row = [&](int sy, uint16_t* out) {
		const uint16_t* s = src + sy * SMALL_W;
		out[0] = s[0];
		for (int x = 0; x < SMALL_W - 1; ++x) {
			uint32_t a = s[x];
			uint32_t b = s[x + 1];
			out[x * 2 + 1] = static_cast<uint16_t>((3u * a + b + 2u) >> 2);
			out[x * 2 + 2] = static_cast<uint16_t>((a + 3u * b + 2u) >> 2);
		}
		out[BIG_W - 1] = s[SMALL_W - 1];
	};

	expand_row(0, row_buf[0]);
	std::memcpy(dst, row_buf[0], BIG_W * sizeof(uint16_t));
	for (int y = 0; y < SMALL_H - 1; ++y) {
		const uint16_t* r0 = row_buf[y & 1];
		uint16_t* r1 = row_buf[(y + 1) & 1];
		expand_row(y + 1, r1);
		uint16_t* d0 = dst + (y * 2 + 1) * BIG_W;
		uint16_t* d1 = dst + (y * 2 + 2) * BIG_W;
		for (int x = 0; x < BIG_W; ++x) {
			uint32_t a = r0[x];
			uint32_t b = r1[x];
			d0[x] = static_cast<uint16_t>((3u * a + b + 2u) >> 2);
			d1[x] = static_cast<uint16_t>((a + 3u * b + 2u) >> 2);
		}
	}
	std::memcpy(dst + (BIG_H - 1) * BIG_W, row_buf[(SMALL_H - 1) & 1], BIG_W * sizeof(uint16_t));
}

void upscale_sharp_2x(const uint16_t* src, uint16_t* dst) {
	// Clamped Catmull-Rom 2x (ringing-free, crisp thermal edges, pure integer)
	// For center-aligned 2x at t = 0.25 and t = 0.75:
	// Catmull-Rom(t=0.25): [-9, 111, 29, -3] / 128; (t=0.75): [-3, 29, 111, -9] / 128
	auto cr_tap = [](int p0, int p1, int p2, int p3, bool right) -> uint16_t {
		int v = right ? (-3 * p0 + 29 * p1 + 111 * p2 - 9 * p3 + 64) >> 7
		              : (-9 * p0 + 111 * p1 + 29 * p2 - 3 * p3 + 64) >> 7;
		int lo = (p1 < p2) ? p1 : p2;
		int hi = (p1 > p2) ? p1 : p2;
		if (v < lo) v = lo;
		if (v > hi) v = hi;
		return static_cast<uint16_t>(v);
	};

	uint16_t rows[4][BIG_W];
	auto expand_row = [&](int sy, uint16_t* out) {
		if (sy < 0) sy = 0;
		if (sy >= SMALL_H) sy = SMALL_H - 1;
		const uint16_t* s = src + sy * SMALL_W;
		out[0] = s[0];
		for (int x = 0; x < SMALL_W - 1; ++x) {
			int p0 = s[(x > 0) ? (x - 1) : 0];
			int p1 = s[x];
			int p2 = s[x + 1];
			int p3 = s[(x + 2 < SMALL_W) ? (x + 2) : (SMALL_W - 1)];
			out[x * 2 + 1] = cr_tap(p0, p1, p2, p3, false);
			out[x * 2 + 2] = cr_tap(p0, p1, p2, p3, true);
		}
		out[BIG_W - 1] = s[SMALL_W - 1];
	};

	expand_row(0, rows[0]);
	expand_row(0, rows[1]);
	expand_row(1, rows[2]);
	std::memcpy(dst, rows[1], BIG_W * sizeof(uint16_t));

	for (int y = 0; y < SMALL_H - 1; ++y) {
		expand_row(y + 2, rows[(y + 3) & 3]);
		const uint16_t* r0 = rows[y & 3];
		const uint16_t* r1 = rows[(y + 1) & 3];
		const uint16_t* r2 = rows[(y + 2) & 3];
		const uint16_t* r3 = rows[(y + 3) & 3];
		uint16_t* d0 = dst + (y * 2 + 1) * BIG_W;
		uint16_t* d1 = dst + (y * 2 + 2) * BIG_W;
		for (int x = 0; x < BIG_W; ++x) {
			d0[x] = cr_tap(r0[x], r1[x], r2[x], r3[x], false);
			d1[x] = cr_tap(r0[x], r1[x], r2[x], r3[x], true);
		}
	}
	std::memcpy(dst + (BIG_H - 1) * BIG_W, rows[SMALL_H & 3], BIG_W * sizeof(uint16_t));
}

void lepton_map_packet(
	const uint8_t packet[164],
	uint8_t line,
	uint8_t segment,
	bool is_lepton3,
	bool rot_vert,
	bool rot_horiz,
	uint16_t* small_buf)
{
	if (!packet || !small_buf || line >= 60) return;

	if (!is_lepton3) {
		const int base_y = static_cast<int>(line) * 2 * SMALL_W;
		for (int col = 0; col < 80; ++col) {
			const int src_col = rot_horiz ? (79 - col) : col;
			const uint16_t val = static_cast<uint16_t>(
				(static_cast<uint16_t>(packet[2 * src_col + 4]) << 8) | packet[2 * src_col + 5]);
			const int idx = base_y + col * 2;
			if (!rot_vert) {
				small_buf[idx] = val;
				small_buf[idx + 1] = val;
				small_buf[idx + SMALL_W] = val;
				small_buf[idx + SMALL_W + 1] = val;
			} else {
				small_buf[19199 - idx] = val;
				small_buf[19199 - (idx + 1)] = val;
				small_buf[19199 - (idx + SMALL_W)] = val;
				small_buf[19199 - (idx + SMALL_W + 1)] = val;
			}
		}
		return;
	}

	if (segment < 1 || segment > 4) return;
	const int seg_idx = static_cast<int>(segment) - 1;
	const int line_half = static_cast<int>(line) / 2;
	const int line_odd = static_cast<int>(line) & 1;

	if (!rot_vert) {
		const int base = seg_idx * 4800 + line_half * SMALL_W + line_odd * 80;
		for (int col = 0; col < 80; ++col) {
			const int src_col = rot_horiz ? (79 - col) : col;
			small_buf[base + col] = static_cast<uint16_t>(
				(static_cast<uint16_t>(packet[2 * src_col + 4]) << 8) | packet[2 * src_col + 5]);
		}
	} else {
		const int seg_top = 19199 - seg_idx * 4800;
		const int odd_term = rot_horiz ? ((1 - line_odd) * 80) : (line_odd * 80);
		const int base = seg_top - (line_half * SMALL_W + odd_term);
		for (int col = 0; col < 80; ++col) {
			const int src_col = rot_horiz ? (79 - col) : col;
			small_buf[base - col] = static_cast<uint16_t>(
				(static_cast<uint16_t>(packet[2 * src_col + 4]) << 8) | packet[2 * src_col + 5]);
		}
	}
}

// Precomputed 256-entry CRC-16/CCITT-XMODEM lookup table (poly 0x1021, init 0x0000)
static constexpr uint16_t kCrc16CcittLut[256] = {
	0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
	0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
	0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
	0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
	0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
	0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
	0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
	0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
	0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,
	0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
	0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
	0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
	0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
	0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
	0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
	0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
	0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
	0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
	0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
	0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
	0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,
	0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
	0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
	0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
	0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
	0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
	0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
	0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
	0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
	0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
	0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
	0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0
};

uint16_t vospi_crc16(const uint8_t packet[164]) {
	// FLIR Lepton VoSPI CRC-16/CCITT (XMODEM): poly 0x1021, init 0x0000,
	// top 4 bits of byte 0 masked to 0, bytes 2..3 (CRC field) treated as 0x00.
	uint16_t crc = kCrc16CcittLut[packet[0] & 0x0Fu];
	crc = static_cast<uint16_t>((crc << 8) ^ kCrc16CcittLut[((crc >> 8) ^ packet[1]) & 0xFFu]);
	crc = static_cast<uint16_t>((crc << 8) ^ kCrc16CcittLut[(crc >> 8) & 0xFFu]); // byte 2 = 0
	crc = static_cast<uint16_t>((crc << 8) ^ kCrc16CcittLut[(crc >> 8) & 0xFFu]); // byte 3 = 0
	for (int i = 4; i < 164; ++i) {
		crc = static_cast<uint16_t>((crc << 8) ^ kCrc16CcittLut[((crc >> 8) ^ packet[i]) & 0xFFu]);
	}
	return crc;
}

bool vospi_packet_crc_ok(const uint8_t packet[164]) {
	uint16_t wire_crc = static_cast<uint16_t>((static_cast<uint16_t>(packet[2]) << 8) | packet[3]);
	return vospi_crc16(packet) == wire_crc;
}

size_t dat_encode(
	uint8_t* out,
	size_t out_cap,
	const uint16_t* small_buf_160x120,
	const DatImageMeta& meta)
{
	const size_t pixel_bytes = meta.is_lepton3 ? 38400u : 9600u;
	const size_t total = pixel_bytes + 21u + (meta.has_temp_points ? 384u : 0u);
	if (!out || !small_buf_160x120 || out_cap < total) return 0;

	size_t pos = 0;
	if (!meta.is_lepton3) {
		for (int line = 0; line < 60; ++line) {
			for (int col = 0; col < 80; ++col) {
				uint16_t v = small_buf_160x120[(line * 2 * SMALL_W) + (col * 2)];
				out[pos++] = static_cast<uint8_t>(v >> 8);
				out[pos++] = static_cast<uint8_t>(v & 0xFFu);
			}
		}
	} else {
		for (int i = 0; i < SMALL_PIXELS; ++i) {
			uint16_t v = small_buf_160x120[i];
			out[pos++] = static_cast<uint8_t>(v >> 8);
			out[pos++] = static_cast<uint8_t>(v & 0xFFu);
		}
	}

	out[pos++] = static_cast<uint8_t>(meta.min_val >> 8);
	out[pos++] = static_cast<uint8_t>(meta.min_val & 0xFFu);
	out[pos++] = static_cast<uint8_t>(meta.max_val >> 8);
	out[pos++] = static_cast<uint8_t>(meta.max_val & 0xFFu);

	tc_v2::write_f32_le(out + pos, meta.spot_temp); pos += 4;
	out[pos++] = meta.color_scheme;
	out[pos++] = meta.temp_format;
	out[pos++] = meta.spot_enabled;
	out[pos++] = meta.colorbar_enabled;
	out[pos++] = meta.min_max_points;
	tc_v2::write_f32_le(out + pos, meta.cal_offset); pos += 4;
	tc_v2::write_f32_le(out + pos, meta.cal_slope); pos += 4;

	if (meta.has_temp_points) {
		for (int i = 0; i < 96; ++i) {
			out[pos++] = static_cast<uint8_t>(meta.temp_points[i][0] >> 8);
			out[pos++] = static_cast<uint8_t>(meta.temp_points[i][0] & 0xFFu);
			out[pos++] = static_cast<uint8_t>(meta.temp_points[i][1] >> 8);
			out[pos++] = static_cast<uint8_t>(meta.temp_points[i][1] & 0xFFu);
		}
	}
	return pos;
}

bool dat_decode(
	const uint8_t* data,
	size_t len,
	uint16_t* small_buf_160x120,
	DatImageMeta& meta)
{
	if (!data || !small_buf_160x120) return false;
	const bool is_l2 = (len == DAT_SIZE_LEPTON2_SMALL || len == DAT_SIZE_LEPTON2_BIG);
	const bool is_l3 = (len == DAT_SIZE_LEPTON3_SMALL || len == DAT_SIZE_LEPTON3_BIG);
	if (!is_l2 && !is_l3) return false;

	meta = DatImageMeta{};
	meta.is_lepton3 = is_l3;
	meta.has_temp_points = (len == DAT_SIZE_LEPTON2_BIG || len == DAT_SIZE_LEPTON3_BIG);

	size_t pos = 0;
	if (is_l2) {
		for (int line = 0; line < 60; ++line) {
			for (int col = 0; col < 80; ++col) {
				uint16_t v = static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
				pos += 2;
				int idx = (line * 2 * SMALL_W) + (col * 2);
				small_buf_160x120[idx] = v;
				small_buf_160x120[idx + 1] = v;
				small_buf_160x120[idx + SMALL_W] = v;
				small_buf_160x120[idx + SMALL_W + 1] = v;
			}
		}
	} else {
		for (int i = 0; i < SMALL_PIXELS; ++i) {
			small_buf_160x120[i] = static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
			pos += 2;
		}
	}

	meta.min_val = static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]); pos += 2;
	meta.max_val = static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]); pos += 2;
	meta.spot_temp = tc_v2::read_f32_le(data + pos); pos += 4;
	meta.color_scheme = data[pos++];
	meta.temp_format = data[pos++];
	meta.spot_enabled = data[pos++];
	meta.colorbar_enabled = data[pos++];
	meta.min_max_points = data[pos++];
	meta.cal_offset = tc_v2::read_f32_le(data + pos); pos += 4;
	meta.cal_slope = tc_v2::read_f32_le(data + pos); pos += 4;

	if (meta.has_temp_points) {
		uint16_t arr[192];
		for (int i = 0; i < 192; ++i) {
			uint16_t v = static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
			pos += 2;
			arr[i] = (v == 65535u) ? 0u : v;
		}
		bool old_fmt = false;
		for (int i = 1; i < 191; ++i) {
			if (arr[i] != 0 && arr[i - 1] == 0 && arr[i + 1] == 0) {
				old_fmt = true;
				break;
			}
		}
		if (old_fmt) {
			int p = 0;
			for (int i = 0; i < 192 && p < 96; ++i) {
				if (arr[i] != 0) {
					uint16_t x = static_cast<uint16_t>((i % 16) * 10);
					uint16_t y = static_cast<uint16_t>((i / 16) * 10);
					meta.temp_points[p][0] = static_cast<uint16_t>(x + y * 160 + 1);
					meta.temp_points[p][1] = arr[i];
					++p;
				}
			}
		} else {
			for (int i = 0; i < 96; ++i) {
				meta.temp_points[i][0] = arr[i * 2];
				meta.temp_points[i][1] = arr[i * 2 + 1];
			}
		}
	}
	return true;
}

} // namespace tc_core
