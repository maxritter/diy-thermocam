/*
 * Host unit tests for DIY-Thermocam V1/V2 USB Protocol v2.0 codec, parser,
 * golden byte vectors (docs/protocol-v2.md), and legacy hardware/sensor mapping.
 */

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tc_proto_v2.h"
#include "v2_core.h"

using namespace tc_v2;

static bool bytes_eq(const uint8_t* a, const uint8_t* b, size_t n) {
	return memcmp(a, b, n) == 0;
}

static void test_checksums() {
	const uint8_t check[] = {'1','2','3','4','5','6','7','8','9'};
	assert(crc8_smbus(check, 9) == 0xF4u);
	assert(crc32_ieee(check, 9) == 0xCBF43926u);
	// Incremental CRC-32 matches one-shot
	uint32_t st = crc32_init();
	st = crc32_update(st, check, 4);
	st = crc32_update(st, check + 4, 5);
	assert(crc32_final(st) == 0xCBF43926u);
}

static void test_golden_vectors() {
	uint8_t buf[128];

	// 1. HELLO_REQ
	const uint8_t v1[] = {
		0x54,0x43,0x32,0x00, 0x02,0x01,0x00,0xA4, 0x01,0x00,0x00,0x00, 0x08,0x00,0x00,0x00,
		0x02,0x00,0x00,0x00, 0x01,0x00,0x00,0x00, 0x86,0x4F,0x91,0x47
	};
	uint8_t p1[8] = {0x02,0x00,0x00,0x00, 0x01,0x00,0x00,0x00};
	size_t n1 = encode_packet(buf, sizeof(buf), {2, MSG_HELLO_REQ, 0, 1, 0, 8}, p1);
	assert(n1 == sizeof(v1) && bytes_eq(buf, v1, n1));

	// 2. PING_REQ
	const uint8_t v2[] = {
		0x54,0x43,0x32,0x00, 0x02,0x03,0x00,0x41, 0x02,0x00,0x00,0x00, 0x08,0x00,0x00,0x00,
		0x40,0x42,0x0F,0x00, 0x00,0x00,0x00,0x00, 0x9F,0xF9,0xEC,0xD6
	};
	uint8_t p2[8];
	write_u64_le(p2, 1000000ull);
	size_t n2 = encode_packet(buf, sizeof(buf), {2, MSG_PING_REQ, 0, 2, 0, 8}, p2);
	assert(n2 == sizeof(v2) && bytes_eq(buf, v2, n2));

	// 3. ENTER_BOOTLOADER_REQ
	const uint8_t v3[] = {
		0x54,0x43,0x32,0x00, 0x02,0xF0,0x00,0x9C, 0x2A,0x00,0x00,0x00, 0x04,0x00,0x00,0x00,
		0xAD,0x10,0x07,0xB0, 0x0F,0xD1,0x8E,0x1C
	};
	uint8_t p3[4];
	write_u32_le(p3, BOOTLOADER_MAGIC);
	size_t n3 = encode_packet(buf, sizeof(buf), {2, MSG_ENTER_BOOTLOADER_REQ, 0, 42, 0, 4}, p3);
	assert(n3 == sizeof(v3) && bytes_eq(buf, v3, n3));

	// 4. SET_RAD_PARAMS_REQ
	const uint8_t v4[] = {
		0x54,0x43,0x32,0x00, 0x02,0x1B,0x00,0x3E, 0x07,0x00,0x00,0x00, 0x12,0x00,0x00,0x00,
		0x66,0x1E,0x4B,0x73, 0x00,0x20,0x4B,0x73, 0x00,0x20,0x4B,0x73, 0x00,0x00,0x4B,0x73,
		0x01,0x00, 0xAD,0x8B,0x28,0xA2
	};
	RadParams rp{};
	rp.scene_emissivity = 7782; // 0.95 * 8192
	rp.flags = 1;
	uint8_t p4[18];
	encode_rad_params(p4, rp);
	size_t n4 = encode_packet(buf, sizeof(buf), {2, MSG_SET_RAD_PARAMS_REQ, 0, 7, 0, 18}, p4);
	assert(n4 == sizeof(v4) && bytes_eq(buf, v4, n4));

	// 5. GENERIC_ERROR_RESP
	const uint8_t v5[] = {
		0x54,0x43,0x32,0x00, 0x02,0xFF,0x02,0x65, 0x63,0x00,0x02,0x00, 0x00,0x00,0x00,0x00,
		0x1B,0x41,0x40,0x96
	};
	size_t n5 = encode_packet(buf, sizeof(buf), {2, MSG_GENERIC_ERROR_RESP, FLAG_ERROR, 99, ERR_UNKNOWN_MSG, 0}, nullptr);
	assert(n5 == sizeof(v5) && bytes_eq(buf, v5, n5));

	// 6. GOODBYE_REQ
	const uint8_t v6[] = {
		0x54,0x43,0x32,0x00, 0x02,0x02,0x00,0x41, 0x64,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
		0xF0,0xC0,0xF2,0x48
	};
	size_t n6 = encode_packet(buf, sizeof(buf), {2, MSG_GOODBYE_REQ, 0, 100, 0, 0}, nullptr);
	assert(n6 == sizeof(v6) && bytes_eq(buf, v6, n6));

	// 7. Resync stream: garbage + fake header with bad crc8 + valid GOODBYE_REQ
	const uint8_t v7[] = {
		0xAA,0xBB, 0x54,0x43,0x32,0x00, 0x02,0x01,0x00,0xFF, 0x00,0x00,0x00,0x00, 0x00,0x00,
		0x01,0x00, 0x54,0x43,0x32,0x00, 0x02,0x02,0x00,0x41, 0x64,0x00,0x00,0x00, 0x00,0x00,
		0x00,0x00, 0xF0,0xC0,0xF2,0x48
	};
	uint8_t rx[64];
	Parser parser(rx, sizeof(rx));
	ParseResult last = ParseResult::NEED_MORE;
	for (size_t i = 0; i < sizeof(v7); ++i) {
		last = parser.feed_byte(v7[i], 10);
		if (i + 1 < sizeof(v7)) assert(last == ParseResult::NEED_MORE);
	}
	assert(last == ParseResult::PACKET_OK);
	assert(parser.header().msg_type == MSG_GOODBYE_REQ);
	assert(parser.header().seq == 100);
	assert(parser.rx_errors() == 1);
}

