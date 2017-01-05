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

/* Methods */

/* Display the GUI elements for the load menu */
void displayGUI(int imgCount, char* infoText) {
	//Set text color
	changeTextColor();
	//set Background transparent
	display_setBackColor(VGA_TRANSPARENT);
	display_setFont(bigFont);
	//Delete image or video from internal storage 
	display_print((char*) "Delete", 220, 10);
	//Find image by time and date
	display_print((char*) "Find", 5, 10);
	//Display prev/next if there is more than one image
	if (imgCount != 1) {
		display_print((char*) "<", 10, 110);
		display_print((char*) ">", 295, 110);
	}
	//Convert image to bitmap
	display_print((char*) "Convert", 5, 210);
	//Exit to main menu
	display_print((char*) "Exit", 250, 210);
	display_setFont(smallFont);
	//Display either frame number or image date and time
	display_print(infoText, 80, 12);
}

/* Asks the user if he wants to delete the video */
void deleteVideo(char* dirname) {
	//Title & Background
	drawTitle((char*) "Delete Video", true);
	display_setColor(VGA_BLACK);
	display_setFont(smallFont);
	display_setBackColor(200, 200, 200);
	display_print((char*)"Do you want to delete this video?", CENTER, 66);
	display_print((char*)"This will also remove the", CENTER, 105);
	display_print((char*)"other related files to it. ", CENTER, 125);
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(15, 160, 140, 55, (char*) "Yes");
	buttons_addButton(165, 160, 140, 55, (char*) "No");
	buttons_drawButtons();
	buttons_setTextFont(smallFont);
	//Touch handler
	while (true) {
		//If touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//YES
			if (pressedButton == 0) {
				showFullMessage((char*) "Delete video..");
				//Start SD
				startAltClockline();

				//Delete the ending for a video
				dirname[14] = '\0';

				//Go into the video folder
				sd.chdir("/");
				sd.chdir(dirname);
				//Delete all files
				uint16_t videoCounter = 0;
				bool exists;
				char filename[] = "00000.DAT";
				//Go through the frames
				while (1) {
					//Get the frame name
					frameFilename(filename, videoCounter);
					//Check frame existance
					exists = sd.exists(filename);
					//If the frame does not exists, end remove
					if (!exists)
						break;
					//Otherwise remove file
					else
						sd.remove(filename);
					//Remove Bitmap if there
					strcpy(&filename[5], ".BMP");
					if (sd.exists(filename))
						sd.remove(filename);
					//Remove Jpeg if there
					strcpy(&filename[5], ".JPG");
					if (sd.exists(filename))
						sd.remove(filename);
					//Reset ending
					strcpy(&filename[5], ".DAT");
					//Raise counter
					videoCounter++;
				}
				//Switch back to the root
				sd.chdir("/");
				//Remove the folder itself
				sd.rmdir(dirname);
				//End SD
				endAltClockline();
				showFullMessage((char*) "Video deleted!");
				delay(1000);
				return;
			}
			//NO
			else if (pressedButton == 1) {
				//Start SD
				startAltClockline();
				sd.chdir("/");
				endAltClockline();
				return;
			}
		}
	}
}

/* Asks the user if he wants to delete the image */
void deleteImage(char* filename) {
	//Title & Background
	drawTitle((char*) "Delete Image", true);
	display_setColor(VGA_BLACK);
	display_setFont(smallFont);
	display_setBackColor(200, 200, 200);
	display_print((char*)"Do you want to delete this image?", CENTER, 66);
	display_print((char*)"This will also remove the", CENTER, 105);
	display_print((char*)"other related files to it. ", CENTER, 125);
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(165, 160, 140, 55, (char*) "Yes");
	buttons_addButton(15, 160, 140, 55, (char*) "No");
	buttons_drawButtons();
	buttons_setTextFont(smallFont);;
	//Touch handler
	while (true) {
		//If touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//YES
			if (pressedButton == 0) {
				showFullMessage((char*) "Delete image..");
				//Start SD
				startAltClockline();
				//Delete .DAT file
				sd.remove(filename);
				//Delete .JPG file
				strcpy(&filename[14], ".JPG");
				if (sd.exists(filename))
					sd.remove(filename);
				//Delete .BMP file
				strcpy(&filename[14], ".BMP");
				if (sd.exists(filename))
					sd.remove(filename);
				endAltClockline();
				showFullMessage((char*) "Image deleted!");
				delay(1000);
				return;
			}
			//NO
			else if (pressedButton == 1) {
				return;
			}
		}
	}
}

