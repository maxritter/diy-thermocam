/*
*
* VC0706 - Driver for the PTC-06 or PTC-08 camera module
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

#ifndef VC0706_H
#define VC0706_H

#ifdef __cplusplus
extern "C" {
#endif

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

#define VC0706_640x480 0x00
#define VC0706_320x240 0x11
#define VC0706_160x120 0x22

/*########################## PUBLIC PROCEDURES ################################*/

uint8_t vc0706_available(void);
void vc0706_begin(void);
boolean vc0706_cameraFrameBuffCtrl(uint8_t command);
boolean vc0706_capture();
boolean vc0706_changeBaudRate();
void vc0706_changeCamRes(uint8_t size);
boolean vc0706_end();
uint32_t vc0706_frameLength(void);
uint8_t vc0706_getImageSize();
boolean vc0706_init(void);
boolean vc0706_readPicture(uint8_t n);
uint8_t vc0706_readResponse(uint8_t numbytes, uint8_t timeout);
boolean vc0706_reset(void);
boolean vc0706_runCommand(uint8_t cmd, uint8_t *args, uint8_t argn, uint8_t resplen, boolean flushflag = 1);
void vc0706_sendCommand(uint8_t cmd, uint8_t args[] = 0, uint8_t argn = 0);
boolean vc0706_setCompression(uint8_t c);
boolean vc0706_setImageSize(uint8_t x);
void vc0706_transfer(uint8_t* jpegData, uint16_t jpegLen, byte mode, char* dirname);
void vc0706_transPackage(byte bytesToRead, uint8_t* buffer);
boolean vc0706_verifyResponse(uint8_t command);

#ifdef __cplusplus
}
#endif

#endif /* VC0706_H */
