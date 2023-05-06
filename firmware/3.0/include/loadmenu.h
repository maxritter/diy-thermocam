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
* https://github.com/maxritter/diy-thermocam
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
void displayGUI(uint32_t imgCount, char* infoText);
void displayVideoFrame(uint32_t imgCount);
uint32_t getVideoFrameNumber();
int loadMenu(char* title, int* array, int length);
void openImage(char* filename, uint32_t imgCount);
void playVideo(char* dirname, uint32_t imgCount);

#endif /* LOADMENU_H */
