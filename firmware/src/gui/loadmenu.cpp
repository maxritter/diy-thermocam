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

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <gui.h>
#include <save.h>
#include <fonts.h>
#include <touchscreen.h>
#include <hardware.h>
#include <buttons.h>
#include <connection.h>
#include <sdcard.h>
#include <load.h>
#include <loadmenu.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Draw sleek 2026 Gallery (Load Menu) screen (Dark / Light / High-Contrast, bezel-safe >=12px) */
void drawLoadMenu() {
	display_fillScr(UI_BG_DARK);

	// Top 28px non-touch header bar
	display_setColor(UI_SURFACE);
	display_fillRect(0, 0, 319, 28);
	display_setColor(UI_ACCENT);
	display_drawHLine(0, 28, 320);

	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print((char*)"GALLERY", 12, 8);

	char sdBadge[20];
	snprintf(sdBadge, sizeof(sdBadge), "%lu MB FREE", static_cast<unsigned long>(getSDFreeKB() / 1024u));
	int rx = 308 - static_cast<int>(strlen(sdBadge)) * 8;
	display_setColor(UI_TEXT_MUTED);
	display_print(sdBadge, rx, 8);

	// Invisible hit targets matching our custom-drawn cards & bezel-safe < Back pill
	buttons_deleteAllButtons();
	buttons_addButton(12,  38,  142, 136, (char*)"", 0x0010); // 0: Images
	buttons_addButton(166, 38,  142, 136, (char*)"", 0x0010); // 1: Videos
	buttons_addButton(12,  190, 142, 36,  (char*)"", 0x0010); // 2: < Back

	// Left card: Images (photo frame vector icon)
	display_setColor(UI_SURFACE);
	display_fillRoundRect(12, 38, 154, 174);
	display_setColor(UI_ACCENT);
	display_drawRoundRect(12, 38, 154, 174);
	const int lcx = 83, lcy = 80;
	display_setColor(UI_TEXT);
	display_drawRoundRect(lcx - 22, lcy - 14, lcx + 22, lcy + 14);
	display_setColor(UI_ACCENT);
	display_fillCircle(lcx - 9, lcy - 4, 4);
	display_setColor(UI_TEXT);
	display_drawLine(lcx - 16, lcy + 11, lcx - 2, lcy - 3);
	display_drawLine(lcx - 2,  lcy - 3,  lcx + 6, lcy + 5);
	display_drawLine(lcx + 6,  lcy + 5,  lcx + 15, lcy - 4);
	display_setBackColor(UI_SURFACE);
	display_setFont(bigFont);
	display_setColor(UI_TEXT);
	display_print((char*)"Images", 35, 114);
	display_setFont(smallFont);
	display_setColor(UI_TEXT_MUTED);
	display_print((char*)".DAT / .BMP", 39, 144);

	// Right card: Videos (film strip / play triangle vector icon)
	display_setColor(UI_SURFACE);
	display_fillRoundRect(166, 38, 308, 174);
	display_setColor(UI_BORDER);
	display_drawRoundRect(166, 38, 308, 174);
	const int rcx = 237, rcy = 80;
	display_setColor(UI_TEXT);
	display_drawRoundRect(rcx - 22, rcy - 14, rcx + 22, rcy + 14);
	display_setColor(UI_ACCENT);
	for (int d = -9; d <= 9; ++d) {
		int len = 14 - (abs(d) * 14) / 10;
		if (len > 0) display_drawHLine(rcx - 5, rcy + d, len);
	}
	display_setBackColor(UI_SURFACE);
	display_setFont(bigFont);
	display_setColor(UI_TEXT);
	display_print((char*)"Videos", 189, 114);
	display_setFont(smallFont);
	display_setColor(UI_TEXT_MUTED);
	display_print((char*)"Recorded Clips", 181, 144);

	// Bezel-safe bottom < Back pill (12px left, 14px bottom margin)
	display_setColor(UI_SURFACE);
	display_fillRoundRect(12, 190, 154, 226);
	display_setColor(UI_BORDER);
	display_drawRoundRect(12, 190, 154, 226);
	display_setBackColor(UI_SURFACE);
	display_setFont(smallFont);
	display_setColor(UI_TEXT);
	display_print((char*)"< Back", 59, 202);

	display_setBackColor(UI_BG_DARK);
	display_setColor(UI_TEXT_MUTED);
	display_print((char*)"SD Card Media", 184, 202);
}

