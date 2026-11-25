/*
 * LEPTON - FLIR Lepton LWIR driver (double-buffered VoSPI + bounded I2C + CCI Radiometry)
 */

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <hardware.h>
#include <display.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <gui.h>
#include <perf.h>
#include <tc_thermal_core.h>
#include <lepton.h>

static constexpr uint8_t LEPTON_I2C_ADDR = 0x2A;
static constexpr uint32_t LEPTON_I2C_TIMEOUT_MS = 250;

static byte lepton_packet[164];
static volatile int lepton_curSeg = 1;
static volatile bool lepton_validSegRegion = false;
static volatile bool irqAttached = false;
static volatile uint32_t lepton_frameStartUs = 0;

static inline uint32_t abs_diff_u32(uint32_t n1, uint32_t n2) {
	return n2 - n1; // unsigned wraparound-safe elapsed us
}

void lepton_begin() {
	SPI1.beginTransaction(SPISettings(g_lepton_spi_hz, MSBFIRST, g_lepton_spi_mode));
	digitalWriteFast(pin_lepton_cs, LOW);
}

void lepton_end() {
	digitalWriteFast(pin_lepton_cs, HIGH);
	SPI1.endTransaction();
}

void lepton_startFrame() {
	if (!irqAttached) {
		lepton_curSeg = 1;
		lepton_validSegRegion = false;
		attachInterrupt(pin_lepton_vsync, lepton_getFrameAsync, RISING);
		irqAttached = true;
	}
}

void lepton_endFrame() {
	if (irqAttached) {
		detachInterrupt(pin_lepton_vsync);
		irqAttached = false;
	}
}

/* Fix #119: proper CS high >=186 ms + VSYNC IRQ detach/reattach on VoSPI reset */
void lepton_reset() {
	bool wasAttached = irqAttached;
	lepton_endFrame();
	digitalWriteFast(pin_lepton_cs, HIGH);
	delay(190);
	noInterrupts();
	lepton_curSeg = 1;
	lepton_validSegRegion = false;
	leptonBufferValid = false;
	g_perf.vospi_resyncs++;
	interrupts();
	if (wasAttached) {
		lepton_startFrame();
	}
}

void lepton_savePacket(uint8_t line, uint8_t segment) {
	uint16_t* dst = smallBufferBack ? smallBufferBack : smallBuffer;
	tc_core::lepton_map_packet(
		lepton_packet,
		line,
		segment,
		leptonVersion == leptonVersion_3_5_shutter,
		rotationVert,
		rotationHorizont,
		dst);
}

bool lepton_getPacketAsync(uint8_t *line, uint8_t *seg) {
	bool valid = false;
	*seg = 0;

	lepton_begin();
	SPI1.transfer(lepton_packet, 164);
	lepton_end();

	if ((lepton_packet[0] & 0x0Fu) != 0x0Fu) {
		if (!tc_core::vospi_packet_crc_ok(lepton_packet)) {
			g_perf.vospi_crc_errors++;
			if (g_vospi_crc_enabled) {
				return false;
			}
		}
		*line = lepton_packet[1];
		if (*line == 20) {
			*seg = (lepton_packet[0] >> 4) & 0x07u;
		}
		valid = true;
	}
	return valid;
}

