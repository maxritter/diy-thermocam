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

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <touchscreen.h>
#include <loadmenu.h>
#include <save.h>
#include <sdcard.h>
#include <hardware.h>
#include <mainmenu.h>
#include <lepton.h>
#include <create.h>
#include <gui.h>
#include <livemode.h>
#include <fonts.h>
#include <buttons.h>
#include <videomenu.h>

/*######################### STATIC DATA DECLARATIONS ##########################*/

//Video save interval in seconds
static int16_t videoInterval;

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Switch the video interval string*/
void videoIntervalString(int pos) {
	char* text = (char*) "";
	switch (pos) {
		//1 second
	case 0:
		text = (char*) "1 second";
		break;
		//5 seconds
	case 1:
		text = (char*) "5 seconds";
		break;
		//10 seconds
	case 2:
		text = (char*) "10 seconds";
		break;
		//20 seconds
	case 3:
		text = (char*) "20 seconds";
		break;
		//30 seconds
	case 4:
		text = (char*) "30 seconds";
		break;
		//1 minute
	case 5:
		text = (char*) "1 minute";
		break;
		//5 minutes
	case 6:
		text = (char*) "5 minutes";
		break;
		//10 minutes
	case 7:
		text = (char*) "10 minutes";
		break;
	}
	//Draws the current selection
	mainMenuSelection(text);
}

/* Touch Handler for the video interval chooser */
bool videoIntervalHandler(byte* pos) {
	//Main loop
	while (true) {
		//Touch screen pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//SELECT
			if (pressedButton == 3) {
				switch (*pos) {
					//1 second
				case 0:
					videoInterval = 1;
					break;
					//5 seconds
				case 1:
					videoInterval = 5;
					break;
					//10 seconds
				case 2:
					videoInterval = 10;
					break;
					//20 seconds
				case 3:
					videoInterval = 20;
					break;
					//30 seconds
				case 4:
					videoInterval = 30;
					break;
					//1 minute
				case 5:
					videoInterval = 60;
					break;
					//5 minutes
				case 6:
					videoInterval = 300;
					break;
					//10 minutes
				case 7:
					videoInterval = 600;
					break;
				}
				return true;
			}
			//BACK
			else if (pressedButton == 2) {
				return false;
			}
			//BACKWARD
			else if (pressedButton == 0) {
				if (*pos > 0)
					*pos = *pos - 1;
				else if (*pos == 0)
					*pos = 7;
			}
			//FORWARD
			else if (pressedButton == 1) {
				if (*pos < 7)
					*pos = *pos + 1;
				else if (*pos == 7)
					*pos = 0;
			}
			//Change the menu name
			videoIntervalString(*pos);
		}
	}
}

/* Start video menu to choose interval */
bool videoIntervalChooser() {
	bool rtn;
	static byte videoIntervalPos = 0;
	//Background
	mainMenuBackground();
	//Title
	mainMenuTitle((char*) "Choose interval");
	//Draw the selection menu
	drawSelectionMenu();
	//Current choice name
	videoIntervalString(videoIntervalPos);
	//Touch handler - return true if exit to Main menu, otherwise false
	rtn = videoIntervalHandler(&videoIntervalPos);
	//Restore old fonts
	display_setFont(smallFont);
	buttons_setTextFont(smallFont);
	//Delete the old buttons
	buttons_deleteAllButtons();
	return rtn;
}

/* Captures video frames in an interval */
void videoCaptureInterval(int16_t* remainingTime, uint32_t* framesCaptured, uint16_t* folderFrames, char* buffer, char* dirName) {
	//Measure time
	long measure = millis();

	//If there is no more time or the first frame
	if ((*remainingTime <= 0) || (*folderFrames == 0)) {
		saveRawData(false, dirName, *folderFrames);
		*folderFrames = *folderFrames + 1;
	}

	//Convert lepton data to RGB565 colors
	convertColors();

	//Display infos
	displayInfos();

	//Write to image buffer
	display_writeToImage = true;

	//Display title
	display_print((char*) "Interval capture", 105, 20);

	//Show saving message
	if ((*remainingTime <= 0) || (*framesCaptured == 0))
		sprintf(buffer, "Saving now..");
	//Show waiting time
	else
		sprintf(buffer, "Saving in %ds", *remainingTime);

	//Display message on buffer
	display_print(buffer, 120, 200);

	//Disable image buffer
	display_writeToImage = false;

	//Draw thermal image on screen
	displayBuffer();

	//If there is no more time or the first frame
	if ((*remainingTime <= 0) || (*framesCaptured == 0)) {
		*remainingTime = videoInterval;
		*framesCaptured = *framesCaptured + 1;
	}
	else
	{
		//Wait rest of the time
		measure = millis() - measure;
		if (measure < 1000)
			delay(1000 - measure);

		//Decrease remaining time by one
		*remainingTime -= 1;
	}
}

