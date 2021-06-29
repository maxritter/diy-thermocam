/*
*
* CONNECTION - Communication protocol for the USB serial data transmission
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

#ifndef CONNECTION_H
#define CONNECTION_H

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

//Start & Stop command
#define CMD_START              0x64
#define CMD_END	               0xC8
#define CMD_INVALID			   0x00

//Serial terminal commands
#define CMD_GET_RAWLIMITS      0x6E
#define CMD_GET_RAWDATA        0x6F
#define CMD_GET_CONFIGDATA     0x70
#define CMD_GET_CALIBDATA      0x72
#define CMD_GET_SPOTTEMP       0x73
#define CMD_GET_TEMPPOINTS     0x75
#define CMD_SET_SHUTTERRUN     0x78
#define CMD_SET_SHUTTERMODE    0x79
#define CMD_SET_FILTERTYPE     0x7A
#define CMD_GET_BATTERYSTATUS  0x7C
#define CMD_GET_DIAGNOSTIC     0x7F
#define CMD_GET_FWVERSION      0x81
#define CMD_SET_LIMITS         0x82
#define CMD_SET_TEXTCOLOR      0x83
#define CMD_SET_COLORSCHEME    0x84
#define CMD_SET_TEMPFORMAT     0x85
#define CMD_SET_SHOWSPOT       0x86
#define CMD_SET_SHOWCOLORBAR   0x87
#define CMD_SET_SHOWMINMAX     0x88
#define CMD_SET_TEMPPOINTS     0x89
#define CMD_GET_HWVERSION      0x8A
#define CMD_SET_ROTATION       0x8B

//Serial frame commands
#define CMD_FRAME_RAW          0x96
#define CMD_FRAME_COLOR        0x97
#define CMD_FRAME_DISPLAY      0x98
#define CMD_FRAME_SAVE         0x99

//Types of raw frame responses
#define FRAME_CAPTURE_THERMAL  0xB4
#define FRAME_CAPTURE_VIDEO    0xB6
#define FRAME_NORMAL           0xB7

/*########################## PUBLIC PROCEDURES ################################*/

void buttonHandler();
bool checkNoDisplay();
void checkSerial();
int getInt(String text);
void saveFrame();
void sendBatteryStatus();
void sendCalibrationData();
void sendConfigData();
void sendDiagnostic();
void sendDisplayFrame();
void sendFramebuffer();
void sendFrame(bool color);
void sendFWVersion();
void sendHardwareVersion();
void sendRawData(bool color = false);
void sendRawLimits();
void sendSpotTemp();
void sendTempPoints();
void serialConnect();
bool serialHandler();
void serialInit();
void serialOutput();
void setColorScheme();
void setFilterType();
void setLimits();
void setMinMax();
void setRotation();
void setShowColorbar();
void setShowSpot();
void setShutterMode();
void setTempFormat();
void setTempPoints();
void setTextColor();
void setTime();
bool touchHandler();

#endif /* CONNECTION_H */