/* Continuous double-buffered VSYNC ISR: fills smallBufferBack while main() renders smallBuffer */
void lepton_getFrameAsync() {
	if (disableSPIIRQ || leptonBufferValid) return;

	uint32_t startUsec = micros();
	uint8_t line = 0, prevLine = 255, segment = 0;
	bool done = false;
	bool beforeValidData = true;
	bool seenValid = false;
	uint16_t discardCount = 0;
	int attempts = 0;

	if (lepton_curSeg == 1 && !lepton_validSegRegion) {
		lepton_frameStartUs = startUsec;
	}

	while (!done && attempts < 100) {
		attempts++;
		if (lepton_getPacketAsync(&line, &segment)) {
			seenValid = true;
			discardCount = 0;
			if (line == prevLine || line > 59) {
				if (lepton_validSegRegion) {
					g_perf.vospi_resyncs++;
					lepton_validSegRegion = false;
					lepton_curSeg = 1;
				}
				done = true;
			} else {
				if ((line == 20) && (leptonVersion == leptonVersion_3_5_shutter)) {
					if (!lepton_validSegRegion) {
						if (segment == 1) {
							beforeValidData = false;
							lepton_validSegRegion = true;
							lepton_curSeg = 1;
						}
					} else if (segment != lepton_curSeg) {
						g_perf.vospi_resyncs++;
						if (segment == 1) {
							lepton_curSeg = 1;
							lepton_validSegRegion = true;
						} else {
							lepton_validSegRegion = false;
							lepton_curSeg = 1;
						}
					}
				}

				if ((leptonVersion == leptonVersion_2_5_shutter) || beforeValidData || lepton_validSegRegion) {
					lepton_savePacket(line, static_cast<uint8_t>(lepton_curSeg));
				}

				if (line == 59) {
					if (leptonVersion == leptonVersion_3_5_shutter) {
						if (lepton_validSegRegion) {
							if (lepton_curSeg < 4) {
								lepton_curSeg++;
							} else {
								if (leptonBufferValid) {
									g_perf.frames_dropped++;
								}
								leptonFrameReadyUs = micros();
								g_perf.acquire_us = leptonFrameReadyUs - lepton_frameStartUs;
								g_perf.frame_count++;
								leptonBufferValid = true;
								lepton_curSeg = 1;
								lepton_validSegRegion = false;
							}
						}
					} else {
						if (leptonBufferValid) {
							g_perf.frames_dropped++;
						}
						leptonFrameReadyUs = micros();
						g_perf.acquire_us = leptonFrameReadyUs - lepton_frameStartUs;
						g_perf.frame_count++;
						leptonBufferValid = true;
					}
					done = true;
				}
			}
			prevLine = line;
		} else {
			discardCount++;
			// Blank/discard VSYNC slot: bail after ~1.8 ms instead of burning the full 9.45 ms VSYNC period!
			if (!seenValid && discardCount >= 25u) {
				done = true;
			}
		}
		if (abs_diff_u32(startUsec, micros()) > 6500u) {
			done = true;
		}
	}
}

bool lepton_hasNewFrame() {
	return leptonBufferValid;
}

/* Swap back->front double buffer atomically; if wait=true blocks until ready (with reset recovery) */
bool lepton_acquireFrame(bool wait) {
	if (!irqAttached) {
		lepton_startFrame();
	}
	if (!wait && !leptonBufferValid) {
		return false;
	}
	uint32_t timer = millis();
	while (!leptonBufferValid) {
		yield();
		if ((millis() - timer) > 3000u) {
			if (!serialMode) {
				showFullMessage((char *)"Lepton error, resetting..");
			}
			lepton_reset();
			lepton_startFrame();
			timer = millis();
		}
	}
	noInterrupts();
	if (smallBufferBack) {
		unsigned short* tmp = smallBuffer;
		smallBuffer = smallBufferBack;
		smallBufferBack = tmp;
	}
	leptonBufferValid = false;
	interrupts();
	return true;
}

void lepton_getFrame() {
	lepton_acquireFrame(true);
}

/*######################## BOUNDED I2C CCI HELPERS (#119) ####################*/

bool lepton_setReg(uint16_t reg) {
	Wire.beginTransmission(LEPTON_I2C_ADDR);
	Wire.write(static_cast<uint8_t>((reg >> 8) & 0xFFu));
	Wire.write(static_cast<uint8_t>(reg & 0xFFu));
	return Wire.endTransmission() == 0;
}

int lepton_readReg(uint16_t reg) {
	if (!lepton_setReg(reg)) return -1;
	uint8_t got = Wire.requestFrom(static_cast<int>(LEPTON_I2C_ADDR), 2);
	if (got < 2) return -1;
	uint16_t hi = static_cast<uint16_t>(Wire.read());
	uint16_t lo = static_cast<uint16_t>(Wire.read());
	return static_cast<int>((hi << 8) | lo);
}

