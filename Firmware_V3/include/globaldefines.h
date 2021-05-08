/*
*
* GLOBAL DEFINES - Global defines, that are used firmware-wide
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

#ifndef GLOBALDEFINES_H
#define GLOBALDEFINES_H

#ifdef __cplusplus
extern "C" {
#endif

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

//Pins
#define pin_button        2
#define pin_lepton_vsync  3
#define pin_touch_irq     5
#define pin_touch_cs      9
#define pin_lcd_dc        10
#define pin_mosi          11
#define pin_miso          12
#define pin_sck           13
#define pin_sda           18
#define pin_scl           19
#define pin_lcd_cs        21
#define pin_lcd_backlight 22
#define pin_bat_measure   23
#define pin_mosi1         26
#define pin_sck1          27
#define pin_lepton_cs     38
#define pin_miso1         39
#define pin_usb_measure   A16

//FLIR Lepton sensor version
#define leptonVersion_2_5_shutter 0 //FLIR Lepton 2.5 Shuttered
#define leptonVersion_3_5_shutter 1 //FLIR Lepton 3.5 Shuttered

//Temperature format
#define tempFormat_celcius    0
#define tempFormat_fahrenheit 1

//Filter type
#define filterType_none     0
#define filterType_gaussian 1
#define filterType_box      2

//Display Min/Max Points
#define minMaxPoints_disabled 0
#define minMaxPoints_min      1
#define minMaxPoints_max      2
#define minMaxPoints_both     3

//Text color
#define textColor_white 0
#define textColor_black 1
#define textColor_red   2
#define textColor_green 3
#define textColor_blue  4

//Screen off time
#define screenOffTime_disabled 0
#define screenOffTime_5min     1
#define screenOffTime_20min    2

//Hot / cold
#define hotColdMode_disabled 0
#define hotColdMode_cold     1
#define hotColdMode_hot      2

//Hot / cold color
#define hotColdColor_white 0
#define hotColdColor_black 1
#define hotColdColor_red   2
#define hotColdColor_green 3
#define hotColdColor_blue  4

//Lepton Gain mode
#define lepton_gain_high 0
#define lepton_gain_low  1

//EEPROM registers
#define eeprom_leptonVersion    100
#define eeprom_tempFormat       101
#define eeprom_colorScheme      102
#define eeprom_convertEnabled   103
#define eeprom_spotEnabled      105
#define eeprom_colorbarEnabled  106
#define eeprom_batteryEnabled   107
#define eeprom_timeEnabled      108
#define eeprom_dateEnabled      109
#define eeprom_storageEnabled   111
#define eeprom_rotationVert     112
#define eeprom_textColor        114
#define eeprom_filterType       115
#define eeprom_minValue1Low     116
#define eeprom_minValue1High    117
#define eeprom_maxValue1Low     118
#define eeprom_maxValue1High    119
#define eeprom_minMax1Set       120
#define eeprom_adjComb1Left     121
#define eeprom_adjComb1Right    122
#define eeprom_adjComb1Up       123
#define eeprom_adjComb1Down     124
#define eeprom_adjComb1Alpha    125
#define eeprom_adjComb1Set      126
#define eeprom_minMaxPoints     127
#define eeprom_screenOffTime    128
#define eeprom_massStorage      129
#define eeprom_hotColdMode      135
#define eeprom_hotColdLevelLow  136
#define eeprom_hotColdLevelHigh 137
#define eeprom_hotColdColor     138
#define eeprom_firstStart       150
#define eeprom_liveHelper       151
#define eeprom_minValue2Low     154
#define eeprom_minValue2High    155
#define eeprom_maxValue2Low     156
#define eeprom_maxValue2High    157
#define eeprom_minMax2Set       158
#define eeprom_minValue3Low     159
#define eeprom_minValue3High    160
#define eeprom_maxValue3Low     161
#define eeprom_maxValue3High    162
#define eeprom_minMax3Set       163
#define eeprom_minMaxPreset     164
#define eeprom_noShutter        169
#define eeprom_batComp			170
#define eeprom_rotationHorizont 171
#define eeprom_minMax1Comp      172 //4 Byte (172-175)
#define eeprom_minMax2Comp      176 //4 Byte (176-179)
#define eeprom_minMax3Comp      180 //4 Byte (180-183)
#define eeprom_lepton_gain      184
#define eeprom_fwVersionLow     250
#define eeprom_fwVersionHigh    251
#define eeprom_setValue         200

//Presets for min/max
#define minMax_temporary  0
#define minMax_preset1    1
#define minMax_preset2    2
#define minMax_preset3    3

//Hardware diagnostic bit codes
#define diag_display 0
#define diag_touch 1
#define diag_sd 2
#define diag_bat 3
#define diag_lepton 4
#define diag_ok 255

//Color scheme numbers
#define colorSchemeTotal          19
#define colorScheme_arctic        0
#define colorScheme_blackHot      1
#define colorScheme_blueRed       2
#define colorScheme_coldest       3
#define colorScheme_contrast      4
#define colorScheme_doubleRainbow 5
#define colorScheme_grayRed       6
#define colorScheme_glowBow       7
#define colorScheme_grayscale     8
#define colorScheme_hottest       9
#define colorScheme_ironblack     10
#define colorScheme_lava          11
#define colorScheme_medical       12
#define colorScheme_rainbow       13
#define colorScheme_wheel1        14
#define colorScheme_wheel2        15
#define colorScheme_wheel3        16
#define colorScheme_whiteHot      17
#define colorScheme_yellow        18

//Image save marker
#define imgSave_disabled 0
#define imgSave_save     1
#define imgSave_set      2
#define imgSave_create   3

//Video save marker
#define videoSave_disabled   0
#define videoSave_menu       1
#define videoSave_recording  2
#define videoSave_processing 3

//Show menu state
#define showMenu_disabled 0
#define showMenu_desired  1
#define showMenu_opened   2

//Load touch decision marker
#define loadTouch_none     0
#define loadTouch_find     1
#define loadTouch_delete   2
#define loadTouch_previous 3
#define loadTouch_next     4
#define loadTouch_exit     5
#define loadTouch_convert  6
#define loadTouch_middle   7

#ifdef __cplusplus
}
#endif

#endif /* GLOBALDEFINES_H */
