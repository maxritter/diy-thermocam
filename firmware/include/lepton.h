/*
 * LEPTON - Access the FLIR Lepton LWIR module (double-buffered VoSPI + bounded I2C + CCI Radiometry)
 */

#ifndef LEPTON_H
#define LEPTON_H

#include <Arduino.h>
#include <tc_proto_v2.h>

void lepton_begin();
void lepton_end();
void lepton_vospiResync();
void lepton_reset();
bool lepton_ffc(bool message = false, bool switch_gain = false);
void lepton_ffcMode(bool automatic);
bool lepton_getPacketAsync(uint8_t *line, uint8_t *seg);
void lepton_getFrame();
bool lepton_acquireFrame(bool wait = true);
bool lepton_hasNewFrame();
void lepton_getFrameAsync();
void lepton_init();
int  lepton_readReg(uint16_t reg);
bool lepton_setReg(uint16_t reg);
float lepton_spotTemp();
float lepton_getFpaTempC();
bool lepton_version();
void lepton_savePacket(uint8_t line, uint8_t segment = 0);
void lepton_setGpioMode(bool vsync_enabled);
void lepton_setSysGainHigh();
void lepton_setSysGainLow();
void lepton_setSysGainAuto();
int  lepton_getSysGainMode();
float lepton_getResolution();
void lepton_setLowGain();
void lepton_setHighGain();
void lepton_startFrame();
void lepton_endFrame();

// A4: FLIR Lepton CCI Flux Linear Parameters (0x4EBC GET / 0x4EBD SET)
bool lepton_getRadParams(tc_v2::RadParams& out);
bool lepton_setRadParams(const tc_v2::RadParams& in);
void lepton_loadRadParamsFromEEPROM();

#endif /* LEPTON_H */
