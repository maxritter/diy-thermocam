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
#include <connection.h>
#include <perf.h>
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
	uint32_t t0 = millis();
	while (touch_touched() && (millis() - t0) < 1000u) { delay(5); }
	//Main loop
	while (true) {
		checkSerial();
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
		delay(5);
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

/* Captures video frames on drift-free millis() deadlines with pre-filter raw .DAT */
void videoCaptureInterval(uint32_t* nextCaptureMs, uint32_t* framesCaptured, uint16_t* folderFrames, char* buffer, char* dirName) {
	uint32_t nowMs = millis();
	bool due = (*framesCaptured == 0) || (static_cast<int32_t>(nowMs - *nextCaptureMs) >= 0);

	//Acquire frame, save PRE-FILTER .DAT if due, then filter/scale/colorize for preview
	createThermalImgVideo(due, dirName, *folderFrames);

	if (due) {
		*folderFrames = *folderFrames + 1;
		*framesCaptured = *framesCaptured + 1;
		uint32_t intervalMs = static_cast<uint32_t>(videoInterval) * 1000u;
		if (intervalMs == 0u) intervalMs = 1000u;
		if (*framesCaptured == 1u) {
			*nextCaptureMs = nowMs + intervalMs;
		} else {
			do {
				*nextCaptureMs += intervalMs;
			} while (static_cast<int32_t>(millis() - *nextCaptureMs) >= 0);
		}
	}

	//Display infos
	displayInfos();

	//Write crisp 2026 translucent HUD pills directly into bigBuffer (zero flicker!)
	display_writeToImage = true;
	display_setFont((uint8_t *)smallFont);
	display_setBackColor(VGA_TRANSPARENT);

	//Top-center INTERVAL REC pill
	display_blendRect(94, 4, 132, 18, UI_HUD_BG, 6);
	display_setColor(UI_HUD_ACCENT);
	display_drawRoundRect(94, 4, 225, 21);
	display_fillCircle(104, 13, 3);
	display_setColor(UI_HUD_TEXT);
	display_print((char*) "INTERVAL REC", 114, 7);

	//Bottom-center countdown / saving pill
	int32_t remMs = static_cast<int32_t>(*nextCaptureMs - millis());
	if (remMs < 0) remMs = 0;
	int32_t remSec = (remMs + 999) / 1000;
	if (due)
		sprintf(buffer, "Saved frame #%lu", static_cast<unsigned long>(*framesCaptured));
	else
		sprintf(buffer, "Next in %lds | %lu FR", static_cast<long>(remSec), static_cast<unsigned long>(*framesCaptured));

	int pw = static_cast<int>(strlen(buffer)) * 8 + 14;
	int px = (320 - pw) / 2;
	if (px < 6) px = 6;
	display_blendRect(px, 218, pw, 18, UI_HUD_BG, 6);
	display_setColor(UI_HUD_BORDER);
	display_drawRoundRect(px, 218, px + pw - 1, 235);
	display_setColor(UI_HUD_TEXT);
	display_print(buffer, px + 7, 221);

	//Disable image buffer
	display_writeToImage = false;

	//Draw thermal image on screen (zero blocking delay — keeps preview & button 100% responsive!)
	displayBuffer();
}

/* Normal video capture with crisp zero-flicker 2026 REC MM:SS pill */
static uint32_t s_videoStartMs = 0;

void drawVideoRecordingOverlay(uint32_t framesCaptured, uint32_t elapsedSec) {
	char buffer[32];
	uint32_t mm = elapsedSec / 60u;
	uint32_t ss = elapsedSec % 60u;

	//Write crisp REC MM:SS pill directly into bigBuffer (zero flicker!)
	display_writeToImage = true;
	display_setFont((uint8_t *)smallFont);
	display_setBackColor(VGA_TRANSPARENT);

	display_blendRect(108, 4, 104, 18, UI_HUD_BG, 6);
	display_setColor(UI_HUD_ERR);
	display_drawRoundRect(108, 4, 211, 21);
	if ((elapsedSec & 1u) == 0u) {
		display_fillCircle(118, 13, 3);
	}
	display_setColor(UI_HUD_TEXT);
	snprintf(buffer, sizeof(buffer), "REC %02lu:%02lu", static_cast<unsigned long>(mm), static_cast<unsigned long>(ss));
	display_print(buffer, 126, 7);

	// Bottom-left frame + live rate pill
	snprintf(buffer, sizeof(buffer), "%lu FR | %.1f FPS", static_cast<unsigned long>(framesCaptured), g_perf.video_fps_x100 / 100.0f);
	int pw = static_cast<int>(strlen(buffer)) * 8 + 12;
	display_blendRect(4, 220, pw, 16, UI_HUD_BG, 5);
	display_setColor(UI_HUD_BORDER);
	display_drawRoundRect(4, 220, 4 + pw - 1, 235);
	display_setColor(UI_HUD_TEXT);
	display_print(buffer, 10, 222);
	display_writeToImage = false;
}

void videoCaptureNormal(uint32_t* framesCaptured, uint16_t* folderFrames, char* buffer, char* dirName) {
	(void)buffer;
	//Acquire frame, save PRE-FILTER raw video .DAT, then filter/scale/colorize for preview
	createThermalImgVideo(true, dirName, *folderFrames);
	*folderFrames = *folderFrames + 1;

	//Display infos
	displayInfos();

	*framesCaptured = *framesCaptured + 1;
	uint32_t elapsedSec = (millis() - s_videoStartMs) / 1000u;
	drawVideoRecordingOverlay(*framesCaptured, elapsedSec);

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
	char buffer[36];
	uint32_t framesCaptured = 0;
	uint16_t folderFrames = 0;

	//Show sleek 2026 startup modal card
	showModalCard(
		(videoInterval == 0) ? "RECORDING VIDEO" : "INTERVAL CAPTURE",
		"Touch screen to toggle display",
		"Press top button to stop",
		"Saving radiometric .DAT frames");
	delay(900);

	//Create folder
	videoCreateFolder(dirName);

	//Switch to recording mode
	videoSave = videoSave_recording;
	s_videoStartMs = millis();
	uint32_t nextCaptureMs = s_videoStartMs;
	bool prevTouched = touch_capacitive ? touch_touched() : (!digitalRead(pin_touch_irq));
	uint32_t lastToggleMs = 0;
	disableSPIIRQ = false;
	lepton_startFrame();
	uint32_t lastVidUs = micros();

	//Main loop
	while (videoSave == videoSave_recording) {
		if (folderFrames >= 1000) {
			videoCreateFolder(dirName);
			folderFrames = 0;
		}

		//Non-blocking edge-triggered backlight toggle (never stalls recording while held!)
		bool touched = touch_capacitive ? touch_touched() : (!digitalRead(pin_touch_irq));
		if (touched && !prevTouched && (millis() - lastToggleMs) >= 250u) {
			digitalWrite(pin_lcd_backlight, !(checkScreenLight()));
			lastToggleMs = millis();
		}
		prevTouched = touched;

		uint32_t loopStartUs = micros();

		if (videoInterval == 0) {
			videoCaptureNormal(&framesCaptured, &folderFrames, buffer, dirName);
		} else {
			videoCaptureInterval(&nextCaptureMs, &framesCaptured, &folderFrames, buffer, dirName);
		}

		uint32_t nowUs = micros();
		g_perf.loop_total_us = nowUs - loopStartUs;
		if (videoInterval == 0) {
			perf_record_frame_period(PERF_MODE_VIDEO, nowUs - lastVidUs);
		}
		lastVidUs = nowUs;
	}
	if (framesCaptured > 0 && videoInterval == 0) {
		perf_save_video_snapshot();
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

	//Disable mode & cleanly restore live view state
	videoSave = videoSave_disabled;
	disableSPIIRQ = false;
	lepton_startFrame();
}

/* Draw sleek 2026 Video Mode selection screen (Dark / Light / High-Contrast, bezel-safe >=12px) */
void drawVideoModeMenu() {
	display_fillScr(UI_BG_DARK);

	// Top 28px non-touch header bar
	display_setColor(UI_SURFACE);
	display_fillRect(0, 0, 319, 28);
	display_setColor(UI_ACCENT);
	display_drawHLine(0, 28, 320);

	display_setFont(smallFont);
	display_setBackColor(UI_SURFACE);
	display_setColor(UI_TEXT);
	display_print((char*)"VIDEO RECORDING", 12, 8);

	char sdBadge[20];
	snprintf(sdBadge, sizeof(sdBadge), "%lu MB FREE", static_cast<unsigned long>(getSDFreeKB() / 1024u));
	int rx = 308 - static_cast<int>(strlen(sdBadge)) * 8;
	display_setColor(UI_TEXT_MUTED);
	display_print(sdBadge, rx, 8);

	// Invisible hit targets matching our custom-drawn cards & bottom pills
	buttons_deleteAllButtons();
	buttons_addButton(12,  38,  142, 136, (char*)"", 0x0010); // 0: Normal
	buttons_addButton(166, 38,  142, 136, (char*)"", 0x0010); // 1: Interval
	buttons_addButton(12,  190, 142, 36,  (char*)"", 0x0010); // 2: < Back / Live

	// Left card: Normal real-time video (~8.8 FPS RAW)
	display_setColor(UI_SURFACE);
	display_fillRoundRect(12, 38, 154, 174);
	display_setColor(UI_ACCENT);
	display_drawRoundRect(12, 38, 154, 174);
	// REC ring + red dot vector icon
	const int lcx = 83, lcy = 82;
	display_setColor(UI_TEXT);
	display_drawCircle(lcx, lcy, 18);
	display_drawCircle(lcx, lcy, 17);
	display_setColor(UI_ERR);
	display_fillCircle(lcx, lcy, 10);
	display_setBackColor(UI_SURFACE);
	display_setFont(bigFont);
	display_setColor(UI_TEXT);
	display_print((char*)"Normal", 35, 114);
	display_setFont(smallFont);
	display_setColor(UI_TEXT_MUTED);
	display_print((char*)"~8.8 FPS RAW", 35, 144);

	// Right card: Interval / Time-Lapse
	display_setColor(UI_SURFACE);
	display_fillRoundRect(166, 38, 308, 174);
	display_setColor(UI_BORDER);
	display_drawRoundRect(166, 38, 308, 174);
	// Stopwatch / interval clock vector icon
	const int rcx = 237, rcy = 82;
	display_setColor(UI_TEXT);
	display_drawCircle(rcx, rcy, 18);
	display_drawCircle(rcx, rcy, 17);
	display_fillRect(rcx - 5, rcy - 22, rcx + 5, rcy - 19);
	display_setColor(UI_ACCENT);
	display_drawVLine(rcx, rcy - 11, 12);
	display_drawHLine(rcx, rcy, 10);
	display_fillCircle(rcx, rcy, 2);
	display_setBackColor(UI_SURFACE);
	display_setFont(bigFont);
	display_setColor(UI_TEXT);
	display_print((char*)"Interval", 173, 114);
	display_setFont(smallFont);
	display_setColor(UI_TEXT_MUTED);
	display_print((char*)"Time-Lapse", 197, 144);

	// Bezel-safe bottom < Back pill (12px left, 14px bottom margin)
	display_setColor(UI_SURFACE);
	display_fillRoundRect(12, 190, 154, 226);
	display_setColor(UI_BORDER);
	display_drawRoundRect(12, 190, 154, 226);
	display_setBackColor(UI_SURFACE);
	display_setFont(smallFont);
	display_setColor(UI_TEXT);
	display_print((char*)"< Back", 59, 202);

	// Subtle hint on bottom right
	display_setBackColor(UI_BG_DARK);
	display_setColor(UI_TEXT_MUTED);
	display_print((char*)"Btn: Stop REC", 184, 202);
}

/* Video mode, choose interval or normal */
void videoMode() {
	//Check if there is at least 1MB of space left
	if (getSDSpace() < 1000) {
		showFullMessage((char*) "The SD card is full");
		delay(1000);
		videoSave = videoSave_disabled;
		ui_waitTouchReleaseAndLockout(250u);
		disableSPIIRQ = false;
		lepton_startFrame();
		return;
	}

redraw:
	drawVideoModeMenu();

	uint32_t t0 = millis();
	while (touch_touched() && (millis() - t0) < 1000u) { delay(5); }

	//Touch handler
	while (true) {
		checkSerial();

		//If touch pressed
		if (touch_touched() == true) {
			//Check which button has been pressed
			int pressedButton = buttons_checkButtons(true);

			//Normal
			if (pressedButton == 0) {
				videoInterval = 0;
				videoCapture();
				break;
			}

			//Interval
			if (pressedButton == 1) {
				if (!videoIntervalChooser())
					goto redraw;
				videoCapture();
				break;
			}

			//Back
			if (pressedButton == 2) {
				videoSave = videoSave_disabled;
				break;
			}
		}
		delay(5);
	}

	videoSave = videoSave_disabled;
	// Only enforce post-menu lockout when returning directly to liveMode (not inside mainMenu)
	if (showMenu == showMenu_disabled) {
		ui_waitTouchReleaseAndLockout(250u);
	}
	disableSPIIRQ = false;
	lepton_startFrame();
}
