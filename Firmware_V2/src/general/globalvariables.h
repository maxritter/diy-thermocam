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

extern unsigned short* bigBuffer;
extern unsigned short* smallBuffer;
extern Metro screenOff;
extern boolean screenPressed;
extern byte screenOffTime;
extern Bounce buttonDebouncer;
extern Bounce touchDebouncer;
extern SdFat sd;
extern SdFile sdFile;
extern String sdInfo;
extern char saveFilename[20];
extern ADC *batMeasure;
extern int8_t batPercentage;
extern long batTimer;
extern int8_t batComp;
extern bool convertEnabled;
extern bool visualEnabled;
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
extern bool laserEnabled;
extern byte displayMode;
extern bool hqRes;
extern bool gainMode;
extern byte leptonVersion;
extern byte leptonShutter;
extern boolean mlx90614Version;
extern byte teensyVersion;
extern byte diagnostic;
extern byte colorScheme;
extern const byte *colorMap;
extern int16_t colorElements;
extern float calOffset;
extern float calSlope;
extern byte calStatus;
extern float calComp;
extern long calTimer;
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
extern float adjCombAlpha;
extern float adjCombFactor;
extern byte adjCombLeft;
extern byte adjCombRight;
extern byte adjCombUp;
extern byte adjCombDown;
extern volatile byte imgSave;
extern volatile byte videoSave;
extern volatile byte showMenu;
extern volatile bool longTouch;
extern volatile bool serialMode;
extern volatile byte loadTouch;

#endif /* GLOBALVARIABLES_H */
