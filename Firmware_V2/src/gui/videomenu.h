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
* https://github.com/maxritter/DIY-Thermocam
*
*/

#ifndef VIDEOMENU_H
#define VIDEOMENU_H

/*########################## PUBLIC PROCEDURES ################################*/

void videoCaptureInterval(int16_t* remainingTime, int* framesCaptured, char* dirname);
void videoCaptureNormal(char* dirname, int* framesCaptured);
void videoCapture();
bool videoIntervalChooser();
bool videoIntervalHandler(byte* pos);
void videoIntervalString(int pos);
void videoMode();

#endif /* VIDEOMENU_H */
