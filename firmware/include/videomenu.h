/*
*
* VIDEO MENU - Record single frames or time interval videos
*
* DIY-Thermocam Firmware
*
* GNU General Public License v3.0
*
* Copyright by Max Ritter
*
* http://www.diy-thermocam.net
* https://github.com/maxritter/diy-thermocam
*
*/

#ifndef VIDEOMENU_H
#define VIDEOMENU_H

/*########################## PUBLIC PROCEDURES ################################*/

void drawVideoModeMenu();
void drawVideoRecordingOverlay(uint32_t framesCaptured, uint32_t elapsedSec);
void videoCaptureInterval(uint32_t* nextCaptureMs, uint32_t* framesCaptured, uint16_t* folderFrames, char* buffer, char* dirName);
void videoCaptureNormal(uint32_t* framesCaptured, uint16_t* folderFrames, char* buffer, char* dirName);
void videoCapture();
bool videoIntervalChooser();
bool videoIntervalHandler(byte* pos);
void videoIntervalString(int pos);
void videoMode();

#endif /* VIDEOMENU_H */
