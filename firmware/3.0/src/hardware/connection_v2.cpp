/*
 * CONNECTION_V2 - Complete USB Protocol v2.0 full-duplex server (docs/protocol-v2.md)
 */

#include <Arduino.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include <SdFat.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <hardware.h>
#include <display.h>
#include <gui.h>
#include <lepton.h>
#include <thermal.h>
#include <create.h>
#include <save.h>
#include <sdcard.h>
#include <battery.h>
#include <touchscreen.h>
#include <livemode.h>
#include <perf.h>
#include <tc_proto_v2.h>
#include <tc_thermal_core.h>
#include <connection.h>
#include <connection_v2.h>

extern "C" void _reboot_Teensyduino_(void);

using namespace tc_v2;

namespace {

static constexpr size_t MAX_RX_PAYLOAD = 512;
static constexpr uint32_t TX_STALL_ABORT_MS = 100;
static constexpr uint32_t FRAME_TX_BUDGET_MS = 15;

// Static buffers (zero heap allocation during USB session)
static uint8_t s_rxBuf[MAX_RX_PAYLOAD];
DMAMEM static uint8_t s_sdChunkBuf[DEFAULT_MAX_CHUNK + 8];
static uint16_t s_lepton2RowBuf[80 * 60]; // compact 80x60 TX scratch for Lepton 2.5

struct ActiveFsRead {
	bool active = false;
	FsFile file;
	uint16_t req_seq = 0;
	uint32_t offset = 0;
	uint32_t remaining = 0;
	uint32_t file_size = 0;
	uint16_t chunk_size = DEFAULT_MAX_CHUNK;
};

struct V2Session {
	bool running = false;
	bool headless = false;
	bool hello_done = false;
	uint32_t host_features = 0;
	uint16_t cam_seq = 1;
	uint32_t last_rx_ms = 0;
	uint32_t dropped_since_stream = 0;
	uint32_t last_stream_us = 0;
	uint32_t last_perf_evt_ms = 0;
	uint32_t last_bat_poll_ms = 0;
	int8_t   last_bat_pct = -128;
	uint8_t  last_usb_pwr = 255;
	uint8_t  ffc_state = 0;
	bool     btn_down = false;
	uint32_t btn_down_ms = 0;
	bool     touch_down = false;
	uint32_t touch_down_ms = 0;
	uint16_t touch_x = 0;
	uint16_t touch_y = 0;
	StreamCtrl stream = {};
	ActiveFsRead fs_read = {};
};

static V2Session s_v2;
static Parser s_parser(s_rxBuf, sizeof(s_rxBuf));

static bool write_all_bounded(const uint8_t* data, size_t len, uint32_t max_ms) {
	uint32_t start = millis();
	size_t off = 0;
	while (off < len) {
		if (!Serial) return false;
		int avail = Serial.availableForWrite();
		if (avail > 0) {
			size_t chunk = (static_cast<size_t>(avail) < (len - off))
				? static_cast<size_t>(avail) : (len - off);
			size_t written = Serial.write(data + off, chunk);
			off += written;
		} else {
			if ((millis() - start) >= max_ms) return false;
			yield();
		}
	}
	return true;
}

static bool tx_packet(uint8_t msg_type, uint8_t flags, uint16_t seq, uint16_t status_or_sub,
                      const uint8_t* payload, uint32_t payload_len) {
	PacketHeader h{PROTO_MAJOR, msg_type, flags, seq, status_or_sub, payload_len};
	uint8_t hdr[HEADER_SIZE];
	encode_header(hdr, h);
	uint32_t crc = crc32_update(crc32_init(), hdr, HEADER_SIZE);
	if (payload_len > 0 && payload) {
		crc = crc32_update(crc, payload, payload_len);
	}
	uint8_t trailer[4];
	write_u32_le(trailer, crc32_final(crc));

	if (!write_all_bounded(hdr, HEADER_SIZE, TX_STALL_ABORT_MS)) return false;
	if (payload_len > 0 && payload) {
		if (!write_all_bounded(payload, payload_len, TX_STALL_ABORT_MS)) return false;
	}
	if (!write_all_bounded(trailer, 4, TX_STALL_ABORT_MS)) return false;
	Serial.send_now();
	return true;
}

static void tx_error(uint8_t resp_type, uint16_t seq, StatusCode code, const char* msg = nullptr) {
	uint32_t len = msg ? static_cast<uint32_t>(strlen(msg)) : 0u;
	if (len > 128u) len = 128u;
	tx_packet(resp_type, FLAG_ERROR, seq, static_cast<uint16_t>(code),
	          reinterpret_cast<const uint8_t*>(msg), len);
}

static void emit_event(uint8_t evt_type, const uint8_t* payload, uint32_t len) {
	tx_packet(evt_type, 0, s_v2.cam_seq++, 0, payload, len);
}

static void emit_ffc_event(uint8_t state, uint16_t duration_ms) {
	uint8_t p[4] = {state, 0, 0, 0};
	write_u16_le(p + 2, duration_ms);
	emit_event(MSG_EVT_FFC, p, 4);
}

static void emit_session_exit(uint8_t reason) {
	if (!Serial) return;
	uint8_t p[2] = {reason, 0};
	emit_event(MSG_EVT_SESSION_EXIT, p, 2);
}

static void emit_perf_event() {
	PerfTelemetry pt{};
	perf_fill_telemetry(pt, false, s_parser.rx_errors(), s_parser.crc_errors());
	uint8_t buf[84];
	encode_perf_telemetry(buf, pt);
	emit_event(MSG_EVT_PERF, buf, 84);
}

static void populate_device_config(DeviceConfig& c) {
	c.config_ver = 1;
	c.color_scheme = colorScheme;
	c.text_color = textColor;
	c.temp_format = tempFormat ? 1 : 0;
	c.filter_type = filterType;
	c.min_max_points = minMaxPoints;
	c.rotation_vert = rotationVert ? 1 : 0;
	c.rotation_horiz = rotationHorizont ? 1 : 0;
	c.limits_mode = autoMode ? (limitsLocked ? 1 : 0) : 2;
	c.shutter_mode = shutterModeAuto ? 1 : 0;
	c.lepton_gain_mode = leptonGainMode ? 1 : 0;
	c.hot_cold_mode = hotColdMode;
	c.hot_cold_color = hotColdColor;
	c.screen_off_time = screenOffTime;
	c.overlay_flags =
		(spotEnabled ? 0x01u : 0u) |
		(colorbarEnabled ? 0x02u : 0u) |
		(batteryEnabled ? 0x04u : 0u) |
		(timeEnabled ? 0x08u : 0u) |
		(dateEnabled ? 0x10u : 0u) |
		(storageEnabled ? 0x20u : 0u) |
		(convertEnabled ? 0x40u : 0u);
	c.manual_min_raw = minValue;
	c.manual_max_raw = maxValue;
	c.hot_cold_level_c10 = static_cast<int16_t>(hotColdLevel * 10);
	c.min_max_preset = minMaxPresetCache;
}

static void apply_device_config(const DeviceConfig& c, bool persist) {
	bool rotChanged = (rotationVert != (c.rotation_vert != 0));
	bool gainChanged = (leptonGainMode != (c.lepton_gain_mode != 0));

	colorScheme = c.color_scheme;
	selectColorScheme();
	textColor = c.text_color;
	tempFormat = (c.temp_format != 0);
	filterType = c.filter_type;
	minMaxPoints = c.min_max_points;
	rotationVert = (c.rotation_vert != 0);
	rotationHorizont = (c.rotation_horiz != 0);
	if (rotChanged && !s_v2.headless) setDisplayRotation();

	if (c.limits_mode == 0) {
		autoMode = true; limitsLocked = false;
	} else if (c.limits_mode == 1) {
		autoMode = true; limitsLocked = true;
	} else {
		autoMode = false; limitsLocked = false;
		minValue = c.manual_min_raw;
		maxValue = c.manual_max_raw;
	}

	hotColdMode = c.hot_cold_mode;
	hotColdColor = c.hot_cold_color;
	hotColdLevel = static_cast<int16_t>(c.hot_cold_level_c10 / 10);
	screenOffTime = c.screen_off_time;
	minMaxPresetCache = c.min_max_preset;

	spotEnabled     = (c.overlay_flags & 0x01u) != 0;
	colorbarEnabled = (c.overlay_flags & 0x02u) != 0;
	batteryEnabled  = (c.overlay_flags & 0x04u) != 0;
	timeEnabled     = (c.overlay_flags & 0x08u) != 0;
	dateEnabled     = (c.overlay_flags & 0x10u) != 0;
	storageEnabled  = (c.overlay_flags & 0x20u) != 0;
	convertEnabled  = (c.overlay_flags & 0x40u) != 0;

	if (gainChanged) {
		if (c.lepton_gain_mode == lepton_gain_low) lepton_setLowGain();
		else lepton_setHighGain();
	}
	lepton_ffcMode(c.shutter_mode != 0);

	if (persist) {
		EEPROM.write(eeprom_colorScheme, colorScheme);
		EEPROM.write(eeprom_textColor, textColor);
		EEPROM.write(eeprom_tempFormat, tempFormat);
		EEPROM.write(eeprom_filterType, filterType);
		EEPROM.write(eeprom_minMaxPoints, minMaxPoints);
		EEPROM.write(eeprom_rotationVert, rotationVert);
		EEPROM.write(eeprom_rotationHorizont, rotationHorizont);
		EEPROM.write(eeprom_lepton_gain, leptonGainMode);
		EEPROM.write(eeprom_hotColdMode, hotColdMode);
		EEPROM.write(eeprom_hotColdColor, hotColdColor);
		EEPROM.write(eeprom_hotColdLevelHigh, (hotColdLevel >> 8) & 0xFF);
		EEPROM.write(eeprom_hotColdLevelLow, hotColdLevel & 0xFF);
		EEPROM.write(eeprom_screenOffTime, screenOffTime);
		EEPROM.write(eeprom_spotEnabled, spotEnabled);
		EEPROM.write(eeprom_colorbarEnabled, colorbarEnabled);
		EEPROM.write(eeprom_batteryEnabled, batteryEnabled);
		EEPROM.write(eeprom_timeEnabled, timeEnabled);
		EEPROM.write(eeprom_dateEnabled, dateEnabled);
		EEPROM.write(eeprom_storageEnabled, storageEnabled);
		EEPROM.write(eeprom_convertEnabled, convertEnabled);
		EEPROM.write(eeprom_minMaxPreset, minMaxPresetCache);
	}
}

/*######################## ZERO-COPY STREAM PUSH #############################*/

static void tx_frame_raw(uint32_t frame_seq, uint64_t ts_us, float spot_c) {
	if (!Serial || Serial.availableForWrite() < 64) {
		s_v2.dropped_since_stream++;
		g_perf.frames_dropped++;
		return;
	}
	uint32_t t0 = micros();
	const bool is_l3 = (leptonVersion == leptonVersion_3_5_shutter);
	const uint16_t w = is_l3 ? 160u : 80u;
	const uint16_t h = is_l3 ? 120u : 60u;
	const uint32_t pix_bytes = static_cast<uint32_t>(w) * static_cast<uint32_t>(h) * 2u;

	const uint8_t* pix_ptr = nullptr;
	if (is_l3) {
		pix_ptr = reinterpret_cast<const uint8_t*>(smallBuffer);
	} else {
		for (int y = 0; y < 60; ++y) {
			for (int x = 0; x < 80; ++x) {
				s_lepton2RowBuf[y * 80 + x] = smallBuffer[(y * 2 * 160) + (x * 2)];
			}
		}
		pix_ptr = reinterpret_cast<const uint8_t*>(s_lepton2RowBuf);
	}

	uint8_t pts_buf[96 * 6];
	uint16_t pt_count = 0;
	if (s_v2.stream.include_temp_points) {
		for (int i = 0; i < 96; ++i) {
			uint16_t idx = tempPoints[i][0];
			if (idx != 0 && idx <= 19200u) {
				uint16_t z = idx - 1u;
				uint16_t px = z % 160u;
				uint16_t py = z / 160u;
				if (!is_l3) { px /= 2u; py /= 2u; }
				write_u16_le(pts_buf + pt_count * 6 + 0, px);
				write_u16_le(pts_buf + pt_count * 6 + 2, py);
				write_u16_le(pts_buf + pt_count * 6 + 4, tempPoints[i][1]);
				pt_count++;
			}
		}
	}

	FrameRawMeta m{};
	m.frame_seq = frame_seq;
	m.timestamp_us = ts_us;
	m.rtc_epoch = static_cast<uint32_t>(now());
	m.dropped_frames = s_v2.dropped_since_stream;
	m.width = w;
	m.height = h;
	m.pixel_format = 0;
	m.gain_mode = leptonGainMode ? 1 : 0;
	m.ffc_state = s_v2.ffc_state;
	if (s_v2.ffc_state == 2) s_v2.ffc_state = 0;
	m.flags =
		(limitsLocked ? 0x01u : 0u) |
		(autoMode ? 0x02u : 0u) |
		(s_v2.stream.include_temp_points ? 0x04u : 0u) |
		(rotationVert ? 0x08u : 0u) |
		(rotationHorizont ? 0x10u : 0u);
	m.min_raw = minValue;
	m.max_raw = maxValue;
	m.min_x = is_l3 ? (minTempPos % 160u) : ((minTempPos % 160u) / 2u);
	m.min_y = is_l3 ? (minTempPos / 160u) : ((minTempPos / 160u) / 2u);
	m.max_x = is_l3 ? (maxTempPos % 160u) : ((maxTempPos % 160u) / 2u);
	m.max_y = is_l3 ? (maxTempPos / 160u) : ((maxTempPos / 160u) / 2u);
	m.spot_temp_c = spot_c;
	m.cal_offset = -273.15f;
	m.cal_slope = leptonCalSlope;
	m.fpa_temp_c = leptonFpaTempC;
	m.bat_pct = batPercentage;
	m.usb_power = isUSBConnected() ? 1 : 0;
	m.temp_point_count = pt_count;

	const uint32_t pts_bytes = static_cast<uint32_t>(pt_count) * 6u;
	const uint32_t payload_len = 64u + pix_bytes + pts_bytes;

	uint8_t hdr[HEADER_SIZE];
	uint8_t meta[64];
	PacketHeader ph{PROTO_MAJOR, MSG_FRAME_RAW, 0, s_v2.cam_seq++, 0, payload_len};
	encode_header(hdr, ph);
	encode_frame_raw_meta(meta, m);

	uint32_t crc = crc32_init();
	crc = crc32_update(crc, hdr, HEADER_SIZE);
	crc = crc32_update(crc, meta, 64);
	crc = crc32_update(crc, pix_ptr, pix_bytes);
	if (pts_bytes > 0) crc = crc32_update(crc, pts_buf, pts_bytes);
	uint8_t trailer[4];
	write_u32_le(trailer, crc32_final(crc));

	bool ok = write_all_bounded(hdr, HEADER_SIZE, FRAME_TX_BUDGET_MS) &&
	          write_all_bounded(meta, 64, FRAME_TX_BUDGET_MS) &&
	          write_all_bounded(pix_ptr, pix_bytes, TX_STALL_ABORT_MS) &&
	          (pts_bytes == 0 || write_all_bounded(pts_buf, pts_bytes, FRAME_TX_BUDGET_MS)) &&
	          write_all_bounded(trailer, 4, FRAME_TX_BUDGET_MS);
	if (ok) {
		Serial.send_now();
		g_perf.usb_tx_us = micros() - t0;
	} else {
		s_v2.stream.mode = 0; // stalled mid-packet -> stop stream cleanly
	}
}

static void tx_frame_rendered(uint32_t frame_seq, uint64_t ts_us) {
	if (!Serial || Serial.availableForWrite() < 64) {
		s_v2.dropped_since_stream++;
		g_perf.frames_dropped++;
		return;
	}
	uint32_t t0 = micros();
	constexpr uint32_t pix_bytes = 320u * 240u * 2u;
	constexpr uint32_t payload_len = 16u + pix_bytes;

	uint8_t hdr[HEADER_SIZE];
	uint8_t meta[16];
	PacketHeader ph{PROTO_MAJOR, MSG_FRAME_RENDERED, 0, s_v2.cam_seq++, 0, payload_len};
	encode_header(hdr, ph);
	write_u32_le(meta + 0, frame_seq);
	write_u64_le(meta + 4, ts_us);
	write_u16_le(meta + 12, 320);
	write_u16_le(meta + 14, 240);

	const uint8_t* pix_ptr = reinterpret_cast<const uint8_t*>(bigBuffer);
	uint32_t crc = crc32_init();
	crc = crc32_update(crc, hdr, HEADER_SIZE);
	crc = crc32_update(crc, meta, 16);
	crc = crc32_update(crc, pix_ptr, pix_bytes);
	uint8_t trailer[4];
	write_u32_le(trailer, crc32_final(crc));

	bool ok = write_all_bounded(hdr, HEADER_SIZE, FRAME_TX_BUDGET_MS) &&
	          write_all_bounded(meta, 16, FRAME_TX_BUDGET_MS) &&
	          write_all_bounded(pix_ptr, pix_bytes, TX_STALL_ABORT_MS) &&
	          write_all_bounded(trailer, 4, FRAME_TX_BUDGET_MS);
	if (ok) {
		Serial.send_now();
		g_perf.usb_tx_us = micros() - t0;
	} else {
		s_v2.stream.mode = 0;
	}
}

/*######################## CONTROL COMMAND HANDLERS ##########################*/

static void handle_hello(uint16_t seq, const uint8_t* p, uint32_t len) {
	HelloReq req{};
	if (!decode_hello_req(p, len, req)) {
		tx_error(MSG_HELLO_RESP, seq, ERR_INVALID_ARG, "HELLO_REQ must be 8 bytes");
		return;
	}
	s_v2.hello_done = true;
	s_v2.host_features = req.host_features;
	if (!s_v2.headless) {
		if (req.host_features & 0x01u) enableScreenLight();
		else disableScreenLight();
	}

	HelloResp resp{};
	resp.proto_major = PROTO_MAJOR;
	resp.proto_minor = PROTO_MINOR;
	resp.fw_version = fwVersion;
	memset(resp.fw_build_str, 0, sizeof(resp.fw_build_str));
	strncpy(resp.fw_build_str, versionString, sizeof(resp.fw_build_str) - 1);
	resp.hw_version = 3;
	resp.lepton_type = checkDiagnostic(diag_lepton) ? leptonVersion : 255;
	resp.sensor_w = (leptonVersion == leptonVersion_2_5_shutter) ? 80 : 160;
	resp.sensor_h = (leptonVersion == leptonVersion_2_5_shutter) ? 60 : 120;
	resp.display_w = checkDiagnostic(diag_display) ? 320 : 0;
	resp.display_h = checkDiagnostic(diag_display) ? 240 : 0;
	resp.diagnostic_mask = diagnostic;
	resp.teensy_uid = perf_teensy_uid();
	resp.capabilities =
		CAP_RAW_STREAM | CAP_DISPLAY_STREAM | CAP_PERF_TELEMETRY |
		CAP_BOOTLOADER | CAP_GAIN_MODE | CAP_HOT_COLD | CAP_RTC_SYNC |
		(checkDiagnostic(diag_sd) ? CAP_SD_FS : 0u) |
		(checkDiagnostic(diag_lepton) ? CAP_LEPTON_RAD_PARAMS : 0u) |
		(checkDiagnostic(diag_display) ? CAP_KEEP_LCD_ON : 0u);
	resp.max_chunk_size = DEFAULT_MAX_CHUNK;
	resp.heartbeat_timeout_ms = DEFAULT_HEARTBEAT_MS;

	uint8_t out[64];
	encode_hello_resp(out, resp);
	tx_packet(MSG_HELLO_RESP, 0, seq, OK, out, 64);
}

static void handle_stream_ctrl(uint16_t seq, const uint8_t* p, uint32_t len) {
	StreamCtrl sc{};
	if (!decode_stream_ctrl(p, len, sc) || sc.mode > 3 || sc.keep_lcd_on > 1 ||
	    sc.include_temp_points > 1 || sc.perf_interval_s > 60) {
		tx_error(MSG_STREAM_CTRL_RESP, seq, ERR_INVALID_ARG);
		return;
	}
	if (s_v2.stream.mode == 0 && sc.mode != 0) {
		s_v2.dropped_since_stream = 0;
		s_v2.last_stream_us = micros();
	}
	if (sc.mode == 0) g_perf.stream_fps_x100 = 0;
	s_v2.stream = sc;
	if (!s_v2.headless) {
		if (sc.keep_lcd_on) enableScreenLight();
		else disableScreenLight();
	}
	uint8_t out[8];
	encode_stream_ctrl(out, s_v2.stream);
	tx_packet(MSG_STREAM_CTRL_RESP, 0, seq, OK, out, 8);
}

static void handle_save_capture(uint16_t seq, const uint8_t* p, uint32_t len) {
	if (len != 4) { tx_error(MSG_SAVE_CAPTURE_RESP, seq, ERR_INVALID_ARG); return; }
	if (!checkDiagnostic(diag_sd) || getSDSpace() < 1000) {
		tx_error(MSG_SAVE_CAPTURE_RESP, seq, ERR_NO_SD);
		return;
	}
	uint8_t flags = p[0];
	bool want_bmp = (flags & 0x02u) ? ((flags & 0x01u) != 0) : convertEnabled;
	if (flags & 0x04u) {
		lepton_acquireFrame(true);
		getSpotTemp();
		refreshTempPoints();
		if (autoMode && !limitsLocked) limitValues();
		else refreshMinMax();
	}

	char stem[20] = {};
	createSDName(stem);
	char nameBuf[24] = {};
	memcpy(nameBuf, stem, 14);
	saveRawData(true, nameBuf);

	uint32_t dat_size = 0, bmp_size = 0;
	if (sdFile.open(nameBuf, O_RDONLY)) {
		dat_size = static_cast<uint32_t>(sdFile.fileSize());
		sdFile.close();
	}

	if (want_bmp) {
		// Render into bigBuffer without mutating raw smallBuffer if filter is on
		uint16_t* savedSmall = smallBuffer;
		static uint16_t tmpSmall[19200];
		memcpy(tmpSmall, smallBuffer, sizeof(tmpSmall));
		smallBuffer = tmpSmall;
		if (filterType == filterType_box) boxFilter();
		else if (filterType == filterType_gaussian) gaussianFilter();
		smallToBigBuffer();
		convertColors(false);
		smallBuffer = savedSmall;
		imgSave = imgSave_create;
		displayInfos();
		imgSave = imgSave_disabled;
		memcpy(nameBuf, stem, 14);
		saveBuffer(nameBuf);
		if (sdFile.open(nameBuf, O_RDONLY)) {
			bmp_size = static_cast<uint32_t>(sdFile.fileSize());
			sdFile.close();
		}
	}

	uint8_t out[40] = {};
	write_u32_le(out + 0, dat_size);
	write_u32_le(out + 4, bmp_size);
	memcpy(out + 8, stem, 14);
	tx_packet(MSG_SAVE_CAPTURE_RESP, 0, seq, OK, out, 40);
}

/*######################## SD FILESYSTEM (0xE0..0xE5) ########################*/

static uint32_t fat_datetime_to_epoch(uint16_t fdate, uint16_t ftime) {
	if (fdate == 0) return 0;
	tmElements_t tm = {};
	tm.Year   = static_cast<uint8_t>(((fdate >> 9) & 0x7Fu) + 10u); // FAT 1980 -> 1970
	tm.Month  = static_cast<uint8_t>((fdate >> 5) & 0x0Fu);
	tm.Day    = static_cast<uint8_t>(fdate & 0x1Fu);
	tm.Hour   = static_cast<uint8_t>((ftime >> 11) & 0x1Fu);
	tm.Minute = static_cast<uint8_t>((ftime >> 5) & 0x3Fu);
	tm.Second = static_cast<uint8_t>((ftime & 0x1Fu) * 2u);
	return static_cast<uint32_t>(makeTime(tm));
}

static StatusCode parse_path_arg(const uint8_t* p, uint32_t len, uint32_t path_len_off,
                                 char out_path[132]) {
	if (len < path_len_off + 2u) return ERR_INVALID_ARG;
	uint16_t plen = read_u16_le(p + path_len_off);
	if (len != path_len_off + 2u + plen) return ERR_INVALID_ARG;
	StatusCode st = validate_sd_path(reinterpret_cast<const char*>(p + path_len_off + 2u), plen);
	if (st != OK) return st;
	memcpy(out_path, p + path_len_off + 2u, plen);
	out_path[plen] = '\0';
	return OK;
}

static void abort_active_fs_read() {
	if (s_v2.fs_read.active) {
		s_v2.fs_read.file.close();
		s_v2.fs_read.active = false;
	}
}

static void handle_fs_info(uint16_t seq) {
	uint8_t out[16] = {};
	bool present = checkDiagnostic(diag_sd) && (getSDFsType() != 0);
	out[0] = present ? 1 : 0;
	out[1] = getSDFsType();
	uint32_t root_count = 0;
	if (present) {
		FsFile root;
		if (root.open("/")) {
			FsFile entry;
			while (entry.openNext(&root, O_RDONLY)) {
				root_count++;
				entry.close();
			}
			root.close();
		}
		write_u32_le(out + 4, getSDTotalKB());
		write_u32_le(out + 8, getSDFreeKB());
		write_u32_le(out + 12, root_count);
	}
	tx_packet(MSG_FS_INFO_RESP, 0, seq, OK, out, 16);
}

static void handle_fs_list(uint16_t seq, const uint8_t* p, uint32_t len) {
	if (!checkDiagnostic(diag_sd)) { tx_error(MSG_FS_LIST_RESP, seq, ERR_NO_SD); return; }
	if (len < 8) { tx_error(MSG_FS_LIST_RESP, seq, ERR_INVALID_ARG); return; }
	uint32_t cursor = read_u32_le(p + 0);
	uint16_t max_entries = read_u16_le(p + 4);
	if (max_entries == 0 || max_entries > 64) { tx_error(MSG_FS_LIST_RESP, seq, ERR_INVALID_ARG); return; }
	char path[132];
	StatusCode st = parse_path_arg(p, len, 6, path);
	if (st != OK) { tx_error(MSG_FS_LIST_RESP, seq, st); return; }

	FsFile d;
	if (!d.open(path) || !d.isDir()) {
		if (d.isOpen()) d.close();
		tx_error(MSG_FS_LIST_RESP, seq, ERR_NOT_FOUND);
		return;
	}

	uint8_t* out = s_sdChunkBuf;
	size_t out_pos = 8;
	uint16_t count = 0;
	uint32_t idx = 0;
	bool has_more = false;

	FsFile entry;
	char nameBuf[256];
	while (entry.openNext(&d, O_RDONLY)) {
		if (idx < cursor) {
			idx++;
			entry.close();
			continue;
		}
		if (count >= max_entries) {
			has_more = true;
			entry.close();
			break;
		}
		nameBuf[0] = '\0';
		entry.getName(nameBuf, sizeof(nameBuf));
		size_t nlen = strlen(nameBuf);
		if (nlen == 0 || nlen > 255) { entry.close(); idx++; continue; }
		if (out_pos + 10u + nlen > sizeof(s_sdChunkBuf)) {
			has_more = true;
			entry.close();
			break;
		}
		bool is_dir = entry.isDir();
		uint32_t fsz = is_dir ? 0u : static_cast<uint32_t>(entry.fileSize());
		uint16_t fd = 0, ft = 0;
		entry.getModifyDateTime(&fd, &ft);
		write_u32_le(out + out_pos + 0, fsz);
		write_u32_le(out + out_pos + 4, fat_datetime_to_epoch(fd, ft));
		out[out_pos + 8] = is_dir ? 1 : 0;
		out[out_pos + 9] = static_cast<uint8_t>(nlen);
		memcpy(out + out_pos + 10, nameBuf, nlen);
		out_pos += 10u + nlen;
		count++;
		idx++;
		entry.close();
	}
	d.close();

	write_u32_le(out + 0, has_more ? idx : 0xFFFFFFFFu);
	write_u16_le(out + 4, count);
	write_u16_le(out + 6, 0);
	tx_packet(MSG_FS_LIST_RESP, 0, seq, OK, out, static_cast<uint32_t>(out_pos));
}

static void handle_fs_stat(uint16_t seq, const uint8_t* p, uint32_t len) {
	if (!checkDiagnostic(diag_sd)) { tx_error(MSG_FS_STAT_RESP, seq, ERR_NO_SD); return; }
	if (len < 4) { tx_error(MSG_FS_STAT_RESP, seq, ERR_INVALID_ARG); return; }
	uint8_t flags = p[0];
	char path[132];
	StatusCode st = parse_path_arg(p, len, 2, path);
	if (st != OK) { tx_error(MSG_FS_STAT_RESP, seq, st); return; }

	FsFile f;
	if (!f.open(path, O_RDONLY)) { tx_error(MSG_FS_STAT_RESP, seq, ERR_NOT_FOUND); return; }
	bool is_dir = f.isDir();
	uint32_t fsz = is_dir ? 0u : static_cast<uint32_t>(f.fileSize());
	uint16_t fd = 0, ft = 0;
	f.getModifyDateTime(&fd, &ft);
	uint32_t crc_val = 0;
	uint8_t crc_valid = 0;
	if (!is_dir && (flags & 0x01u)) {
		uint32_t c = crc32_init();
		int r;
		while ((r = f.read(s_sdChunkBuf, DEFAULT_MAX_CHUNK)) > 0) {
			c = crc32_update(c, s_sdChunkBuf, static_cast<size_t>(r));
		}
		crc_val = crc32_final(c);
		crc_valid = 1;
	}
	f.close();

	uint8_t out[16] = {};
	write_u32_le(out + 0, fsz);
	write_u32_le(out + 4, fat_datetime_to_epoch(fd, ft));
	write_u32_le(out + 8, crc_val);
	out[12] = is_dir ? 1 : 0;
	out[13] = crc_valid;
	tx_packet(MSG_FS_STAT_RESP, 0, seq, OK, out, 16);
}

static void handle_fs_read_start(uint16_t seq, const uint8_t* p, uint32_t len) {
	if (!checkDiagnostic(diag_sd)) { tx_error(MSG_FS_READ_DATA, seq, ERR_NO_SD); return; }
	if (s_v2.fs_read.active) { tx_error(MSG_FS_READ_DATA, seq, ERR_BUSY); return; }
	if (len < 12) { tx_error(MSG_FS_READ_DATA, seq, ERR_INVALID_ARG); return; }
	uint32_t offset = read_u32_le(p + 0);
	uint32_t req_len = read_u32_le(p + 4);
	uint16_t chunk = read_u16_le(p + 8);
	if (chunk == 0) chunk = DEFAULT_MAX_CHUNK;
	if (chunk < 512) chunk = 512;
	if (chunk > DEFAULT_MAX_CHUNK) chunk = DEFAULT_MAX_CHUNK;
	char path[132];
	StatusCode st = parse_path_arg(p, len, 10, path);
	if (st != OK) { tx_error(MSG_FS_READ_DATA, seq, st); return; }

	if (!s_v2.fs_read.file.open(path, O_RDONLY) || s_v2.fs_read.file.isDir()) {
		if (s_v2.fs_read.file.isOpen()) s_v2.fs_read.file.close();
		tx_error(MSG_FS_READ_DATA, seq, ERR_NOT_FOUND);
		return;
	}
	uint32_t fsz = static_cast<uint32_t>(s_v2.fs_read.file.fileSize());
	if (offset > fsz) {
		s_v2.fs_read.file.close();
		tx_error(MSG_FS_READ_DATA, seq, ERR_INVALID_ARG);
		return;
	}
	s_v2.fs_read.file.seekSet(offset);
	uint32_t avail = fsz - offset;
	uint32_t to_send = (req_len == 0 || req_len > avail) ? avail : req_len;

	s_v2.fs_read.active = true;
	s_v2.fs_read.req_seq = seq;
	s_v2.fs_read.offset = offset;
	s_v2.fs_read.remaining = to_send;
	s_v2.fs_read.file_size = fsz;
	s_v2.fs_read.chunk_size = chunk;
}

static void pump_fs_read() {
	if (!s_v2.fs_read.active) return;
	uint32_t want = (s_v2.fs_read.remaining < s_v2.fs_read.chunk_size)
		? s_v2.fs_read.remaining : s_v2.fs_read.chunk_size;
	int got = (want > 0) ? s_v2.fs_read.file.read(s_sdChunkBuf + 8, want) : 0;
	if (got < 0) {
		uint16_t seq = s_v2.fs_read.req_seq;
		abort_active_fs_read();
		tx_error(MSG_FS_READ_DATA, seq, ERR_IO);
		return;
	}
	uint32_t cur_off = s_v2.fs_read.offset;
	s_v2.fs_read.offset += static_cast<uint32_t>(got);
	s_v2.fs_read.remaining -= static_cast<uint32_t>(got);
	bool eof = (s_v2.fs_read.offset >= s_v2.fs_read.file_size);
	bool done = (s_v2.fs_read.remaining == 0) || (want > 0 && got == 0);

	write_u32_le(s_sdChunkBuf + 0, cur_off);
	write_u16_le(s_sdChunkBuf + 4, static_cast<uint16_t>(got));
	s_sdChunkBuf[6] = eof ? 1 : 0;
	s_sdChunkBuf[7] = 0;

	uint8_t flags = done ? 0u : FLAG_MORE_CHUNKS;
	uint16_t seq = s_v2.fs_read.req_seq;
	if (done) abort_active_fs_read();
	tx_packet(MSG_FS_READ_DATA, flags, seq, OK, s_sdChunkBuf, static_cast<uint32_t>(8 + got));
}

static uint32_t rm_rf(const char* path) {
	uint32_t deleted = 0;
	FsFile d;
	if (!d.open(path)) return 0;
	FsFile e;
	char name[128];
	char child[260];
	while (e.openNext(&d, O_RDONLY)) {
		name[0] = '\0';
		e.getName(name, sizeof(name));
		bool is_dir = e.isDir();
		e.close();
		if (name[0] == '\0') continue;
		snprintf(child, sizeof(child), "%s/%s", path, name);
		if (is_dir) deleted += rm_rf(child);
		else if (sd.remove(child)) deleted++;
	}
	d.close();
	if (sd.rmdir(path)) deleted++;
	return deleted;
}

static void handle_fs_delete(uint16_t seq, const uint8_t* p, uint32_t len) {
	if (!checkDiagnostic(diag_sd)) { tx_error(MSG_FS_DELETE_RESP, seq, ERR_NO_SD); return; }
	if (len < 4) { tx_error(MSG_FS_DELETE_RESP, seq, ERR_INVALID_ARG); return; }
	uint8_t flags = p[0];
	char path[132];
	StatusCode st = parse_path_arg(p, len, 2, path);
	if (st != OK) { tx_error(MSG_FS_DELETE_RESP, seq, st); return; }
	if (strcmp(path, "/") == 0) { tx_error(MSG_FS_DELETE_RESP, seq, ERR_FORBIDDEN); return; }

	FsFile f;
	if (!f.open(path, O_RDONLY)) { tx_error(MSG_FS_DELETE_RESP, seq, ERR_NOT_FOUND); return; }
	bool is_dir = f.isDir();
	f.close();

	uint32_t deleted = 0;
	if (is_dir) {
		if (flags & 0x01u) {
			deleted = rm_rf(path);
		} else {
			if (!sd.rmdir(path)) { tx_error(MSG_FS_DELETE_RESP, seq, ERR_FORBIDDEN); return; }
			deleted = 1;
		}
	} else {
		if (!sd.remove(path)) { tx_error(MSG_FS_DELETE_RESP, seq, ERR_IO); return; }
		deleted = 1;
	}
	refreshFreeSpace();
	uint8_t out[4];
	write_u32_le(out, deleted);
	tx_packet(MSG_FS_DELETE_RESP, 0, seq, OK, out, 4);
}

/*######################## CONTROL COMMAND DISPATCH ##########################*/
/*######################## PACKET DISPATCHER #################################*/

static void dispatch_packet(const PacketHeader& h, const uint8_t* p) {
	if (h.flags & FLAG_COMPRESSED) {
		tx_error(MSG_GENERIC_ERROR_RESP, h.seq, ERR_UNSUPPORTED, "compression unsupported");
		return;
	}

	switch (h.msg_type) {
	case MSG_HELLO_REQ:
		handle_hello(h.seq, p, h.payload_len);
		break;

	case MSG_GOODBYE_REQ:
		if (h.payload_len != 0) { tx_error(MSG_GOODBYE_RESP, h.seq, ERR_INVALID_ARG); break; }
		abort_active_fs_read();
		s_v2.stream.mode = 0;
		tx_packet(MSG_GOODBYE_RESP, 0, h.seq, OK, nullptr, 0);
		Serial.flush();
		s_v2.running = false;
		break;

	case MSG_PING_REQ:
		if (h.payload_len != 8) { tx_error(MSG_PONG_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			uint8_t out[16];
			memcpy(out, p, 8);
			write_u64_le(out + 8, perf_uptime_us());
			tx_packet(MSG_PONG_RESP, 0, h.seq, OK, out, 16);
		}
		break;

	case MSG_STREAM_CTRL_REQ:
		handle_stream_ctrl(h.seq, p, h.payload_len);
		break;

	case MSG_GET_CONFIG_REQ:
		if (h.payload_len != 0) { tx_error(MSG_GET_CONFIG_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			DeviceConfig c{};
			populate_device_config(c);
			uint8_t out[32];
			encode_device_config(out, c);
			tx_packet(MSG_GET_CONFIG_RESP, 0, h.seq, OK, out, 32);
		}
		break;

	case MSG_SET_CONFIG_REQ:
		if (h.payload_len != 36) { tx_error(MSG_SET_CONFIG_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			uint8_t persist = p[0];
			DeviceConfig c{};
			if (persist > 1 || !decode_device_config(p + 4, 32, c) || !validate_device_config(c)) {
				tx_error(MSG_SET_CONFIG_RESP, h.seq, ERR_INVALID_ARG);
				break;
			}
			apply_device_config(c, persist != 0);
			DeviceConfig active{};
			populate_device_config(active);
			uint8_t out[32];
			encode_device_config(out, active);
			tx_packet(MSG_SET_CONFIG_RESP, 0, h.seq, OK, out, 32);
		}
		break;

	case MSG_SET_TIME_REQ:
		if (h.payload_len != 4) { tx_error(MSG_SET_TIME_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			uint32_t epoch = read_u32_le(p);
			setTime(static_cast<time_t>(epoch));
			Teensy3Clock.set(static_cast<time_t>(epoch));
			uint8_t out[4];
			write_u32_le(out, epoch);
			tx_packet(MSG_SET_TIME_RESP, 0, h.seq, OK, out, 4);
		}
		break;

	case MSG_TRIGGER_FFC_REQ:
		if (h.payload_len != 1) { tx_error(MSG_TRIGGER_FFC_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			bool force = (p[0] & 0x01u) != 0;
			if (!autoMode && !force) { tx_error(MSG_TRIGGER_FFC_RESP, h.seq, ERR_FORBIDDEN); break; }
			s_v2.ffc_state = 1;
			emit_ffc_event(0, 0);
			uint32_t t0 = millis();
			bool err = lepton_ffc(false, force);
			uint16_t dur = static_cast<uint16_t>(millis() - t0);
			if (err) {
				s_v2.ffc_state = 0;
				emit_ffc_event(2, dur);
				tx_error(MSG_TRIGGER_FFC_RESP, h.seq, ERR_IO);
			} else {
				s_v2.ffc_state = 2;
				emit_ffc_event(1, dur);
				uint8_t out[2] = {0, 0};
				tx_packet(MSG_TRIGGER_FFC_RESP, 0, h.seq, OK, out, 2);
			}
		}
		break;

	case MSG_GET_TEMP_POINTS_REQ:
		if (h.payload_len != 0) { tx_error(MSG_GET_TEMP_POINTS_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			const bool is_l3 = (leptonVersion == leptonVersion_3_5_shutter);
			uint8_t out[2 + 96 * 6];
			uint16_t cnt = 0;
			for (int i = 0; i < 96; ++i) {
				uint16_t idx = tempPoints[i][0];
				if (idx != 0 && idx <= 19200u) {
					uint16_t z = idx - 1u;
					uint16_t px = is_l3 ? (z % 160u) : ((z % 160u) / 2u);
					uint16_t py = is_l3 ? (z / 160u) : ((z / 160u) / 2u);
					write_u16_le(out + 2 + cnt * 6 + 0, px);
					write_u16_le(out + 2 + cnt * 6 + 2, py);
					write_u16_le(out + 2 + cnt * 6 + 4, tempPoints[i][1]);
					cnt++;
				}
			}
			write_u16_le(out, cnt);
			tx_packet(MSG_GET_TEMP_POINTS_RESP, 0, h.seq, OK, out, 2u + cnt * 6u);
		}
		break;

	case MSG_SET_TEMP_POINTS_REQ:
		if (h.payload_len < 2) { tx_error(MSG_SET_TEMP_POINTS_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			uint16_t cnt = read_u16_le(p);
			if (cnt > 96 || h.payload_len != 2u + cnt * 4u) {
				tx_error(MSG_SET_TEMP_POINTS_RESP, h.seq, ERR_INVALID_ARG);
				break;
			}
			const bool is_l3 = (leptonVersion == leptonVersion_3_5_shutter);
			const uint16_t max_w = is_l3 ? 160u : 80u;
			const uint16_t max_h = is_l3 ? 120u : 60u;
			bool valid = true;
			for (uint16_t i = 0; i < cnt; ++i) {
				uint16_t x = read_u16_le(p + 2 + i * 4);
				uint16_t y = read_u16_le(p + 2 + i * 4 + 2);
				if (x >= max_w || y >= max_h) { valid = false; break; }
			}
			if (!valid) { tx_error(MSG_SET_TEMP_POINTS_RESP, h.seq, ERR_INVALID_ARG); break; }
			clearTempPoints();
			for (uint16_t i = 0; i < cnt; ++i) {
				uint16_t x = read_u16_le(p + 2 + i * 4);
				uint16_t y = read_u16_le(p + 2 + i * 4 + 2);
				uint16_t sx = is_l3 ? x : (x * 2u);
				uint16_t sy = is_l3 ? y : (y * 2u);
				tempPoints[i][0] = static_cast<uint16_t>(sy * 160u + sx + 1u);
				tempPoints[i][1] = smallBuffer[sy * 160u + sx];
			}
			uint8_t out[2];
			write_u16_le(out, cnt);
			tx_packet(MSG_SET_TEMP_POINTS_RESP, 0, h.seq, OK, out, 2);
		}
		break;

	case MSG_SAVE_CAPTURE_REQ:
		handle_save_capture(h.seq, p, h.payload_len);
		break;

	case MSG_GET_DIAGNOSTICS_REQ:
		if (h.payload_len != 0) { tx_error(MSG_GET_DIAGNOSTICS_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			checkBattery(false, false);
			lepton_getFpaTempC();
			DiagnosticsResp d{};
			d.diagnostic_mask = diagnostic;
			d.bat_pct = batPercentage;
			d.usb_power = isUSBConnected() ? 1 : 0;
			d.sd_fs_type = getSDFsType();
			d.bat_mv = batMillivolts;
			d.cpu_mhz = static_cast<uint16_t>(F_CPU_ACTUAL / 1000000u);
			d.mcu_temp_c = perf_mcu_temp_c();
			d.sd_total_kb = getSDTotalKB();
			d.sd_free_kb = getSDFreeKB();
			d.uptime_us = perf_uptime_us();
			d.free_dtcm_bytes = perf_free_dtcm_bytes();
			d.free_ocram_bytes = perf_free_ocram_bytes();
			d.lepton_fpa_temp_c = leptonFpaTempC;
			memcpy(d.lepton_part_num, leptonPartNum, sizeof(d.lepton_part_num));
			uint8_t out[80];
			encode_diagnostics(out, d);
			tx_packet(MSG_GET_DIAGNOSTICS_RESP, 0, h.seq, OK, out, 80);
		}
		break;

	case MSG_GET_PERF_REQ:
		if (h.payload_len != 1) { tx_error(MSG_GET_PERF_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			uint8_t flags = p[0];
			PerfTelemetry pt{};
			perf_fill_telemetry(pt, (flags & 0x02u) != 0, s_parser.rx_errors(), s_parser.crc_errors());
			if (flags & 0x01u) {
				perf_reset_counters();
				s_parser.clear_error_counters();
			}
			uint8_t out[84];
			encode_perf_telemetry(out, pt);
			tx_packet(MSG_GET_PERF_RESP, 0, h.seq, OK, out, 84);
		}
		break;

	case MSG_GET_RAD_PARAMS_REQ:
		if (h.payload_len != 0) { tx_error(MSG_GET_RAD_PARAMS_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			if (!checkDiagnostic(diag_lepton)) { tx_error(MSG_GET_RAD_PARAMS_RESP, h.seq, ERR_UNSUPPORTED); break; }
			RadParams rp{};
			if (!lepton_getRadParams(rp)) { tx_error(MSG_GET_RAD_PARAMS_RESP, h.seq, ERR_IO); break; }
			uint8_t out[18];
			encode_rad_params(out, rp);
			tx_packet(MSG_GET_RAD_PARAMS_RESP, 0, h.seq, OK, out, 18);
		}
		break;

	case MSG_SET_RAD_PARAMS_REQ:
		{
			RadParams rp{};
			if (!decode_rad_params(p, h.payload_len, rp) || !validate_rad_params(rp)) {
				tx_error(MSG_SET_RAD_PARAMS_RESP, h.seq, ERR_INVALID_ARG);
				break;
			}
			if (!checkDiagnostic(diag_lepton)) { tx_error(MSG_SET_RAD_PARAMS_RESP, h.seq, ERR_UNSUPPORTED); break; }
			if (!lepton_setRadParams(rp)) { tx_error(MSG_SET_RAD_PARAMS_RESP, h.seq, ERR_IO); break; }
			RadParams back{};
			if (!lepton_getRadParams(back)) back = rp;
			uint8_t out[18];
			encode_rad_params(out, back);
			tx_packet(MSG_SET_RAD_PARAMS_RESP, 0, h.seq, OK, out, 18);
		}
		break;

	case MSG_FS_INFO_REQ:   handle_fs_info(h.seq); break;
	case MSG_FS_LIST_REQ:   handle_fs_list(h.seq, p, h.payload_len); break;
	case MSG_FS_STAT_REQ:   handle_fs_stat(h.seq, p, h.payload_len); break;
	case MSG_FS_READ_REQ:   handle_fs_read_start(h.seq, p, h.payload_len); break;
	case MSG_FS_DELETE_REQ: handle_fs_delete(h.seq, p, h.payload_len); break;

	case MSG_FS_ABORT_REQ:
		if (h.payload_len != 2) { tx_error(MSG_FS_ABORT_RESP, h.seq, ERR_INVALID_ARG); break; }
		{
			uint16_t target = read_u16_le(p);
			if (s_v2.fs_read.active && (target == 0 || target == s_v2.fs_read.req_seq)) {
				abort_active_fs_read();
			}
			tx_packet(MSG_FS_ABORT_RESP, 0, h.seq, OK, nullptr, 0);
		}
		break;

	case MSG_ENTER_BOOTLOADER_REQ:
		if (h.payload_len != 4) { tx_error(MSG_ENTER_BOOTLOADER_RESP, h.seq, ERR_INVALID_ARG); break; }
		if (read_u32_le(p) != BOOTLOADER_MAGIC) {
			tx_error(MSG_ENTER_BOOTLOADER_RESP, h.seq, ERR_BAD_MAGIC);
			break;
		}
		abort_active_fs_read();
		lepton_endFrame();
		display_waitDmaComplete();
		if (!s_v2.headless && checkDiagnostic(diag_display)) {
			enableScreenLight();
			showFullMessage((char *)"Firmware Update Mode...");
		}
		emit_session_exit(2); // reboot_bootloader
		tx_packet(MSG_ENTER_BOOTLOADER_RESP, 0, h.seq, OK, nullptr, 0);
		Serial.send_now();
		delay(50);
		_reboot_Teensyduino_();
		break;

	default:
		tx_error(MSG_GENERIC_ERROR_RESP, h.seq, ERR_UNKNOWN_MSG, "unknown msg_type");
		break;
	}
}

static void poll_async_events(uint32_t now_ms) {
	// Hardware button edge tracking -> EVT_BUTTON
	bool btn = extButtonPressed();
	if (btn && !s_v2.btn_down) {
		s_v2.btn_down = true;
		s_v2.btn_down_ms = now_ms;
		uint8_t p[4] = {2, 0, 0, 0}; // down
		emit_event(MSG_EVT_BUTTON, p, 4);
	} else if (!btn && s_v2.btn_down) {
		s_v2.btn_down = false;
		uint16_t dur = static_cast<uint16_t>(now_ms - s_v2.btn_down_ms);
		uint8_t p_up[4] = {3, 0, 0, 0};
		write_u16_le(p_up + 2, dur);
		emit_event(MSG_EVT_BUTTON, p_up, 4);
		uint8_t p_act[4] = {static_cast<uint8_t>(dur >= 1000 ? 1 : 0), 0, 0, 0};
		write_u16_le(p_act + 2, dur);
		emit_event(MSG_EVT_BUTTON, p_act, 4);
	}

	// Touch tracking -> EVT_TOUCH or long-touch session exit
	if (!s_v2.headless && checkDiagnostic(diag_touch)) {
		bool pressed = touch_capacitive ? touch_touched() : (!digitalRead(pin_touch_irq));
		if (pressed && !s_v2.touch_down) {
			s_v2.touch_down = true;
			s_v2.touch_down_ms = now_ms;
			TS_Point pt = touch_getPoint();
			s_v2.touch_x = static_cast<uint16_t>(pt.x);
			s_v2.touch_y = static_cast<uint16_t>(pt.y);
		} else if (pressed && s_v2.touch_down && (now_ms - s_v2.touch_down_ms) >= 1200u) {
			uint8_t p[6] = {1, 0, 0, 0, 0, 0};
			write_u16_le(p + 2, s_v2.touch_x);
			write_u16_le(p + 4, s_v2.touch_y);
			emit_event(MSG_EVT_TOUCH, p, 6);
			emit_session_exit(0); // user_touch
			s_v2.running = false;
			s_v2.touch_down = false;
			return;
		} else if (!pressed && s_v2.touch_down) {
			s_v2.touch_down = false;
			uint8_t p[6] = {0, 0, 0, 0, 0, 0};
			write_u16_le(p + 2, s_v2.touch_x);
			write_u16_le(p + 4, s_v2.touch_y);
			emit_event(MSG_EVT_TOUCH, p, 6);
		}
	}

	// Periodic battery telemetry (every 5 s, emit on change)
	if ((now_ms - s_v2.last_bat_poll_ms) >= 5000u) {
		s_v2.last_bat_poll_ms = now_ms;
		checkBattery(false, false);
		uint8_t usb_p = isUSBConnected() ? 1 : 0;
		if (s_v2.last_bat_pct == -128 || abs(batPercentage - s_v2.last_bat_pct) >= 5 || usb_p != s_v2.last_usb_pwr) {
			s_v2.last_bat_pct = batPercentage;
			s_v2.last_usb_pwr = usb_p;
			uint8_t p[6] = {
				static_cast<uint8_t>(batPercentage),
				static_cast<uint8_t>((usb_p && batPercentage >= 0 && batPercentage < 100) ? 1 : 0),
				static_cast<uint8_t>((batPercentage >= 0 && batPercentage <= 20) ? 1 : 0),
				0, 0, 0
			};
			write_u16_le(p + 4, batMillivolts);
			emit_event(MSG_EVT_BATTERY, p, 6);
		}
	}

	// Periodic EVT_PERF
	if (s_v2.stream.perf_interval_s > 0 &&
	    (now_ms - s_v2.last_perf_evt_ms) >= (static_cast<uint32_t>(s_v2.stream.perf_interval_s) * 1000u)) {
		s_v2.last_perf_evt_ms = now_ms;
		emit_perf_event();
	}
}

static void drain_serial_rx(uint32_t now_ms) {
	while (Serial.available() > 0 && s_v2.running) {
		uint8_t b = static_cast<uint8_t>(Serial.read());
		ParseResult r = s_parser.feed_byte(b, now_ms);
		if (r == ParseResult::PACKET_OK) {
			s_v2.last_rx_ms = now_ms;
			dispatch_packet(s_parser.header(), s_parser.payload());
		} else if (r == ParseResult::BAD_CRC) {
			tx_error(MSG_GENERIC_ERROR_RESP, s_parser.header().seq, ERR_BAD_CRC, "CRC-32 mismatch");
		} else if (r == ParseResult::PAYLOAD_TOO_LARGE) {
			tx_error(MSG_GENERIC_ERROR_RESP, s_parser.header().seq, ERR_INVALID_ARG, "payload too large");
		}
	}
	s_parser.check_timeout(now_ms);
}

} // namespace

void serialConnectV2(bool headless) {
	s_v2 = V2Session{};
	s_v2.running = true;
	s_v2.headless = headless;
	s_v2.last_rx_ms = millis();
	s_v2.last_perf_evt_ms = s_v2.last_rx_ms;
	s_v2.last_bat_poll_ms = s_v2.last_rx_ms;

	s_parser.reset();
	// Feed the 4 already-consumed magic bytes "TC2\0" into parser
	for (size_t i = 0; i < 4; ++i) {
		s_parser.feed_byte(MAGIC[i], s_v2.last_rx_ms);
	}

	serialMode = true;
	lepton_startFrame();

	uint32_t lastFramePushUs = micros();
	uint32_t lastFpaPollMs = millis();

	while (s_v2.running) {
		uint32_t now_ms = millis();

		// 1. Always drain incoming USB control commands first (<1 ms latency!)
		drain_serial_rx(now_ms);
		if (!s_v2.running) break;

		// 2. Check USB DTR drop & 5 s heartbeat watchdog
		if (!Serial) {
			s_v2.running = false;
			break;
		}
		if ((now_ms - s_v2.last_rx_ms) >= DEFAULT_HEARTBEAT_MS) {
			emit_session_exit(1); // heartbeat_timeout
			s_v2.running = false;
			break;
		}

		// 3. Pump one chunk of any active windowed SD FS_READ
		if (s_v2.fs_read.active) {
			pump_fs_read();
			drain_serial_rx(millis());
			if (!s_v2.running) break;
		}

		// 4. Async button/touch/battery/perf events
		if (s_v2.hello_done) {
			poll_async_events(now_ms);
			if (!s_v2.running) break;
		}

		// 5. Process fresh double-buffered Lepton frame when ready
		if (lepton_hasNewFrame()) {
			uint32_t loopT0 = micros();
			lepton_acquireFrame(false);
			uint32_t seq = g_perf.frame_count;
			uint64_t ts = perf_uptime_us();

			uint32_t t_st = micros();
			float spot_c = lepton_spotTemp();
			spotTemp = (tempFormat == tempFormat_fahrenheit)
				? tc_core::celsius_to_fahrenheit(spot_c) : spot_c;
			refreshTempPoints();
			if (autoMode && !limitsLocked) limitValues();
			else refreshMinMax();
			g_perf.stats_us = micros() - t_st;

			if ((now_ms - lastFpaPollMs) >= 5000u) {
				lastFpaPollMs = now_ms;
				lepton_getFpaTempC();
			}

			// Optional stream rate cap
			bool should_push = (s_v2.stream.mode != 0);
			if (should_push && s_v2.stream.max_fps_x10 > 0) {
				uint32_t min_interval_us = 10000000u / s_v2.stream.max_fps_x10;
				if ((micros() - lastFramePushUs) < min_interval_us) should_push = false;
			}

			if (should_push && (s_v2.stream.mode == 1 || s_v2.stream.mode == 3)) {
				tx_frame_raw(seq, ts, spot_c);
			}

			bool need_render = (should_push && (s_v2.stream.mode == 2 || s_v2.stream.mode == 3)) ||
			                   (!headless && s_v2.stream.keep_lcd_on);
			if (need_render) {
				if (filterType == filterType_box) boxFilter();
				else if (filterType == filterType_gaussian) gaussianFilter();
				smallToBigBuffer();
				convertColors(false);
				displayInfos();
				if (should_push && (s_v2.stream.mode == 2 || s_v2.stream.mode == 3)) {
					tx_frame_rendered(seq, ts);
				}
				if (!headless && s_v2.stream.keep_lcd_on) {
					displayBuffer();
				}
			}

			uint32_t now_us = micros();
			g_perf.loop_total_us = now_us - loopT0;
			if (should_push) {
				perf_record_frame_period(PERF_MODE_SERIAL_STREAM, now_us - lastFramePushUs);
				lastFramePushUs = now_us;
			} else {
				g_perf.perf_mode = PERF_MODE_SERIAL_IDLE;
			}
		}
	}

	abort_active_fs_read();
	s_v2.stream.mode = 0;
	g_perf.stream_fps_x100 = 0;
	serialMode = false;
	if (!headless) {
		enableScreenLight();
		// Drain any leftover touch release
		uint32_t t0 = millis();
		while (touch_touched() && (millis() - t0) < 500u) {}
	}
	Serial.clear();
}

bool checkSerialV2OrV1(bool headless) {
	if (Serial.available() <= 0) return false;
	uint8_t b = static_cast<uint8_t>(Serial.read());

	// Protocol v2 magic: 'T' followed within 50 ms by 'C', '2', '\0'
	if (b == MAGIC[0]) {
		uint32_t start = millis();
		uint8_t matched = 1;
		while (matched < 4 && (millis() - start) < 50u) {
			if (Serial.available() > 0) {
				uint8_t nxt = static_cast<uint8_t>(Serial.read());
				if (nxt == MAGIC[matched]) {
					matched++;
				} else {
					matched = (nxt == MAGIC[0]) ? 1 : 0;
				}
			}
		}
		if (matched == 4) {
			serialConnectV2(headless);
			return true;
		}
		return false;
	}

	// Temporary Phase 3 legacy v1 auto-detect (deleted at Phase 4 cutover per A2)
	if (b == CMD_START) {
		serialMode = true;
		if (headless) {
			Serial.write(CMD_START);
			Serial.send_now();
			serialOutput();
			Serial.write(CMD_END);
			Serial.send_now();
		} else {
			serialConnect();
		}
		serialMode = false;
		lepton_startFrame();
		return true;
	}

	return false;
}
