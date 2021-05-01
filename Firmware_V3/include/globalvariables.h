/*
*
* GLOBAL VARIABLES - Global variable declarations, that are used firmware-wide
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

#ifndef GLOBALVARIABLES_H
#define GLOBALVARIABLES_H

/*################################# INCLUDES ##################################*/

#include <Metro.h>
#include <Bounce.h>
#include <SdFat.h>
#include <ADC.h>

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

extern char versionString[];
extern uint16_t fwVersion;
extern unsigned short* bigBuffer;
extern unsigned short* smallBuffer;
extern Metro screenOff;
extern boolean screenPressed;
extern byte screenOffTime;
extern Bounce buttonDebouncer;
extern Bounce touchDebouncer;
extern SdFat32 sd;
extern File32 sdFile;
extern String sdInfo;
extern File32 dir;
extern char saveFilename[20];
extern ADC *batMeasure;
extern int8_t batPercentage;
extern long batTimer;
extern int8_t batComp;
extern bool convertEnabled;
extern bool autoMode;
extern bool limitsLocked;
extern bool rotationVert;
extern bool rotationHorizont;
extern bool batteryEnabled;
extern bool timeEnabled;
extern bool dateEnabled;
extern bool spotEnabled;
extern bool colorbarEnabled;
extern bool storageEnabled;
extern byte filterType;
extern byte minMaxPoints;
extern bool tempFormat;
extern byte textColor;
extern bool leptonGainMode;
extern byte leptonVersion;
extern byte diagnostic;
extern byte colorScheme;
extern const byte *colorMap;
extern int16_t colorElements;
extern uint16_t maxValue;
extern uint16_t minValue;
extern float spotTemp;
extern float ambTemp;
extern uint16_t minTempPos;
extern uint16_t minTempVal;
extern uint16_t maxTempPos;
extern uint16_t maxTempVal;
extern byte hotColdMode;
extern int16_t hotColdLevel;
extern byte hotColdColor;
extern uint16_t tempPoints[96][2];
extern bool usbConnected;
extern float leptonCalSlope;
extern volatile byte imgSave;
extern volatile byte videoSave;
extern volatile byte showMenu;
extern volatile bool longTouch;
extern volatile bool serialMode;
extern volatile byte loadTouch;
extern volatile bool leptonBufferValid;

#endif /* GLOBALVARIABLES_H */
