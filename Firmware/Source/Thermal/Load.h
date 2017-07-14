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
* https://github.com/maxritter/DIY-Thermocam
*
*/

/* Defines */

#define lepton2_small 9621
#define lepton2_big 10005
#define lepton3_small 38421
#define lepton3_big 38805
#define bitmap 614466
#define maxFiles 500
#define loadMode_image 0
#define loadMode_video 1

/* Variables */

//Storage for up to maxFiles images/videos
uint16_t* yearStorage;
byte* monthStorage;
byte* dayStorage;
byte* hourStorage;
byte* minuteStorage;
byte* secondStorage;

//Buffer for the single elements
char yearBuf[] = "2016";
char monthBuf[] = "12";
char dayBuf[] = "31";
char hourBuf[] = "23";
char minuteBuf[] = "60";
char secondBuf[] = "60";

//Save how many different elements we have
byte yearnum = 0;
byte monthnum = 0;
byte daynum = 0;
byte hournum = 0;
byte minutenum = 0;
byte secondnum = 0;

//Keep track how many images are on the SDCard
int imgCount = 0;

//Decide if we load videos or images
bool loadMode;

/* Methods */

/* Clear all previous data */
void clearData() {
	for (int i = 0; i < maxFiles; i++) {
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

/* Display the image on the screen */
void displayRawData() {
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

	//Teensy 3.6 - Resize to big buffer
	if (teensyVersion == teensyVersion_new)
		smallToBigBuffer();

	//Convert lepton data to RGB565 colors
	convertColors();

	//Display additional information
	displayInfos();

	//Display on screen
	displayBuffer();
}

/* Loads a 640x480 BMP image from the SDCard and prints it on screen */
void loadBMPImage(char* filename) {
	//Help variables
	byte low, high;
	//Switch Clock to Alternative
	startAltClockline();
	// Open the file for reading
	sdFile.open(filename, O_READ);

	//Skip the 66 bytes BMP header
	for (int i = 0; i < 66; i++)
		sdFile.read();

	//For Teensy 3.1 / 3.2, repeat the procedure 4 times to fill all the buffers
	if (teensyVersion == teensyVersion_old)
	{
		for (int i = 3; i >= 0; i--) {
			//Save those 20 lines to the SD card. Ascending to mirror vertically
			for (int y = 119; y >= 0; y--) {
				for (int x = 0; x < 640; x++) {
					low = sdFile.read();
					high = sdFile.read();
					//Get the image color
					smallBuffer[(x / 2) + ((y / 2) * 320)] = (high << 8) | low;
				}
			}
			//End alternative clock line
			endAltClockline();
			//Draw on the screen
			display_drawBitmap(0, i * 60, 320, 60, smallBuffer);
			//Start alternative clock line
			startAltClockline();
		}
		//Close data file
		sdFile.close();
		//Switch clock back
		endAltClockline();
	}
	//For Teensy 3.6, use the big buffer
	else
	{
		for (int y = 479; y >= 0; y--)
		{
			for (int x = 0; x < 640; x++) {
				low = sdFile.read();
				high = sdFile.read();
				//Get the image color
				bigBuffer[(x/2) + ((y / 2) * 320)] = (high << 8) | low;
			}
		}
		//Close data file
		sdFile.close();
		//Switch clock back
		endAltClockline();
		//Draw it on the screen
		display_drawBitmap(0, 0, 320, 240, bigBuffer);
	}
}

/* Checks if the file is an image*/
bool isImage(char* filename) {
	bool isImg;

	//Switch Clock to Alternative
	startAltClockline(true);
	//Open file
	sdFile.open(filename, O_READ);

	//Check if it is a file
	if (sdFile.isFile())
		isImg = true;

	//Otherwise it is a video
	else {
		//Delete the ending
		filename[14] = '\0';
		isImg = false;
	}

	sdFile.close();
	//Switch Clock back to Standard
	endAltClockline();

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
		for (byte i = 0; i < 96; i++) {
			//Read index
			tempPoints[i][0] = tempArray[(i * 2)];
			//Read value
			tempPoints[i][1] = tempArray[(i * 2) + 1];
		}
	}
}

/* Loads raw data from the internal storage*/
void loadRawData(char* filename, char* dirname) {
	byte msb, lsb;
	uint16_t result;
	uint32_t fileSize;

	//Switch Clock to Alternative
	startAltClockline();

	//Go into the video folder if video
	if (dirname != NULL)
		sd.chdir(dirname);

	// Open the file for reading
	sdFile.open(filename, O_READ);

	//Get file size
	fileSize = sdFile.fileSize();

	//For the Lepton2 sensor, read 4800 raw values
	if ((fileSize == lepton2_small) || (fileSize == lepton2_big)) {
		for (int line = 0; line < 60; line++) {
			for (int column = 0; column < 80; column++) {
				msb = sdFile.read();
				lsb = sdFile.read();
				result = (((msb) << 8) + lsb);
				smallBuffer[(line * 2 * 160) + (column * 2)] = result;
				smallBuffer[(line * 2 * 160) + (column * 2) + 1] = result;
				smallBuffer[(line * 2 * 160) + 160 + (column * 2)] = result;
				smallBuffer[(line * 2 * 160) + 160 + (column * 2) + 1] = result;
			}
		}
		leptonVersion = leptonVersion_2_0_shutter;
	}

	//For the Lepton3 sensor, read 19200 raw values
	else if ((fileSize == lepton3_small) || (fileSize == lepton3_big)) {
		for (int i = 0; i < 19200; i++) {
			msb = sdFile.read();
			lsb = sdFile.read();
			smallBuffer[i] = (((msb) << 8) + lsb);
		}
		leptonVersion = leptonVersion_3_0_shutter;
	}
	//Invalid data
	else {
		showFullMessage((char*) "Invalid file size");
		delay(1000);
		sdFile.close();
		endAltClockline();
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

	//Read calibration offset
	for (int i = 0; i < 4; i++)
		farray[i] = sdFile.read();
	calOffset = bytesToFloat(farray);
	//Read calibration slope
	for (int i = 0; i < 4; i++)
		farray[i] = sdFile.read();
	calSlope = bytesToFloat(farray);

	//Clear temperature points array
	clearTempPoints();

	//Read temperatures if they are included
	if ((fileSize == lepton3_big) || (fileSize == lepton2_big))
		readTempPoints();

	//Close data file
	sdFile.close();
	//Switch clock back
	endAltClockline();
}

/* A method to choose the right yearStorage */
bool yearChoose(char* filename) {
	//Can imgCount up to 50 years
	bool years[50] = { 0 };
	//Go through all images
	for (int i = 0; i < imgCount; i++) {
		int yearCheck = yearStorage[i] - 2016;
		//Check if the yearStorage is at least 2016
		if (yearCheck < 0) {
			//if it is not, return to main menu with error message
			showFullMessage((char*) "The year must be >= 2016");
			delay(1000);
			return true;
			//Check if yearStorage is smaller than 2064 - unlikely the Thermocam is still in use then !
		}
		else if (yearCheck > 49) {
			//if it is not, return to main menu with error message
			showFullMessage((char*) "The year must be < 2064");
			delay(1000);
			return true;
			//Add yearStorage to the array if passes the checks
		}
		else {
			years[yearCheck] = true;
		}
	}
	for (int i = 0; i < 50; i++) {
		if (years[i])
			yearnum = yearnum + 1;
	}
	//Create an array for those years
	int Years[yearnum];
	yearnum = 0;
	//Add them in descending order
	for (int i = 49; i >= 0; i--) {
		if (years[i]) {
			Years[yearnum] = 2016 + i;
			yearnum = yearnum + 1;
		}
	}
	//If there is only one yearStorage, choose it directly
	if (yearnum == 1) {
		itoa(Years[0], yearBuf, 10);
	}
	//Otherwise open the yearStorage chooser
	else {
		int res = loadMenu((char*) "Load file: Year", Years, yearnum);
		//User want to return to the start menu
		if (res == -1) {
			return true;
			//Save the chosen yearStorage
		}
		else {
			itoa(Years[res], yearBuf, 10);
		}
	}
	//Copy the chosen yearStorage to the filename
	strncpy(&filename[0], yearBuf, 4);
	return false;
}

/* A method to choose the right monthStorage */
bool monthChoose(bool* months, char* filename) {
	for (int i = 0; i < 12; i++) {
		if (months[i])
			monthnum = monthnum + 1;
	}
	//Add them to the array in descending order
	int Months[monthnum];
	monthnum = 0;
	for (int i = 11; i >= 0; i--) {
		if (months[i]) {
			//Add one as monthStorage start with January
			Months[monthnum] = 1 + i;
			monthnum = monthnum + 1;
		}
	}
	//If there is only one, chose it directly
	if (monthnum == 1) {
		itoa(Months[0], monthBuf, 10);
	}
	//If not, open the image chooser
	else {
		int res = loadMenu((char*) "Load file: Month", Months,
			monthnum);
		//the user wants to go back to the years
		if (res == -1) {
			return true;
		}
		else {
			itoa(Months[res], monthBuf, 10);
		}
	}
	//Add a zero if monthStorage is smaller than 10
	if (atoi(monthBuf) < 10) {
		filename[4] = '0';
		strncpy(&filename[5], monthBuf, 1);
	}
	//Else copy those two digits
	else
		strncpy(&filename[4], monthBuf, 2);
	return false;
}

/* A method to choose the right dayStorage */
bool dayChoose(bool* days, char* filename) {
	for (int i = 0; i < 31; i++) {
		if (days[i])
			daynum = daynum + 1;
	}
	//Sort them descending
	int Days[daynum];
	daynum = 0;
	for (int i = 30; i >= 0; i--) {
		if (days[i]) {
			Days[daynum] = 1 + i;
			daynum = daynum + 1;
		}
	}
	//We only have one
	if (daynum == 1) {
		itoa(Days[0], dayBuf, 10);

	}
	//Choose dayStorage
	else {
		int res = loadMenu((char*) "Load file: Day", Days, daynum);
		if (res == -1) {
			return true;
		}
		else {
			itoa(Days[res], dayBuf, 10);
		}
	}
	//Add dayStorage to the filename
	if (atoi(dayBuf) < 10) {
		filename[6] = '0';
		strncpy(&filename[7], dayBuf, 1);
	}
	else
		strncpy(&filename[6], dayBuf, 2);
	return false;
}

/* A method to choose the right hourStorage */
bool hourChoose(bool* hours, char* filename) {
	for (int i = 0; i < 24; i++) {
		if (hours[i])
			hournum = hournum + 1;
	}
	//Sort them descending
	int Hours[hournum];
	hournum = 0;
	for (int i = 23; i >= 0; i--) {
		if (hours[i]) {
			Hours[hournum] = i;
			hournum = hournum + 1;
		}
	}
	//If there is only one
	if (hournum == 1) {
		itoa(Hours[0], hourBuf, 10);

	}
	//Otherwise choose
	else {
		int res = loadMenu((char*) "Load file: Hour", Hours, hournum);
		if (res == -1) {
			return true;
		}
		else {
			itoa(Hours[res], hourBuf, 10);
		}
	}
	//Add hourStorage to the filename
	if (atoi(hourBuf) < 10) {
		filename[8] = '0';
		strncpy(&filename[9], hourBuf, 1);
	}
	else
		strncpy(&filename[8], hourBuf, 2);
	return false;
}

/* A method to choose the right minuteStorage */
bool minuteChoose(bool* minutes, char* filename) {
	for (int i = 0; i < 60; i++) {
		if (minutes[i])
			minutenum = minutenum + 1;
	}
	//Sort them descending
	int Minutes[minutenum];
	minutenum = 0;
	for (int i = 59; i >= 0; i--) {
		if (minutes[i]) {
			Minutes[minutenum] = i;
			minutenum = minutenum + 1;
		}
	}
	//if there is only one
	if (minutenum == 1) {
		itoa(Minutes[0], minuteBuf, 10);

	}
	//Otherwise choose
	else {
		int res = loadMenu((char*) "Load file: Minute", Minutes,
			minutenum);
		if (res == -1) {
			return true;
		}
		else {
			itoa(Minutes[res], minuteBuf, 10);
		}
	}
	//Copy minutes to the filename
	if (atoi(minuteBuf) < 10) {
		filename[10] = '0';
		strncpy(&filename[11], minuteBuf, 1);
	}
	else
		strncpy(&filename[10], minuteBuf, 2);
	return false;
}

/* A method to choose the right secondStorage */
bool secondChoose(bool* seconds, char* filename) {
	for (int i = 0; i < 60; i++) {
		if (seconds[i])
			secondnum = secondnum + 1;
	}
	//Sort them descending
	int Seconds[secondnum];
	secondnum = 0;
	for (int i = 59; i >= 0; i--) {
		if (seconds[i]) {
			Seconds[secondnum] = i;
			secondnum = secondnum + 1;
		}
	}
	//if there is only one
	if (secondnum == 1) {
		itoa(Seconds[0], secondBuf, 10);
	}
	//Otherwise choose the right
	else {
		int res = loadMenu((char*) "Load file: Second", Seconds,
			secondnum);
		if (res == -1) {
			return true;
		}
		else {
			itoa(Seconds[res], secondBuf, 10);
		}
	}
	//Add secondStorage to the filename
	if (atoi(secondBuf) < 10) {
		filename[12] = '0';
		strncpy(&filename[13], secondBuf, 1);
	}
	else
		strncpy(&filename[12], secondBuf, 2);
	return false;
}

/* Extract the filename into the buffers */
void copyIntoBuffers(char* filename) {
	//Save filename into the buffer
	sdFile.getName(filename, 19);
	//Extract the time and date components into extra buffer
	strncpy(yearBuf, &filename[0], 4);
	strncpy(monthBuf, &filename[4], 2);
	strncpy(dayBuf, &filename[6], 2);
	strncpy(hourBuf, &filename[8], 2);
	strncpy(minuteBuf, &filename[10], 2);
	strncpy(secondBuf, &filename[12], 2);
}

/* Check if the file is a valid one */
bool checkFileValidity() {
	//Load images
	if (loadMode == loadMode_image)
	{
		uint32_t fileSize = sdFile.fileSize();
		return (sdFile.isFile() && ((fileSize == lepton2_small) || (fileSize == lepton2_big) ||
			(fileSize == lepton3_small) || (fileSize == lepton3_big) || (fileSize == bitmap)));
	}
	//Load videos
	return sdFile.isDir();
}

/* Check if the name matches the criterion */
void checkFileStructure(bool* check) {
	//Check if yearStorage is 4 digit
	for (int i = 0; i < 4; i++) {
		if (!(isdigit(yearBuf[i])))
			*check = false;
	}
	//Check if the other elements are two digits each
	for (int i = 0; i < 2; i++) {
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
void checkFileEnding(bool* check, char* filename) {
	char ending_dat[] = ".DAT";
	//Check for DAT first
	if (((filename[14] != ending_dat[0]) || (filename[15] != ending_dat[1])
		|| (filename[16] != ending_dat[2])
		|| (filename[17] != ending_dat[3]))) {

		//If it is not DAT, it could be BMP
		char ending_bmp[] = ".BMP";
		if ((filename[14] != ending_bmp[0]) || (filename[15] != ending_bmp[1])
			|| (filename[16] != ending_bmp[2])
			|| (filename[17] != ending_bmp[3]))
			//None of both
			*check = false;

		//If bitmap, check if the file has a corresponding DAT
		else {
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

/* Find the next or previous file/folder on the SD card or the position */
bool findFile(char* filename, bool next, bool restart, int* position = 0, char* compare = NULL) {
	bool found = false;
	int counter = 0;

	//Start SD Transmission
	startAltClockline(restart);

	//Get filenames from SD Card - one after another and a maximum of maxFiles
	while (sdFile.openNext(sd.vwd(), O_READ)) {

		//Either folder for video or file with specific size for single image
		if (checkFileValidity()) {
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
			if (check) {
				//If we want to get the next image
				if (next) {
					found = true;
					break;
				}
				//If we want to get the previous image or get the position
				if (compare != NULL) {
					if (strcmp(compare, filename) == 0) {
						*position = counter;
						found = true;
						break;
					}
					counter++;
				}
				//In case we found it
				else if (*position == counter) {
					found = true;
					break;
				}
				//If not, raise the counter
				else
					counter++;
			}
		}
		//Close the file
		sdFile.close();
	}

	//End SD Transmission
	endAltClockline();
	//Return result
	return found;
}

/* Search for files and videos */
void searchFiles() {
	char filename[20];
	//Start SD Transmission
	startAltClockline(true);

	//Get filenames from SD Card - one after another and a maximum of maxFiles
	while ((imgCount < maxFiles) && (sdFile.openNext(sd.vwd(), O_READ))) {
		//Either folder for video or file with specific size for single image
		if (checkFileValidity()) {
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
			if (check) {
				//If the size of images is not too high
				if (imgCount < maxFiles) {
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
				else {
					//Close the file
					sdFile.close();
					//End SD Transmission
					endAltClockline();
					//Display an error message
					showFullMessage((char*) "Maximum number of files exceeded");
					delay(1000);
					//And return to the main menu
					mainMenu();
					return;
				}
			}
		}
		//Close the file
		sdFile.close();
	}

	//End SD Transmission
	endAltClockline();
}

/* Choose file */
void chooseFile(char* filename) {
	//Look for Years
YearLabel:
	//If the user wants to return to the main menu
	if (yearnum == 1
		|| yearChoose(filename)) {
		return;
	}
	//Look for monthStorage
MonthLabel:
	//We have twelve months
	bool months[12] = { 0 };
	for (int i = 0; i < imgCount; i++) {
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
	bool days[31] = { 0 };
	for (int i = 0; i < imgCount; i++) {
		//The dayStorage has to match the yearStorage and the monthStorage chosen
		if ((yearStorage[i] == atoi(yearBuf)) && (monthStorage[i] == atoi(monthBuf)))
			//Substract one to start array by zero
			days[dayStorage[i] - 1] = true;
	}
	//If the user wants to go back to the months
	if (dayChoose(days, filename)) {
		if (monthnum > 1)
			goto MonthLabel;
		else
			goto YearLabel;
	}
	//Look for hourStorage
HourLabel:
	//We have 24 hours
	bool hours[24] = { 0 };
	for (int i = 0; i < imgCount; i++) {
		//Look for match at years, monthStorage and dayStorage
		if ((yearStorage[i] == atoi(yearBuf)) && (monthStorage[i] == atoi(monthBuf))
			&& (dayStorage[i] == atoi(dayBuf)))
			hours[hourStorage[i]] = true;
	}
	//If the user wants to go back to the days
	if (hourChoose(hours, filename)) {
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
	bool minutes[60] = { 0 };
	for (int i = 0; i < imgCount; i++) {
		//Watch for yearStorage, monthStorage, dayStorage and hourStorage
		if ((yearStorage[i] == atoi(yearBuf)) && (monthStorage[i] == atoi(monthBuf))
			&& (dayStorage[i] == atoi(dayBuf)) && (hourStorage[i] == atoi(hourBuf)))
			minutes[minuteStorage[i]] = true;
	}
	//If the user wants to go back to the hours
	if (minuteChoose(minutes, filename)) {
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
	bool seconds[60] = { 0 };
	for (int i = 0; i < imgCount; i++) {
		//Watch for yearStorage, monthStorage, dayStorage, hourStorage and minuteStorage
		if ((yearStorage[i] == atoi(yearBuf)) && (monthStorage[i] == atoi(monthBuf))
			&& (dayStorage[i] == atoi(dayBuf)) && (hourStorage[i] == atoi(hourBuf))
			&& (minuteStorage[i] == atoi(minuteBuf)))
			seconds[secondStorage[i]] = true;
	}
	//If the user wants to go back to the minutes
	if (secondChoose(seconds, filename)) {
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
		//Start SD Transmission
		startAltClockline();
		//Try to add the DAT file
		strcpy(&filename[14], ".DAT");
		//Check if it is a DAT file
		sdFile.open(filename, O_READ);
		if (sdFile.isFile()) {
			sdFile.close();
			return;
		}
		//If not, use bitmap
		strcpy(&filename[14], ".BMP");
		//Close the file
		sdFile.close();
		//End SD Transmission
		endAltClockline();
	}

	//Video ending
	else
		//Delete the ending for a video
		filename[14] = '\0';
}

/* Delete image / video function */
bool loadDelete(char* filename, int* pos) {
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
	if (imgCount == 0) {
		if (loadMode == loadMode_image)
			showFullMessage((char*) "No images found");
		else
			showFullMessage((char*) "No videos found");
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
void loadFind(char* filename, int* pos) {
	//If there is only one image
	if (imgCount == 1) {
		if (loadMode == loadMode_image)
			showFullMessage((char*) "Only one image available");
		else
			showFullMessage((char*) "Only one video available");
		delay(1000);
		return;
	}
	//Clear all previous data
	clearData();
	//Search files
	searchFiles();
	//Fill screen
	display_fillScr(200, 200, 200);
	//Let the user choose a new file
	chooseFile(filename);
	//Copy the filename for a compare value
	char compare[20];
	strncpy(compare, filename, 20);
	//Find the new file position
	findFile(filename, false, true, pos, compare);
}

/* Alloc space for the different arrays*/
void loadAlloc() {
	yearStorage = (uint16_t*)calloc(maxFiles, sizeof(uint16_t));
	monthStorage = (byte*)calloc(maxFiles, sizeof(byte));
	dayStorage = (byte*)calloc(maxFiles, sizeof(byte));
	hourStorage = (byte*)calloc(maxFiles, sizeof(byte));
	minuteStorage = (byte*)calloc(maxFiles, sizeof(byte));
	secondStorage = (byte*)calloc(maxFiles, sizeof(byte));
}

/* Change settings for load menu */
void loadSettings() {
	//Set calibration status to manual
	calStatus = cal_manual;
	//Do not show additional information that are not required
	batteryEnabled = false;
	dateEnabled = false;
	timeEnabled = false;
	storageEnabled = false;
	minMaxPoints = minMaxPoints_disabled;
	hotColdMode = hotColdMode_disabled;
	//For Teensy 3.6, use HQ resolution for loading
	if (teensyVersion == teensyVersion_new)
		hqRes = true;
}

/* De-Alloc space for the different arrays*/
void loadDeAlloc() {
	free(yearStorage);
	free(monthStorage);
	free(dayStorage);
	free(hourStorage);
	free(minuteStorage);
	free(secondStorage);
}

/* Interrupt handler for the load touch menu */
void loadTouchIRQ() {
	//Get touch coordinates 
	TS_Point p = touch_getPoint();
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

	//Middle
	else if ((x > 140) && (x < 180) && (y > 80) && (y < 160))
		loadTouch = loadTouch_middle;
}

/* Main entry point for loading images/videos*/
void loadFiles() {
	//Old hardware
	//ThermocamV4 or DIY-Thermocam V2, check SD card
	if ((mlx90614Version == mlx90614Version_old) ||
		(teensyVersion == teensyVersion_new)) {
		showFullMessage((char*) "Checking SD card..", true);
		if (!checkSDCard()) {
			showFullMessage((char*) "Insert SD card", true);
			delay(1000);
			mainMenu();
			return;
		}
	}

redraw:
	//Title & Background
	mainMenuBackground();
	mainMenuTitle((char*)"Load Menu");
	//Draw the buttons
	buttons_deleteAllButtons();
	buttons_setTextFont(bigFont);
	buttons_addButton(15, 47, 140, 120, (char*) "Images");
	buttons_addButton(165, 47, 140, 120, (char*) "Videos");
	buttons_addButton(15, 188, 140, 40, (char*) "Back");
	buttons_drawButtons();
	//Touch handler
	while (true) {
		//If touch pressed
		if (touch_touched() == true) {
			int pressedButton = buttons_checkButtons(true);
			//IMAGES
			if (pressedButton == 0) {
				loadMode = loadMode_image;
				break;
			}
			//VIDEOS
			if (pressedButton == 1) {
				loadMode = loadMode_video;
				break;
			}
			//BACK
			if (pressedButton == 2)
				return;
		}
	}

	//Store the filename
	char filename[20];

	//Save old settings
	uint16_t old_minValue = minValue;
	uint16_t old_maxValue = maxValue;
	byte old_leptonVersion = leptonVersion;
	byte old_calStatus = calStatus;
	float old_calOffset = calOffset;
	float old_calSlope = calSlope;

	//Load message
	showFullMessage((char*) "Please wait..", true);
	//Alloc space
	loadAlloc();
	//Clear all previous data
	clearData();
	//Search files
	searchFiles();

	//If there are no images or videos, return
	if (imgCount == 0) {
		//Show message
		if (loadMode == loadMode_image)
			showFullMessage((char*) "No images found", true);
		else
			showFullMessage((char*) "No videos found", true);
		delay(1000);
		//Deallocate space
		loadDeAlloc();
		//Redraw menu
		goto redraw;
	}

	//Change settings
	loadSettings();

	//Open the latest file
	int pos = imgCount - 1;
	findFile(filename, false, true, &pos);
	bool exit = false;

	//New touch interrupt
	detachInterrupt(pin_touch_irq);

	//Main loop
	while (true) {

		//Disable decision marker
		loadTouch = loadTouch_none;

		//Load image
		if (isImage(filename) && (loadMode == loadMode_image))
			openImage(filename, imgCount);

		//Play video
		else
			playVideo(filename, imgCount);

		//Touch actions
		switch (loadTouch) {

			//Find
		case loadTouch_find:
			loadFind(filename, &pos);
			break;

			//Delete
		case loadTouch_delete:
			if (!loadDelete(filename, &pos))
				exit = true;
			break;

			//Previous
		case loadTouch_previous:
			showFullMessage((char*) "Loading..");
			if (!findFile(filename, true, false)) {
				findFile(filename, true, true);
				pos = 0;
			}
			else
				pos++;
			break;

			//Next
		case loadTouch_next:
			showFullMessage((char*) "Loading..");
			if (pos == 0)
				pos = imgCount - 1;
			else
				pos--;
			findFile(filename, false, true, &pos);
			break;

			//Exit
		case loadTouch_exit:
			exit = true;
			break;

			//Convert
		case loadTouch_convert:
			//Image
			if (isImage(filename) && (loadMode == loadMode_image))
				convertImage(filename);
			//Video
			else
				convertVideo(filename);
			break;
		}

		//Wait for touch release
		if (touch_capacitive)
			while (touch_touched());
		else
			while (!digitalRead(pin_touch_irq));

		//Leave
		if (exit)
			break;
	}
	//Show message
	drawMainMenuBorder();
	showFullMessage((char*) "Please wait..", true);

	//Deallocate space
	loadDeAlloc();

	//Restore old settings from variables
	minValue = old_minValue;
	maxValue = old_maxValue;
	leptonVersion = old_leptonVersion;
	calStatus = old_calStatus;
	calOffset = old_calOffset;
	calSlope = old_calSlope;

	//Restore the rest from EEPROM
	readEEPROM();

	//Refresh SD space
	refreshFreeSpace();

	//Restore old touch handler
	attachInterrupt(pin_touch_irq, touchIRQ, FALLING);
}