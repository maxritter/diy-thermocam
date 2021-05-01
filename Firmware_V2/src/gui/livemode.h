/*
*
* LIVE MODE - GUI functions used in the live mode
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

#ifndef LIVEMODE_H
#define LIVEMODE_H

/*########################## PUBLIC PROCEDURES ################################*/

void displayBatteryStatus();
void displayDate();
void displayFreeSpace();
void displayInfos();
void displayMinMaxPoint(bool min);
void displayTempMode();
void displayTime();
void displayWarmup();
void showSpot();

#endif /* LIVEMODE_H */
