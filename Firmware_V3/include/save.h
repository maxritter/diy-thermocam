/*
*
* SAVE - Save images and videos to the internal storage
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

#ifndef SAVE_H
#define SAVE_H

/*########################## PUBLIC PROCEDURES ################################*/

void createSDName(char* filename, boolean folder = false);
void frameFilename(char* filename, uint32_t count);
void imgSaveEnd();
void imgSaveStart();
void processVideoFrames(uint32_t framesCaptured, char* dirname);
void saveBuffer(char* filename);
void saveRawData(bool isImage, char* name, uint32_t framesCaptured = 0);
void saveVideoFrame(char* filename);

#endif /* SAVE_H */
