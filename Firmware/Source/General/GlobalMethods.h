/*
*
* GLOBAL METHODS - Global method definitions, that are used firmware-wide
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

/* Abstract methods */

void showFullMessage(char*, bool small = false);
void setDiagnostic(byte device);
boolean checkDiagnostic(byte device);
void startAltClockline(boolean sdStart = false);
void showDiagnostic();
void endAltClockline();
void bootScreen();
void createSDName(char* filename, boolean folder = false);
void saveBuffer(char* filename);
void saveRawData(bool image, char* name, uint16_t framesCaptured = 0);
void settingsMenu();
void displayMenu();
void storageMenu();
void otherMenu();
void dateMenu(bool firstStart = false);
void timeMenu(bool firstStart = false);
void displayBuffer();
void calibrationProcess(bool serial = false, bool firstStart = false);
void readCalibration();
void createVisCombImg();
void lepton_getRawValues();
void serialConnect();
void showColorBar();
void calculatePointPos(int16_t* xpos, int16_t* ypos, uint16_t pixelIndex);
uint16_t tempToRaw(float temp);
void checkWarmup();
void showTemperatures();
void calibrationScreen(bool firstStart = false);
void createJPEGFile(char* dirname);
void toggleLaser(bool message = false);
void tempPointFunction(bool remove = false);
void convertImage(char* filename);
void convertVideo(char* dirname);
void touchIRQ();
void loadTouchIRQ();
void openImage(char* filename, int imgCount);
void playVideo(char* dirname, int imgCount);
bool massStoragePrompt();
void clearTempPoints();
float calFunction(uint16_t rawValue);
void createThermalImg(bool small = false);
void limitValues();
void changeDisplayOptions(byte* pos);
void changeColorScheme(byte* pos);
void loadFiles();
void settingsMenuHandler();
bool calibration();
bool adjustCombinedMenu();
void loadRawData(char* filename, char* dirname = NULL);
void selectColorScheme();
int loadMenu(char* title, int* array, int length);
void deleteImage(char* filename);
void deleteVideo(char* dirname);
void frameFilename(char* filename, uint16_t count);
uint16_t getVideoFrameNumber(char* dirname);
void processVideoFrames(int framesCaptured, char* dirname);
void displayRawData();
void loadBMPImage(char* filename);
void boxFilter();
void gaussianFilter();
void smallToBigBuffer(bool trans = false);
void convertColors(bool small = false);
void createVideoFolder(char* dirname);
void compensateCalib();
void refreshTempPoints();
void floatToBytes(uint8_t* farray, float val);
float bytesToFloat(uint8_t* farray);
void disableScreenLight();
void enableScreenLight();
boolean extButtonPressed();
void refreshMinMax();
void storeCalibration();
void drawMainMenuBorder();
void displayInfos();
void setDisplayRotation();
void changeTextColor();
void readEEPROM();
void showImage();
boolean checkSDCard();
void bootFFC();
void camera_capture(void);
boolean checkFirstStart();
float celciusToFahrenheit(float Tc);
void getSpotTemp();