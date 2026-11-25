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
* https://github.com/maxritter/diy-thermocam
*
*/

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <TimeLib.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <hardware.h>
#include <gui.h>
#include <sdcard.h>
#include <lepton.h>
#include <display.h>
#include <fonts.h>
#include <load.h>
#include <create.h>
#include <livemode.h>
#include <perf.h>
#include <save.h>

// Reusable 8 KB SD write buffer in RAM (replaces 38,400 single-byte SD writes!)
static uint8_t s_sdWriteBuf[8192];

/*######################### STATIC DATA DECLARATIONS ##########################*/

//160 x 120 bitmap header
static const char bmp_header_small[66] = {0x42, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00,
										  0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00,
										  0x00, 0xA0, 0x00, 0x00, 0x00, 0x88, 0xFF, 0xFF, 0xFF, 0x01,
										  0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
										  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
										  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00,
										  0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};

//640 x 480 bitmap header
static const char bmp_header_large[66] = {0x42, 0x4D, 0x36, 0x60, 0x09, 0x00, 0x00,
										  0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00,
										  0x80, 0x02, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x01, 0x00, 0x10,
										  0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x60, 0x09, 0x00, 0xC4, 0x0E,
										  0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
										  0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00,
										  0x1F, 0x00, 0x00, 0x00};

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Creates a filename from the current time & date */
void createSDName(char *filename, boolean folder)
{
	char buffer[5];
	//Year
	itoa(year(), buffer, 10);
	memcpy(&filename[0], buffer, 4);
	//Month
	itoa(month(), buffer, 10);
	if (month() < 10)
	{
		filename[4] = '0';
		filename[5] = buffer[0];
	}
	else
	{
		memcpy(&filename[4], buffer, 2);
	}
	//Day
	itoa(day(), buffer, 10);
	if (day() < 10)
	{
		filename[6] = '0';
		filename[7] = buffer[0];
	}
	else
	{
		memcpy(&filename[6], buffer, 2);
	}
	//Hour
	itoa(hour(), buffer, 10);
	if (hour() < 10)
	{
		filename[8] = '0';
		filename[9] = buffer[0];
	}
	else
	{
		memcpy(&filename[8], buffer, 2);
	}
	//Minute
	itoa(minute(), buffer, 10);
	if (minute() < 10)
	{
		filename[10] = '0';
		filename[11] = buffer[0];
	}
	else
	{
		memcpy(&filename[10], buffer, 2);
	}
	//Second
	itoa(second(), buffer, 10);
	if (second() < 10)
	{
		filename[12] = '0';
		if (!folder)
			filename[13] = buffer[0];
		else
			strcpy(&filename[13], buffer);
	}
	else
	{
		if (!folder)
			memcpy(&filename[12], buffer, 2);
		else
			strcpy(&filename[12], buffer);
	}
}

/* Start the image save procedure */
void imgSaveStart()
{
	//Check if there is at least 1MB of space left
	if (getSDSpace() < 1000)
	{
		//Show message
		showFullMessage((char *)"The SD card is full");
		delay(1000);

		//Disable and return
		imgSave = imgSave_disabled;
		return;
	}

	//Build save filename from the current time & date
	createSDName(saveFilename);

	//Mark create: showImage() flushes clean bigBuffer, then draws SAVING card directly to LCD
	imgSave = imgSave_create;
}

/* Creates the filename for the video frames */
void frameFilename(char *filename, uint32_t count)
{
	filename[0] = '0' + count / 100000 % 10;
	filename[1] = '0' + count / 10000 % 10;
	filename[2] = '0' + count / 1000 % 10;
	filename[3] = '0' + count / 100 % 10;
	filename[4] = '0' + count / 10 % 10;
	filename[5] = '0' + count % 10;
}

/* Save video frame to image file */
void saveVideoFrame(char *filename)
{
	// Open the file for writing
	if (!sdFile.open(filename, O_RDWR | O_CREAT | O_AT_END))
	{
		beginSD();
		if (!sdFile.open(filename, O_RDWR | O_CREAT | O_AT_END))
		{
			showFullMessage((char *)"Error saving BMP!");
			delay(1000);
			return;
		}
	}

	//Write 160x120 BMP header + bulk buffered RGB565 pixels (with full SD write/close checks)
	bool ok = (sdFile.write((const uint8_t *)bmp_header_small, 66) == 66u);
	size_t off = 0;
	for (int i = 0; ok && i < 19200; i++)
	{
		uint16_t v = smallBuffer[i];
		s_sdWriteBuf[off++] = v & 0xFFu;
		s_sdWriteBuf[off++] = (v >> 8) & 0xFFu;
		if (off == sizeof(s_sdWriteBuf)) {
			if (sdFile.write(s_sdWriteBuf, off) != off) ok = false;
			off = 0;
		}
	}
	if (ok && off > 0 && sdFile.write(s_sdWriteBuf, off) != off) ok = false;
	if (!sdFile.close()) ok = false;
	if (ok) {
		sdNoteBytesWritten(66 + 38400);
	} else if (!serialMode) {
		showFullMessage((char *)"Error writing BMP!");
		delay(1000);
	}
}