/* Composite the load/video HUD pills directly into bigBuffer in RAM (zero flicker!) */
void displayGUI(uint32_t imgCount, char* infoText) {
	const bool prevWrite = display_writeToImage;
	display_writeToImage = true;

	auto drawHudAction = [](int x, int y, int w, const char* label, bool accent = false) {
		display_blendRect(x, y, w, 22, UI_HUD_BG, 6);
		display_setColor(accent ? UI_HUD_ACCENT : UI_HUD_BORDER);
		display_drawRect(x, y, x + w - 1, y + 21);
		display_setFont(smallFont);
		display_setBackColor(VGA_TRANSPARENT);
		display_setColor(accent ? UI_HUD_ACCENT : UI_HUD_TEXT);
		int tx = x + (w - static_cast<int>(strlen(label)) * 8) / 2;
		display_print((char*)label, tx, y + 5);
	};

	drawHudAction(12, 10, 56, "Find");
	drawHudAction(240, 10, 68, "Delete");
	if (imgCount != 1) {
		drawHudAction(12, 109, 26, "<");
		drawHudAction(282, 109, 26, ">");
	}
	drawHudAction(12, 206, 76, "Convert");
	drawHudAction(228, 206, 80, "< Back", true);

	// Centered timestamp / frame counter pill
	int iw = static_cast<int>(strlen(infoText)) * 8 + 12;
	int ix = (320 - iw) / 2;
	if (ix < 72) ix = 72;
	if (ix + iw > 236) iw = 236 - ix;
	if (iw > 8) {
		display_blendRect(ix, 10, iw, 22, UI_HUD_BG, 6);
		display_setColor(UI_HUD_BORDER);
		display_drawRect(ix, 10, ix + iw - 1, 31);
		display_setFont(smallFont);
		display_setBackColor(VGA_TRANSPARENT);
		display_setColor(UI_HUD_TEXT);
		int tx = ix + (iw - static_cast<int>(strlen(infoText)) * 8) / 2;
		display_print(infoText, tx, 15);
	}
	changeTextColor();
	display_writeToImage = prevWrite;
}

/* Composite the centered translucent Play pill into bigBuffer in RAM (zero flicker!) */
static void drawPlayPillToBuffer() {
	const bool prevWrite = display_writeToImage;
	display_writeToImage = true;
	int py = spotEnabled ? 66 : 104;
	display_blendRect(108, py, 105, 33, UI_HUD_BG, 6);
	display_setColor(UI_ACCENT);
	display_drawRoundRect(108, py, 212, py + 32);
	display_setBackColor(VGA_TRANSPARENT);
	display_setColor(UI_HUD_TEXT);
	display_setFont(bigFont);
	display_print((char*) "Play", CENTER, py + 8);
	display_setFont(smallFont);
	changeTextColor();
	display_writeToImage = prevWrite;
}

/* Remove all sequential frame files inside a video folder and remove the folder itself. */
static void removeVideoFolderFiles(char* dirname) {
	dirname[14] = '\0';
	sd.chdir("/");
	sd.chdir(dirname);

	uint32_t videoCounter = 0;
	char filename[] = "000000.DAT";
	while (true) {
		frameFilename(filename, videoCounter);
		if (!sd.exists(filename))
			break;
		sd.remove(filename);
		strcpy(&filename[6], ".BMP");
		if (sd.exists(filename)) sd.remove(filename);
		strcpy(&filename[6], ".JPG");
		if (sd.exists(filename)) sd.remove(filename);
		strcpy(&filename[6], ".DAT");
		videoCounter++;
	}

	sd.chdir("/");
	sd.rmdir(dirname);
}

/* Confirm and delete a recorded video folder and all of its frames. */
void deleteVideo(char* dirname) {
	if (!ui_confirmDialog("Delete Video", "Delete this recorded video?",
		"This removes all .DAT / .BMP", "frames in this recording folder.")) {
		return;
	}
	showFullMessage((char*) "Delete video..");
	removeVideoFolderFiles(dirname);
	showFullMessage((char*) "Video deleted");
	delay(1000);
}

/* Confirm and delete a thermal image and any companion .BMP/.JPG files. */
void deleteImage(char* filename) {
	if (!ui_confirmDialog("Delete Image", "Delete this thermal image?",
		"This also removes any matching", ".BMP or .JPG companion file.")) {
		return;
	}
	showFullMessage((char*) "Delete image..");
	sd.remove(filename);
	strcpy(&filename[14], ".JPG");
	if (sd.exists(filename)) sd.remove(filename);
	strcpy(&filename[14], ".BMP");
	if (sd.exists(filename)) sd.remove(filename);
	showFullMessage((char*) "Image deleted");
	delay(1000);
}

