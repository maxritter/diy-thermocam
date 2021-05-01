/*
*
* MAIN MENU - Display the main menu with icons
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

#ifndef MAINMENU_H
#define MAINMENU_H

/*########################## PUBLIC PROCEDURES ################################*/

bool calibrationRepeat();
void calibrationScreen(bool firstStart = false);
bool colorMenu();
void colorMenuString(int pos);
void drawMainMenu(byte pos);
void drawSelectionMenu();
void hotColdChooserHandler();
void hotColdChooser();
bool hotColdColorMenu();
void hotColdColorMenuString(int pos);
bool hotColdMenu();
void hqResolutionMenu();
bool liveDispMenu();
void liveDispMenuString(int pos);
void mainMenuBackground();
void mainMenuHandler(byte* pos);
bool mainMenuSelect(byte pos, byte page);
void mainMenuSelection(char* selection);
void mainMenuTitle(char* title);
void mainMenu();
bool massStoragePrompt();
bool modeMenu();
bool tempLimits();
bool tempLimitsManualHandler();
void tempLimitsManual();
bool tempLimitsPresetSaveMenu();
void tempLimitsPresetSaveString(int pos);
bool tempLimitsPresets();
void tempLimitsPresetsString(int pos);
bool tempPointsMenu();

#endif /* MAINMENU_H */

