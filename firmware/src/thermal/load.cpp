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

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <create.h>
#include <thermal.h>
#include <livemode.h>
#include <hardware.h>
#include <display.h>
#include <gui.h>
#include <loadmenu.h>
#include <mainmenu.h>
#include <touchscreen.h>
#include <buttons.h>
#include <fonts.h>
#include <sdcard.h>
#include <connection.h>
#include <lepton.h>
#include <load.h>

/*################# DATA TYPES, CONSTANTS & MACRO DEFINITIONS #################*/

#define lepton2_small 9621
#define lepton2_big 10005
#define lepton3_small 38421
#define lepton3_big 38805
#define bitmap 614466
#define maxFiles 500
#define loadMode_image 0
#define loadMode_video 1

/*######################### STATIC DATA DECLARATIONS ##########################*/

//Storage for up to maxFiles images/videos
static uint16_t *yearStorage;
static byte *monthStorage;
static byte *dayStorage;
static byte *hourStorage;
static byte *minuteStorage;
static byte *secondStorage;

//Buffer for the single elements
static char yearBuf[] = "2016";
static char monthBuf[] = "12";
static char dayBuf[] = "31";
static char hourBuf[] = "23";
static char minuteBuf[] = "60";
static char secondBuf[] = "60";

//Save how many different elements we have
static byte yearnum = 0;
static byte monthnum = 0;
static byte daynum = 0;
static byte hournum = 0;
static byte minutenum = 0;
static byte secondnum = 0;

//Keep track how many images are on the SDCard
static int imgCount = 0;

//Decide if we load videos or images
static bool loadMode;

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Clear all previous data */
void clearData()
{
	for (int i = 0; i < maxFiles; i++)
	{
		yearStorage[i] = 0;
		monthStorage[i] = 0;
		dayStorage[i] = 0;
		hourStorage[i] = 0;
		minuteStorage[i] = 0;
		secondStorage[i] = 0;
	}
	strcpy(yearBuf, "2016");
	strcpy(monthBuf, "12");
	strcpy(dayBuf, "31");
	strcpy(hourBuf, "23");
	strcpy(minuteBuf, "60");
	strcpy(secondBuf, "60");
	yearnum = 0;
	monthnum = 0;
	daynum = 0;
	hournum = 0;
	minutenum = 0;
	secondnum = 0;
	imgCount = 0;
	clearTempPoints();
}

/* Render raw smallBuffer into bigBuffer in RAM (without flushing to LCD yet) */
void renderRawDataToBuffer()
{
	//Select Color Scheme
	selectColorScheme();

	//Apply low-pass filter
	if (filterType == filterType_box)
		boxFilter();
	else if (filterType == filterType_gaussian)
		gaussianFilter();

	//Find min / max position
	if (minMaxPoints != minMaxPoints_disabled)
		refreshMinMax();

	//Resize to big buffer
	smallToBigBuffer();

	//Convert lepton data to RGB565 colors
	convertColors();

	//Composite thermal HUD overlays into bigBuffer in RAM
	displayInfos();
}

/* Display the image on the screen */
void displayRawData()
{
	renderRawDataToBuffer();
	displayBuffer();
}

/* Loads a 640x480 BMP image from the SDCard into bigBuffer (without flushing to LCD yet) */
void loadBMPToBuffer(char *filename)
{
	//Help variables
	byte low, high;
	// Open the file for reading
	if (!sdFile.open(filename, O_READ))
		return;

	//Skip the 66 bytes BMP header
	for (int i = 0; i < 66; i++)
		sdFile.read();

	for (int y = 479; y >= 0; y--)
	{
		for (int x = 0; x < 640; x++)
		{
			low = sdFile.read();
			high = sdFile.read();
			//Get the image color
			bigBuffer[(x / 2) + ((y / 2) * 320)] = (high << 8) | low;
		}
	}
	//Close data file
	sdFile.close();
}

/* Checks if the file is an image*/
bool isImage(char *filename)
{
	bool isImg;

	//Open file
	sdFile.open(filename, O_READ);

	//Check if it is a file
	if (sdFile.isFile())
		isImg = true;

	//Otherwise it is a video
	else
	{
		//Delete the ending
		filename[14] = '\0';
		isImg = false;
	}

	sdFile.close();

	return isImg;
}