/* Asks the user if he really wants to convert the image/video */
bool convertPrompt() {
	//Title & Background
	drawTitle((char*) "Conversion Prompt", true);
	display_setColor(VGA_BLACK);
	display_setFont(smallFont);
	display_setBackColor(200, 200, 200);
	display_print((char*)"Do you want to convert?", CENTER, 66);
	display_print((char*)"That process will create", CENTER, 105);
	display_print((char*)"bitmap(s) out of the raw data.", CENTER, 125);
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(165, 160, 140, 55, (char*) "Yes");
	buttons_addButton(15, 160, 140, 55, (char*) "No");
	buttons_drawButtons();
	buttons_setTextFont(smallFont);
	//Wait for touch release
	while (touch_touched());
	//Touch handler
	while (true) {
		//If touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//YES
			if (pressedButton == 0) {
				return true;
			}
			//NO
			else if (pressedButton == 1) {
				return false;
			}
		}
	}
}

/* Convert a raw image lately to BMP */
void convertImage(char* filename) {

	//Check if image is a bitmap
	if (filename[15] == 'B') {
		showFullMessage((char*) "Image is already converted!");
		delay(500);
		return;
	}

	//Check if the image is already there
	strcpy(&filename[14], ".BMP");
	startAltClockline();
	bool exists = sd.exists(filename);
	endAltClockline();

	//If image is already converted, return
	if (exists) {
		showFullMessage((char*) "Image is already converted!");
		delay(500);
		strcpy(&filename[14], ".DAT");
		return;
	}

	//If the user does not want to convert, return
	if (!convertPrompt()) {
		strcpy(&filename[14], ".DAT");
		return;
	}

	//Show convert message
	showFullMessage((char*) "Converting image to BMP..");

	//Save image
	saveBuffer(filename);

	//Show finish message
	showFullMessage((char*) "Image converted!");
	delay(1000);
	strcpy(&filename[14], ".DAT");
}

/* Convert a raw video lately to BMP frames */
void convertVideo(char* dirname) {
	uint16_t frames = getVideoFrameNumber(dirname);
	char filename[] = "00000.BMP";

	//Switch Clock to Alternative
	startAltClockline();

	//Delete the ending for a video
	dirname[14] = '\0';

	//Go into the folder
	sd.chdir(dirname);

	//Get the frame name of the first frame
	frameFilename(filename, 0);
	bool exists = sd.exists(filename);
	endAltClockline();

	//If video is already converted, return
	if (exists) {
		showFullMessage((char*) "Video is already converted!");
		delay(500);
		return;
	}

	//If the user does not want to convert the video, return
	if (!convertPrompt())
		return;

	//Show convert message
	showFullMessage((char*) "Converting video to BMP..");
	delay(1000);

	//Convert video
	processVideoFrames(frames, dirname);
	videoSave = videoSave_disabled;
}

/* Loads an image from the SDCard and prints it on screen */
void openImage(char* filename, int imgCount) {
	//Show message on screen
	showFullMessage((char*) "Please wait, image is loading..");

	//Display raw data
	if (filename[15] == 'D') {
		//Load Raw data
		loadRawData(filename);
		//Display Raw Data
		displayRawData();
	}

	//Load bitmap
	else if (filename[15] == 'B') {
		loadBMPImage(filename);
	}

	//Unsupported file type
	else {
		showFullMessage((char*) "Unsupported file type!");
		delay(1000);
		return;
	}

	//Create string for time and date
	char nameStr[20] = {
		//Day
		filename[6], filename[7], '.',
		//Month
		filename[4], filename[5], '.',
		//Year
		filename[2], filename[3], ' ',
		//Hour
		filename[8], filename[9], ':',
		//Minute
		filename[10], filename[11], ':',
		//Second
		filename[12], filename[13], '\0'
	};

	//Display GUI
	displayGUI(imgCount, nameStr);

	//Attach interrupt
	attachInterrupt(pin_touch_irq, loadTouchIRQ, FALLING);

	//Wait for touch press
	while (loadTouch == loadTouch_none);

	//Disable touch handler
	detachInterrupt(pin_touch_irq);
}

