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

#include <Arduino.h>

/*########################## PUBLIC PROCEDURES ################################*/

void enterMassStorage();
void setMassStorage();
void checkMassStorage();

#endif /* MASSSTORAGE_H */
