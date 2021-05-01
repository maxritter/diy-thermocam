/*
 *
 * BATTERY - Measure the lithium battery status
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
#include <globaldefines.h>
#include <globalvariables.h>
#include <ADC.h>
#include <EEPROM.h>
#include <hardware.h>
#include <gui.h>
#include <battery.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* A method to calculate the lipo percentage out of its voltage */
int getLipoPerc(float vol) {
	if (vol >= 4.10)
		return 100;
	if (vol >= 4.00)
		return 90;
	if (vol >= 3.93)
		return 80;
	if (vol >= 3.88)
		return 70;
	if (vol >= 3.84)
		return 60;
	if (vol >= 3.80)
		return 50;
	if (vol >= 3.76)
		return 40;
	if (vol >= 3.73)
		return 30;
	if (vol >= 3.70)
		return 20;
	if (vol >= 3.66)
		return 10;
	if (vol >= 3.00)
		return 0;
	return -1;
}

/* Measure the battery voltage and convert it to percent */
void checkBattery(bool start, bool calibrate) {
	//Read battery voltage
	float vBat = (batMeasure->analogRead(pin_bat_measure) * 1.5 * 3.3) / batMeasure->adc0->getMaxValue();

	//Check if the device is charging
	int vUSB = analogRead(pin_usb_measure);
	//Battery is not working if no voltage measured and not connected to USB
	if ((vBat == -1) && (vUSB <= 50))
		setDiagnostic(diag_bat);

	//If not charging, add some value to correct the voltage
	if (vUSB <= 50)
		vBat += 0.15;

	//Recalibrate the battery gauge
	if (calibrate)
	{
		//Calculate value to correct
		float compensation = (4.15 - vBat) * 100;
		batComp = (int8_t) round(compensation);

		//Save to EEPROM
		EEPROM.write(eeprom_batComp, batComp);
	}

	//At first launch, read value from EEPROM
	if(start)
		batComp = EEPROM.read(eeprom_batComp);

	//Correct voltage
	if (batComp != 0)
		vBat += (float) batComp / 100.0;

	//Calculate the percentage out of the voltage
	batPercentage = getLipoPerc(vBat);

	//Show warning on startup if the battery is low
	if ((batPercentage <= 20) && (batPercentage != -1) && (start)) {
		showFullMessage((char*) "Battery almost empty, charge");
		delay(1000);
	}
}
