/*
 * Host unit tests for DIY-Thermocam V1/V2 Firmware 2.4.9 (v2_core + bug regression suite)
 */

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <globaldefines.h>
#include <v2_core.h>

static int g_passed = 0;

#define RUN_TEST(fn) do { fn(); g_passed++; printf("  PASS: %s\n", #fn); } while (0)

static void test_lepton_version_constants_distinct() {
	assert(leptonVersion_2_0_shutter != leptonVersion_3_0_shutter);
	assert(leptonVersion_2_5_shutter != leptonVersion_3_5_shutter);
	assert(leptonVersion_2_5_shutter == 3);
	assert(leptonVersion_3_5_shutter == 4);
	assert(fwVersion == 250);
}

static void test_compensate_calib_stability_fahrenheit_and_celsius() {
	// Reproduce Ron Brown's scenario: non-radiometric Lepton in Fahrenheit mode,
	// room temp ~20 C (68 F), scene span 10..15 C (50..59 F).
	// In 2.4.8 this exploded exponentially by -1.8x per frame to +-28,743!
	const float ambC = 22.0f;
	const float slope = 0.03f;
	const uint16_t minRaw = 7800; // ~10.2 C with comp=0
	const uint16_t maxRaw = 7960; // ~15.0 C with comp=0
	const float spotF = 68.17f;
	const float spotC = v2_core::fahrenheit_to_celsius(spotF); // ~20.09 C

	float comp = 0.0f;
	for (int frame = 0; frame < 500; frame++) {
		comp = v2_core::compensate_calib_step(spotC, minRaw, maxRaw, ambC, slope, comp);
		assert(isfinite(comp));
		assert(comp >= v2_core::kCalCompMinC && comp <= v2_core::kCalCompMaxC);
	}
	// After converging, maxC must bracket spotC within 1 C (and stay stable, zero oscillation)
	float minC = v2_core::raw_to_celsius(minRaw, false, ambC, slope, comp);
	float maxC = v2_core::raw_to_celsius(maxRaw, false, ambC, slope, comp);
	assert(spotC >= minC - 1.05f && spotC <= maxC + 1.05f);
	// One more step must not change comp at all
	float compNext = v2_core::compensate_calib_step(spotC, minRaw, maxRaw, ambC, slope, comp);
	assert(fabsf(compNext - comp) < 1e-5f);

	// Also test when spot is colder than minRaw
	float coldComp = 0.0f;
	for (int frame = 0; frame < 200; frame++) {
		coldComp = v2_core::compensate_calib_step(-10.0f, minRaw, maxRaw, ambC, slope, coldComp);
	}
	float coldMinC = v2_core::raw_to_celsius(minRaw, false, ambC, slope, coldComp);
	assert(fabsf(coldMinC - (-10.0f)) < 1.05f);

	// Clamping on extreme/invalid spot readings
	assert(v2_core::compensate_calib_step(NAN, minRaw, maxRaw, ambC, slope, 5.0f) == 5.0f);
	assert(v2_core::compensate_calib_step(9999.0f, minRaw, maxRaw, ambC, slope, 5.0f) == 5.0f);
	// Invalid raw range (minRaw >= maxRaw, e.g. uninitialized 65535..0) must be a no-op
	assert(v2_core::compensate_calib_step(20.0f, 65535, 0, ambC, slope, 2.0f) == 2.0f);
	assert(v2_core::compensate_calib_step(20.0f, 8000, 8000, ambC, slope, 2.0f) == 2.0f);
}

static void test_preset_and_calibration_validation() {
	// Corrupt Preset 1 from Ron's screenshot: calComp ~ +28710 C
	assert(!v2_core::preset_valid(8100, 8200, 28710.0f, false));
	assert(!v2_core::preset_valid(8100, 8200, NAN, false));
	assert(!v2_core::preset_valid(8100, 8200, INFINITY, false));
	assert(!v2_core::preset_valid(8200, 8100, 0.0f, false));
	assert(!v2_core::preset_valid(0, 8200, 0.0f, false));
	assert(v2_core::preset_valid(8100, 8200, 5.5f, false));
	// Radiometric Lepton ignores calComp
	assert(v2_core::preset_valid(29000, 31000, NAN, true));

	assert(!v2_core::cal_slope_valid(0.0f));
	assert(!v2_core::cal_slope_valid(-0.03f));
	assert(!v2_core::cal_slope_valid(NAN));
	assert(!v2_core::cal_slope_valid(5.0f));
	assert(v2_core::cal_slope_valid(0.01f));
	assert(v2_core::cal_slope_valid(0.03f));
	assert(v2_core::cal_slope_valid(0.1f));
}

static void test_temp_to_raw_roundtrip_and_clamping() {
	// Radiometric high gain (0.01 K/LSB)
	uint16_t raw25 = v2_core::celsius_to_raw(25.0f, true, 0.0f, 0.01f, 0.0f);
	assert(raw25 == 29815u);
	assert(fabsf(v2_core::raw_to_celsius(raw25, true, 0.0f, 0.01f, 0.0f) - 25.0f) < 0.02f);

	// Radiometric low gain (0.1 K/LSB)
	uint16_t raw400 = v2_core::celsius_to_raw(400.0f, true, 0.0f, 0.1f, 0.0f);
	assert(raw400 == 6732u);
	assert(fabsf(v2_core::raw_to_celsius(raw400, true, 0.0f, 0.1f, 0.0f) - 400.0f) < 0.15f);

	// Clamping prevents uint16 underflow/overflow
	assert(v2_core::celsius_to_raw(-500.0f, true, 0.0f, 0.01f, 0.0f) == 0u);
	assert(v2_core::celsius_to_raw(5000.0f, true, 0.0f, 0.01f, 0.0f) == 65535u);
}

