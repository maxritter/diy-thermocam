#include "tc_thermal_core.h"
#include "tc_proto_v2.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

using namespace tc_core;

void run_proto_v2_tests();

static void test_temp_and_float_format() {
	assert(std::fabs(celsius_to_fahrenheit(0.0f) - 32.0f) < 1e-4f);
	assert(std::fabs(celsius_to_fahrenheit(100.0f) - 212.0f) < 1e-4f);
	assert(std::fabs(fahrenheit_to_celsius(32.0f) - 0.0f) < 1e-4f);
	assert(std::fabs(raw_to_temp(29515, 0.01f, false) - 22.0f) < 1e-3f);
	assert(temp_to_raw(22.0f, 0.01f, false) == 29515);
	assert(temp_to_raw(-300.0f, 0.01f, false) == 0);
	assert(temp_to_raw(1000.0f, 0.01f, false) == 65535);

	char buf[16];
	format_float_2dp(buf, sizeof(buf), -0.45f);
	assert(std::strcmp(buf, "-0.45") == 0);
	format_float_2dp(buf, sizeof(buf), -0.05f);
	assert(std::strcmp(buf, "-0.05") == 0);
	format_float_2dp(buf, sizeof(buf), -0.004f);
	assert(std::strcmp(buf, "0.00") == 0);
	format_float_2dp(buf, sizeof(buf), -12.34f);
	assert(std::strcmp(buf, "-12.34") == 0);
	format_float_2dp(buf, sizeof(buf), 25.05f);
	assert(std::strcmp(buf, "25.05") == 0);
	format_float_2dp(buf, sizeof(buf), -0.999f);
	assert(std::strcmp(buf, "-1.00") == 0);
}

static void test_lut_and_convert_colors() {
	uint8_t cmap[256 * 3];
	for (int i = 0; i < 256; ++i) {
		cmap[i * 3 + 0] = static_cast<uint8_t>(i);
		cmap[i * 3 + 1] = static_cast<uint8_t>(255 - i);
		cmap[i * 3 + 2] = static_cast<uint8_t>(i / 2);
	}
	uint16_t lut[256];
	build_rgb565_lut(cmap, 256, lut);
	assert(lut[0] == pack_rgb565(0, 255, 0));
	assert(lut[255] == pack_rgb565(255, 0, 127));

	// Guard min == max (must not divide by zero)
	uint16_t flat[8] = {30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000};
	convert_colors_u16(flat, 8, 30000, 30000, lut, 256, 0, 0, 0xF800);
	for (int i = 0; i < 8; ++i) assert(flat[i] == lut[0]);

	// Normal linear ramp + hot highlight
	uint16_t ramp[5] = {29000, 29500, 30000, 30500, 31000};
	convert_colors_u16(ramp, 5, 29000, 31000, lut, 256, 2, 30800, 0xF800);
	assert(ramp[0] == lut[0]);
	assert(ramp[2] == lut[127] || ramp[2] == lut[128]);
	assert(ramp[4] == 0xF800);
}

static void test_filters_and_upscalers() {
	std::vector<uint16_t> small(SMALL_PIXELS, 30000);
	small[60 * SMALL_W + 80] = 31000;
	MinMaxStats st{};
	scan_min_max(small.data(), small.size(), st);
	assert(st.min_val == 30000 && st.max_val == 31000);
	assert(st.max_pos == 60 * SMALL_W + 80);

	std::vector<uint16_t> g = small;
	filter_gaussian_3x3(g.data(), SMALL_W, SMALL_H);
	assert(g[60 * SMALL_W + 80] == 30250); // (12*30000 + 4*31000)/16 = 30250
	assert(g[60 * SMALL_W + 81] == 30125); // (14*30000 + 2*31000)/16 = 30125

	std::vector<uint16_t> b = small;
	filter_box_3x3(b.data(), SMALL_W, SMALL_H);
	assert(b[60 * SMALL_W + 80] == 30111); // (8*30000 + 31000 + 4)/9 = 30111

	// Upscalers preserve constant fields and stay bounded on step edges
	std::vector<uint16_t> step(SMALL_PIXELS, 28000);
	for (int y = 0; y < SMALL_H; ++y)
		for (int x = 80; x < SMALL_W; ++x)
			step[y * SMALL_W + x] = 32000;

	std::vector<uint16_t> big_lin(BIG_PIXELS, 0), big_sharp(BIG_PIXELS, 0);
	upscale_bilinear_2x(step.data(), big_lin.data());
	upscale_sharp_2x(step.data(), big_sharp.data());
	for (int i = 0; i < BIG_PIXELS; ++i) {
		assert(big_lin[i] >= 28000 && big_lin[i] <= 32000);
		assert(big_sharp[i] >= 28000 && big_sharp[i] <= 32000);
	}
	assert(big_lin[0] == 28000 && big_lin[BIG_PIXELS - 1] == 32000);
	assert(big_sharp[0] == 28000 && big_sharp[BIG_PIXELS - 1] == 32000);
}