/* Confirm converting raw .DAT image/video data into 640x480 .BMP bitmaps. */
bool convertPrompt() {
	return ui_confirmDialog("Convert to BMP", "Convert raw thermal data?",
		"Creates 640x480 .BMP bitmap(s)", "alongside the original .DAT.", true);
}

/* Convert a raw image lately to BMP */
void convertImage(char* filename) {

	//Check if image is a bitmap
	if (filename[15] == 'B') {
		showFullMessage((char*) "Image is already converted");
		delay(500);
		return;
	}

	//Check if the image is already there
	sd.chdir("/");
	strcpy(&filename[14], ".BMP");
	bool exists = sd.exists(filename);

	//If image is already converted, return
	if (exists) {
		showFullMessage((char*) "Image is already converted");
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

	//Re-render clean thermal image for this exact .DAT (no Gallery HUD buttons in BMP)
	strcpy(&filename[14], ".DAT");
	loadRawData(filename);
	renderRawDataToBuffer();

	//Save image (saveBuffer sets .BMP extension and writes 640x480 bitmap)
	saveBuffer(filename);

	//Show finish message
	showFullMessage((char*) "Image converted");
	delay(1000);
	strcpy(&filename[14], ".DAT");
}

/* Convert a raw video lately to BMP frames */
void convertVideo(char* dirname) {
	//Delete the ending for a video before entering folder
	dirname[14] = '\0';

	//Go into the folder
	sd.chdir("/");
	sd.chdir(dirname);

	uint32_t frames = getVideoFrameNumber();
	char filename[] = "000000.BMP";

	//Get the frame name of the first frame
	frameFilename(filename, 0);
	bool exists = sd.exists(filename);

	//If video is already converted, return
	if (exists) {
		showFullMessage((char*) "Video is already converted");
		delay(500);
		sd.chdir("/");
		return;
	}

	//If the user does not want to convert the video, return
	if (!convertPrompt()) {
		sd.chdir("/");
		return;
	}

	//Show convert message
	showFullMessage((char*) "Converting video to BMP..");
	delay(1000);

	//Convert video
	processVideoFrames(frames, dirname);
	videoSave = videoSave_disabled;

	sd.chdir("/");
}

/* Loads an image from the SDCard and prints it on screen (RAM-composited HUD, single atomic flush) */
void openImage(char* filename, uint32_t imgCount) {
	//Show message on screen
	showFullMessage((char*) "Please wait, image is loading..");

	//Unsupported file type check first
	if ((filename[15] != 'D') && (filename[15] != 'B')) {
		showFullMessage((char*) "Unsupported file type");
		delay(1000);
		loadTouch = loadTouch_exit;
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

	auto renderImageWithHUD = [&]() {
		if (filename[15] == 'D') {
			loadRawData(filename);
			renderRawDataToBuffer();
		} else {
			loadBMPToBuffer(filename);
		}
		displayGUI(imgCount, nameStr);
		displayBuffer();
	};

	//Composite image + HUD in RAM and push to LCD in ONE atomic write
	renderImageWithHUD();

	//Wait for finger release from the button that opened or stepped to this image
	uint32_t tRel = millis();
	while (touch_touched() && (millis() - tRel) < 1000u) {
		delay(5);
	}

	//Wait for touch press safely in main-loop context (no XPT2046 PENIRQ-vs-SPI race!)
	while (loadTouch == loadTouch_none) {
		if (checkSerial()) {
			renderImageWithHUD();
		}
		if (touch_touched()) {
			loadTouchProcess();
			//Still images have no center Play button; ignore center taps instead of reloading from SD
			if (loadTouch == loadTouch_middle) {
				loadTouch = loadTouch_none;
			}
		}
		delay(5);
	}
}

/* Get the number of frames in the video */
uint32_t getVideoFrameNumber() {
	uint32_t videoCounter = 0;
	bool exists;
	char filename[] = "000000.DAT";

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

	return videoCounter;
}

/* Play a video from the internal storage (100% RAM-composited zero-flicker playback!) */
void playVideo(char* dirname, uint32_t imgCount) {
	char buffer[32];
	char frameName[] = "000000.DAT";
	//Save the current frame number
	uint32_t frameNumber = 0;

	//Switch to video folder
	sd.chdir("/");
	sd.chdir(dirname);

	//Get the total number of frames in the dir
	uint32_t numberOfFrames = getVideoFrameNumber();
	if (numberOfFrames == 0) {
		showFullMessage((char*) "Empty video folder");
		delay(1000);
		sd.chdir("/");
		loadTouch = loadTouch_exit;
		return;
	}

	// Composite thermal frame + HUD pills + optional Play pill into bigBuffer BEFORE ONE atomic LCD write!
	auto renderVideoFrameWithHUD = [&](uint32_t idx, bool showPlayPill) {
		frameFilename(frameName, idx);
		loadRawData(frameName);
		renderRawDataToBuffer();
		snprintf(buffer, sizeof(buffer), "%lu / %lu",
			static_cast<unsigned long>(idx + 1),
			static_cast<unsigned long>(numberOfFrames));
		displayGUI(imgCount, buffer);
		if (showPlayPill) {
			drawPlayPillToBuffer();
		}
		displayBuffer();
	};

	//Jump here when pausing a video
showFrame:
	renderVideoFrameWithHUD(frameNumber, true);

	//Wait for finger release from the button that opened or paused this video
	{
		uint32_t tEnter = millis();
		while (touch_touched() && (millis() - tEnter) < 1000u) {
			delay(5);
		}
	}

	//Repeat until we get a valid touch
	do {
		//Wait for touch press while staying responsive to USB host (redraw frame if USB ran)
		while (!touch_touched()) {
			if (checkSerial()) {
				sd.chdir("/");
				sd.chdir(dirname);
				renderVideoFrameWithHUD(frameNumber, true);
			}
			delay(5);
		}
		//Interpret touch coordinates safely in main loop
		loadTouchProcess();
	} while (loadTouch == loadTouch_none);

	//Wait for touch to release (bounded)
	uint32_t tRel = millis();
	while (touch_touched() && (millis() - tRel) < 1000u) delay(5);

	//Check if we play the video
	if (loadTouch != loadTouch_middle)
	{
		sd.chdir("/");
		return;
	}
		
	loadTouch = loadTouch_none;

	//Play forever until paused or exited
	while (true) {
		//Go through the frames
		for (; frameNumber < numberOfFrames; frameNumber++) {
			//Service USB host if connected during playback
			if (Serial.available() > 0 && checkSerial()) {
				sd.chdir("/");
				sd.chdir(dirname);
			}
			//Check for touch press
			if (touch_touched())
				//Get touch function
				loadTouchProcess();
			//Pause the video
			if (loadTouch == loadTouch_middle)
			{
				//Wait for touch release (bounded)
				uint32_t tPause = millis();
				while (touch_touched() && (millis() - tPause) < 1000u) delay(5);
				//Display the static frame
				goto showFrame;
			}
			//Any other action
			if (loadTouch != loadTouch_none)
			{
				sd.chdir("/");
				return;
			}

			//Render frame + HUD composited in bigBuffer and flush atomically (zero flicker!)
			renderVideoFrameWithHUD(frameNumber, false);
		}

		//Reset frame number for next play
		frameNumber = 0;
	}

	sd.chdir("/");
}

/* Shows a menu where the user can choose the time & date items for the image */
int loadMenu(char* title, int* array, int length) {
	//Draw the title on screen
	drawTitle(title);
	//Draw bezel-safe 2026 stepper buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(12, 54, 72, 76, (char*) "-");
	buttons_addButton(236, 54, 72, 76, (char*) "+");
	buttons_setTextFont(smallFont);
	buttons_addButton(12, 190, 142, 36, (char*) "< Back");
	buttons_addButton(166, 190, 142, 36, (char*) "Choose");
	buttons_drawButtons();
	buttons_setActive(3);
	int currentPos = 0;
	//Display the first element for the array
	drawCenterElement(array[currentPos]);
	//Touch handler
	while (true) {
		checkSerial();
		//Touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//Minus (button 0 is "-")
			if (pressedButton == 0) {
				//Decrease element by one
				if (currentPos > 0)
					currentPos--;
				//Go from lowest to highest element
				else if (currentPos == 0)
					currentPos = length - 1;
				//Draw it on screen
				drawCenterElement(array[currentPos]);
			}
			//Plus (button 1 is "+")
			else if (pressedButton == 1) {
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
		delay(5);
	}
}