/* Read the temperature points from the file, new or old format */
void readTempPoints()
{
	uint16_t tempArray[192];
	bool oldFormat = false;

	//Read the array
	for (byte i = 0; i < 192; i++)
	{
		//Read from SD file
		tempArray[i] = (sdFile.read() << 8) + sdFile.read();

		//Correct old not_set marker
		if (tempArray[i] == 65535)
			tempArray[i] = 0;
	}

	//Check for old format
	for (byte i = 1; i < 191; i++)
	{
		//Valid value found
		if (tempArray[i] != 0)
		{
			//If the value before and after is zero, it is the old format
			if ((tempArray[i - 1] == 0) && (tempArray[i + 1] == 0))
				oldFormat = true;
		}
	}

	//Load the old format
	if (oldFormat)
	{
		//Position counter
		byte pos = 0;

		//Go through the array
		for (byte i = 0; i < 192; i++)
		{
			//Valid value found
			if (tempArray[i] != 0)
			{
				//Calculate x and y position
				uint16_t xpos = (i % 16) * 10;
				uint16_t ypos = (i / 16) * 10;

				//Calculate index
				tempPoints[pos][0] = xpos + (ypos * 160) + 1;
				//Save value
				tempPoints[pos][1] = tempArray[i];

				//Raise position
				pos++;
				//When maximum number of temp points reached, exit
				if (pos == 96)
					return;
			}
		}
	}

	//Load the new format
	else
	{
		//Go through the array
		for (byte i = 0; i < 96; i++)
		{
			//Read index
			tempPoints[i][0] = tempArray[(i * 2)];
			//Read value
			tempPoints[i][1] = tempArray[(i * 2) + 1];
		}
	}
}

/* Loads raw data from the internal storage*/
void loadRawData(char *filename)
{
	byte msb, lsb;
	uint16_t result;
	uint32_t fileSize;

	// Open the file for reading
	sdFile.open(filename, O_READ);

	//Get file size
	fileSize = sdFile.fileSize();

	//For the Lepton2 sensor, read 4800 raw values
	if ((fileSize == lepton2_small) || (fileSize == lepton2_big))
	{
		for (int line = 0; line < 60; line++)
		{
			for (int column = 0; column < 80; column++)
			{
				msb = sdFile.read();
				lsb = sdFile.read();
				result = (((msb) << 8) + lsb);
				smallBuffer[(line * 2 * 160) + (column * 2)] = result;
				smallBuffer[(line * 2 * 160) + (column * 2) + 1] = result;
				smallBuffer[(line * 2 * 160) + 160 + (column * 2)] = result;
				smallBuffer[(line * 2 * 160) + 160 + (column * 2) + 1] = result;
			}
		}
	}

	//For the Lepton3 sensor, read 19200 raw values
	else if ((fileSize == lepton3_small) || (fileSize == lepton3_big))
	{
		for (int i = 0; i < 19200; i++)
		{
			msb = sdFile.read();
			lsb = sdFile.read();
			smallBuffer[i] = (((msb) << 8) + lsb);
		}
	}
	//Invalid data
	else
	{
		showFullMessage((char *)"Invalid file size");
		delay(1000);
		sdFile.close();
		return;
	}

	//Read Min
	msb = sdFile.read();
	lsb = sdFile.read();
	minValue = (((msb) << 8) + lsb);
	//Read Max
	msb = sdFile.read();
	lsb = sdFile.read();
	maxValue = (((msb) << 8) + lsb);

	//Read object temperature
	uint8_t farray[4];
	for (int i = 0; i < 4; i++)
		farray[i] = sdFile.read();
	spotTemp = bytesToFloat(farray);

	//Read color scheme
	colorScheme = sdFile.read();
	//Read temp format
	tempFormat = sdFile.read();
	//Read spot enabled
	spotEnabled = sdFile.read();
	//Read colorbar enabled
	colorbarEnabled = sdFile.read();
	//Read min max enabled
	minMaxPoints = sdFile.read();

	//Read calibration offset but do not use
	for (int i = 0; i < 4; i++)
		farray[i] = sdFile.read();
	//Read calibration slope but do not use
	for (int i = 0; i < 4; i++)
		farray[i] = sdFile.read();

	//Lepton2.5
	if ((fileSize == lepton2_small) || (fileSize == lepton2_big))
	{
		leptonVersion = leptonVersion_2_5_shutter;
	}

	//Lepton3.5
	if ((fileSize == lepton3_small) || (fileSize == lepton3_big))
	{
		leptonVersion = leptonVersion_3_5_shutter;
	}

	//Clear temperature points array
	clearTempPoints();

	//Read temperatures if they are included
	if ((fileSize == lepton3_big) || (fileSize == lepton2_big))
		readTempPoints();

	sdFile.close();
}

