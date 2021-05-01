/*
*
* SD Card - Methods to access the internal SD storage
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

#ifndef SD_H
#define SD_H

/*########################## PUBLIC PROCEDURES ################################*/

bool beginSD();
void clearCache(uint8_t addSig);
void clearFatDir(uint32_t bgn, uint32_t count);
void dateTime(uint16_t* date, uint16_t* time);
bool formatCard();
void formatFAT16();
void formatFAT32();
uint32_t getCardSize();
uint32_t getSDSpace();
void initSD();
uint16_t lbnToCylinder(uint32_t lbn);
uint8_t lbnToHead(uint32_t lbn);
uint8_t lbnToSector(uint32_t lbn);
void refreshFreeSpace();
uint32_t volSerialNumber();
uint8_t writeCache(uint32_t lbn);
void writeMbr();

#endif /* SD_H */
