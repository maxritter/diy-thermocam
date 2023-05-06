/*
 *
 * XPT2046 Touch controller
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

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <touchscreen.h>
#include <SPI.h>
#include <globaldefines.h>
#include <xpt2046_touchscreen.h>

/*################# DATA TYPES, CONSTANTS & MACRO DEFINITIONS #################*/

#define Z_THRESHOLD     400
#define MSEC_THRESHOLD  3

/*###################### PRIVATE FUNCTION BODIES ##############################*/

XPT2046_Touchscreen::XPT2046_Touchscreen()
{
	csPin = pin_touch_cs;
	tirqPin = 255;
	msraw = 0x80000000;
	xraw = 0;
	yraw = 0;
	zraw = 0;
	isrWake = true;
}

static XPT2046_Touchscreen 	*isrPinptr;
void isrPin(void);

bool XPT2046_Touchscreen::begin()
{
	if (255 != tirqPin) {
		pinMode(tirqPin, INPUT);
		attachInterrupt(tirqPin, isrPin, FALLING);
		isrPinptr = this;
	}
	return true;
}

void isrPin(void)
{
	XPT2046_Touchscreen *o = isrPinptr;
	o->isrWake = true;
}

TS_Point XPT2046_Touchscreen::getPoint()
{
	update();
	if (rotated) {
		yraw = map(yraw, 0, 240, 240, 0);
		xraw = map(xraw, 0, 320, 320, 0);
	}
	return TS_Point(xraw, yraw, zraw);
}

bool XPT2046_Touchscreen::touched()
{
	update();
	//Prevent double touch
	delay(20);
	return (zraw >= Z_THRESHOLD);
}

void XPT2046_Touchscreen::readData(uint16_t *x, uint16_t *y, uint8_t *z)
{
	update();
	*x = xraw;
	*y = yraw;
	*z = zraw;
}

bool XPT2046_Touchscreen::bufferEmpty()
{
	return ((millis() - msraw) < MSEC_THRESHOLD);
}

static int16_t besttwoavg(int16_t x, int16_t y, int16_t z) {
	int16_t da, db, dc;
	int16_t reta;
	if (x > y) da = x - y; else da = y - x;
	if (x > z) db = x - z; else db = z - x;
	if (z > y) dc = z - y; else dc = y - z;
	if (da <= db && da <= dc) reta = (x + y) >> 1;
	else if (db <= da && db <= dc) reta = (x + z) >> 1;
	else reta = (y + z) >> 1;

	return (reta);
}

void XPT2046_Touchscreen::update()
{
	int16_t data[6];
	if (!isrWake) return;
	uint32_t now = millis();
	if (now - msraw < MSEC_THRESHOLD) return;
	SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
#if defined(__MK20DX256__)
		CORE_PIN13_CONFIG = PORT_PCR_MUX(1);
		CORE_PIN14_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
#endif
	digitalWrite(csPin, LOW);
	SPI.transfer(0xB1);
	int16_t z1 = SPI.transfer16(0xC1) >> 3;
	int z = z1 + 4095;
	int16_t z2 = SPI.transfer16(0x91) >> 3;
	z -= z2;
	if (z >= Z_THRESHOLD) {
		SPI.transfer16(0x91);
		data[0] = SPI.transfer16(0xD1) >> 3;
		data[1] = SPI.transfer16(0x91) >> 3;
		data[2] = SPI.transfer16(0xD1) >> 3;
		data[3] = SPI.transfer16(0x91) >> 3;
	}
	else data[0] = data[1] = data[2] = data[3] = 0;
	data[4] = SPI.transfer16(0xD0) >> 3;
	data[5] = SPI.transfer16(0) >> 3;
	digitalWrite(csPin, HIGH);
#if defined(__MK20DX256__)
		CORE_PIN13_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
		CORE_PIN14_CONFIG = PORT_PCR_MUX(1);
#endif
	SPI.endTransaction();
	if (z < 0) z = 0;
	if (z < Z_THRESHOLD) {
		zraw = 0;
		if (255 != tirqPin) isrWake = false;
		return;
	}
	zraw = z;
	int16_t x = besttwoavg(data[0], data[2], data[4]);
	int16_t y = besttwoavg(data[1], data[3], data[5]);
	if (z >= Z_THRESHOLD) {
		msraw = now;
		xraw = x;
		yraw = y;
	}
	xraw = map(xraw, 230, 3670, 0, 320);
	yraw = map(yraw, 230, 3800, 240, 0);
	if (xraw < 0) xraw = 0;
	if (xraw > 319) xraw = 319;
	if (yraw < 0) yraw = 0;
	if (yraw > 239) yraw = 239;
}
