/*
*
* FIRST START - Menu that is displayed on the first device start
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

#ifndef FIRSTSTART_H
#define FIRSTSTART_H

/*########################## PUBLIC PROCEDURES ################################*/

void calibrationHelperScreen();
boolean checkFirstStart();
boolean checkLiveModeHelper();
void combinedAlignmentScreen();
void convertImageScreen();
void firstFormat();
void firstStartComplete();
void firstStart();
void infoScreen(String* text, bool cont = true);
void liveModeHelper();
void stdEEPROMSet();
void tempFormatScreen();
void timeDateScreen();
void visualImageScreen();
void welcomeScreen();

#endif /* FIRSTSTART_H */
