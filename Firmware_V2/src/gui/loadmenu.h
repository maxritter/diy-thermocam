/*
*
* LOAD MENU - Display the menu to load images and videos
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

#ifndef LOADMENU_H
#define LOADMENU_H

/*########################## PUBLIC PROCEDURES ################################*/

void convertImage(char* filename);
bool convertPrompt();
void convertVideo(char* dirname);
void deleteImage(char* filename);
void deleteVideo(char* dirname);
void displayGUI(int imgCount, char* infoText);
void displayVideoFrame(int i, char* dirname);
uint16_t getVideoFrameNumber(char* dirname);
int loadMenu(char* title, int* array, int length);
void openImage(char* filename, int imgCount);
void playVideo(char* dirname, int imgCount);

#endif /* LOADMENU_H */