/* A method to choose the right yearStorage */
bool yearChoose(char *filename)
{
	//Can imgCount up to 50 years
	bool years[50] = {0};
	//Go through all images
	for (int i = 0; i < imgCount; i++)
	{
		int yearCheck = yearStorage[i] - 2016;
		//Check if the yearStorage is at least 2016
		if (yearCheck < 0)
		{
			//if it is not, return to main menu with error message
			showFullMessage((char *)"The year must be >= 2016");
			delay(1000);
			return true;
			//Check if yearStorage is smaller than 2064 - unlikely the Thermocam is still in use then !
		}
		else if (yearCheck > 49)
		{
			//if it is not, return to main menu with error message
			showFullMessage((char *)"The year must be < 2064");
			delay(1000);
			return true;
			//Add yearStorage to the array if passes the checks
		}
		else
		{
			years[yearCheck] = true;
		}
	}
	for (int i = 0; i < 50; i++)
	{
		if (years[i])
			yearnum = yearnum + 1;
	}
	//Create an array for those years
	int Years[yearnum];
	yearnum = 0;
	//Add them in descending order
	for (int i = 49; i >= 0; i--)
	{
		if (years[i])
		{
			Years[yearnum] = 2016 + i;
			yearnum = yearnum + 1;
		}
	}
	//If there is only one yearStorage, choose it directly
	if (yearnum == 1)
	{
		itoa(Years[0], yearBuf, 10);
	}
	//Otherwise open the yearStorage chooser
	else
	{
		int res = loadMenu((char *)"Load file: Year", Years, yearnum);
		//User want to return to the start menu
		if (res == -1)
		{
			return true;
			//Save the chosen yearStorage
		}
		else
		{
			itoa(Years[res], yearBuf, 10);
		}
	}
	//Copy the chosen yearStorage to the filename
	memcpy(&filename[0], yearBuf, 4);
	return false;
}

/* Shared selector for Month/Day/Hour/Minute/Second buckets (eliminates 5x copy-paste). */
static bool chooseDateBucket(const char *title, const bool *present, int count, int baseVal,
                             char *outBuf, char *filenameDst, byte *outCount)
{
	int values[60];
	byte num = 0;
	for (int i = count - 1; i >= 0; i--)
	{
		if (present[i])
			values[num++] = baseVal + i;
	}
	*outCount = num;
	if (num == 0)
		return true;

	int chosen = values[0];
	if (num > 1)
	{
		int res = loadMenu(const_cast<char *>(title), values, num);
		if (res == -1)
			return true;
		chosen = values[res];
	}
	snprintf(outBuf, 3, "%d", chosen);
	filenameDst[0] = static_cast<char>('0' + (chosen / 10));
	filenameDst[1] = static_cast<char>('0' + (chosen % 10));
	return false;
}

/* Choose month bucket for Find dialog */
bool monthChoose(bool *months, char *filename)
{
	return chooseDateBucket("Load file: Month", months, 12, 1, monthBuf, &filename[4], &monthnum);
}

/* Choose day bucket for Find dialog */
bool dayChoose(bool *days, char *filename)
{
	return chooseDateBucket("Load file: Day", days, 31, 1, dayBuf, &filename[6], &daynum);
}

/* Choose hour bucket for Find dialog */
bool hourChoose(bool *hours, char *filename)
{
	return chooseDateBucket("Load file: Hour", hours, 24, 0, hourBuf, &filename[8], &hournum);
}

/* Choose minute bucket for Find dialog */
bool minuteChoose(bool *minutes, char *filename)
{
	return chooseDateBucket("Load file: Minute", minutes, 60, 0, minuteBuf, &filename[10], &minutenum);
}

/* Choose second bucket for Find dialog */
bool secondChoose(bool *seconds, char *filename)
{
	return chooseDateBucket("Load file: Second", seconds, 60, 0, secondBuf, &filename[12], &secondnum);
}

