/*
 *
 * FT6206 Touch controller
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

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

#ifndef FT6206_TOUCHSCREEN_H
#define FT6206_TOUCHSCREEN_H

#define FT6206_ADDR           0x38
#define FT6206_G_FT5201ID     0xA8
#define FT6206_REG_NUMTOUCHES 0x02

#define FT6206_NUM_X             0x33
#define FT6206_NUM_Y             0x34
#define FT6206_REG_MODE 0x00
#define FT6206_REG_CALIBRATE 0x02
#define FT6206_REG_WORKMODE 0x00
#define FT6206_REG_FACTORYMODE 0x40
#define FT6206_REG_THRESHHOLD 0x80
#define FT6206_REG_POINTRATE 0x88
#define FT6206_REG_FIRMVERS 0xA6
#define FT6206_REG_CHIPID 0xA3
#define FT6206_REG_VENDID 0xA8

#define FT6206_DEFAULT_THRESSHOLD 128

/*########################## PUBLIC PROCEDURES ################################*/

class FT6206_Touchscreen {
public:
	boolean begin(uint8_t thresh = FT6206_DEFAULT_THRESSHOLD);
	void writeRegister8(uint8_t reg, uint8_t val);
	uint8_t readRegister8(uint8_t reg);
	void readData(uint16_t *x, uint16_t *y);
	boolean touched(void);
	TS_Point getPoint(void);
	bool rotated = false;
private:
	uint8_t touches;
	uint16_t touchX[2], touchY[2], touchID[2];
};

#endif /* FT6206_TOUCHSCREEN_H */