static void test_parser_errors_and_timeouts() {
	uint8_t rx[16];
	Parser parser(rx, sizeof(rx));

	// Bad CRC-32
	uint8_t pkt[64];
	uint8_t payload[8] = {1,2,3,4,5,6,7,8};
	size_t n = encode_packet(pkt, sizeof(pkt), {2, MSG_PING_REQ, 0, 5, 0, 8}, payload);
	pkt[n - 1] ^= 0x01u; // flip bit in CRC
	ParseResult r = ParseResult::NEED_MORE;
	for (size_t i = 0; i < n; ++i) r = parser.feed_byte(pkt[i], 100);
	assert(r == ParseResult::BAD_CRC);
	assert(parser.crc_errors() == 1);

	// Oversized payload (> 16 B rx buffer)
	uint8_t big[32] = {};
	n = encode_packet(pkt, sizeof(pkt), {2, MSG_SET_CONFIG_REQ, 0, 6, 0, 32}, big);
	for (size_t i = 0; i < n; ++i) r = parser.feed_byte(pkt[i], 200);
	assert(r == ParseResult::PAYLOAD_TOO_LARGE);

	// Incomplete payload timeout
	n = encode_packet(pkt, sizeof(pkt), {2, MSG_PING_REQ, 0, 7, 0, 8}, payload);
	for (size_t i = 0; i < 18; ++i) assert(parser.feed_byte(pkt[i], 1000) == ParseResult::NEED_MORE);
	assert(!parser.check_timeout(1400));
	assert(parser.check_timeout(1500));
}

