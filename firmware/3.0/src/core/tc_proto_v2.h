/*
 * TC_PROTO_V2 - Byte-exact DIY-Thermocam USB Protocol v2.0 (pure C++17)
 * Zero <Arduino.h> dependencies; compiles identically on Teensy 4.1 and host CI.
 */

#ifndef TC_PROTO_V2_H
#define TC_PROTO_V2_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tc_v2 {

static constexpr uint8_t MAGIC[4] = {0x54, 0x43, 0x32, 0x00}; // "TC2\0"
static constexpr uint8_t PROTO_MAJOR = 2;
static constexpr uint8_t PROTO_MINOR = 0;
static constexpr size_t HEADER_SIZE = 16;
static constexpr size_t TRAILER_SIZE = 4;
static constexpr size_t FRAME_OVERHEAD = HEADER_SIZE + TRAILER_SIZE;
static constexpr uint32_t MAX_PAYLOAD_LEN = 163840;
static constexpr uint32_t BOOTLOADER_MAGIC = 0xB00710ADu;
static constexpr uint16_t DEFAULT_MAX_CHUNK = 8192;
static constexpr uint16_t DEFAULT_HEARTBEAT_MS = 5000;
static constexpr uint32_t RX_PACKET_TIMEOUT_MS = 500;

// Packet flags
static constexpr uint8_t FLAG_ACK_REQ     = 0x01;
static constexpr uint8_t FLAG_ERROR       = 0x02;
static constexpr uint8_t FLAG_MORE_CHUNKS = 0x04;
static constexpr uint8_t FLAG_COMPRESSED  = 0x08;

// Status codes
enum StatusCode : uint16_t {
	OK              = 0x0000,
	ERR_BAD_CRC     = 0x0001,
	ERR_UNKNOWN_MSG = 0x0002,
	ERR_INVALID_ARG = 0x0003,
	ERR_BUSY        = 0x0004,
	ERR_IO          = 0x0005,
	ERR_NOT_FOUND   = 0x0006,
	ERR_FORBIDDEN   = 0x0007,
	ERR_UNSUPPORTED = 0x0008,
	ERR_BAD_MAGIC   = 0x0009,
	ERR_NO_SD       = 0x000A,
};

// Message type IDs
enum MsgType : uint8_t {
	MSG_HELLO_REQ           = 0x01,
	MSG_GOODBYE_REQ         = 0x02,
	MSG_PING_REQ            = 0x03,
	MSG_STREAM_CTRL_REQ     = 0x10,
	MSG_GET_CONFIG_REQ      = 0x11,
	MSG_SET_CONFIG_REQ      = 0x12,
	MSG_SET_TIME_REQ        = 0x13,
	MSG_TRIGGER_FFC_REQ     = 0x14,
	MSG_GET_TEMP_POINTS_REQ = 0x15,
	MSG_SET_TEMP_POINTS_REQ = 0x16,
	MSG_SAVE_CAPTURE_REQ    = 0x17,
	MSG_GET_DIAGNOSTICS_REQ = 0x18,
	MSG_GET_PERF_REQ        = 0x19,
	MSG_GET_RAD_PARAMS_REQ  = 0x1A,
	MSG_SET_RAD_PARAMS_REQ  = 0x1B,

	MSG_HELLO_RESP           = 0x41,
	MSG_GOODBYE_RESP         = 0x42,
	MSG_PONG_RESP            = 0x43,
	MSG_STREAM_CTRL_RESP     = 0x50,
	MSG_GET_CONFIG_RESP      = 0x51,
	MSG_SET_CONFIG_RESP      = 0x52,
	MSG_SET_TIME_RESP        = 0x53,
	MSG_TRIGGER_FFC_RESP     = 0x54,
	MSG_GET_TEMP_POINTS_RESP = 0x55,
	MSG_SET_TEMP_POINTS_RESP = 0x56,
	MSG_SAVE_CAPTURE_RESP    = 0x57,
	MSG_GET_DIAGNOSTICS_RESP = 0x58,
	MSG_GET_PERF_RESP        = 0x59,
	MSG_GET_RAD_PARAMS_RESP  = 0x5A,
	MSG_SET_RAD_PARAMS_RESP  = 0x5B,

	MSG_FRAME_RAW      = 0x80,
	MSG_FRAME_RENDERED = 0x81,