static bool lepton_waitReady(uint32_t timeout_ms = LEPTON_I2C_TIMEOUT_MS) {
	uint32_t start = millis();
	while ((millis() - start) < timeout_ms) {
		int status = lepton_readReg(0x0002);
		if (status < 0) return false;
		if ((status & 0x0001) == 0) return true;
		delayMicroseconds(200);
	}
	return false;
}

static int lepton_i2cReadDataRegister(byte *data, int byte_count) {
	if (!lepton_waitReady()) return -1;
	int words_avail = lepton_readReg(0x0006);
	if (words_avail < 0) return -1;
	// Point explicitly to DATA0 (0x0008) before reading
	if (!lepton_setReg(0x0008)) return -1;
	int got = Wire.requestFrom(static_cast<int>(LEPTON_I2C_ADDR), byte_count);
	if (got < byte_count) return -1;
	return static_cast<int>(Wire.readBytes(data, byte_count));
}

static byte lepton_i2cWriteDataRegister(const byte *data, int byte_count) {
	if (!lepton_waitReady()) return 1;
	Wire.beginTransmission(LEPTON_I2C_ADDR);
	Wire.write(0x00);
	Wire.write(0x08);
	for (int i = 0; i < byte_count; i++) {
		Wire.write(data[i]);
	}
	if (Wire.endTransmission() != 0) return 1;

	uint16_t word_count = static_cast<uint16_t>((byte_count + 1) / 2);
	Wire.beginTransmission(LEPTON_I2C_ADDR);
	Wire.write(0x00);
	Wire.write(0x06);
	Wire.write(static_cast<uint8_t>((word_count >> 8) & 0xFFu));
	Wire.write(static_cast<uint8_t>(word_count & 0xFFu));
	return Wire.endTransmission();
}

static byte lepton_i2c_execute_command(byte cmdbyte0, byte cmdbyte1) {
	if (!lepton_waitReady()) return 1;
	Wire.beginTransmission(LEPTON_I2C_ADDR);
	Wire.write(0x00);
	Wire.write(0x04);
	Wire.write(cmdbyte0);
	Wire.write(cmdbyte1);
	return Wire.endTransmission();
}

/*######################## HIGH-LEVEL CCI COMMANDS ###########################*/

bool lepton_ffc(bool message, bool switch_gain) {
	if (message) {
		if ((!autoMode) && (!switch_gain)) {
			showFullMessage((char *)"No FFC in manual mode", true);
			delay(1000);
			return false;
		}
		showFullMessage((char *)"Performing FFC..", true);
	}

	byte error;
	if ((leptonVersion == leptonVersion_2_5_shutter) || (leptonVersion == leptonVersion_3_5_shutter)) {
		error = lepton_i2c_execute_command(0x4E, 0x2E); // RAD FFC Normalization
	} else {
		error = lepton_i2c_execute_command(0x02, 0x42); // SYS Run FFC Normalization
	}

	if (message) delay(1500);
	return error != 0;
}

float lepton_spotTemp() {
	if (lepton_i2c_execute_command(0x4E, 0xD0) != 0) return spotTemp;
	byte response[8] = {};
	if (lepton_i2cReadDataRegister(response, 8) < 2) return spotTemp;
	uint16_t raw = (static_cast<uint16_t>(response[0]) << 8) | response[1];
	float t = static_cast<float>(raw) * ((leptonGainMode == lepton_gain_high) ? 0.01f : 0.1f);
	return t - 273.15f;
}

