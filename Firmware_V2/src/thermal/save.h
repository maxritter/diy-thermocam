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

void createBMPFile(char* filename);
void createJPEGFile(char* dirname);
void createSDName(char* filename, boolean folder = false);
void createVideoFolder(char* dirname);
void frameFilename(char* filename, uint16_t count);
void imgSaveEnd();
void imgSaveStart();
void processVideoFrames(int framesCaptured, char* dirname);
void saveBuffer(char* filename);
void saveRawData(bool isImage, char* name, uint16_t framesCaptured = 0);
void saveVideoFrame(char* filename, char* dirname);

#endif /* SAVE_H */