	MSG_EVT_BUTTON       = 0xC0,
	MSG_EVT_TOUCH        = 0xC1,
	MSG_EVT_BATTERY      = 0xC2,
	MSG_EVT_FFC          = 0xC3,
	MSG_EVT_SESSION_EXIT = 0xC4,
	MSG_EVT_PERF         = 0xC5,

	MSG_FS_INFO_REQ   = 0xE0,
	MSG_FS_LIST_REQ   = 0xE1,
	MSG_FS_STAT_REQ   = 0xE2,
	MSG_FS_READ_REQ   = 0xE3,
	MSG_FS_DELETE_REQ = 0xE4,
	MSG_FS_ABORT_REQ  = 0xE5,

	MSG_FS_INFO_RESP   = 0xE8,
	MSG_FS_LIST_RESP   = 0xE9,
	MSG_FS_STAT_RESP   = 0xEA,
	MSG_FS_READ_DATA   = 0xEB,
	MSG_FS_DELETE_RESP = 0xEC,
	MSG_FS_ABORT_RESP  = 0xED,

	MSG_ENTER_BOOTLOADER_REQ  = 0xF0,
	MSG_ENTER_BOOTLOADER_RESP = 0xF1,
	MSG_GENERIC_ERROR_RESP    = 0xFF,
};

// Capability bits
static constexpr uint32_t CAP_RAW_STREAM        = 0x0001u;
static constexpr uint32_t CAP_DISPLAY_STREAM    = 0x0002u;
static constexpr uint32_t CAP_SD_FS             = 0x0004u;
static constexpr uint32_t CAP_PERF_TELEMETRY    = 0x0008u;
static constexpr uint32_t CAP_BOOTLOADER        = 0x0010u;
static constexpr uint32_t CAP_LEPTON_RAD_PARAMS = 0x0020u;
static constexpr uint32_t CAP_GAIN_MODE         = 0x0040u;
static constexpr uint32_t CAP_HOT_COLD          = 0x0080u;
static constexpr uint32_t CAP_RTC_SYNC          = 0x0100u;
static constexpr uint32_t CAP_KEEP_LCD_ON       = 0x0200u;

