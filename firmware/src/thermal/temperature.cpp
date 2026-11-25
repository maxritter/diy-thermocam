/*
 * TEMPERATURE - Convert Lepton raw values to absolute temperatures and back
 */

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <tc_thermal_core.h>

float celciusToFahrenheit(float Tc) {
	return tc_core::celsius_to_fahrenheit(Tc);
}

float fahrenheitToCelcius(float Tf) {
	return tc_core::fahrenheit_to_celsius(Tf);
}

float rawToTemp(uint16_t rawValue) {
	return tc_core::raw_to_temp(rawValue, leptonCalSlope, tempFormat == tempFormat_fahrenheit);
}

uint16_t tempToRaw(float temp) {
	return tc_core::temp_to_raw(temp, leptonCalSlope, tempFormat == tempFormat_fahrenheit);
}

uint16_t calcAverage() {
	uint32_t sum = 0;
	for (byte vert = 52; vert < 66; vert++) {
		for (byte horiz = 72; horiz < 86; horiz++) {
			sum += smallBuffer[(vert * 160) + horiz];
		}
	}
	return static_cast<uint16_t>((sum + 98u) / 196u);
}
