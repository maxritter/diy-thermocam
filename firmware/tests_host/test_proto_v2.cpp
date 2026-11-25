#include "tc_proto_v2.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace tc_v2;

static void check_bytes(const char* name, const uint8_t* actual, const std::vector<uint8_t>& expected) {
	if (std::memcmp(actual, expected.data(), expected.size()) != 0) {
		std::fprintf(stderr, "FAIL %s: byte mismatch\n", name);
		assert(false);
	}
}

void run_proto_v2_tests() {
	// 5.1 Checksum Reference Checks
	const uint8_t check_str[] = {'1','2','3','4','5','6','7','8','9'};
	assert(crc8_smbus(check_str, 9) == 0xF4u);
	assert(crc32_ieee(check_str, 9) == 0xCBF43926u);

	uint8_t wire[256];

	// Vector 1: HELLO_REQ
	{
		uint8_t payload[8] = {0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
		PacketHeader h{PROTO_MAJOR, MSG_HELLO_REQ, 0x00, 1, 0, 8};
		size_t n = encode_packet(wire, sizeof(wire), h, payload);
		assert(n == 28);
		check_bytes("HELLO_REQ", wire, {
			0x54,0x43,0x32,0x00, 0x02,0x01,0x00,0xA4, 0x01,0x00,0x00,0x00, 0x08,0x00,0x00,0x00,
			0x02,0x00,0x00,0x00, 0x01,0x00,0x00,0x00, 0x86,0x4F,0x91,0x47
		});
	}

	// Vector 2: PING_REQ
	{
		uint8_t payload[8];
		write_u64_le(payload, 1000000ULL);
		PacketHeader h{PROTO_MAJOR, MSG_PING_REQ, 0x00, 2, 0, 8};
		size_t n = encode_packet(wire, sizeof(wire), h, payload);
		assert(n == 28);
		check_bytes("PING_REQ", wire, {
			0x54,0x43,0x32,0x00, 0x02,0x03,0x00,0x41, 0x02,0x00,0x00,0x00, 0x08,0x00,0x00,0x00,
			0x40,0x42,0x0F,0x00, 0x00,0x00,0x00,0x00, 0x9F,0xF9,0xEC,0xD6
		});
	}

	// Vector 3: ENTER_BOOTLOADER_REQ
	{
		uint8_t payload[4];
		write_u32_le(payload, BOOTLOADER_MAGIC);
		PacketHeader h{PROTO_MAJOR, MSG_ENTER_BOOTLOADER_REQ, 0x00, 42, 0, 4};
		size_t n = encode_packet(wire, sizeof(wire), h, payload);
		assert(n == 24);
		check_bytes("ENTER_BOOTLOADER_REQ", wire, {
			0x54,0x43,0x32,0x00, 0x02,0xF0,0x00,0x9C, 0x2A,0x00,0x00,0x00, 0x04,0x00,0x00,0x00,
			0xAD,0x10,0x07,0xB0, 0x0F,0xD1,0x8E,0x1C
		});
	}

	// Vector 4: SET_RAD_PARAMS_REQ
	{
		RadParams rp{};
		rp.scene_emissivity = 7782;
		rp.t_bkg_ck = 29515;
		rp.tau_window = 8192;
		rp.t_window_ck = 29515;
		rp.tau_atm = 8192;
		rp.t_atm_ck = 29515;
		rp.refl_window = 0;
		rp.t_refl_ck = 29515;
		rp.flags = 1;
		rp.reserved = 0;
		uint8_t payload[18];
		encode_rad_params(payload, rp);
		PacketHeader h{PROTO_MAJOR, MSG_SET_RAD_PARAMS_REQ, 0x00, 7, 0, 18};
		size_t n = encode_packet(wire, sizeof(wire), h, payload);
		assert(n == 38);
		check_bytes("SET_RAD_PARAMS_REQ", wire, {
			0x54,0x43,0x32,0x00, 0x02,0x1B,0x00,0x3E, 0x07,0x00,0x00,0x00, 0x12,0x00,0x00,0x00,
			0x66,0x1E,0x4B,0x73, 0x00,0x20,0x4B,0x73, 0x00,0x20,0x4B,0x73, 0x00,0x00,0x4B,0x73,
			0x01,0x00, 0xAD,0x8B,0x28,0xA2
		});
	}

	// Vector 5: GENERIC_ERROR_RESP
	{
		PacketHeader h{PROTO_MAJOR, MSG_GENERIC_ERROR_RESP, FLAG_ERROR, 99, ERR_UNKNOWN_MSG, 0};
		size_t n = encode_packet(wire, sizeof(wire), h, nullptr);
		assert(n == 20);
		check_bytes("GENERIC_ERROR_RESP", wire, {
			0x54,0x43,0x32,0x00, 0x02,0xFF,0x02,0x65, 0x63,0x00,0x02,0x00, 0x00,0x00,0x00,0x00,
			0x1B,0x41,0x40,0x96
		});
	}

	// Vector 6: GOODBYE_REQ
	{
		PacketHeader h{PROTO_MAJOR, MSG_GOODBYE_REQ, 0x00, 100, 0, 0};
		size_t n = encode_packet(wire, sizeof(wire), h, nullptr);
		assert(n == 20);
		check_bytes("GOODBYE_REQ", wire, {
			0x54,0x43,0x32,0x00, 0x02,0x02,0x00,0x41, 0x64,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
			0xF0,0xC0,0xF2,0x48
		});
	}

	// Vector 7: Resync Stream Test
	{
		const std::vector<uint8_t> stream = {
			0xAA,0xBB,0x54,0x43,0x32,0x00,0x02,0x01,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
			0x01,0x00,0x54,0x43,0x32,0x00,0x02,0x02,0x00,0x41,0x64,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0xF0,0xC0,0xF2,0x48
		};
		uint8_t rx[512];
		Parser p(rx, sizeof(rx));
		int ok_count = 0;
		for (uint8_t b : stream) {
			ParseResult r = p.feed_byte(b, 100);
			if (r == ParseResult::PACKET_OK) {
				++ok_count;
				assert(p.header().msg_type == MSG_GOODBYE_REQ);
				assert(p.header().seq == 100);
				assert(p.header().payload_len == 0);
			}
		}
		assert(ok_count == 1);
		assert(p.rx_errors() == 1);
		assert(p.crc_errors() == 0);
	}

	// Overlapping magic inside a corrupt header (real GOODBYE starts at byte 4 of bogus header)
	{
		uint8_t goodbye[20];
		PacketHeader h{PROTO_MAJOR, MSG_GOODBYE_REQ, 0x00, 100, 0, 0};
		encode_packet(goodbye, sizeof(goodbye), h, nullptr);
		std::vector<uint8_t> stream = {0x54, 0x43, 0x32, 0x00};
		stream.insert(stream.end(), goodbye, goodbye + 20);
		uint8_t rx[512];
		Parser p(rx, sizeof(rx));
		int ok_count = 0;
		for (uint8_t b : stream) {
			if (p.feed_byte(b, 10) == ParseResult::PACKET_OK) {
				++ok_count;
				assert(p.header().msg_type == MSG_GOODBYE_REQ);
				assert(p.header().seq == 100);
			}
		}
		assert(ok_count == 1);
		assert(p.rx_errors() == 1);
	}

	// Bad CRC-32 detection & recovery + RX timeout + oversized payload drain
	{
		uint8_t pkt[28];
		uint8_t payload[8] = {2, 0, 0, 0, 1, 0, 0, 0};
		PacketHeader h{PROTO_MAJOR, MSG_HELLO_REQ, 0x00, 55, 0, 8};
		encode_packet(pkt, sizeof(pkt), h, payload);
		pkt[27] ^= 0xFF; // corrupt CRC
		uint8_t rx[512];
		Parser p(rx, sizeof(rx));
		ParseResult last = ParseResult::NEED_MORE;
		for (size_t i = 0; i < 28; ++i) last = p.feed_byte(pkt[i], 20);
		assert(last == ParseResult::BAD_CRC);
		assert(p.header().seq == 55);
		assert(p.crc_errors() == 1);

		// Oversized payload against small 4-byte rx buffer
		pkt[27] ^= 0xFF;
		uint8_t tiny[4];
		Parser ps(tiny, sizeof(tiny));
		for (size_t i = 0; i < 28; ++i) last = ps.feed_byte(pkt[i], 30);
		assert(last == ParseResult::PAYLOAD_TOO_LARGE);
		assert(ps.header().seq == 55);

		// RX timeout mid-payload
		for (size_t i = 0; i < 18; ++i) ps.feed_byte(pkt[i], 1000);
		assert(!ps.check_timeout(1400));
		assert(ps.check_timeout(1500));
		assert(ps.rx_errors() == 1);
	}

	// Path root-jail validation
	assert(validate_sd_path("/", 1) == OK);
	assert(validate_sd_path("/20261125103045.DAT", 19) == OK);
	assert(validate_sd_path("/20261125103045/000000.DAT", 26) == OK);
	assert(validate_sd_path("", 0) == ERR_INVALID_ARG);
	assert(validate_sd_path("foo.DAT", 7) == ERR_FORBIDDEN);
	assert(validate_sd_path("/../secret", 10) == ERR_FORBIDDEN);
	assert(validate_sd_path("/a/..", 5) == ERR_FORBIDDEN);
	assert(validate_sd_path("/a\\b", 4) == ERR_FORBIDDEN);
	assert(validate_sd_path("/a/b/", 5) == ERR_FORBIDDEN);
	assert(validate_sd_path("/a//b", 5) == ERR_FORBIDDEN);

	// DeviceConfig & RadParams validation
	DeviceConfig cfg{};
	assert(validate_device_config(cfg));
	cfg.limits_mode = 2; cfg.manual_min_raw = 30000; cfg.manual_max_raw = 30000;
	assert(!validate_device_config(cfg));
	cfg.manual_max_raw = 31000;
	assert(validate_device_config(cfg));

	RadParams rp{};
	assert(validate_rad_params(rp));
	rp.tau_window = 5000; rp.refl_window = 4000;
	assert(!validate_rad_params(rp));
	rp.flags = 0x02; // reset_to_defaults
	assert(validate_rad_params(rp));

	std::printf("PASS: proto_v2 tests (all 7 golden vectors, resync, CRC, timeout, validation)\n");
}
