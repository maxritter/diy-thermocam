/*
*
* LEPTON - Access the FLIR Lepton LWIR module
*
* DIY-Thermocam Firmware
*
* GNU General Public License v3.0
*
* Copyright by Max Ritter
*
* http://www.diy-thermocam.net
* https://github.com/maxritter/diy-thermocam
*
*/

#ifndef LEPTON_H
#define LEPTON_H

/*########################## PUBLIC PROCEDURES ################################*/

void lepton_begin();
void lepton_end();
bool lepton_ffc(bool message = false, bool switch_gain = false);
void lepton_ffcMode(bool automatic);
bool lepton_getPacketAsync(uint8_t *line, uint8_t *seg);
void lepton_getFrame();
void lepton_getFrameAsync();
void lepton_init();
int lepton_readReg(byte reg);
void lepton_setReg(byte reg);
float lepton_spotTemp();
bool lepton_version();
void lepton_savePacket(uint8_t line, uint8_t segment = 0);
void lepton_setGpioMode(bool vsync_enabled);
void lepton_setSysGainHigh();
void lepton_setSysGainLow();
void lepton_setSysGainAuto();
int lepton_getSysGainMode();
float lepton_getResolution();
void lepton_setLowGain();
void lepton_setHighGain();
void lepton_startFrame();
void lepton_endFrame();

#endif /* LEPTON_H */
