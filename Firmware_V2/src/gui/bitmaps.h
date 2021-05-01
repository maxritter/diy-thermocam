/*
*
* BITMAPS - Icons and graphics shown inside the GUI
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

#ifndef BITMAPS_H
#define BITMAPS_H

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

extern const uint16_t icon1Colors[];
extern const uint8_t icon1Bitmap[];
extern const uint16_t icon2Colors[];
extern const uint8_t icon2Bitmap[];
extern const uint16_t icon3Colors[];
extern const uint8_t icon3Bitmap[];
extern const uint16_t icon4Colors[];
extern const uint8_t icon4Bitmap[];
extern const uint16_t icon5Colors[];
extern const uint8_t icon5Bitmap_1[];
#if defined(__MK66FX1M0__)
extern const uint8_t icon5Bitmap_2[];
#else
extern const uint8_t* icon5Bitmap_2;
#endif
extern const uint16_t icon6Colors[];
extern const uint8_t icon6Bitmap[];
extern const uint16_t icon7Colors[];
extern const uint8_t icon7Bitmap[];
extern const uint16_t icon8Colors[];
extern const uint8_t icon8Bitmap_1[];
#if defined(__MK66FX1M0__)
extern const uint8_t icon8Bitmap_2[];
#else
extern const uint8_t* icon8Bitmap_2;
#endif
extern const uint16_t icon9Colors[];
extern const uint8_t icon9Bitmap[];
extern const uint16_t icon10Colors[];
extern const uint8_t icon10Bitmap[];
extern const uint16_t icon11_1Colors[];
extern const uint8_t icon11_1Bitmap[];
extern const uint16_t icon11_2Colors[];
extern const uint8_t icon11_2Bitmap[];
extern const uint16_t icon12Colors[];
extern const uint8_t icon12Bitmap[];
extern const uint16_t iconBWColors[];
extern const uint8_t iconBWBitmap[];
extern const uint16_t iconReturnColors[];
extern const uint8_t iconReturnBitmap[];
extern const uint16_t iconFWColors[];
extern const uint8_t iconFWBitmap[];
extern const uint16_t logoColors[];
extern const uint8_t logoBitmap[];

#endif /* BITMAPS_H */