/* Get the number of frames in the video */
uint16_t getVideoFrameNumber(char* dirname) {
	uint16_t videoCounter = 0;
	bool exists;
	char filename[] = "00000.DAT";
	//Switch Clock to Alternative
	startAltClockline();
	//Go into the folder
	sd.chdir(dirname);
	//Look how many frames we have
	while (true) {
		//Get the frame name
		frameFilename(filename, videoCounter);
		//Check frame existance
		exists = sd.exists(filename);
		//Raise counter
		if (exists)
			videoCounter++;
		//Leave
		else
			break;
	}
	//Switch Clock back to Standard
	endAltClockline();
	return videoCounter;
}

/* Display the selected video frame */
void displayVideoFrame(int i, char* dirname)
{
	char filename[] = "00000.DAT";

	//Get the frame name
	frameFilename(filename, i);

	//Load Raw data
	loadRawData(filename, dirname);

	//Display Raw Data
	displayRawData();
}

/* Play a video from the internal storage */
void playVideo(char* dirname, int imgCount) {
	char buffer[14];
	//Save the current frame number
	int frameNumber = 0;

	//Get the total number of frames in the dir
	uint16_t numberOfFrames = getVideoFrameNumber(dirname);

	//Jump here when pausing a video
showFrame:
	//Display frame
	displayVideoFrame(frameNumber, dirname);
	//Create string
	sprintf(buffer, "%5d / %-5d", frameNumber + 1, numberOfFrames);
	//Display GUI
	displayGUI(imgCount, buffer);
	//Display play message
	display_setFont(bigFont);
	if (spotEnabled)
		display_print((char*) "Play", CENTER, 70);
	else
		display_print((char*) "Play", CENTER, 110);
	display_setFont(smallFont);

	//Repeat until we get a valid touch
	do {
		//Wait for touch press
		while (!touch_touched());
		//Interpret touch coordinates
		loadTouchIRQ();
	} while (loadTouch == loadTouch_none);

	//Wait for touch to release
	while (touch_touched());

	//Check if we play the video
	if (loadTouch != loadTouch_middle)
		return;
	loadTouch = loadTouch_none;

	//Play forever
	while (true) {
		//Go through the frames
		for (; frameNumber < numberOfFrames; frameNumber++) {
			//Check for touch press
			if (touch_touched())
				//Get touch function
				loadTouchIRQ();
			//Pause the video
			if (loadTouch == loadTouch_middle)
			{
				//Wait for touch release
				while (touch_touched());
				//Display the static frame
				goto showFrame;
			}
			//Any other action
			if (loadTouch != loadTouch_none)
				return;

			//Display frame
			displayVideoFrame(frameNumber, dirname);
			//Create string
			sprintf(buffer, "%5d / %-5d", frameNumber + 1, numberOfFrames);
			//Display GUI
			displayGUI(imgCount, buffer);
		}
		//Reset frame number for next play
		frameNumber = 0;
	}
}

/* Shows a menu where the user can choose the time & date items for the image */
int loadMenu(char* title, int* array, int length) {
	//Draw the title on screen
	drawTitle(title);
	//Draw the Buttons
	buttons_deleteAllButtons();
	buttons_addButton(20, 60, 70, 70, (char*) "-");
	buttons_addButton(230, 60, 70, 70, (char*) "+");
	buttons_addButton(20, 150, 130, 70, (char*) "Back");
	buttons_addButton(170, 150, 130, 70, (char*) "Choose");
	buttons_drawButtons();
	int currentPos = 0;
	//Display the first element for the array
	drawCenterElement(array[currentPos]);
	//Touch handler
	while (true) {
		//Touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons();
			//Minus
			if (pressedButton == 1) {
				//Decrease element by one
				if (currentPos > 0)
					currentPos--;
				//Go from lowest to highest element
				else if (currentPos == 0)
					currentPos = length - 1;
				//Draw it on screen
				drawCenterElement(array[currentPos]);
			}
			//Plus
			else if (pressedButton == 0) {
				//Increase element by one
				if (currentPos < (length - 1))
					currentPos++;
				//Go from highest to lowest element
				else if (currentPos == (length - 1))
					currentPos = 0;
				//Draw it on screen
				drawCenterElement(array[currentPos]);
			}
			//Back - return minus 1
			else if (pressedButton == 2) {
				return -1;
			}
			//Set - return element's position
			else if (pressedButton == 3) {
				return currentPos;
			}
		}
	}
}