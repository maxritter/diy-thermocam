/*
*
* GUI - Main Methods to lcd the Graphical-User-Interface
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

#ifndef GUI_H
#define GUI_H

/*########################## PUBLIC PROCEDURES ################################*/

void bootScreen();
void changeTextColor();
void drawCenterElement(int element);
void drawMainMenuBorder();
void drawTitle(char* name, bool firstStart = false);
void floatToChar(char* buffer, float val);
void showDiagnostic();
void showFullMessage(char* message, bool small = false);
void showTransMessage(char* msg);

#endif /* GUI_H */
