/*
*
* HARDWARE - Main hardware functions
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

#ifndef HARDWARE_H
#define HARDWARE_H

/*########################## PUBLIC PROCEDURES ################################*/

float bytesToFloat(uint8_t* farray);
void checkHardware();
bool checkDiagnostic(byte device);
void checkFWUpgrade();
void checkNoFFC();
bool checkScreenLight();
boolean checkSDCard();
void clearEEPROM();
void detectTeensyVersion();
void disableScreenLight();
void displayBuffer();
void enableScreenLight();
void endAltClockline();
boolean extButtonPressed();
void floatToBytes(uint8_t* farray, float val);
void getSpotTemp();
time_t getTeensy3Time();
void initADC();
void initBuffer();
void initGPIO();
void initHardware();
void initI2C();
void initRTC();
void initScreenOffTimer();
void initSPI();
void readAdjustCombined();
void readCalibration();
void readEEPROM();
void readTempLimits();
bool screenOffCheck();
void setDiagnostic(byte device);
void setDisplayRotation();
void startAltClockline(boolean sdStart = false);
void storeCalibration();
void toggleDisplay();
void toggleLaser(bool message = false);
boolean touchScreenPressed();

#endif /* HARDWARE_H */