/* Extract the filename into the buffers */
void copyIntoBuffers(char *filename)
{
	//Save filename into the buffer
	sdFile.getName(filename, 19);
	//Extract the time and date components into extra buffer
	memcpy(yearBuf, &filename[0], 4);
	yearBuf[4] = '\0';
	memcpy(monthBuf, &filename[4], 2);
	monthBuf[2] = '\0';
	memcpy(dayBuf, &filename[6], 2);
	dayBuf[2] = '\0';
	memcpy(hourBuf, &filename[8], 2);
	hourBuf[2] = '\0';
	memcpy(minuteBuf, &filename[10], 2);
	minuteBuf[2] = '\0';
	memcpy(secondBuf, &filename[12], 2);
	secondBuf[2] = '\0';
}

static bool isValidImageFileSize(uint32_t fileSize)
{
	return (fileSize == lepton2_small) || (fileSize == lepton2_big) ||
	       (fileSize == lepton3_small) || (fileSize == lepton3_big) || (fileSize == bitmap);
}

/* Check if the file is a valid thermal image or video folder */
bool checkFileValidity()
{
	if (loadMode == loadMode_image)
		return sdFile.isFile() && isValidImageFileSize(sdFile.fileSize());
	return sdFile.isDir();
}

/* Check if the name matches the criterion */
void checkFileStructure(bool *check)
{
	//Check if yearStorage is 4 digit
	for (int i = 0; i < 4; i++)
	{
		if (!(isdigit(yearBuf[i])))
			*check = false;
	}
	//Check if the other elements are two digits each
	for (int i = 0; i < 2; i++)
	{
		if (!(isdigit(monthBuf[i])))
			*check = false;
		if (!(isdigit(dayBuf[i])))
			*check = false;
		if (!(isdigit(hourBuf[i])))
			*check = false;
		if (!(isdigit(minuteBuf[i])))
			*check = false;
		if (!(isdigit(secondBuf[i])))
			*check = false;
	}
}

/* Check if the filename ends with .DAT or .BMP if the file is a single image */
void checkFileEnding(bool *check, char *filename)
{
	char ending_dat[] = ".DAT";
	//Check for DAT first
	if (((filename[14] != ending_dat[0]) || (filename[15] != ending_dat[1]) || (filename[16] != ending_dat[2]) || (filename[17] != ending_dat[3])))
	{

		//If it is not DAT, it could be BMP
		char ending_bmp[] = ".BMP";
		if ((filename[14] != ending_bmp[0]) || (filename[15] != ending_bmp[1]) || (filename[16] != ending_bmp[2]) || (filename[17] != ending_bmp[3]))
			//None of both
			*check = false;

		//If bitmap, check if the file has a corresponding DAT
		else
		{
			strcpy(&filename[14], ".DAT");
			sdFile.close();
			//Check if it is a file
			sdFile.open(filename, O_READ);
			if (sdFile.isFile())
				*check = false;
			//Open the old file
			strcpy(&filename[14], ".BMP");
			sdFile.close();
			sdFile.open(filename, O_READ);
		}
	}
}

/* Validate current open SD entry as a timestamped gallery item */
static bool matchesGalleryEntry(char *filename)
{
	if (!checkFileValidity())
		return false;
	copyIntoBuffers(filename);
	bool check = true;
	checkFileStructure(&check);
	if (loadMode == loadMode_image)
		checkFileEnding(&check, filename);
	return check;
}

/* Find the next or previous file/folder on the SD card or the position */
bool findFile(char *filename, bool next, bool restart, int *position, char *compare)
{
	(void)restart;
	bool found = false;
	int counter = 0;

	dir.open("/");
	while (sdFile.openNext(&dir, O_RDONLY))
	{
		if (matchesGalleryEntry(filename))
		{
			if (next)
			{
				found = true;
				break;
			}
			if (compare != NULL && strcmp(compare, filename) == 0)
			{
				*position = counter;
				found = true;
				break;
			}
			if (compare == NULL && *position == counter)
			{
				found = true;
				break;
			}
			counter++;
		}
		sdFile.close();
	}

	if (found)
		sdFile.close();
	dir.close();
	return found;
}

