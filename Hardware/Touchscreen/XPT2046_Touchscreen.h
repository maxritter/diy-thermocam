#ifndef _XPT2046_Touchscreen_h_
#define _XPT2046_Touchscreen_h_

#include "Arduino.h"
#include "../../Libraries/SPI/SPI.h"
#include "Point.h"

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

#endif