float lepton_getFpaTempC() {
	// SYS FPA Temperature Kelvin GET: 0x0210 -> 1 word (centi-Kelvin)
	if (lepton_i2c_execute_command(0x02, 0x10) != 0) return leptonFpaTempC;
	byte response[2] = {};
	if (lepton_i2cReadDataRegister(response, 2) < 2) return leptonFpaTempC;
	uint16_t ck = (static_cast<uint16_t>(response[0]) << 8) | response[1];
	if (ck == 0) return leptonFpaTempC;
	leptonFpaTempC = static_cast<float>(ck) * 0.01f - 273.15f;
	return leptonFpaTempC;
}

void lepton_ffcMode(bool automatic) {
	if ((automatic) && ((hotColdMode != hotColdMode_disabled) || (!autoMode) || limitsLocked))
		return;
	shutterModeAuto = automatic;
	byte package[] = {
		static_cast<byte>(automatic ? 1 : 0), 0, 0, 0, 0, 0, 0, 0,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		224, 147, 4, 0, 0, 0, 0, 0, 44, 1, 52, 0
	};
	lepton_i2cWriteDataRegister(package, sizeof(package));
	lepton_i2c_execute_command(0x02, 0x3D);
}

void lepton_setGpioMode(bool vsync_enabled) {
	byte gpio_mode = vsync_enabled ? 0x05 : 0x00;
	byte data[4] = {0x00, gpio_mode, 0x00, 0x00};
	lepton_i2cWriteDataRegister(data, 4);
	lepton_i2c_execute_command(0x48, 0x55);
}

bool lepton_version() {
	if (lepton_i2c_execute_command(0x48, 0x1C) != 0) return false;
	char leptonhw[33] = {};
	if (lepton_i2cReadDataRegister(reinterpret_cast<byte *>(leptonhw), 32) < 12) return false;
	leptonhw[32] = '\0';
	memset(leptonPartNum, 0, sizeof(leptonPartNum));
	memcpy(leptonPartNum, leptonhw, sizeof(leptonPartNum) - 1);

	if (strstr(leptonhw, "05-070360") != NULL) {
		leptonVersion = leptonVersion_2_5_shutter;
	} else if ((strstr(leptonhw, "05-070170") != NULL) || (strstr(leptonhw, "05-070850") != NULL)) {
		leptonVersion = leptonVersion_3_5_shutter;
	} else {
		return false;
	}
	EEPROM.write(eeprom_leptonVersion, leptonVersion);
	return true;
}

void lepton_setSysGainMode(byte mode) {
	if (mode > 2) return;
	byte data[4] = {0x00, mode, 0x00, 0x00};
	lepton_i2cWriteDataRegister(data, 4);
	lepton_i2c_execute_command(0x02, 0x49);
}

void lepton_setSysGainHigh() { lepton_setSysGainMode(0x00); }
void lepton_setSysGainLow()  { lepton_setSysGainMode(0x01); }
void lepton_setSysGainAuto() { lepton_setSysGainMode(0x02); }

int lepton_getSysGainMode() {
	byte data[4] = {};
	if (lepton_i2c_execute_command(0x02, 0x48) != 0) return -1;
	if (lepton_i2cReadDataRegister(data, 4) < 2) return -1;
	return data[1];
}

byte lepton_getRadTlinearResolution() {
	byte data[4] = {};
	if (lepton_i2c_execute_command(0x4E, 0xC4) != 0) return 1;
	if (lepton_i2cReadDataRegister(data, 4) < 2) return 1;
	return data[1];
}

float lepton_getResolution() {
	return (lepton_getRadTlinearResolution() == 0) ? 0.1f : 0.01f;
}

void lepton_setRadTlinearResolution(byte resolution) {
	if (resolution > 1) return;
	byte data[4] = {0x00, resolution, 0x00, 0x00};
	lepton_i2cWriteDataRegister(data, 4);
	lepton_i2c_execute_command(0x4E, 0xC5);
}

void lepton_setLowGain() {
	lepton_setSysGainLow();
	lepton_setRadTlinearResolution(0);
	leptonCalSlope = 0.1f;
	leptonGainMode = lepton_gain_low;
}

