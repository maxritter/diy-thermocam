/*
 * PERF - On-device timing, FPS, VoSPI & SPI telemetry (Teensy 4.1)
 */

#include <Arduino.h>
#include <SPI.h>
#include <perf.h>

extern "C" float tempmonGetTemp(void);
extern unsigned long _heap_start;
extern unsigned long _heap_end;
extern char *__brkval;
extern unsigned long _ebss;

// Conservative defaults (empirically verified on V3.0 PCB during Phase 3)
uint32_t g_display_spi_hz  = 40000000u; // 40 MHz LPSPI4 (tested against 25/40/60 MHz)
uint32_t g_lepton_spi_hz   = 20000000u; // 20 MHz SPI1 (Lepton 3.x max spec; tested on V3.0 PCB)
uint8_t  g_lepton_spi_mode = SPI_MODE3; // FLIR Lepton VoSPI spec MODE3 (tested on V3.0 PCB)
bool     g_vospi_crc_enabled  = true;
bool     g_display_dma_active = false;

PerfRuntime g_perf = {};

static uint32_t s_last_micros = 0;
static uint64_t s_high_micros = 0;
static uint32_t s_ema_period[4] = {0, 0, 0, 0};

void perf_init() {
	s_last_micros = micros();
	s_high_micros = 0;
	g_perf = PerfRuntime{};
}

uint64_t perf_uptime_us() {
	noInterrupts();
	uint32_t now = micros();
	if (now < s_last_micros) {
		s_high_micros += (1ULL << 32);
	}
	s_last_micros = now;
	uint64_t res = s_high_micros | static_cast<uint64_t>(now);
	interrupts();
	return res;
}

uint64_t perf_teensy_uid() {
#if defined(__IMXRT1062__)
	return (static_cast<uint64_t>(HW_OCOTP_CFG1) << 32) | static_cast<uint64_t>(HW_OCOTP_CFG0);
#else
	return 0;
#endif
}

uint32_t perf_free_dtcm_bytes() {
#if defined(__IMXRT1062__)
	volatile uint8_t stack_probe = 0;
	uintptr_t sp = reinterpret_cast<uintptr_t>(&stack_probe);
	uintptr_t ebss = reinterpret_cast<uintptr_t>(&_ebss);
	return (sp > ebss) ? static_cast<uint32_t>(sp - ebss) : 0u;
#else
	return 0;
#endif
}

uint32_t perf_free_ocram_bytes() {
#if defined(__IMXRT1062__)
	uintptr_t end = reinterpret_cast<uintptr_t>(&_heap_end);
	uintptr_t cur = __brkval ? reinterpret_cast<uintptr_t>(__brkval)
	                         : reinterpret_cast<uintptr_t>(&_heap_start);
	return (end > cur) ? static_cast<uint32_t>(end - cur) : 0u;
#else
	return 0;
#endif
}

float perf_mcu_temp_c() {
#if defined(__IMXRT1062__)
	return tempmonGetTemp();
#else
	return 0.0f;
#endif
}

void perf_record_frame_period(uint8_t mode, uint32_t period_us) {
	if (mode > 3 || period_us == 0 || period_us > 5000000u) return;
	if (s_ema_period[mode] == 0) {
		s_ema_period[mode] = period_us;
	} else {
		s_ema_period[mode] = (s_ema_period[mode] * 7u + period_us + 4u) / 8u;
	}
	g_perf.perf_mode = mode;
	g_perf.frame_period_us = s_ema_period[mode];
	uint32_t fps_x100 = (s_ema_period[mode] > 0)
		? static_cast<uint32_t>(100000000ULL / s_ema_period[mode])
		: 0u;
	if (fps_x100 > 65535u) fps_x100 = 65535u;
	if (mode == PERF_MODE_LIVE) g_perf.live_fps_x100 = static_cast<uint16_t>(fps_x100);
	else if (mode == PERF_MODE_VIDEO) g_perf.video_fps_x100 = static_cast<uint16_t>(fps_x100);
	else if (mode == PERF_MODE_SERIAL_STREAM) g_perf.stream_fps_x100 = static_cast<uint16_t>(fps_x100);
}

void perf_save_video_snapshot() {
	perf_fill_telemetry(g_perf.last_video, false, 0, 0);
	g_perf.last_video.perf_mode = PERF_MODE_VIDEO;
	g_perf.last_video.frame_period_us = s_ema_period[PERF_MODE_VIDEO];
	g_perf.has_video_snapshot = true;
}

void perf_fill_telemetry(tc_v2::PerfTelemetry& out, bool select_last_video,
                         uint32_t usb_rx_errors, uint32_t usb_crc_errors) {
	if (select_last_video && g_perf.has_video_snapshot) {
		out = g_perf.last_video;
		out.usb_rx_errors = usb_rx_errors;
		out.usb_crc_errors = usb_crc_errors;
		out.live_fps_x100 = g_perf.live_fps_x100;
		out.video_fps_x100 = g_perf.video_fps_x100;
		out.stream_fps_x100 = g_perf.stream_fps_x100;
		out.mcu_temp_c = perf_mcu_temp_c();
		return;
	}
	out.acquire_us = g_perf.acquire_us;
	out.stats_us = g_perf.stats_us;
	out.filter_us = g_perf.filter_us;
	out.scale_us = g_perf.scale_us;
	out.colorize_us = g_perf.colorize_us;
	out.overlay_us = g_perf.overlay_us;
	out.display_us = g_perf.display_us;
	out.sd_write_us = g_perf.sd_write_us;
	out.usb_tx_us = g_perf.usb_tx_us;
	out.loop_total_us = g_perf.loop_total_us;
	out.frame_count = g_perf.frame_count;
	out.frames_dropped = g_perf.frames_dropped;
	out.vospi_resyncs = g_perf.vospi_resyncs;
	out.vospi_crc_errors = g_perf.vospi_crc_errors;
	out.usb_rx_errors = usb_rx_errors;
	out.usb_crc_errors = usb_crc_errors;
	out.live_fps_x100 = g_perf.live_fps_x100;
	out.video_fps_x100 = g_perf.video_fps_x100;
	out.stream_fps_x100 = g_perf.stream_fps_x100;
	out.perf_mode = g_perf.perf_mode;
	out.flags = PERF_FLAG_DOUBLE_BUF |
	            (g_display_dma_active ? PERF_FLAG_DISPLAY_DMA : 0) |
	            (g_vospi_crc_enabled ? PERF_FLAG_VOSPI_CRC_CHECK : 0);
	out.display_spi_mhz = static_cast<uint8_t>(g_display_spi_hz / 1000000u);
	out.lepton_spi_mhz = static_cast<uint8_t>(g_lepton_spi_hz / 1000000u);
	out.cpu_mhz = static_cast<uint16_t>(F_CPU_ACTUAL / 1000000u);
	out.mcu_temp_c = perf_mcu_temp_c();
	out.frame_period_us = g_perf.frame_period_us;
}

void perf_reset_counters() {
	noInterrupts();
	g_perf.frames_dropped = 0;
	g_perf.vospi_resyncs = 0;
	g_perf.vospi_crc_errors = 0;
	interrupts();
}