static void test_codecs_and_validation() {
	// DeviceConfig round-trip
	DeviceConfig dc{};
	dc.color_scheme = 10;
	dc.limits_mode = 2;
	dc.manual_min_raw = 29000;
	dc.manual_max_raw = 31000;
	dc.hot_cold_level_c10 = -55;
	dc.overlay_flags = 0x007F;
	assert(validate_device_config(dc));
	uint8_t dcbuf[32];
	encode_device_config(dcbuf, dc);
	DeviceConfig dc2{};
	assert(decode_device_config(dcbuf, 32, dc2));
	assert(dc2.color_scheme == 10 && dc2.manual_min_raw == 29000 && dc2.manual_max_raw == 31000);
	assert(dc2.hot_cold_level_c10 == -55 && dc2.overlay_flags == 0x007F);
	dc.manual_min_raw = 31000;
	assert(!validate_device_config(dc));

	// RadParams validation
	RadParams rp{};
	assert(validate_rad_params(rp));
	rp.tau_window = 5000;
	rp.refl_window = 4000; // sum > 8192
	assert(!validate_rad_params(rp));
	rp.flags = 0x02; // reset_defaults bypasses check
	assert(validate_rad_params(rp));

	// SD path validation
	assert(validate_sd_path("/", 1) == OK);
	assert(validate_sd_path("/20261127120000.DAT", 19) == OK);
	assert(validate_sd_path("/20261127120000/00000.DAT", 25) == OK);
	assert(validate_sd_path("", 0) == ERR_INVALID_ARG);
	assert(validate_sd_path("relative.DAT", 12) == ERR_FORBIDDEN);
	assert(validate_sd_path("/dir/", 5) == ERR_FORBIDDEN);
	assert(validate_sd_path("/a/../b", 7) == ERR_FORBIDDEN);
	assert(validate_sd_path("/a/..", 5) == ERR_FORBIDDEN);
	assert(validate_sd_path("/a//b", 5) == ERR_FORBIDDEN);
	assert(validate_sd_path("/a\\b", 4) == ERR_FORBIDDEN);
}

static void test_legacy_mapping_and_calibration() {
	// Diagnostic bit remapping:
	// legacy: 0=spot, 1=display, 2=camera, 3=touch, 4=sd, 5=bat, 6=lep_conf, 7=lep_data
	// v2:     0=display, 1=touch, 2=sd, 3=battery, 4=lepton, 5..7=1
	assert(map_legacy_diagnostic_mask(0xFF, true) == 0xFF);
	assert(map_legacy_diagnostic_mask(0xFF, false) == 0xFB); // SD bit 2 clear
	assert(map_legacy_diagnostic_mask(static_cast<uint8_t>(~(1u << 1)), true) == 0xFE); // display bit 0 clear
	assert(map_legacy_diagnostic_mask(static_cast<uint8_t>(~(1u << 3)), true) == 0xFD); // touch bit 1 clear
	assert(map_legacy_diagnostic_mask(static_cast<uint8_t>(~(1u << 5)), true) == 0xF7); // battery bit 3 clear
	assert(map_legacy_diagnostic_mask(static_cast<uint8_t>(~(1u << 6)), true) == 0xEF); // lepton bit 4 clear
	assert(map_legacy_diagnostic_mask(static_cast<uint8_t>(~(1u << 7)), true) == 0xEF); // lepton bit 4 clear
	// Spot or camera failure alone does not fail v2's 5 self-test bits
	assert(map_legacy_diagnostic_mask(static_cast<uint8_t>(~((1u << 0) | (1u << 2))), true) == 0xFF);

	// Lepton version -> Protocol v2 lepton_type
	assert(map_legacy_lepton_type(0, true) == 0); // 2.0 shutter -> 2.x (80x60)
	assert(map_legacy_lepton_type(2, true) == 0); // 2.0 noShutter -> 2.x (80x60)
	assert(map_legacy_lepton_type(3, true) == 0); // 2.5 shutter -> 2.x (80x60)
	assert(map_legacy_lepton_type(1, true) == 1); // 3.0 shutter -> 3.x (160x120)
	assert(map_legacy_lepton_type(4, true) == 1); // 3.5 shutter -> 3.x (160x120)
	assert(map_legacy_lepton_type(4, false) == 255); // absent

	// Wire formula T_C = raw * cal_slope + cal_offset for both radiometric and non-radiometric
	float off_rad = v2_core::cal_offset_celsius(true, 22.0f, 0.01f, 0.0f);
	assert(fabsf(off_rad - (-273.15f)) < 1e-4f);
	assert(fabsf(29815.0f * 0.01f + off_rad - 25.0f) < 1e-3f);

	float off_nonrad = v2_core::cal_offset_celsius(false, 22.0f, 0.03f, 1.5f);
	float t_c = 8192.0f * 0.03f + off_nonrad;
	assert(fabsf(t_c - 23.5f) < 1e-3f);
	assert(fabsf(t_c - v2_core::raw_to_celsius(8192, false, 22.0f, 0.03f, 1.5f)) < 1e-4f);
}

int main() {
	test_checksums();
	test_golden_vectors();
	test_parser_errors_and_timeouts();
	test_codecs_and_validation();
	test_legacy_mapping_and_calibration();
	printf("test_tc_proto_v2: ALL TESTS PASSED\n");
	return 0;
}
