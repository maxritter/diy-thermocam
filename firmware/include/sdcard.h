/*
 * SD Card - Methods to access the internal SD storage (SdFs: FAT16/FAT32/exFAT)
 */

#ifndef SD_H
#define SD_H

#include <Arduino.h>

bool beginSD();
void dateTime(uint16_t* date, uint16_t* time);
bool formatCard();
uint32_t getCardSize();       // MB
uint32_t getSDSpace();        // KB (legacy helper, >= 1000 means >= 1 MB)
uint32_t getSDTotalKB();      // KiB (cached)
uint32_t getSDFreeKB();       // KiB (cached)
uint8_t  getSDFsType();       // 0=none, 1=FAT16, 2=FAT32, 3=exFAT
void initSD();
void refreshFreeSpace();
void sdNoteBytesWritten(uint32_t bytes);

#endif /* SD_H */
