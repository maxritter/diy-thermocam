/*
*
* LOAD - Load images and videos from the internal storage
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

#ifndef LOAD_H
#define LOAD_H

/*########################## PUBLIC PROCEDURES ################################*/

void checkFileEnding(bool* check, char* filename);
void checkFileStructure(bool* check);
bool checkFileValidity();
void chooseFile(char* filename);
void clearData();
void copyIntoBuffers(char* filename);
bool dayChoose(bool* days, char* filename);
void displayRawData();
bool findFile(char* filename, bool next, bool restart, int* position = 0, char* compare = NULL);
bool hourChoose(bool* hours, char* filename);
bool isImage(char* filename);
void loadAlloc();
void loadBMPImage(char* filename);
void loadDeAlloc();
bool loadDelete(char* filename, int* pos);
void loadFiles();
void loadFind(char* filename, int* pos);
void loadRawData(char* filename);
void loadSettings();
void loadTouchIRQ();
bool minuteChoose(bool* minutes, char* filename);
bool monthChoose(bool* months, char* filename);
void readTempPoints();
void searchFiles();
bool secondChoose(bool* seconds, char* filename);
bool yearChoose(char* filename);

#endif /* LOAD_H */
