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
* https://github.com/maxritter/DIY-Thermocam
*
*/

#ifndef SETTINGSMENU_H
#define SETTINGSMENU_H

/*########################## PUBLIC PROCEDURES ################################*/

void adjustCombinedGUI();
void adjustCombinedLoading();
bool adjustCombinedMenu();
void adjustCombinedNewMenuHandler(bool firstStart = false);
void adjustCombinedNewMenu(bool firstStart = false);
bool adjustCombinedPresetSaveMenu();
void adjustCombinedPresetSaveString(int pos);
void adjustCombinedRefresh();
void adjustCombinedString(int pos);
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
void otherMenuHandler();
void otherMenu();
void rotateDisplayMenu(bool firstStart = false);
void screenTimeoutMenu();
void secondMenu(bool firstStart);
void settingsMenuHandler();
void settingsMenu();
void storageMenuHandler();
void storageMenu();
void tempFormatMenu(bool firstStart = false);
void timeMenuHandler(bool firstStart = false);
void timeMenu(bool firstStart = false);
void visualImageMenu(bool firstStart = false);
void yearMenu(bool firstStart);

#endif /* SETTINGSMENU_H */