/* Process video frames with modern 2026 themed progress card */
void processVideoFrames(uint32_t framesCaptured, char *dirname)
{
	char buffer[48];
	char filename[] = "000000.DAT";
	uint32_t framesConverted = 0;

	auto drawProgress = [&](uint32_t done) {
		display_setColor(UI_SURFACE);
		display_fillRoundRect(12, 46, 308, 196);
		display_setColor(UI_BORDER);
		display_drawRoundRect(12, 46, 308, 196);
		display_setFont(smallFont);
		display_setBackColor(UI_SURFACE);
		display_setColor(UI_TEXT);
		display_print((char *)"Converting .DAT -> .BMP frames", CENTER, 60);
		display_setColor(UI_TEXT_MUTED);
		snprintf(buffer, sizeof(buffer), "Folder: %s", dirname);
		display_print(buffer, CENTER, 84);

		// Progress bar (x=28..292, w=264)
		display_setColor(UI_ELEVATED);
		display_fillRoundRect(28, 114, 292, 134);
		display_setColor(UI_BORDER);
		display_drawRoundRect(28, 114, 292, 134);
		if (framesCaptured > 0 && done > 0) {
			uint32_t fillW = (done * 260u) / framesCaptured;
			if (fillW > 260u) fillW = 260u;
			if (fillW > 4u) {
				display_setColor(UI_ACCENT);
				display_fillRoundRect(30, 116, 30 + static_cast<int>(fillW), 132);
			}
		}

		display_setColor(UI_ACCENT);
		snprintf(buffer, sizeof(buffer), "%lu / %lu frames",
			static_cast<unsigned long>(done), static_cast<unsigned long>(framesCaptured));
		display_print(buffer, CENTER, 148);
		display_setColor(UI_TEXT_MUTED);
		display_print((char *)"Press top button to abort", CENTER, 174);
	};

	drawTitle((char *)"Video Conversion");
	drawProgress(0);

	//Switch to processing mode
	videoSave = videoSave_processing;

	//Go through all the frames in the folder
	for (framesConverted = 0; framesConverted < framesCaptured; framesConverted++)
	{
		//Button pressed, exit
		if (videoSave != videoSave_processing)
			break;

		//Get filename
		frameFilename(filename, framesConverted);
		strcpy(&filename[6], ".DAT");

		//Load Raw data
		loadRawData(filename);

		//Apply low-pass filter
		if (filterType == filterType_box)
			boxFilter();
		else if (filterType == filterType_gaussian)
			gaussianFilter();

		//Find min / max position
		if (minMaxPoints != minMaxPoints_disabled)
			refreshMinMax();

		//Convert lepton data to RGB565 colors
		convertColors(true);

		//Display additional information
		displayInfos();

		//Save frame to image file
		strcpy(&filename[6], ".BMP");
		saveVideoFrame(filename);

		//Update themed progress card
		drawProgress(framesConverted + 1);
	}

	//All images converted!
	showFullMessage((char *)"Video converted");
	delay(1000);
}