/* Search for files and videos */
void searchFiles()
{
	char filename[20];

	//Point to root directory
	dir.open("/");

	//Get filenames from SD Card - one after another and a maximum of maxFiles
	while ((imgCount < maxFiles) && (sdFile.openNext(&dir, O_RDONLY)))
	{
		//Either folder for video or file with specific size for single image
		if (checkFileValidity())
		{
			//Extract the filename into the buffers
			copyIntoBuffers(filename);
			//Check if the name matches the criterion
			bool check = true;
			//Check if the other elements are two digits each
			checkFileStructure(&check);
			//Check if the filename ends with .DAT or .BMP if the file is a single image
			if (loadMode == loadMode_image)
				checkFileEnding(&check, filename);
			//If all checks were successfull, add image to the results
			if (check)
			{
				//If the size of images is not too high
				if (imgCount < maxFiles)
				{
					yearStorage[imgCount] = atoi(yearBuf);
					monthStorage[imgCount] = atoi(monthBuf);
					dayStorage[imgCount] = atoi(dayBuf);
					hourStorage[imgCount] = atoi(hourBuf);
					minuteStorage[imgCount] = atoi(minuteBuf);
					secondStorage[imgCount] = atoi(secondBuf);
					//And raise imgCount by one
					imgCount++;
				}
				//If there are maxFiles images or more
				else
				{
					//Close the file
					sdFile.close();

					//Display an error message
					showFullMessage((char *)"Maximum number of files exceeded");
					delay(1000);
					break;
				}
			}
		}
		//Close the file
		sdFile.close();
	}

	dir.close();
}

/* Choose file */
void chooseFile(char *filename)
{
	//Look for Years
YearLabel:
	//If the user wants to return to the main menu
	if (yearnum == 1 || yearChoose(filename))
	{
		return;
	}
	//Look for monthStorage
MonthLabel:
	//We have twelve months
	bool months[12] = {0};
	for (int i = 0; i < imgCount; i++)
	{
		//Add monthStorage if it belongs to the chosen yearStorage
		if (yearStorage[i] == atoi(yearBuf))
			//Substract one to start array by zero
			months[monthStorage[i] - 1] = true;
	}
	//If the user wants to go back to the years
	if (monthChoose(months, filename))
		goto YearLabel;
	//Look for dayStorage
DayLabel:
	//We have 31 days
	bool days[31] = {0};
	for (int i = 0; i < imgCount; i++)
	{
		//The dayStorage has to match the yearStorage and the monthStorage chosen
		if ((yearStorage[i] == atoi(yearBuf)) && (monthStorage[i] == atoi(monthBuf)))
			//Substract one to start array by zero
			days[dayStorage[i] - 1] = true;
	}
	//If the user wants to go back to the months
	if (dayChoose(days, filename))
	{
		if (monthnum > 1)
			goto MonthLabel;
		else
			goto YearLabel;
	}
	//Look for hourStorage
HourLabel:
	//We have 24 hours
	bool hours[24] = {0};
	for (int i = 0; i < imgCount; i++)
	{
		//Look for match at years, monthStorage and dayStorage
		if ((yearStorage[i] == atoi(yearBuf)) && (monthStorage[i] == atoi(monthBuf)) && (dayStorage[i] == atoi(dayBuf)))
			hours[hourStorage[i]] = true;
	}
	//If the user wants to go back to the days
	if (hourChoose(hours, filename))
	{
		if (daynum > 1)
			goto DayLabel;
		else if (monthnum > 1)
			goto MonthLabel;
		else
			goto YearLabel;
	}
	//Look for minuteStorage
MinuteLabel:
	//We have 60 minutes
	bool minutes[60] = {0};
	for (int i = 0; i < imgCount; i++)
	{
		//Watch for yearStorage, monthStorage, dayStorage and hourStorage
		if ((yearStorage[i] == atoi(yearBuf)) && (monthStorage[i] == atoi(monthBuf)) && (dayStorage[i] == atoi(dayBuf)) && (hourStorage[i] == atoi(hourBuf)))
			minutes[minuteStorage[i]] = true;
	}
	//If the user wants to go back to the hours
	if (minuteChoose(minutes, filename))
	{
		if (hournum > 1)
			goto HourLabel;
		else if (daynum > 1)
			goto DayLabel;
		else if (monthnum > 1)
			goto MonthLabel;
		else
			goto YearLabel;
	}
	//Look for secondStorage
	//We have 60 seconds
	bool seconds[60] = {0};
	for (int i = 0; i < imgCount; i++)
	{
		//Watch for yearStorage, monthStorage, dayStorage, hourStorage and minuteStorage
		if ((yearStorage[i] == atoi(yearBuf)) && (monthStorage[i] == atoi(monthBuf)) && (dayStorage[i] == atoi(dayBuf)) && (hourStorage[i] == atoi(hourBuf)) && (minuteStorage[i] == atoi(minuteBuf)))
			seconds[secondStorage[i]] = true;
	}
	//If the user wants to go back to the minutes
	if (secondChoose(seconds, filename))
	{
		if (minutenum > 1)
			goto MinuteLabel;
		else if (hournum > 1)
			goto HourLabel;
		else if (daynum > 1)
			goto DayLabel;
		else if (monthnum > 1)
			goto MonthLabel;
		else
			goto YearLabel;
	}

	//For an image, add the ending
	if (loadMode == loadMode_image)
	{
		//Try to add the DAT file
		strcpy(&filename[14], ".DAT");
		//Check if it is a DAT file
		sdFile.open(filename, O_READ);
		if (sdFile.isFile())
		{
			sdFile.close();
			return;
		}
		//If not, use bitmap
		strcpy(&filename[14], ".BMP");
		//Close the file
		sdFile.close();
	}

	//Video ending
	else
		//Delete the ending for a video
		filename[14] = '\0';
}

