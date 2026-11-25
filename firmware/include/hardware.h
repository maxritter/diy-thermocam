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
* https://github.com/maxritter/diy-thermocam
*
*/

#ifndef HARDWARE_H
#define HARDWARE_H

/*########################## PUBLIC PROCEDURES ################################*/

void t4_direct_write_low(volatile uint32_t *base, uint32_t mask);
void t4_direct_write_high(volatile uint32_t *base, uint32_t mask);
bool isUSBConnected();
float bytesToFloat(uint8_t* farray);
void checkHardware();
bool checkDiagnostic(byte device);
void checkFWUpgrade();
bool checkScreenLight();
void clearEEPROM();
void disableScreenLight();
void displayBuffer();
void enableScreenLight();
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
void readEEPROM();
void readTempLimits();
bool screenOffCheck();
void setDiagnostic(byte device);
void setDisplayRotation();
void toggleDisplay();
boolean touchScreenPressed();

#endif /* HARDWARE_H */
