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
* https://github.com/maxritter/diy-thermocam
*
*/

#ifndef FIRSTSTART_H
#define FIRSTSTART_H

/*########################## PUBLIC PROCEDURES ################################*/

boolean checkFirstStart();
boolean checkLiveModeHelper();
void firstFormat();
void firstStartComplete();
void firstStart();
void infoScreen(String* text, bool cont = true);
void liveModeHelper();
void stdEEPROMSet();
void tempFormatScreen();
void timeDateScreen();
void welcomeScreen();
void convertImageScreen();

#endif /* FIRSTSTART_H */
