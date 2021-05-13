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

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <Metro.h>
#include <Bounce.h>
#include <SdFat.h>
#include <ADC.h>
#include <globalvariables.h>

/*############################# PUBLIC VARIABLES ##############################*/

//Current firmware version
char versionString[] = "Firmware 3.02 from 13.05.2021";
uint16_t fwVersion = 302;

//320x240 buffer
unsigned short* bigBuffer;
//160x120 buffer
unsigned short* smallBuffer;

//Timer
Metro screenOff;
boolean screenPressed;
byte screenOffTime;

//Button Debouncer
Bounce buttonDebouncer(pin_button, 200);
Bounce touchDebouncer(pin_touch_irq, 200);

//SD
SdFat32 sd;
File32 sdFile;
String sdInfo;
File32 dir;

//Save filename
char saveFilename[20];

//Battery ADC
ADC *batMeasure;
//Battery
int8_t batPercentage;
long batTimer;
int8_t batComp;
bool usbConnected;

//Convert RAW to BMP
bool convertEnabled;
//Automatic mode
bool autoMode;
//Lock current limits
bool limitsLocked;
//Vertical display rotation
bool rotationVert;
bool rotationHorizont;

//Display options
bool batteryEnabled;
bool timeEnabled;
bool dateEnabled;
bool spotEnabled;
bool colorbarEnabled;
bool storageEnabled;
byte filterType;
byte minMaxPoints;

//Temperature format
bool tempFormat;

//Text color
byte textColor;

//Lepton Gain mode
bool leptonGainMode;

//Calibration slope
float leptonCalSlope;

//FLIR Lepton sensor version
byte leptonVersion;

//HW diagnostic information
byte diagnostic = diag_ok;

//Current color scheme - standard is rainbow
byte colorScheme;
//Pointer to the current color scheme
const byte *colorMap;
//Number of rgb elements inside the color scheme
int16_t colorElements;

//Min & max lepton raw values
uint16_t maxValue;
uint16_t minValue;

//Spot & ambient temperature
float spotTemp;
float ambTemp;

//Position of min and maxtemp
uint16_t minTempPos;
uint16_t minTempVal;
uint16_t maxTempPos;
uint16_t maxTempVal;

//Hot / Cold mode
byte hotColdMode;
int16_t hotColdLevel;
byte hotColdColor;

//Array to store the tempPoints
uint16_t tempPoints[96][2];

//Adjust combined image
float adjCombAlpha;
float adjCombFactor;
byte adjCombLeft;
byte adjCombRight;
byte adjCombUp;
byte adjCombDown;

//Save Image in the next cycle
volatile byte imgSave;
//Save Video in the next cycle
volatile byte videoSave;
//Show Live Mode Menu in the next cycle
volatile byte showMenu;
//Handler for a long touch press
volatile bool longTouch;
//Check if in serial mode
volatile bool serialMode;
//Load touch decision marker
volatile byte loadTouch;
//Current buffer valid
volatile bool leptonBufferValid;
//Display is currently updated, do not use SPI in IRQ
volatile bool displayUpdated;
