/*
*
* MASS STORAGE -  Mass storage mode to connect the internal storage to the PC
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

#ifndef MASSSTORAGE_H
#define MASSSTORAGE_H

/*########################## PUBLIC PROCEDURES ################################*/

void jumpToApplicationAt0x38980();
void massStorage();
void resetPeripherals();
void restartAndJumpToApp(void);
void startup_late_hook(void);

#endif /* MASSSTORAGE_H */