// Explicit little-endian serialization helpers
inline uint16_t read_u16_le(const uint8_t* p) {
	return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
inline int16_t read_i16_le(const uint8_t* p) {
	return static_cast<int16_t>(read_u16_le(p));
}
inline uint32_t read_u32_le(const uint8_t* p) {
	return static_cast<uint32_t>(p[0]) |
	       (static_cast<uint32_t>(p[1]) << 8) |
	       (static_cast<uint32_t>(p[2]) << 16) |
	       (static_cast<uint32_t>(p[3]) << 24);
}
inline uint64_t read_u64_le(const uint8_t* p) {
	return static_cast<uint64_t>(read_u32_le(p)) |
	       (static_cast<uint64_t>(read_u32_le(p + 4)) << 32);
}
inline float read_f32_le(const uint8_t* p) {
	uint32_t bits = read_u32_le(p);
	float v = 0.0f;
	std::memcpy(&v, &bits, sizeof(v));
	return v;
}
inline void write_u16_le(uint8_t* p, uint16_t v) {
	p[0] = static_cast<uint8_t>(v & 0xFFu);
	p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
}
inline void write_i16_le(uint8_t* p, int16_t v) {
	write_u16_le(p, static_cast<uint16_t>(v));
}
inline void write_u32_le(uint8_t* p, uint32_t v) {
	p[0] = static_cast<uint8_t>(v & 0xFFu);
	p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
	p[2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
	p[3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
}
inline void write_u64_le(uint8_t* p, uint64_t v) {
	write_u32_le(p, static_cast<uint32_t>(v & 0xFFFFFFFFu));
	write_u32_le(p + 4, static_cast<uint32_t>((v >> 32) & 0xFFFFFFFFu));
}
inline void write_f32_le(uint8_t* p, float v) {
	uint32_t bits = 0;
	std::memcpy(&bits, &v, sizeof(bits));
	write_u32_le(p, bits);
}

// Checksums
uint8_t crc8_smbus(const uint8_t* data, size_t len);
uint8_t header_crc8(const uint8_t hdr[HEADER_SIZE]);
uint32_t crc32_update(uint32_t state, const uint8_t* data, size_t len);
inline uint32_t crc32_init() { return 0xFFFFFFFFu; }
inline uint32_t crc32_final(uint32_t state) { return state ^ 0xFFFFFFFFu; }
uint32_t crc32_ieee(const uint8_t* data, size_t len);

// Wire header
struct PacketHeader {
	uint8_t version = PROTO_MAJOR;
	uint8_t msg_type = 0;
	uint8_t flags = 0;
	uint16_t seq = 0;
	uint16_t status_or_sub = 0;
	uint32_t payload_len = 0;
};

void encode_header(uint8_t out[HEADER_SIZE], const PacketHeader& h);
bool decode_header(const uint8_t in[HEADER_SIZE], PacketHeader& h);
size_t encode_packet(uint8_t* out, size_t out_cap, const PacketHeader& h, const uint8_t* payload);

// Incremental streaming parser with 1-byte sliding-window resync
enum class ParseResult : uint8_t {
	NEED_MORE = 0,
	PACKET_OK,
	BAD_CRC,
	PAYLOAD_TOO_LARGE,
};

class Parser {
public:
	Parser(uint8_t* payload_buf, size_t payload_cap);
	void reset();
	ParseResult feed_byte(uint8_t b, uint32_t now_ms = 0);
	bool check_timeout(uint32_t now_ms, uint32_t timeout_ms = RX_PACKET_TIMEOUT_MS);

	const PacketHeader& header() const { return hdr_; }
	const uint8_t* payload() const { return payload_buf_; }
	uint32_t rx_errors() const { return rx_errors_; }
	uint32_t crc_errors() const { return crc_errors_; }
	void clear_error_counters() { rx_errors_ = 0; crc_errors_ = 0; }

private:
	enum class State : uint8_t { FIND_MAGIC, READ_HEADER, READ_PAYLOAD_AND_CRC };
	void resync_from_header();

	uint8_t* payload_buf_;
	size_t payload_cap_;
	State state_ = State::FIND_MAGIC;
	uint8_t hdr_buf_[HEADER_SIZE] = {};
	size_t hdr_pos_ = 0;
	PacketHeader hdr_ = {};
	uint32_t body_pos_ = 0;
	uint32_t crc_state_ = 0xFFFFFFFFu;
	uint8_t crc_buf_[4] = {};
	bool oversized_ = false;
	uint32_t last_rx_ms_ = 0;
	uint32_t rx_errors_ = 0;
	uint32_t crc_errors_ = 0;
};

// Typed payload structs & exact LE codecs
struct HelloReq {
	uint8_t host_proto_major = PROTO_MAJOR;
	uint8_t host_proto_minor = PROTO_MINOR;
	uint16_t reserved = 0;
	uint32_t host_features = 0;
};
struct HelloResp {
	uint8_t proto_major = PROTO_MAJOR;
	uint8_t proto_minor = PROTO_MINOR;
	uint16_t fw_version = 0;
	char fw_build_str[32] = {};
	uint8_t hw_version = 3;
	uint8_t lepton_type = 1;
	uint16_t sensor_w = 160;
	uint16_t sensor_h = 120;
	uint16_t display_w = 320;
	uint16_t display_h = 240;
	uint8_t diagnostic_mask = 0xFF;
	uint8_t reserved = 0;
	uint64_t teensy_uid = 0;
	uint32_t capabilities = 0;
	uint16_t max_chunk_size = DEFAULT_MAX_CHUNK;
	uint16_t heartbeat_timeout_ms = DEFAULT_HEARTBEAT_MS;
};
struct StreamCtrl {
	uint8_t mode = 0;
	uint8_t keep_lcd_on = 0;
	uint8_t include_temp_points = 0;
	uint8_t perf_interval_s = 0;
	uint16_t max_fps_x10 = 0;
	uint16_t reserved = 0;
};
struct FrameRawMeta {
	uint32_t frame_seq = 0;
	uint64_t timestamp_us = 0;
	uint32_t rtc_epoch = 0;
	uint32_t dropped_frames = 0;
	uint16_t width = 160;
	uint16_t height = 120;
	uint8_t pixel_format = 0;
	uint8_t gain_mode = 0;
	uint8_t ffc_state = 0;
	uint8_t flags = 0;
	uint16_t min_raw = 0;
	uint16_t max_raw = 0;
	uint16_t min_x = 0;
	uint16_t min_y = 0;
	uint16_t max_x = 0;
	uint16_t max_y = 0;
	float spot_temp_c = 0.0f;
	float cal_offset = -273.15f;
	float cal_slope = 0.01f;
	float fpa_temp_c = 0.0f;
	int8_t bat_pct = -1;
	uint8_t usb_power = 0;
	uint16_t temp_point_count = 0;
};
struct DeviceConfig {
	uint8_t config_ver = 1;
	uint8_t color_scheme = 13;
	uint8_t text_color = 0;
	uint8_t temp_format = 0;
	uint8_t filter_type = 1;
	uint8_t min_max_points = 0;
	uint8_t rotation_vert = 0;
	uint8_t rotation_horiz = 0;
	uint8_t limits_mode = 0;
	uint8_t shutter_mode = 1;
	uint8_t lepton_gain_mode = 0;
	uint8_t hot_cold_mode = 0;
	uint8_t hot_cold_color = 0;
	uint8_t screen_off_time = 0;
	uint16_t overlay_flags = 0;
	uint16_t manual_min_raw = 0;
	uint16_t manual_max_raw = 0;
	int16_t hot_cold_level_c10 = 0;
	uint8_t min_max_preset = 0;
};
struct PerfTelemetry {
	uint32_t acquire_us = 0;
	uint32_t stats_us = 0;
	uint32_t filter_us = 0;
	uint32_t scale_us = 0;
	uint32_t colorize_us = 0;
	uint32_t overlay_us = 0;
	uint32_t display_us = 0;
	uint32_t sd_write_us = 0;
	uint32_t usb_tx_us = 0;
	uint32_t loop_total_us = 0;
	uint32_t frame_count = 0;
	uint32_t frames_dropped = 0;
	uint32_t vospi_resyncs = 0;
	uint32_t vospi_crc_errors = 0;
	uint32_t usb_rx_errors = 0;
	uint32_t usb_crc_errors = 0;
	uint16_t live_fps_x100 = 0;
	uint16_t video_fps_x100 = 0;
	uint16_t stream_fps_x100 = 0;
	uint8_t perf_mode = 0;
	uint8_t flags = 0;
	uint8_t display_spi_mhz = 25;
	uint8_t lepton_spi_mhz = 20;
	uint16_t cpu_mhz = 450;
	float mcu_temp_c = 0.0f;
	uint32_t frame_period_us = 0;
};
struct DiagnosticsResp {
	uint8_t diagnostic_mask = 0xFF;
	int8_t bat_pct = -1;
	uint8_t usb_power = 0;
	uint8_t sd_fs_type = 0;
	uint16_t bat_mv = 0;
	uint16_t cpu_mhz = 450;
	float mcu_temp_c = 0.0f;
	uint32_t sd_total_kb = 0;
	uint32_t sd_free_kb = 0;
	uint64_t uptime_us = 0;
	uint32_t free_dtcm_bytes = 0;
	uint32_t free_ocram_bytes = 0;
	float lepton_fpa_temp_c = 0.0f;
	char lepton_part_num[32] = {};
};
struct RadParams {
	uint16_t scene_emissivity = 8192;
	uint16_t t_bkg_ck = 29515;
	uint16_t tau_window = 8192;
	uint16_t t_window_ck = 29515;
	uint16_t tau_atm = 8192;
	uint16_t t_atm_ck = 29515;
	uint16_t refl_window = 0;
	uint16_t t_refl_ck = 29515;
	uint8_t flags = 0;
	uint8_t reserved = 0;
};

bool decode_hello_req(const uint8_t* p, size_t len, HelloReq& out);
void encode_hello_resp(uint8_t out[64], const HelloResp& v);
bool decode_stream_ctrl(const uint8_t* p, size_t len, StreamCtrl& out);
void encode_stream_ctrl(uint8_t out[8], const StreamCtrl& v);
void encode_frame_raw_meta(uint8_t out[64], const FrameRawMeta& v);
bool decode_device_config(const uint8_t* p, size_t len, DeviceConfig& out);
void encode_device_config(uint8_t out[32], const DeviceConfig& v);
bool validate_device_config(const DeviceConfig& c);
void encode_perf_telemetry(uint8_t out[84], const PerfTelemetry& v);
void encode_diagnostics(uint8_t out[80], const DiagnosticsResp& v);
bool decode_rad_params(const uint8_t* p, size_t len, RadParams& out);
void encode_rad_params(uint8_t out[18], const RadParams& v);
bool validate_rad_params(const RadParams& r);
StatusCode validate_sd_path(const char* path, size_t len);

} // namespace tc_v2

#endif /* TC_PROTO_V2_H */