/* Saves raw data for an image or a video frame (8 KB bulk-buffered, 100% V1/V2/V3 .DAT compatible) */
void saveRawData(bool isImage, char *name, uint32_t framesCaptured)
{
	uint32_t t0 = micros();
	uint16_t result;

	//Create filename for image
	if (isImage)
	{
		strcpy(&name[14], ".DAT");
		if (!sdFile.open(name, O_RDWR | O_CREAT | O_TRUNC))
		{
			beginSD();
			if (!sdFile.open(name, O_RDWR | O_CREAT | O_TRUNC))
			{
				if (!serialMode) {
					showFullMessage((char *)"Error saving image!");
					delay(1000);
				}
				return;
			}
		}
	}
	//Create filename for video frame
	else
	{
		char fileName[] = "000000.DAT";
		frameFilename(fileName, framesCaptured);
		if (!sdFile.open(fileName, O_RDWR | O_CREAT | O_TRUNC))
		{
			beginSD();
			sd.chdir(("/" + String(name)).c_str());
			if (!sdFile.open(fileName, O_RDWR | O_CREAT | O_TRUNC))
			{
				if (!serialMode) {
					showFullMessage((char *)"Error saving frame!");
					delay(1000);
				}
				return;
			}
		}
	}

	size_t off = 0;
	bool writeOk = true;
	auto flush_buf = [&]() {
		if (off > 0) {
			if (writeOk && sdFile.write(s_sdWriteBuf, off) != off) {
				writeOk = false;
			}
			off = 0;
		}
	};
	auto put_u8 = [&](uint8_t b) {
		if (off == sizeof(s_sdWriteBuf)) flush_buf();
		s_sdWriteBuf[off++] = b;
	};
	auto put_be16 = [&](uint16_t v) {
		if (off + 2 > sizeof(s_sdWriteBuf)) flush_buf();
		s_sdWriteBuf[off++] = static_cast<uint8_t>(v >> 8);
		s_sdWriteBuf[off++] = static_cast<uint8_t>(v & 0xFFu);
	};

	uint32_t totalBytes = 0;
	if (leptonVersion == leptonVersion_2_5_shutter)
	{
		for (int line = 0; line < 60; line++)
		{
			for (int column = 0; column < 80; column++)
			{
				result = smallBuffer[(line * 2 * 160) + (column * 2)];
				put_be16(result);
			}
		}
		totalBytes = 9600u + 21u + 384u;
	}
	else
	{
		for (int i = 0; i < 19200; i++)
		{
			put_be16(smallBuffer[i]);
		}
		totalBytes = 38400u + 21u + 384u;
	}

	put_be16(minValue);
	put_be16(maxValue);

	uint8_t farray[4];
	floatToBytes(farray, spotTemp);
	for (int i = 0; i < 4; i++) put_u8(farray[i]);

	put_u8(colorScheme);
	put_u8(tempFormat);
	put_u8(spotEnabled);
	put_u8(colorbarEnabled);
	put_u8(minMaxPoints);

	floatToBytes(farray, -273.15f);
	for (int i = 0; i < 4; i++) put_u8(farray[i]);
	floatToBytes(farray, leptonCalSlope);
	for (int i = 0; i < 4; i++) put_u8(farray[i]);

	for (byte i = 0; i < 96; i++)
	{
		put_be16(tempPoints[i][0]);
		put_be16(tempPoints[i][1]);
	}

	flush_buf();
	if (!sdFile.close()) writeOk = false;
	if (writeOk) {
		sdNoteBytesWritten(totalBytes);
	} else {
		if (!isImage && videoSave == videoSave_recording) {
			videoSave = videoSave_processing; // abort video recording cleanly on SD write error
		}
		if (!serialMode) {
			showFullMessage((char *)"Error writing SD card!");
			delay(1000);
		}
	}
	g_perf.sd_write_us = micros() - t0;
}

/* End the image save procedure */
void imgSaveEnd()
{
	//Save Bitmap image if activated
	if (convertEnabled)
		saveBuffer(saveFilename);

	//Update cached free space string without expensive full-FAT cluster walk
	uint32_t totalMB = getSDTotalKB() / 1024u;
	uint32_t freeMB = getSDFreeKB() / 1024u;
	if (totalMB != 0)
		sdInfo = String(freeMB) + "/" + String(totalMB) + " MB";

	//Disable image save marker & ensure Lepton capture stays armed
	imgSave = imgSave_disabled;
	disableSPIIRQ = false;
	lepton_startFrame();
}

/* Saves the content of the screen buffer to the sd card (8 KB buffered 640x480 BMP) */
void saveBuffer(char *filename)
{
	uint32_t t0 = micros();
	strcpy(&filename[14], ".BMP");
	if (!sdFile.open(filename, O_RDWR | O_CREAT | O_TRUNC))
	{
		beginSD();
		if (!sdFile.open(filename, O_RDWR | O_CREAT | O_TRUNC))
		{
			if (!serialMode) {
				showFullMessage((char *)"Error saving BMP!");
				delay(1000);
			}
			return;
		}
	}

	bool ok = (sdFile.write((const uint8_t *)bmp_header_large, 66) == 66u);

	// Pack 3 double-lines (3 * 2 * 1280 = 7680 B <= 8192 B) per bulk SD write!
	size_t off = 0;
	for (int16_t y = 239; ok && y >= 0; y--)
	{
		const uint16_t* row = &bigBuffer[y * 320];
		uint8_t* lineBuf = &s_sdWriteBuf[off];
		for (uint16_t x = 0; x < 320; x++)
		{
			uint16_t pixel = row[x];
			uint8_t lo = pixel & 0xFFu;
			uint8_t hi = (pixel >> 8) & 0xFFu;
			lineBuf[x * 4]     = lo;
			lineBuf[x * 4 + 1] = hi;
			lineBuf[x * 4 + 2] = lo;
			lineBuf[x * 4 + 3] = hi;
		}
		// Duplicate row immediately in buffer for 2x vertical scale (640x480)
		memcpy(&s_sdWriteBuf[off + 1280], lineBuf, 1280);
		off += 2560;
		if (off >= 7680) {
			if (sdFile.write(s_sdWriteBuf, off) != off) ok = false;
			off = 0;
		}
	}
	if (ok && off > 0 && sdFile.write(s_sdWriteBuf, off) != off) ok = false;
	if (!sdFile.close()) ok = false;
	if (ok) {
		sdNoteBytesWritten(614466u);
	} else if (!serialMode) {
		showFullMessage((char *)"Error writing BMP!");
		delay(1000);
	}
	g_perf.sd_write_us += (micros() - t0);
}