static uint16_t compute_ref_crc(const uint8_t pkt[164]) {
	uint16_t crc = 0x0000;
	for (int i = 0; i < 164; i++) {
		uint8_t b = pkt[i];
		if (i == 0) b &= 0x0Fu;
		else if (i == 2 || i == 3) b = 0x00u;
		crc ^= static_cast<uint16_t>(b) << 8;
		for (int bit = 0; bit < 8; bit++) {
			crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u) : static_cast<uint16_t>(crc << 1);
		}
	}
	return crc;
}

static void test_vospi_crc16() {
	uint8_t pkt[164];
	for (int i = 0; i < 164; i++) pkt[i] = static_cast<uint8_t>((i * 37 + 11) & 0xFF);
	pkt[0] = 0x30; // segment 3 in upper nibble (must be masked to 0 for CRC!)
	pkt[1] = 20;
	uint16_t crc = compute_ref_crc(pkt);
	pkt[2] = static_cast<uint8_t>(crc >> 8);
	pkt[3] = static_cast<uint8_t>(crc & 0xFF);
	assert(v2_core::vospi_packet_crc_ok(pkt));

	// Flip a single bit in pixel payload -> must reject!
	pkt[80] ^= 0x01;
	assert(!v2_core::vospi_packet_crc_ok(pkt));
}

static void test_integer_filters_symmetric_and_no_overflow() {
	static uint16_t buf[160 * 120];
	// Constant 65535 must remain 65535 without uint32 overflow
	for (int i = 0; i < 160 * 120; i++) buf[i] = 65535u;
	v2_core::gaussian_filter_160x120(buf);
	assert(buf[60 * 160 + 80] == 65535u);
	v2_core::box_filter_160x120(buf);
	assert(buf[60 * 160 + 80] == 65535u);

	// Impulse response of gaussian must be symmetric (not directional like old in-place filter!)
	memset(buf, 0, sizeof(buf));
	buf[60 * 160 + 80] = 16000u;
	v2_core::gaussian_filter_160x120(buf);
	assert(buf[60 * 160 + 80] == 4000u); // 4/16
	assert(buf[60 * 160 + 79] == 2000u); // 2/16
	assert(buf[60 * 160 + 81] == 2000u); // 2/16 (symmetric!)
	assert(buf[59 * 160 + 80] == 2000u);
	assert(buf[61 * 160 + 80] == 2000u);
	assert(buf[59 * 160 + 79] == 1000u); // 1/16
	assert(buf[61 * 160 + 81] == 1000u); // 1/16 (symmetric!)
}

static void test_lut_mapping_and_divide_by_zero_safety() {
	// Zero-span (min == max) and inverted span (min > max) must not crash or exceed bounds
	assert(v2_core::map_raw_to_lut_index(8000, 8000, 8000, 256) == 0u);
	assert(v2_core::map_raw_to_lut_index(8001, 8000, 8000, 256) == 255u);
	assert(v2_core::map_raw_to_lut_index(8000, 9000, 8000, 224) == 0u);
	assert(v2_core::map_raw_to_lut_index(8500, 8000, 9000, 256) == 127u);
	assert(v2_core::map_raw_to_lut_index(9000, 8000, 9000, 224) == 223u);

	uint8_t rgb[6] = { 255, 0, 0, 0, 255, 0 };
	uint16_t lut[256];
	v2_core::build_rgb565_lut(rgb, 2, lut);
	assert(lut[0] == 0xF800u);
	assert(lut[1] == 0x07E0u);
	assert(lut[255] == 0x07E0u);
}

static void test_formatting_safety() {
	char buf[16];
	v2_core::format_float_2dp(buf, sizeof(buf), -0.50f);
	assert(strcmp(buf, "-0.50") == 0);
	v2_core::format_float_2dp(buf, sizeof(buf), 68.17f);
	assert(strcmp(buf, "68.17") == 0);
	v2_core::format_float_2dp(buf, sizeof(buf), NAN);
	assert(strcmp(buf, " -- ") == 0);

	v2_core::format_temp_int(buf, sizeof(buf), 25.6f);
	assert(strcmp(buf, "26") == 0);
	v2_core::format_temp_int(buf, sizeof(buf), -12.4f);
	assert(strcmp(buf, "-12") == 0);
	v2_core::format_temp_int(buf, sizeof(buf), 50000.0f);
	assert(strcmp(buf, "9999") == 0);
	v2_core::format_temp_int(buf, sizeof(buf), NAN);
	assert(strcmp(buf, "---") == 0);
}

int main() {
	printf("Running Firmware 2.50 host tests...\n");
	RUN_TEST(test_lepton_version_constants_distinct);
	RUN_TEST(test_compensate_calib_stability_fahrenheit_and_celsius);
	RUN_TEST(test_preset_and_calibration_validation);
	RUN_TEST(test_temp_to_raw_roundtrip_and_clamping);
	RUN_TEST(test_vospi_crc16);
	RUN_TEST(test_integer_filters_symmetric_and_no_overflow);
	RUN_TEST(test_lut_mapping_and_divide_by_zero_safety);
	RUN_TEST(test_formatting_safety);
	printf("All %d tests passed!\n", g_passed);
	return 0;
}
