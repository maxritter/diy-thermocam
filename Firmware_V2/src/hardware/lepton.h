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
* https://github.com/maxritter/DIY-Thermocam
*
*/

#ifndef LEPTON_H
#define LEPTON_H

namespace tc_v2 {
struct RadParams;
}

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

//Lepton frame error return
enum LeptonReadError {
	NONE, DISCARD, SEGMENT_ERROR, ROW_ERROR, SEGMENT_INVALID
};

/*########################## PUBLIC PROCEDURES ################################*/

void lepton_begin();
void lepton_end();
bool lepton_ffc(bool message = false, bool switch_gain = false);
void lepton_ffcMode(bool automatic);
float lepton_getFpaTempC();
LeptonReadError lepton_getPackage(byte line, byte seg);
void lepton_getRawValues();
bool lepton_getRawValuesEx(uint32_t timeoutMs, bool abortOnSerial);
void lepton_init();
bool lepton_isLepton3();
bool lepton_isRadiometric();
int lepton_readReg(byte reg);
void lepton_reset();
void lepton_setReg(byte reg);
float lepton_spotTemp();
void lepton_version();
bool savePackage(byte line, byte segment = 0);
void lepton_set_sys_gain_high();
void lepton_set_sys_gain_low();
void lepton_set_sys_gain_auto();
int lepton_get_sys_gain_mode();
float lepton_get_resolution();
void lepton_3_5_set_low_gain();
void lepton_3_5_set_high_gain();
bool lepton_getRadParams(tc_v2::RadParams& out);
bool lepton_setRadParams(const tc_v2::RadParams& in);
void lepton_loadRadParamsFromEEPROM();

#endif /* LEPTON_H */
