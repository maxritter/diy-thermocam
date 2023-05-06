/*
*
* THERMAL - Main functions in the live mode
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

#ifndef THERMAL_H
#define THERMAL_H

/*########################## PUBLIC PROCEDURES ################################*/

void buttonIRQ();
void changeColorScheme(byte* pos);
void changeDisplayOptions(byte* pos);
void liveModeInit();
void liveMode();
void longTouchHandler();
void selectColorScheme();
void showColorBar();
void showImage();
void touchIRQ();

#endif /* THERMAL_H */
