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

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>

/*########################## PUBLIC PROCEDURES ################################*/

uint16_t calcAverage();
float rawToTemp(uint16_t rawValue);
float celciusToFahrenheit(float Tc);
float fahrenheitToCelcius(float Tf);
uint16_t tempToRaw(float temp);

#endif /* CALIBRATION_H */
