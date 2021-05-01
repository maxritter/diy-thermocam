/*
 *
 * OV2640Regs - Register for the Arducam camera module
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

#ifndef OV2640_REGS_H
#define OV2640_REGS_H

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

extern const uint8_t exifHeader_rotated[];
extern const struct sensor_reg OV2640_QVGA[] PROGMEM;
extern const struct sensor_reg OV2640_JPEG_INIT[] PROGMEM;
extern const struct sensor_reg OV2640_YUV422[] PROGMEM;
extern const struct sensor_reg OV2640_JPEG[] PROGMEM;
extern const struct sensor_reg OV2640_160x120_JPEG[] PROGMEM;
extern const struct sensor_reg OV2640_176x144_JPEG[] PROGMEM;
extern const struct sensor_reg OV2640_320x240_JPEG[] PROGMEM;
extern const struct sensor_reg OV2640_352x288_JPEG[] PROGMEM;
extern const struct sensor_reg OV2640_640x480_JPEG[] PROGMEM;
extern const struct sensor_reg OV2640_800x600_JPEG[] PROGMEM;
extern const struct sensor_reg OV2640_1024x768_JPEG[] PROGMEM;
extern const struct sensor_reg OV2640_1280x1024_JPEG[] PROGMEM;
extern const struct sensor_reg OV2640_1600x1200_JPEG[] PROGMEM;

#endif /* OV2640_REGS_H */
