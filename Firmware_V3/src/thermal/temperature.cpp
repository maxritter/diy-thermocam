/*
 *
 * TEMPERATURE - Functions to convert Lepton raw values to absolute temperatures and back
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

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Converts a given Temperature in Celcius to Fahrenheit */
float celciusToFahrenheit(float Tc) {
	float Tf = ((float) 9.0 / 5.0) * Tc + 32.0;
	return (Tf);
}

/* Converts a given temperature in Fahrenheit to Celcius */
float fahrenheitToCelcius(float Tf) {
	float Tc = (Tf - 32.0) * ((float) 5.0 / 9.0);
	return Tc;
}

/* Function to calculate temperature out of Lepton value */
float rawToTemp(uint16_t rawValue) {
	//Calculate the temperature in Celcius out of the coefficients
	float temp = (leptonCalSlope * rawValue) - 273.15;

	//Convert to Fahrenheit if needed
	if (tempFormat == tempFormat_fahrenheit)
		temp = celciusToFahrenheit(temp);

	return temp;
}

/* Calculate the lepton value out of an absolute temperature */
uint16_t tempToRaw(float temp) {
	//Convert to Celcius if needed
	if (tempFormat == tempFormat_fahrenheit)
		temp = fahrenheitToCelcius(temp);

	//Calcualte the raw value
	uint16_t rawValue = (temp + 273.15) / leptonCalSlope;
	return rawValue;
}

/* Calculates the average of the 196 (14x14) pixels in the middle */
uint16_t calcAverage() {
	int sum = 0;
	uint16_t val;
	for (byte vert = 52; vert < 66; vert++) {
		for (byte horiz = 72; horiz < 86; horiz++) {
			val = smallBuffer[(vert * 160) + horiz];
			sum += val;
		}
	}
	uint16_t avg = (uint16_t)(sum / 196.0);
	return avg;
}