static void test_vospi_mapping_and_crc() {
	uint8_t pkt[164] = {};
	pkt[0] = 0x10; // seg 1 in top nibble
	pkt[1] = 20;   // line 20
	for (int c = 0; c < 80; ++c) {
		uint16_t v = static_cast<uint16_t>(1000 + c);
		pkt[4 + 2 * c] = static_cast<uint8_t>(v >> 8);
		pkt[5 + 2 * c] = static_cast<uint8_t>(v & 0xFF);
	}
	uint16_t crc = vospi_crc16(pkt);
	pkt[2] = static_cast<uint8_t>(crc >> 8);
	pkt[3] = static_cast<uint8_t>(crc & 0xFF);
	assert(vospi_packet_crc_ok(pkt));
	pkt[0] |= 0xA0; // top 4 bits masked out by VoSPI CRC spec
	assert(vospi_packet_crc_ok(pkt));
	pkt[50] ^= 0x01;
	assert(!vospi_packet_crc_ok(pkt));
	pkt[50] ^= 0x01;

	// Verify all 8 mapping combos write in-bounds and place pixels at expected corners
	for (int is_l3 = 0; is_l3 <= 1; ++is_l3) {
		for (int rv = 0; rv <= 1; ++rv) {
			for (int rh = 0; rh <= 1; ++rh) {
				std::vector<uint16_t> buf(SMALL_PIXELS, 0);
				lepton_map_packet(pkt, 0, 1, is_l3 != 0, rv != 0, rh != 0, buf.data());
				if (is_l3 && !rv && !rh) {
					assert(buf[0] == 1000 && buf[79] == 1079);
				} else if (is_l3 && !rv && rh) {
					assert(buf[0] == 1079 && buf[79] == 1000);
				} else if (!is_l3 && !rv && !rh) {
					assert(buf[0] == 1000 && buf[1] == 1000 && buf[160] == 1000);
				}
			}
		}
	}
}

static void test_dat_roundtrip() {
	std::vector<uint16_t> orig(SMALL_PIXELS);
	for (int i = 0; i < SMALL_PIXELS; ++i) orig[i] = static_cast<uint16_t>(28000 + (i % 4000));

	DatImageMeta m{};
	m.is_lepton3 = true;
	m.min_val = 28000;
	m.max_val = 31999;
	m.spot_temp = 26.75f;
	m.color_scheme = 13;
	m.temp_format = 0;
	m.spot_enabled = 1;
	m.colorbar_enabled = 1;
	m.min_max_points = 3;
	m.cal_offset = -273.15f;
	m.cal_slope = 0.01f;
	m.has_temp_points = true;
	m.temp_points[0][0] = 1234;
	m.temp_points[0][1] = 29999;

	std::vector<uint8_t> raw(DAT_SIZE_LEPTON3_BIG);
	size_t n = dat_encode(raw.data(), raw.size(), orig.data(), m);
	assert(n == DAT_SIZE_LEPTON3_BIG);

	std::vector<uint16_t> dec(SMALL_PIXELS, 0);
	DatImageMeta dm{};
	assert(dat_decode(raw.data(), n, dec.data(), dm));
	assert(dec == orig);
	assert(dm.min_val == 28000 && dm.max_val == 31999);
	assert(std::fabs(dm.spot_temp - 26.75f) < 1e-4f);
	assert(dm.temp_points[0][0] == 1234 && dm.temp_points[0][1] == 29999);

	// Also verify Lepton 2.5 small (9621 B)
	m.is_lepton3 = false;
	m.has_temp_points = false;
	std::vector<uint8_t> raw2(DAT_SIZE_LEPTON2_SMALL);
	assert(dat_encode(raw2.data(), raw2.size(), orig.data(), m) == DAT_SIZE_LEPTON2_SMALL);
	assert(dat_decode(raw2.data(), raw2.size(), dec.data(), dm));
	assert(!dm.is_lepton3 && !dm.has_temp_points);
}

static void test_colormap_lengths() {
	std::ifstream cs_in("../src/general/colorschemes.cpp");
	assert(cs_in.good());
	std::string cs_src((std::istreambuf_iterator<char>(cs_in)), std::istreambuf_iterator<char>());
	std::ifstream th_in("../src/thermal/thermal.cpp");
	assert(th_in.good());
	std::string th_src((std::istreambuf_iterator<char>(th_in)), std::istreambuf_iterator<char>());

	std::regex case_re(R"(case colorScheme_(\w+):\s*colorMap = colorMap_(\w+);\s*colorElements = (\d+);)");
	int matched = 0;
	for (auto it = std::sregex_iterator(th_src.begin(), th_src.end(), case_re); it != std::sregex_iterator(); ++it) {
		std::string name = (*it)[2].str();
		int elems = std::stoi((*it)[3].str());
		assert(elems == 192 || elems == 224 || elems == 240 || elems == 256);
		size_t pos = cs_src.find("colorMap_" + name + "[]");
		assert(pos != std::string::npos);
		size_t open = cs_src.find('{', pos);
		size_t close = cs_src.find('}', open);
		assert(open != std::string::npos && close != std::string::npos);
		int commas = 0;
		for (size_t i = open; i < close; ++i) if (cs_src[i] == ',') ++commas;
		assert(commas + 1 >= elems * 3);
		++matched;
	}
	assert(matched == 19);
}

int main() {
	run_proto_v2_tests();
	test_temp_and_float_format();
	test_lut_and_convert_colors();
	test_filters_and_upscalers();
	test_vospi_mapping_and_crc();
	test_dat_roundtrip();
	test_colormap_lengths();
	std::printf("ALL HOST C++17 UNIT TESTS PASSED!\n");
	return 0;
}
