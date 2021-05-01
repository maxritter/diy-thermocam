/*
*
* Touchscreen - FT6206 or XPT2046 controller
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

#ifndef TOUCHSCREEN_H
#define TOUCHSCREEN_H

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

class TS_Point {
public:
	TS_Point(void) : x(0), y(0), z(0) {}
	TS_Point(int16_t x, int16_t y, int16_t z) : x(x), y(y), z(z) {}
	bool operator==(TS_Point p) { return ((p.x == x) && (p.y == y) && (p.z == z)); }
	bool operator!=(TS_Point p) { return ((p.x != x) || (p.y != y) || (p.z != z)); }
	int16_t x, y, z;
};

extern volatile bool touch_capacitive;

/*########################## PUBLIC PROCEDURES ################################*/

TS_Point touch_getPoint();
void touch_init();
void touch_setRotation(bool rotated);
volatile bool touch_touched();


#endif /* TOUCHSCREEN_H */
