/*
*
* CALIBRATION - Functions to convert Lepton raw values to absolute values
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

/*########################## PUBLIC PROCEDURES ################################*/

uint16_t calcAverage();
float calFunction(uint16_t rawValue);
bool calibration();
void calibrationProcess(bool serial = false, bool firstStart = false);
float celciusToFahrenheit(float Tc);
void checkWarmup();
void compensateCalib();
float fahrenheitToCelcius(float Tf);
int linreg(int n, const uint16_t x[], const float y[], float* m, float* b, float* r);
uint16_t tempToRaw(float temp);

#endif /* CALIBRATION_H */
