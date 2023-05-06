/*
*
* SETTINGS MENU - Adjust different on-device settings
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

#ifndef SETTINGSMENU_H
#define SETTINGSMENU_H

/*########################## PUBLIC PROCEDURES ################################*/

void batteryGauge();
void convertImageMenu(bool firstStart = false);
void dateMenuHandler(bool firstStart = false);
void dateMenu(bool firstStart = false);
void dayMenu(bool firstStart);
void displayMenuHandler();
void displayMenu();
void formatStorage();
void hourMenu(bool firstStart);
void minuteMenu(bool firstStart);
void monthMenu(bool firstStart);
void generalMenuHandler();
void generalMenu();
void rotateDisplayMenu(bool firstStart = false);
void screenTimeoutMenu();
void secondMenu(bool firstStart);
void settingsMenuHandler();
void settingsMenu();
void hardwareMenuHandler();
void hardwareMenu();
void tempFormatMenu(bool firstStart = false);
void timeMenuHandler(bool firstStart = false);
void timeMenu(bool firstStart = false);
void yearMenu(bool firstStart);

#endif /* SETTINGSMENU_H */