void lepton_setHighGain() {
	lepton_setSysGainHigh();
	lepton_setRadTlinearResolution(1);
	leptonCalSlope = 0.01f;
	leptonGainMode = lepton_gain_high;
}

/* A4: FLIR Lepton CCI RAD Flux Linear Parameters (0x4EBC GET / 0x4EBD SET) */
bool lepton_getRadParams(tc_v2::RadParams& out) {
	if (!checkDiagnostic(diag_lepton)) return false;
	if (lepton_i2c_execute_command(0x4E, 0xBC) != 0) return false;
	byte raw[16] = {};
	if (lepton_i2cReadDataRegister(raw, 16) < 16) return false;
	auto rd_be16 = [&](int idx) -> uint16_t {
		return static_cast<uint16_t>((static_cast<uint16_t>(raw[idx * 2]) << 8) | raw[idx * 2 + 1]);
	};
	out.scene_emissivity = rd_be16(0);
	out.t_bkg_ck         = rd_be16(1);
	out.tau_window       = rd_be16(2);
	out.t_window_ck      = rd_be16(3);
	out.tau_atm          = rd_be16(4);
	out.t_atm_ck         = rd_be16(5);
	out.refl_window      = rd_be16(6);
	out.t_refl_ck        = rd_be16(7);
	out.flags = 0x01u | ((EEPROM.read(eeprom_radParamsSet) == eeprom_setValue) ? 0x02u : 0x00u);
	out.reserved = 0;
	return true;
}

bool lepton_setRadParams(const tc_v2::RadParams& in) {
	if (!checkDiagnostic(diag_lepton)) return false;
	tc_v2::RadParams eff = in;
	if (in.flags & 0x02u) {
		eff = tc_v2::RadParams{}; // factory defaults
	}
	byte raw[16];
	auto wr_be16 = [&](int idx, uint16_t v) {
		raw[idx * 2] = static_cast<byte>((v >> 8) & 0xFFu);
		raw[idx * 2 + 1] = static_cast<byte>(v & 0xFFu);
	};
	wr_be16(0, eff.scene_emissivity);
	wr_be16(1, eff.t_bkg_ck);
	wr_be16(2, eff.tau_window);
	wr_be16(3, eff.t_window_ck);
	wr_be16(4, eff.tau_atm);
	wr_be16(5, eff.t_atm_ck);
	wr_be16(6, eff.refl_window);
	wr_be16(7, eff.t_refl_ck);

	if (lepton_i2cWriteDataRegister(raw, 16) != 0) return false;
	if (lepton_i2c_execute_command(0x4E, 0xBD) != 0) return false;
	if (!lepton_waitReady()) return false;

	if (in.flags & 0x02u) {
		EEPROM.write(eeprom_radParamsSet, 0);
	} else if (in.flags & 0x01u) {
		uint8_t le[18];
		tc_v2::encode_rad_params(le, eff);
		for (int i = 0; i < 16; ++i) {
			EEPROM.write(eeprom_radParamsData + i, le[i]);
		}
		EEPROM.write(eeprom_radParamsSet, eeprom_setValue);
	}
	return true;
}

void lepton_loadRadParamsFromEEPROM() {
	if (EEPROM.read(eeprom_radParamsSet) != eeprom_setValue) return;
	uint8_t le[18] = {};
	for (int i = 0; i < 16; ++i) {
		le[i] = EEPROM.read(eeprom_radParamsData + i);
	}
	tc_v2::RadParams rp{};
	if (tc_v2::decode_rad_params(le, 18, rp) && tc_v2::validate_rad_params(rp)) {
		rp.flags = 0;
		lepton_setRadParams(rp);
	}
}

void lepton_init() {
	if (!lepton_version()) {
		setDiagnostic(diag_lepton);
		return;
	}
	lepton_getFpaTempC();
	lepton_setGpioMode(true);
	lepton_loadRadParamsFromEEPROM();
	lepton_ffc();
	autoMode = true;
	limitsLocked = false;
	shutterModeAuto = true;
	leptonBufferValid = false;
}