/* Delete image / video function */
bool loadDelete(char *filename, int *pos)
{
	//Image
	if (isImage(filename))
		deleteImage(filename);
	//Video
	else
		deleteVideo(filename);
	//Clear all previous data
	clearData();
	//Search files
	searchFiles();
	//If there are no files left, return
	if (imgCount == 0)
	{
		if (loadMode == loadMode_image)
			showFullMessage((char *)"No images found");
		else
			showFullMessage((char *)"No videos found");
		delay(1000);
		return false;
	}
	//Decrease by one if the last image/video was deleted
	if (*pos > (imgCount - 1))
		*pos = imgCount - 1;
	//Find the name of the next file
	findFile(filename, false, true, pos);
	return true;
}

/* Find image / video function */
void loadFind(char *filename, int *pos)
{
	//If there is only one image
	if (imgCount == 1)
	{
		if (loadMode == loadMode_image)
			showFullMessage((char *)"Only one image available");
		else
			showFullMessage((char *)"Only one video available");
		delay(1000);
		return;
	}
	//Clear all previous data
	clearData();
	//Search files
	searchFiles();
	//Fill screen with active 2026 theme background
	display_fillScr(UI_BG_DARK);
	//Let the user choose a new file
	chooseFile(filename);
	//Copy the filename for a compare value
	char compare[20];
	snprintf(compare, sizeof(compare), "%s", filename);
	//Find the new file position
	findFile(filename, false, true, pos, compare);
}

/* Alloc space for the different arrays*/
void loadAlloc()
{
	yearStorage = (uint16_t *)calloc(maxFiles, sizeof(uint16_t));
	monthStorage = (byte *)calloc(maxFiles, sizeof(byte));
	dayStorage = (byte *)calloc(maxFiles, sizeof(byte));
	hourStorage = (byte *)calloc(maxFiles, sizeof(byte));
	minuteStorage = (byte *)calloc(maxFiles, sizeof(byte));
	secondStorage = (byte *)calloc(maxFiles, sizeof(byte));
}

/* Change settings for load menu */
void loadSettings()
{
	//Do not show additional information that are not required
	batteryEnabled = false;
	dateEnabled = false;
	timeEnabled = false;
	storageEnabled = false;
	minMaxPoints = minMaxPoints_disabled;
	hotColdMode = hotColdMode_disabled;
}

/* De-Alloc space for the different arrays*/
void loadDeAlloc()
{
	free(yearStorage);
	free(monthStorage);
	free(dayStorage);
	free(hourStorage);
	free(minuteStorage);
	free(secondStorage);
}

