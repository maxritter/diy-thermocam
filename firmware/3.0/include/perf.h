/*
 * PERF - On-device timing, FPS, VoSPI & SPI telemetry (Teensy 4.1)
 */

#ifndef PERF_H
#define PERF_H

#include <Arduino.h>
#include <tc_proto_v2.h>

// Perf flags (match docs/protocol-v2.md PerfTelemetry.flags)
#define PERF_FLAG_DISPLAY_DMA      0x01
#define PERF_FLAG_DOUBLE_BUF       0x02
#define PERF_FLAG_VOSPI_CRC_CHECK  0x04

// Perf mode (match docs/protocol-v2.md PerfTelemetry.perf_mode)
#define PERF_MODE_LIVE          0
#define PERF_MODE_VIDEO         1
#define PERF_MODE_SERIAL_STREAM 2
#define PERF_MODE_SERIAL_IDLE   3

// Runtime SPI tuning knobs (validated empirically on V3.0 PCB)
extern uint32_t g_display_spi_hz;
extern uint32_t g_lepton_spi_hz;
extern uint8_t  g_lepton_spi_mode;
extern bool     g_vospi_crc_enabled;
extern bool     g_display_dma_active;

// Live perf counters & stage timings
struct PerfRuntime {
	uint32_t acquire_us;
	uint32_t stats_us;
	uint32_t filter_us;
	uint32_t scale_us;
	uint32_t colorize_us;
	uint32_t overlay_us;
	uint32_t display_us;
	uint32_t sd_write_us;
	uint32_t usb_tx_us;
	uint32_t loop_total_us;

	volatile uint32_t frame_count;
	volatile uint32_t frames_dropped;
	volatile uint32_t vospi_resyncs;
	volatile uint32_t vospi_crc_errors;

	uint16_t live_fps_x100;
	uint16_t video_fps_x100;
	uint16_t stream_fps_x100;
	uint8_t  perf_mode;
	uint32_t frame_period_us;

	// Retained snapshot of the last on-device video recording
	bool     has_video_snapshot;
	tc_v2::PerfTelemetry last_video;
};

extern PerfRuntime g_perf;

void perf_init();
uint64_t perf_uptime_us();
uint64_t perf_teensy_uid();
uint32_t perf_free_dtcm_bytes();
uint32_t perf_free_ocram_bytes();
float    perf_mcu_temp_c();

void perf_record_frame_period(uint8_t mode, uint32_t period_us);
void perf_save_video_snapshot();
void perf_fill_telemetry(tc_v2::PerfTelemetry& out, bool select_last_video,
                         uint32_t usb_rx_errors, uint32_t usb_crc_errors);
void perf_reset_counters();

#endif /* PERF_H */
