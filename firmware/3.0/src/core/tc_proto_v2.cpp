/*
 * TC_PROTO_V2 - Byte-exact DIY-Thermocam USB Protocol v2.0 (pure C++17)
 */

#include "tc_proto_v2.h"
#include <array>

namespace tc_v2 {

namespace {

constexpr std::array<uint8_t, 256> make_crc8_table() {
	std::array<uint8_t, 256> t{};
	for (uint32_t i = 0; i < 256; ++i) {
		uint8_t c = static_cast<uint8_t>(i);
		for (int b = 0; b < 8; ++b) {
			c = (c & 0x80u) ? static_cast<uint8_t>((c << 1) ^ 0x07u) : static_cast<uint8_t>(c << 1);
		}
		t[i] = c;
	}
	return t;
}

constexpr std::array<uint32_t, 256> make_crc32_table() {
	std::array<uint32_t, 256> t{};
	for (uint32_t i = 0; i < 256; ++i) {
		uint32_t c = i;
		for (int b = 0; b < 8; ++b) {
			c = (c & 1u) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
		}
		t[i] = c;
	}
	return t;
}

static constexpr auto kCrc8Table = make_crc8_table();
static constexpr auto kCrc32Table = make_crc32_table();

} // namespace

uint8_t crc8_smbus(const uint8_t* data, size_t len) {
	uint8_t crc = 0x00;
	for (size_t i = 0; i < len; ++i) {
		crc = kCrc8Table[static_cast<uint8_t>(crc ^ data[i])];
	}
	return crc;
}

uint8_t header_crc8(const uint8_t hdr[HEADER_SIZE]) {
	uint8_t crc = 0x00;
	for (size_t i = 0; i < 7; ++i) {
		crc = kCrc8Table[static_cast<uint8_t>(crc ^ hdr[i])];
	}
	for (size_t i = 8; i < HEADER_SIZE; ++i) {
		crc = kCrc8Table[static_cast<uint8_t>(crc ^ hdr[i])];
	}
	return crc;
}

uint32_t crc32_update(uint32_t state, const uint8_t* data, size_t len) {
	uint32_t crc = state;
	for (size_t i = 0; i < len; ++i) {
		crc = kCrc32Table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
	}
	return crc;
}

uint32_t crc32_ieee(const uint8_t* data, size_t len) {
	return crc32_final(crc32_update(crc32_init(), data, len));
}

void encode_header(uint8_t out[HEADER_SIZE], const PacketHeader& h) {
	std::memcpy(out, MAGIC, 4);
	out[4] = h.version;
	out[5] = h.msg_type;
	out[6] = h.flags;
	out[7] = 0;
	write_u16_le(out + 8, h.seq);
	write_u16_le(out + 10, h.status_or_sub);
	write_u32_le(out + 12, h.payload_len);
	out[7] = header_crc8(out);
}

bool decode_header(const uint8_t in[HEADER_SIZE], PacketHeader& h) {
	if (std::memcmp(in, MAGIC, 4) != 0) return false;
	if (in[4] != PROTO_MAJOR) return false;
	uint32_t plen = read_u32_le(in + 12);
	if (plen > MAX_PAYLOAD_LEN) return false;
	if (in[7] != header_crc8(in)) return false;
	h.version = in[4];
	h.msg_type = in[5];
	h.flags = in[6];
	h.seq = read_u16_le(in + 8);
	h.status_or_sub = read_u16_le(in + 10);
	h.payload_len = plen;
	return true;
}

size_t encode_packet(uint8_t* out, size_t out_cap, const PacketHeader& h, const uint8_t* payload) {
	const size_t total = FRAME_OVERHEAD + h.payload_len;
	if (!out || out_cap < total || h.payload_len > MAX_PAYLOAD_LEN) return 0;
	encode_header(out, h);
	if (h.payload_len > 0 && payload) {
		std::memcpy(out + HEADER_SIZE, payload, h.payload_len);
	}
	uint32_t crc = crc32_ieee(out, HEADER_SIZE + h.payload_len);
	write_u32_le(out + HEADER_SIZE + h.payload_len, crc);
	return total;
}

Parser::Parser(uint8_t* payload_buf, size_t payload_cap)
	: payload_buf_(payload_buf), payload_cap_(payload_cap) {}

void Parser::reset() {
	state_ = State::FIND_MAGIC;
	hdr_pos_ = 0;
	body_pos_ = 0;
	crc_state_ = crc32_init();
	oversized_ = false;
}

void Parser::resync_from_header() {
	++rx_errors_;
	// Shift hdr_buf_[1..15] left by 1 byte and scan for an embedded/overlapping "TC2\0"
	uint8_t tmp[HEADER_SIZE - 1];
	std::memcpy(tmp, hdr_buf_ + 1, HEADER_SIZE - 1);
	const size_t rem = HEADER_SIZE - 1;

	// Search for complete 4-byte magic inside tmp[0..rem-1]
	for (size_t i = 0; i + 4 <= rem; ++i) {
		if (std::memcmp(tmp + i, MAGIC, 4) == 0) {
			const size_t keep = rem - i;
			std::memcpy(hdr_buf_, tmp + i, keep);
			hdr_pos_ = keep;
			state_ = State::READ_HEADER;
			return;
		}
	}
	// Otherwise keep the longest suffix of tmp matching a prefix of MAGIC (1..3 bytes)
	for (size_t prefix = 3; prefix >= 1; --prefix) {
		if (std::memcmp(tmp + (rem - prefix), MAGIC, prefix) == 0) {
			std::memcpy(hdr_buf_, MAGIC, prefix);
			hdr_pos_ = prefix;
			state_ = State::FIND_MAGIC;
			return;
		}
	}
	hdr_pos_ = 0;
	state_ = State::FIND_MAGIC;
}

ParseResult Parser::feed_byte(uint8_t b, uint32_t now_ms) {
	last_rx_ms_ = now_ms;

	if (state_ == State::FIND_MAGIC) {
		if (b == MAGIC[hdr_pos_]) {
			hdr_buf_[hdr_pos_++] = b;
			if (hdr_pos_ == 4) {
				state_ = State::READ_HEADER;
			}
		} else if (b == MAGIC[0]) {
			hdr_buf_[0] = b;
			hdr_pos_ = 1;
		} else {
			hdr_pos_ = 0;
		}
		return ParseResult::NEED_MORE;
	}

	if (state_ == State::READ_HEADER) {
		hdr_buf_[hdr_pos_++] = b;
		if (hdr_pos_ < HEADER_SIZE) {
			return ParseResult::NEED_MORE;
		}
		if (!decode_header(hdr_buf_, hdr_)) {
			resync_from_header();
			return ParseResult::NEED_MORE;
		}
		crc_state_ = crc32_update(crc32_init(), hdr_buf_, HEADER_SIZE);
		body_pos_ = 0;
		oversized_ = (hdr_.payload_len > payload_cap_);
		state_ = State::READ_PAYLOAD_AND_CRC;
		return ParseResult::NEED_MORE;
	}

	// READ_PAYLOAD_AND_CRC
	if (body_pos_ < hdr_.payload_len) {
		if (!oversized_ && payload_buf_) {
			payload_buf_[body_pos_] = b;
		}
		crc_state_ = crc32_update(crc_state_, &b, 1);
		++body_pos_;
		return ParseResult::NEED_MORE;
	}

	const uint32_t crc_idx = body_pos_ - hdr_.payload_len;
	crc_buf_[crc_idx] = b;
	++body_pos_;
	if (crc_idx < 3) {
		return ParseResult::NEED_MORE;
	}

	const uint32_t expected_crc = read_u32_le(crc_buf_);
	const uint32_t actual_crc = crc32_final(crc_state_);
	const bool was_oversized = oversized_;
	reset();

	if (actual_crc != expected_crc) {
		++crc_errors_;
		return ParseResult::BAD_CRC;
	}
	if (was_oversized) {
		return ParseResult::PAYLOAD_TOO_LARGE;
	}
	return ParseResult::PACKET_OK;
}

bool Parser::check_timeout(uint32_t now_ms, uint32_t timeout_ms) {
	if (state_ == State::READ_PAYLOAD_AND_CRC && (now_ms - last_rx_ms_) >= timeout_ms) {
		++rx_errors_;
		reset();
		return true;
	}
	return false;
}

bool decode_hello_req(const uint8_t* p, size_t len, HelloReq& out) {
	if (!p || len != 8) return false;
	out.host_proto_major = p[0];
	out.host_proto_minor = p[1];
	out.reserved = read_u16_le(p + 2);
	out.host_features = read_u32_le(p + 4);
	return true;
}

void encode_hello_resp(uint8_t out[64], const HelloResp& v) {
	std::memset(out, 0, 64);
	out[0] = v.proto_major;
	out[1] = v.proto_minor;
	write_u16_le(out + 2, v.fw_version);
	std::memcpy(out + 4, v.fw_build_str, 32);
	out[36] = v.hw_version;
	out[37] = v.lepton_type;
	write_u16_le(out + 38, v.sensor_w);
	write_u16_le(out + 40, v.sensor_h);
	write_u16_le(out + 42, v.display_w);
	write_u16_le(out + 44, v.display_h);
	out[46] = v.diagnostic_mask;
	out[47] = v.reserved;
	write_u64_le(out + 48, v.teensy_uid);
	write_u32_le(out + 56, v.capabilities);
	write_u16_le(out + 60, v.max_chunk_size);
	write_u16_le(out + 62, v.heartbeat_timeout_ms);
}

bool decode_stream_ctrl(const uint8_t* p, size_t len, StreamCtrl& out) {
	if (!p || len != 8) return false;
	out.mode = p[0];
	out.keep_lcd_on = p[1];
	out.include_temp_points = p[2];
	out.perf_interval_s = p[3];
	out.max_fps_x10 = read_u16_le(p + 4);
	out.reserved = read_u16_le(p + 6);
	return true;
}

void encode_stream_ctrl(uint8_t out[8], const StreamCtrl& v) {
	out[0] = v.mode;
	out[1] = v.keep_lcd_on;
	out[2] = v.include_temp_points;
	out[3] = v.perf_interval_s;
	write_u16_le(out + 4, v.max_fps_x10);
	write_u16_le(out + 6, v.reserved);
}

void encode_frame_raw_meta(uint8_t out[64], const FrameRawMeta& v) {
	std::memset(out, 0, 64);
	write_u32_le(out + 0, v.frame_seq);
	write_u64_le(out + 4, v.timestamp_us);
	write_u32_le(out + 12, v.rtc_epoch);
	write_u32_le(out + 16, v.dropped_frames);
	write_u16_le(out + 20, v.width);
	write_u16_le(out + 22, v.height);
	out[24] = v.pixel_format;
	out[25] = v.gain_mode;
	out[26] = v.ffc_state;
	out[27] = v.flags;
	write_u16_le(out + 28, v.min_raw);
	write_u16_le(out + 30, v.max_raw);
	write_u16_le(out + 32, v.min_x);
	write_u16_le(out + 34, v.min_y);
	write_u16_le(out + 36, v.max_x);
	write_u16_le(out + 38, v.max_y);
	write_f32_le(out + 40, v.spot_temp_c);
	write_f32_le(out + 44, v.cal_offset);
	write_f32_le(out + 48, v.cal_slope);
	write_f32_le(out + 52, v.fpa_temp_c);
	out[56] = static_cast<uint8_t>(v.bat_pct);
	out[57] = v.usb_power;
	write_u16_le(out + 58, v.temp_point_count);
}

bool decode_device_config(const uint8_t* p, size_t len, DeviceConfig& out) {
	if (!p || len != 32) return false;
	out.config_ver = p[0];
	out.color_scheme = p[1];
	out.text_color = p[2];
	out.temp_format = p[3];
	out.filter_type = p[4];
	out.min_max_points = p[5];
	out.rotation_vert = p[6];
	out.rotation_horiz = p[7];
	out.limits_mode = p[8];
	out.shutter_mode = p[9];
	out.lepton_gain_mode = p[10];
	out.hot_cold_mode = p[11];
	out.hot_cold_color = p[12];
	out.screen_off_time = p[13];
	out.overlay_flags = read_u16_le(p + 14);
	out.manual_min_raw = read_u16_le(p + 16);
	out.manual_max_raw = read_u16_le(p + 18);
	out.hot_cold_level_c10 = read_i16_le(p + 20);
	out.min_max_preset = p[22];
	return true;
}

void encode_device_config(uint8_t out[32], const DeviceConfig& v) {
	std::memset(out, 0, 32);
	out[0] = v.config_ver;
	out[1] = v.color_scheme;
	out[2] = v.text_color;
	out[3] = v.temp_format;
	out[4] = v.filter_type;
	out[5] = v.min_max_points;
	out[6] = v.rotation_vert;
	out[7] = v.rotation_horiz;
	out[8] = v.limits_mode;
	out[9] = v.shutter_mode;
	out[10] = v.lepton_gain_mode;
	out[11] = v.hot_cold_mode;
	out[12] = v.hot_cold_color;
	out[13] = v.screen_off_time;
	write_u16_le(out + 14, v.overlay_flags);
	write_u16_le(out + 16, v.manual_min_raw);
	write_u16_le(out + 18, v.manual_max_raw);
	write_i16_le(out + 20, v.hot_cold_level_c10);
	out[22] = v.min_max_preset;
}

bool validate_device_config(const DeviceConfig& c) {
	if (c.config_ver != 1) return false;
	if (c.color_scheme > 18) return false;
	if (c.text_color > 4) return false;
	if (c.temp_format > 1) return false;
	if (c.filter_type > 2) return false;
	if (c.min_max_points > 3) return false;
	if (c.rotation_vert > 1) return false;
	if (c.rotation_horiz > 1) return false;
	if (c.limits_mode > 2) return false;
	if (c.shutter_mode > 1) return false;
	if (c.lepton_gain_mode > 1) return false;
	if (c.hot_cold_mode > 2) return false;
	if (c.hot_cold_color > 4) return false;
	if (c.screen_off_time > 2) return false;
	if (c.min_max_preset > 3) return false;
	if (c.limits_mode == 2 && c.manual_min_raw >= c.manual_max_raw) return false;
	return true;
}

void encode_perf_telemetry(uint8_t out[84], const PerfTelemetry& v) {
	std::memset(out, 0, 84);
	write_u32_le(out + 0, v.acquire_us);
	write_u32_le(out + 4, v.stats_us);
	write_u32_le(out + 8, v.filter_us);
	write_u32_le(out + 12, v.scale_us);
	write_u32_le(out + 16, v.colorize_us);
	write_u32_le(out + 20, v.overlay_us);
	write_u32_le(out + 24, v.display_us);
	write_u32_le(out + 28, v.sd_write_us);
	write_u32_le(out + 32, v.usb_tx_us);
	write_u32_le(out + 36, v.loop_total_us);
	write_u32_le(out + 40, v.frame_count);
	write_u32_le(out + 44, v.frames_dropped);
	write_u32_le(out + 48, v.vospi_resyncs);
	write_u32_le(out + 52, v.vospi_crc_errors);
	write_u32_le(out + 56, v.usb_rx_errors);
	write_u32_le(out + 60, v.usb_crc_errors);
	write_u16_le(out + 64, v.live_fps_x100);
	write_u16_le(out + 66, v.video_fps_x100);
	write_u16_le(out + 68, v.stream_fps_x100);
	out[70] = v.perf_mode;
	out[71] = v.flags;
	out[72] = v.display_spi_mhz;
	out[73] = v.lepton_spi_mhz;
	write_u16_le(out + 74, v.cpu_mhz);
	write_f32_le(out + 76, v.mcu_temp_c);
	write_u32_le(out + 80, v.frame_period_us);
}

void encode_diagnostics(uint8_t out[80], const DiagnosticsResp& v) {
	std::memset(out, 0, 80);
	out[0] = v.diagnostic_mask;
	out[1] = static_cast<uint8_t>(v.bat_pct);
	out[2] = v.usb_power;
	out[3] = v.sd_fs_type;
	write_u16_le(out + 4, v.bat_mv);
	write_u16_le(out + 6, v.cpu_mhz);
	write_f32_le(out + 8, v.mcu_temp_c);
	write_u32_le(out + 12, v.sd_total_kb);
	write_u32_le(out + 16, v.sd_free_kb);
	write_u64_le(out + 20, v.uptime_us);
	write_u32_le(out + 28, v.free_dtcm_bytes);
	write_u32_le(out + 32, v.free_ocram_bytes);
	write_f32_le(out + 36, v.lepton_fpa_temp_c);
	std::memcpy(out + 40, v.lepton_part_num, 32);
}

bool decode_rad_params(const uint8_t* p, size_t len, RadParams& out) {
	if (!p || len != 18) return false;
	out.scene_emissivity = read_u16_le(p + 0);
	out.t_bkg_ck = read_u16_le(p + 2);
	out.tau_window = read_u16_le(p + 4);
	out.t_window_ck = read_u16_le(p + 6);
	out.tau_atm = read_u16_le(p + 8);
	out.t_atm_ck = read_u16_le(p + 10);
	out.refl_window = read_u16_le(p + 12);
	out.t_refl_ck = read_u16_le(p + 14);
	out.flags = p[16];
	out.reserved = p[17];
	return true;
}

void encode_rad_params(uint8_t out[18], const RadParams& v) {
	write_u16_le(out + 0, v.scene_emissivity);
	write_u16_le(out + 2, v.t_bkg_ck);
	write_u16_le(out + 4, v.tau_window);
	write_u16_le(out + 6, v.t_window_ck);
	write_u16_le(out + 8, v.tau_atm);
	write_u16_le(out + 10, v.t_atm_ck);
	write_u16_le(out + 12, v.refl_window);
	write_u16_le(out + 14, v.t_refl_ck);
	out[16] = v.flags;
	out[17] = v.reserved;
}

bool validate_rad_params(const RadParams& r) {
	if (r.flags & 0x02u) return true; // reset_to_defaults ignores offsets 0..15
	if (r.scene_emissivity < 82 || r.scene_emissivity > 8192) return false;
	if (r.tau_window < 82 || r.tau_window > 8192) return false;
	if (r.tau_atm < 82 || r.tau_atm > 8192) return false;
	if (r.refl_window > 8192) return false;
	if (static_cast<uint32_t>(r.tau_window) + static_cast<uint32_t>(r.refl_window) > 8192u) return false;
	return true;
}

StatusCode validate_sd_path(const char* path, size_t len) {
	if (!path || len == 0 || len > 128) return ERR_INVALID_ARG;
	if (path[0] != '/') return ERR_FORBIDDEN;
	if (len > 1 && path[len - 1] == '/') return ERR_FORBIDDEN;
	size_t seg_start = 1;
	for (size_t i = 0; i < len; ++i) {
		char c = path[i];
		if (c == '\0' || c == '\\') return ERR_FORBIDDEN;
		if (i > 0 && c == '/') {
			size_t seg_len = i - seg_start;
			if (seg_len == 0) return ERR_FORBIDDEN; // "//"
			if (seg_len == 2 && path[seg_start] == '.' && path[seg_start + 1] == '.') {
				return ERR_FORBIDDEN;
			}
			seg_start = i + 1;
		}
	}
	if (len > 1) {
		size_t seg_len = len - seg_start;
		if (seg_len == 2 && path[seg_start] == '.' && path[seg_start + 1] == '.') {
			return ERR_FORBIDDEN;
		}
	}
	return OK;
}

} // namespace tc_v2
