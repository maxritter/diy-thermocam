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
* https://github.com/maxritter/diy-thermocam
*
*/

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <touchscreen.h>
#include <ft6206_touchscreen.h>
#include <xpt2046_touchscreen.h>
#include <Metro.h>
#include <Bounce.h>
#include <SdFat.h>
#include <ADC.h>
#include <globalvariables.h>
#include <SPI.h>
#include <hardware.h>

/* Probe XPT2046 via TEMP0 (0x84) / TEMP1 (0xF4) ADC reads to detect disconnected SPI (#115) */
static bool xpt2046_probe_spi() {
	SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
	digitalWrite(pin_touch_cs, LOW);
	SPI.transfer(0x84); // TEMP0, 12-bit, single-ended, power-down between conversions
	uint16_t t0 = (SPI.transfer16(0xF4) >> 3) & 0x0FFFu;
	uint16_t t1 = (SPI.transfer16(0x00) >> 3) & 0x0FFFu;
	digitalWrite(pin_touch_cs, HIGH);
	SPI.endTransaction();
	// Disconnected MISO reads all-0 or all-1 (0x000 or 0xFFF)
	return (t0 > 10 && t0 < 4085) || (t1 > 10 && t1 < 4085);
}

/*######################### STATIC DATA DECLARATIONS ##########################*/

//Resistive Touch Controller
static XPT2046_Touchscreen resTouch;
//Capacitive Touch Controller
static FT6206_Touchscreen capTouch;

/*############################# PUBLIC VARIABLES ##############################*/

//Choose the right touch screen
volatile bool touch_capacitive;

/*###################### PRIVATE FUNCTION BODIES ##############################*/

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

	//If not capacitive, check if resistive XPT2046 is genuinely present (#115)
	if (!touch_capacitive)
	{
		if (!xpt2046_probe_spi()) {
			setDiagnostic(diag_touch);
			return;
		}
		//Ensure IRQ pin is not stuck low when untouched
		delay(10);
		digitalRead(pin_touch_irq);
		bool irqHigh = false;
		for (int i = 0; i < 10; i++)
		{
			if (digitalRead(pin_touch_irq))
				irqHigh = true;
			delay(5);
		}
		if (!irqHigh)
			setDiagnostic(diag_touch);
	}
}

/* Returns if the screen is currently touched */
volatile bool touch_touched() {
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
