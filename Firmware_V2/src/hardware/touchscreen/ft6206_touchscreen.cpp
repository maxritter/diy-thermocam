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

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <touchscreen.h>
#include <i2c_t3.h>
#include <ft6206_touchscreen.h>

/*###################### PRIVATE FUNCTION BODIES ##############################*/

boolean FT6206_Touchscreen::begin(uint8_t threshhold) {
	writeRegister8(FT6206_REG_THRESHHOLD, threshhold);
	if ((readRegister8(FT6206_REG_VENDID) != 17) || (readRegister8(FT6206_REG_CHIPID) != 54)) return false;
	return true;
}

boolean FT6206_Touchscreen::touched(void) {
	uint8_t n = readRegister8(FT6206_REG_NUMTOUCHES);
	if ((n == 1) || (n == 2)) return true;
	return false;
}

void FT6206_Touchscreen::readData(uint16_t *x, uint16_t *y) {
	uint8_t i2cdat[16];
	Wire.beginTransmission(FT6206_ADDR);
	Wire.write((byte)0);
	Wire.endTransmission();
	Wire.beginTransmission(FT6206_ADDR);
	Wire.requestFrom((byte)FT6206_ADDR, (byte)32);
	for (uint8_t i = 0; i < 16; i++)
		i2cdat[i] = Wire.read();
	Wire.endTransmission();
	touches = i2cdat[0x02];
	if (touches > 2) {
		touches = 0;
		*x = *y = 0;
	}
	if (touches == 0) {
		*x = *y = 0;
		return;
	}
	for (uint8_t i = 0; i < 2; i++) {
		touchX[i] = i2cdat[0x03 + i * 6] & 0x0F;
		touchX[i] <<= 8;
		touchX[i] |= i2cdat[0x04 + i * 6];
		touchY[i] = i2cdat[0x05 + i * 6] & 0x0F;
		touchY[i] <<= 8;
		touchY[i] |= i2cdat[0x06 + i * 6];
		touchID[i] = i2cdat[0x05 + i * 6] >> 4;
	}
	*x = touchY[0];
	*y = touchX[0];
	*y = map(*y, 0, 240, 240, 0);
}

TS_Point FT6206_Touchscreen::getPoint(void) {
	uint16_t x, y;
	readData(&x, &y);
	if (rotated) {
		y = map(y, 0, 240, 240, 0);
		x = map(x, 0, 320, 320, 0);
	}
	return TS_Point(x, y, 1);
}

uint8_t FT6206_Touchscreen::readRegister8(uint8_t reg) {
	uint8_t x;
	Wire.beginTransmission(FT6206_ADDR);
	Wire.write((byte)reg);
	Wire.endTransmission();
	Wire.beginTransmission(FT6206_ADDR);
	Wire.requestFrom((byte)FT6206_ADDR, (byte)1);
	x = Wire.read();
	Wire.endTransmission();
	return x;
}

void FT6206_Touchscreen::writeRegister8(uint8_t reg, uint8_t val) {
	Wire.beginTransmission(FT6206_ADDR);
	Wire.write((byte)reg);
	Wire.write((byte)val);
	Wire.endTransmission();
}
