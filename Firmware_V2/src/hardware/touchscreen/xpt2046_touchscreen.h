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

#ifndef XPT2046_TOUCHSCREEN_H
#define XPT2046_TOUCHSCREEN_H

/*########################## PUBLIC PROCEDURES ################################*/

class XPT2046_Touchscreen {
public:
	XPT2046_Touchscreen();
	bool begin();
	TS_Point getPoint();
	bool touched();
	void readData(uint16_t *x, uint16_t *y, uint8_t *z);
	bool bufferEmpty();
	uint8_t bufferSize() { return 1; }
	bool isrWake;
	bool rotated = false;
private:
	void update();
	uint8_t csPin, tirqPin;
	int16_t xraw, yraw, zraw;
	uint32_t msraw;
};

#endif /* XPT2046_TOUCHSCREEN_H */