/* Main-loop touch decoder for load/gallery viewer */
void loadTouchProcess()
{
	//Get touch coordinates safely in main-loop context
	TS_Point p = touch_getPoint();
	//Ignore phantom/stale coordinates when no touch is actively pressed
	if (p.z == 0)
		return;
	uint16_t x = p.x;
	uint16_t y = p.y;

	//Find
	if ((x >= 10) && (x <= 140) && (y >= 10) && (y <= 80))
		loadTouch = loadTouch_find;

	//Exit
	else if ((x >= 180) && (x <= 310) && (y >= 160) && (y <= 230))
		loadTouch = loadTouch_exit;

	//Previous
	else if ((x >= 10) && (x <= 140) && (y >= 90) && (y <= 150) && (imgCount != 1))
		loadTouch = loadTouch_previous;

	//Next
	else if ((x >= 180) && (x <= 310) && (y >= 90) && (y <= 150) && (imgCount != 1))
		loadTouch = loadTouch_next;

	//Delete
	else if ((x >= 180) && (x <= 310) && (y >= 10) && (y <= 80))
		loadTouch = loadTouch_delete;

	//Convert
	else if ((x >= 10) && (x <= 140) && (y >= 160) && (y <= 230))
		loadTouch = loadTouch_convert;

	//Middle (covers centered Play pill whether spotEnabled is on or off)
	else if ((x > 140) && (x < 180) && (y >= 64) && (y <= 160))
		loadTouch = loadTouch_middle;
}

/* Dispatch one viewer touch command inside loadFiles(); returns true to exit viewer */
static bool dispatchGalleryAction(char *filename, int *pos)
{
	switch (loadTouch)
	{
	case loadTouch_find:
		loadFind(filename, pos);
		return false;
	case loadTouch_delete:
		return !loadDelete(filename, pos);
	case loadTouch_previous:
		showFullMessage((char *)"Loading..");
		*pos = (*pos == imgCount - 1) ? 0 : (*pos + 1);
		return !findFile(filename, false, true, pos);
	case loadTouch_next:
		showFullMessage((char *)"Loading..");
		*pos = (*pos == 0) ? (imgCount - 1) : (*pos - 1);
		return !findFile(filename, false, true, pos);
	case loadTouch_convert:
		if (isImage(filename) && (loadMode == loadMode_image))
			convertImage(filename);
		else
			convertVideo(filename);
		return false;
	case loadTouch_none:
	case loadTouch_exit:
		return true;
	default:
		return false;
	}
}

/* Gallery chooser prompt: returns false if user tapped < Back */
static bool promptGalleryMode()
{
	drawLoadMenu();
	ui_waitTouchRelease();
	while (true)
	{
		if (checkSerial())
			drawLoadMenu();
		if (touch_touched())
		{
			int pressed = buttons_checkButtons(true);
			if (pressed == 0) { loadMode = loadMode_image; return true; }
			if (pressed == 1) { loadMode = loadMode_video; return true; }
			if (pressed == 2) return false;
		}
		delay(5);
	}
}

/* Main entry point for loading images/videos */
void loadFiles()
{
	while (true)
	{
		if (!promptGalleryMode())
			return;

		showFullMessage((char *)"Please wait..", true);
		loadAlloc();
		clearData();
		searchFiles();

		if (imgCount > 0)
			break;

		showFullMessage((loadMode == loadMode_image) ? (char *)"No images found" : (char *)"No videos found", true);
		delay(1000);
		loadDeAlloc();
	}

	char filename[20];
	uint16_t old_minValue = minValue;
	uint16_t old_maxValue = maxValue;
	byte old_leptonVersion = leptonVersion;

	loadSettings();

	int pos = imgCount - 1;
	bool exit = !findFile(filename, false, true, &pos);
	detachInterrupt(pin_touch_irq);

	while (!exit)
	{
		loadTouch = loadTouch_none;
		if (isImage(filename) && (loadMode == loadMode_image))
			openImage(filename, imgCount);
		else
			playVideo(filename, imgCount);

		exit = dispatchGalleryAction(filename, &pos);

		uint32_t tRel = millis();
		if (touch_capacitive)
			while (touch_touched() && (millis() - tRel) < 1500u) delay(5);
		else
			while (!digitalRead(pin_touch_irq) && (millis() - tRel) < 1500u) delay(5);
	}

	drawMainMenuBorder();
	showFullMessage((char *)"Please wait..", true);
	loadDeAlloc();

	minValue = old_minValue;
	maxValue = old_maxValue;
	leptonVersion = old_leptonVersion;
	readEEPROM();
	refreshFreeSpace();

	attachInterrupt(pin_touch_irq, touchIRQ, FALLING);
	disableSPIIRQ = false;
	lepton_startFrame();
}
