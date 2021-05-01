/*
*
* MAIN SKETCH - Main entry point for the firmware
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
#include <connection.h>
#include <firststart.h>
#include <thermal.h>
#include <hardware.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Main entry point */
void setup()
{
	//Init the hardware components
	initHardware();

	//Check for firmware upgrade done
	checkFWUpgrade();

	//Enter USB connection if no display attached
	if (checkNoDisplay())
		serialInit();

	//Check for hardware issues
	checkHardware();

	//Do the first start setup if required
	if (checkFirstStart())
		firstStart();

	//Read all settings from EEPROM
	readEEPROM();

	//Show the live mode helper if required
	if (checkLiveModeHelper())
		liveModeHelper();

	//Go to the live Mode
	liveMode();
}

/* Loop not used */
void loop()
{
}
