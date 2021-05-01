/*
*
* OV2640 - Driver for the Arducam camera module
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

#ifndef OV2640_H
#define OV2640_H

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

#define OV2640_160x120 		0
#define OV2640_176x144 		1
#define OV2640_320x240 		2
#define OV2640_352x288 		3
#define OV2640_640x480		4
#define OV2640_800x600 		5
#define OV2640_1024x768		6
#define OV2640_1280x1024	7
#define OV2640_1600x1200	8

#define CAP_DONE_MASK 0x08
#define ARDUCHIP_TRIG 0x41

struct sensor_reg
{
	uint16_t reg;
	uint16_t val;
};

/*########################## PUBLIC PROCEDURES ################################*/

uint8_t ov2640_busRead(int address);
void ov2640_busWrite(int address, int value);
void ov2640_capture(void);
void ov2640_clearBit(uint8_t addr, uint8_t bit);
void ov2640_clearFifoFlag(void);
void ov2640_endFifoBurst(void);
void ov2640_flushFifo(void);
uint8_t ov2640_getBit(uint8_t addr, uint8_t bit);
bool ov2640_init(void);
byte ov2640_rdSensorReg8_8(uint8_t regID, uint8_t* regDat);
uint32_t ov2640_readFifoLength(void);
uint8_t ov2640_readFifo(void);
uint8_t ov2640_readReg(uint8_t addr);
void ov2640_setBit(uint8_t addr, uint8_t bit);
void ov2640_setFormat(byte fmt);
void ov2640_setJPEGSize(uint8_t size);
void ov2640_setMode(uint8_t mode);
void ov2640_startCapture(void);
void ov2640_startFifoBurst(void);
void ov2640_transfer(uint8_t * jpegData, boolean stream, uint32_t* length);
void ov2640_writeReg(uint8_t addr, uint8_t data);
byte ov2640_wrSensorReg8_8(int regID, int regDat);
int ov2640_wrSensorRegs8_8(const struct sensor_reg* reglist);

#endif /* OV2640_H */
