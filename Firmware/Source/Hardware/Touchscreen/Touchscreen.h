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

#include "FT6206_Touchscreen.h"
#include "XPT2046_Touchscreen.h"

//Resistive Touch Controller
XPT2046_Touchscreen resTouch;
//Capacitive Touch Controller
FT6206_Touchscreen capTouch;
//Choose the right touch screen
bool touch_capacitive;

/* Returns the coordinates of the touched point */
TS_Point touch_getPoint() {
	//Get touch from capacitive
	if (touch_capacitive)
		return capTouch.getPoint();
	//Get point from resistive
	return resTouch.getPoint();
}

/* Initializes the touch module and checks if it is working */
void touch_init() {
	//Capacitive screen
	if (capTouch.begin())
		touch_capacitive = true;

	//Resistive screen or none
	else {
		resTouch.begin();
		touch_capacitive = false;
	}

	//If not capacitive, check if connected
	if (!touch_capacitive)
	{
		//Get a point
		TS_Point point = touch_getPoint();
		//Wait short
		delay(10);
		//Read one time to stabilize
		digitalRead(pin_touch_irq);

		//Init touch status
		bool touchStatus = true;
		//Check IRQ 10 times, should be HIGH
		for (int i = 0; i < 10; i++)
		{
			if (!digitalRead(pin_touch_irq))
				touchStatus = false;
			delay(10);
		}

		//Comparison value depending on rotation
		uint16_t xval, yval;
		if (rotationVert) {
			xval = 320;
			yval = 240;
		}
		else {
			xval = 0;
			yval = 0;
		}

		//Check if touch is working, otherwise set diagnostic
		if (!(((point.x == xval) && (point.y == yval) && (touchStatus == true))
			|| ((point.x != xval) && (point.y != yval) && (touchStatus == false))))
			setDiagnostic(diag_touch);
	}
}

/* Returns if the screen is currently touched */
bool touch_touched() {
	bool touch;
	//Check for touch, capacitive or resistive
	if (touch_capacitive)
		touch = capTouch.touched();
	else
		touch = resTouch.touched();
	//If touch registered, set screen pressed marker
	if (touch)
		screenPressed = true;
	return touch;
}

/* Set rotation for touch screen */
void touch_setRotation(bool rotated) {
	if (touch_capacitive)
		capTouch.rotated = rotated;
	else
		resTouch.rotated = rotated;
}