/* Normal video capture */
void videoCaptureNormal(uint32_t* framesCaptured, uint16_t* folderFrames, char* buffer, char* dirName) {
	//Save video raw frame
	saveRawData(false, dirName, *folderFrames);
	*folderFrames = *folderFrames + 1;

	//Convert the colors
	convertColors();

	//Display infos
	displayInfos();

	//Write to image buffer
	display_writeToImage = true;

	//Display title
	display_print((char*) "Video capture", 115, 20);

	//Raise capture counter
	*framesCaptured = *framesCaptured + 1;

	//Display current frames captured
	sprintf(buffer, "Frames captured: %6lu", *framesCaptured);
	display_print(buffer, 70, 200);

	//Disable image buffer
	display_writeToImage = false;

	//Refresh capture
	displayBuffer();
}

void videoCreateFolder(char *dirName) {
	createSDName(dirName, true);
	if(!sd.chdir("/"))
	{
		beginSD();
		if(!sd.chdir("/"))
		{
			showFullMessage((char*) "Error creating folder!");
			delay(1000);
			return;
		}
	}
	sd.mkdir(dirName);
	sd.chdir(dirName);
}

/* This screen is shown during the video capture */
void videoCapture() {
	//Help variables
	char dirName[20];
	char buffer[30];
	int16_t delayTime = videoInterval;
	uint32_t framesCaptured = 0;
	uint16_t folderFrames = 0;

	//Show message
	showFullMessage((char*)"Touch screen to turn it on/off");
	display_print((char*) "CAPTURING FRAMES..", CENTER, 50);
	display_print((char*) "Press push button to stop", CENTER, 170);
	delay(1000);

	//Create folder
	videoCreateFolder(dirName);

	//Switch to recording mode
	videoSave = videoSave_recording;
	lepton_startFrame();

	//Main loop
	while (videoSave == videoSave_recording) {
		//Do not store too many files in one folder, otherwise MTP will make issues
		if(folderFrames >= 1000)
		{
			videoCreateFolder(dirName);
			folderFrames = 0;
		}

		//Touch - turn display on or off
		if (!digitalRead(pin_touch_irq)) {
			digitalWrite(pin_lcd_backlight, !(checkScreenLight()));
			while (!digitalRead(pin_touch_irq));
		}

		//Create the thermal image
		createThermalImg();

		//Video capture
		if (videoInterval == 0) {
			videoCaptureNormal(&framesCaptured, &folderFrames, buffer, dirName);
		}
		//Interval capture
		else {
			videoCaptureInterval(&delayTime, &framesCaptured, &folderFrames, buffer, dirName);
		}

		lepton_startFrame();
	}

	//Turn the display on if it was off before
	if (!checkScreenLight())
		enableScreenLight();

	//Post processing for interval videos if enabled
	if ((framesCaptured > 0) && convertEnabled)
		processVideoFrames(framesCaptured, dirName);

	//Show finished message
	else {
		showFullMessage((char*) "Video capture finished");
		delay(1000);
	}

	//Refresh free space
	refreshFreeSpace();

	//Disable mode
	videoSave = videoSave_disabled;
}

/* Video mode, choose intervall or normal */
void videoMode() {
	//Check if there is at least 1MB of space left
	if (getSDSpace() < 1000) {
		//Show message
		showFullMessage((char*) "The SD card is full");
		delay(1000);

		//Disable mode and return
		videoSave = videoSave_disabled;
		return;
	}

	//Border
	drawMainMenuBorder();

redraw:
	//Title & Background
	mainMenuBackground();
	mainMenuTitle((char*)"Video Mode");
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(15, 47, 140, 120, (char*) "Normal");
	buttons_addButton(165, 47, 140, 120, (char*) "Interval");
	buttons_addButton(15, 188, 140, 40, (char*) "Back");
	buttons_drawButtons();

	//Touch handler
	while (true) {

		//If touch pressed
		if (touch_touched() == true) {
			//Check which button has been pressed
			int pressedButton = buttons_checkButtons(true);

			//Normal
			if (pressedButton == 0) {
				//Set video interval to zero, means normal
				videoInterval = 0;
				//Start capturing a video
				videoCapture();
				break;
			}

			//Interval
			if (pressedButton == 1) {
				//Choose the time interval
				if (!videoIntervalChooser())
					//Redraw video mode if user pressed back
					goto redraw;
				//Start capturing a video
				videoCapture();
				break;
			}

			//Back
			if (pressedButton == 2) {
				//Disable mode and return
				videoSave = videoSave_disabled;
				return;
			}
		}
	}
}
