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
#define CMD_START              100
#define CMD_END	               200
#define CMD_INVALID				 0

//Serial terminal commands
#define CMD_GET_RAWLIMITS      110
#define CMD_GET_RAWDATA        111
#define CMD_GET_CONFIGDATA     112
#define CMD_GET_CALSTATUS      113
#define CMD_GET_CALIBDATA      114
#define CMD_GET_SPOTTEMP       115
#define CMD_SET_TIME           116
#define CMD_GET_TEMPPOINTS     117
#define CMD_SET_LASER          118
#define CMD_GET_LASER          119
#define CMD_SET_SHUTTERRUN     120
#define CMD_SET_SHUTTERMODE    121
#define CMD_SET_FILTERTYPE     122
#define CMD_GET_SHUTTERMODE    123
#define CMD_GET_BATTERYSTATUS  124
#define CMD_SET_CALSLOPE       125
#define CMD_SET_CALOFFSET      126
#define CMD_GET_DIAGNOSTIC     127
#define CMD_GET_VISUALIMG      128
#define CMD_GET_FWVERSION      129
#define CMD_SET_LIMITS         130
#define CMD_SET_TEXTCOLOR      131
#define CMD_SET_COLORSCHEME    132
#define CMD_SET_TEMPFORMAT     133
#define CMD_SET_SHOWSPOT       134
#define CMD_SET_SHOWCOLORBAR   135
#define CMD_SET_SHOWMINMAX     136
#define CMD_SET_TEMPPOINTS     137
#define CMD_GET_HWVERSION      138
#define CMD_SET_ROTATION       139
#define CMD_SET_CALIBRATION    140
#define CMD_GET_HQRESOLUTION   141

//Serial frame commands
#define CMD_FRAME_RAW          150
#define CMD_FRAME_COLOR        151
#define CMD_FRAME_DISPLAY      152
#define CMD_FRAME_SAVE         153

//Types of raw frame responses
#define FRAME_CAPTURE_THERMAL  180
#define FRAME_CAPTURE_VISUAL   181
#define FRAME_CAPTURE_VIDEO    182
#define FRAME_NORMAL           183

/*########################## PUBLIC PROCEDURES ################################*/

void buttonHandler();
void checkForUpdater();
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
void sendHQResolution();
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
