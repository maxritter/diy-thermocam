#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

struct SimAbortLoop : public std::runtime_error {
	using std::runtime_error::runtime_error;
};

struct SimTouchEvent {
	uint32_t start_ms;
	uint32_t end_ms;
	int16_t x;
	int16_t y;
};

struct SimDisplayStats {
	uint32_t direct_fill_scr = 0;   // display_fillScr() directly to LCD (causes full-screen blink/wipe)
	uint32_t buffer_fill_scr = 0;   // display_fillScr() into bigBuffer RAM
	uint32_t atomic_flushes = 0;    // display_writeScreen() / displayBuffer()
	uint32_t direct_draw_ops = 0;   // direct-to-LCD primitive draw calls
	uint32_t load_raw_calls = 0;    // loadRawData() invocations (image/frame loads)
};

struct SimRuntime {
	uint32_t now_ms = 1000u;
	uint32_t max_ms = 0u; // 0 = no watchdog limit
	bool enforce_watchdog = false;

	// Touch hardware model
	bool xpt2046_penirq_glitch_on_poll = false; // models real XPT2046 PENIRQ pulse during SPI update()
	bool xpt2046_stale_coords = true;           // models XPT2046 keeping last xraw/yraw when z == 0
	int16_t last_x = 0;
	int16_t last_y = 0;
	std::vector<SimTouchEvent> touch_script;

	// Simulated GPIO interrupt table
	void (*irq_handlers[64])() = {};

	// Telemetry / dynamic hooks
	float sim_mcu_temp_c = 42.3f;
	std::function<void(uint32_t)> on_tick;

	SimDisplayStats stats = {};

	void reset(uint32_t start_ms = 1000u) {
		now_ms = start_ms;
		max_ms = 0u;
		enforce_watchdog = false;
		xpt2046_penirq_glitch_on_poll = false;
		xpt2046_stale_coords = true;
		last_x = 0;
		last_y = 0;
		touch_script.clear();
		for (auto& h : irq_handlers) h = nullptr;
		sim_mcu_temp_c = 42.3f;
		on_tick = nullptr;
		stats = {};
	}

	void setWatchdog(uint32_t duration_ms) {
		max_ms = now_ms + duration_ms;
		enforce_watchdog = true;
	}

	void scheduleTap(uint32_t at_ms, int16_t x, int16_t y, uint32_t hold_ms = 40u) {
		touch_script.push_back({at_ms, at_ms + hold_ms, x, y});
	}

	void advanceMs(uint32_t dt) {
		now_ms += dt;
		if (on_tick) on_tick(now_ms);
		if (enforce_watchdog && now_ms > max_ms) {
			throw SimAbortLoop("Simulator watchdog exceeded (" + std::to_string(now_ms) + " ms) — infinite loop detected");
		}
	}

	bool activeTouchAtNow(int16_t& ox, int16_t& oy) const {
		for (const auto& ev : touch_script) {
			if (now_ms >= ev.start_ms && now_ms < ev.end_ms) {
				ox = ev.x;
				oy = ev.y;
				return true;
			}
		}
		return false;
	}

	void fireIrq(int pin) {
		if (pin >= 0 && pin < 64 && irq_handlers[pin]) {
			irq_handlers[pin]();
		}
	}
};

extern SimRuntime g_sim;

// Virtual SD filesystem helpers for host UI tests
void sim_sd_clear();
void sim_sd_add_dat_image(const std::string& filename, uint16_t fill_raw = 30500u);
void sim_sd_add_video_folder(const std::string& dirname, uint32_t num_frames);
void sim_sd_add_raw_file(const std::string& path, const std::vector<uint8_t>& bytes);
std::string sim_sd_cwd();
bool sim_sd_exists(const std::string& path);
