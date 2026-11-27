/*
*
* CREATE - Functions to create and display the thermal frameBuffer
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

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <calibration.h>
#include <touchscreen.h>
#include <gui.h>
#include <fonts.h>
#include <hardware.h>
#include <camera.h>
#include <lepton.h>
#include <save.h>
#include <v2_core.h>
#include <create.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Filter a 160x120 smallBuffer with symmetric 3x3 integer gaussian kernel */
void gaussianFilter() {
	v2_core::gaussian_filter_160x120(smallBuffer);
}

/* Filter a 160x120 smallBuffer with symmetric 3x3 integer box kernel */
void boxFilter() {
	v2_core::box_filter_160x120(smallBuffer);
}

//Resize the pixels of thermal smallBuffer
void resizePixels(unsigned short* pixels, int w1, int h1, int w2, int h2) {
	//Calculate the ratio
	int x_ratio = (int)((w1 << 16) / w2) + 1;
	int y_ratio = (int)((h1 << 16) / h2) + 1;
	int x2, y2;
	for (int i = 0; i < h2; i++) {
		for (int j = 0; j < w2; j++) {
			x2 = ((j * x_ratio) >> 16);
			y2 = ((i * y_ratio) >> 16);
			pixels[(i * w1) + j] = pixels[(y2 * w1) + x2];
		}
	}
	//Set the other pixels to zero
	for (int j = 0; j < h2; j++) {
		for (int i = w2; i < w1; i++) {
			pixels[i + (j * w1)] = 65535;
		}
	}
	for (int j = h2; j < h1; j++) {
		for (int i = 0; i < w1; i++) {
			pixels[i + (j * w1)] = 65535;
		}
	}
}

/* Resize the thermal smallBuffer */
void resizeImage() {
	uint16_t  xmax, ymax;
	unsigned short* pixelBuffer;

	//For 160x120
	if ((teensyVersion == teensyVersion_old) || (!hqRes))
	{
		xmax = 160;
		ymax = 120;
		pixelBuffer = smallBuffer;
	}
	//For 320x240
	else
	{
		xmax = 320;
		ymax = 240;
		pixelBuffer = bigBuffer;
	}

	uint16_t newWidth = round(adjCombFactor * xmax);
	uint16_t newHeight = round(adjCombFactor * ymax);

	//Resize pixels
	resizePixels(pixelBuffer, xmax, ymax, newWidth, newHeight);

	//Move the smallBuffer right
	uint16_t rightMove = round((xmax - newWidth) / 2.0);
	for (int i = 0; i < rightMove; i++) {
		//First for all pixels
		for (int col = (xmax - 1); col > 0; col--)
			for (int row = 0; row < ymax; row++)
				pixelBuffer[col + (row * xmax)] = pixelBuffer[col - 1 + (row * xmax)];
		//And then fill the last one with white
		for (int row = 0; row < ymax; row++)
			pixelBuffer[row * xmax] = 65535;
	}
	//Move the smallBuffer down
	uint16_t downMove = round((ymax - newHeight) / 2.0);
	for (int i = 0; i < downMove; i++) {
		//First for all pixels
		for (int col = 0; col < xmax; col++)
			for (int row = (ymax - 1); row > 0; row--)
				pixelBuffer[col + (row * xmax)] = pixelBuffer[col + ((row - 1) * xmax)];
		//And then fill the last one with white
		for (int col = 0; col < xmax; col++)
			pixelBuffer[col] = 65535;
	}
}

/* Calculates the fill pixel for visual/combined */
void calcFillPixel(uint16_t x, uint16_t y) {
	uint16_t pixel;
	byte red, green, blue;

	//Combined - set to darker thermal
	if (displayMode == displayMode_combined) {
		//Get the thermal image color
		pixel = smallBuffer[x + (y * 160)];
		//And extract the RGB values out of it
		byte redT = (pixel & 0xF800) >> 8;
		byte greenT = (pixel & 0x7E0) >> 3;
		byte blueT = (pixel & 0x1F) << 3;
		//Mix both
		red = (byte)redT * (1 - adjCombAlpha) + 127 * adjCombAlpha;
		green = (byte)greenT * (1 - adjCombAlpha) + 127 * adjCombAlpha;
		blue = (byte)blueT * (1 - adjCombAlpha) + 127 * adjCombAlpha;
	}
	//Visual - set to black
	else {
		red = 0;
		green = 0;
		blue = 0;
	}

	//Set image to that calculated RGB565 value
	pixel = (((red & 248) | green >> 5) << 8)
		| ((green & 28) << 3 | blue >> 3);
	//Save
	smallBuffer[x + (y * 160)] = pixel;
}

/* Fill out the edges in combined or visual mode */
void fillEdges() {
	//Fill the edges
	uint16_t  x, y;

	//Top & Bottom edges
	for (x = 0; x < 160; x++) {
		//Top edge
		for (y = 0; y < (5 * adjCombDown); y++) {
			calcFillPixel(x, y);
		}

		//Bottom edge
		for (y = 119; y > (119 - (5 * adjCombUp)); y--) {
			calcFillPixel(x, y);
		}
	}

	//Left & right edges
	for (y = 5 * adjCombDown; y < (120 - (5 * adjCombUp)); y++) {
		//Left edge
		for (x = 0; x < (5 * adjCombRight); x++) {
			calcFillPixel(x, y);
		}

		//Right edge
		for (x = 159; x > (159 - (5 * adjCombLeft)); x--) {
			calcFillPixel(x, y);
		}
	}
}


/* Write the smallBuffer to the bigBuffer by fixed-point bilinear resizing, eventually add transparency */
void smallToBigBuffer(bool trans)
{
	//Fixed-point 8.8 step ratios: 159 / 320 and 119 / 240
	const uint32_t xStepFp = (159u << 16) / 320u;
	const uint32_t yStepFp = (119u << 16) / 240u;

	const uint16_t alpha256 = static_cast<uint16_t>(lroundf(adjCombAlpha * 256.0f));
	const uint16_t invAlpha256 = (alpha256 <= 256u) ? (256u - alpha256) : 0u;

	uint32_t offset = 0;
	uint32_t yFp = 0;
	for (int i = 0; i < 240; i++, yFp += yStepFp) {
		uint32_t y = yFp >> 16;
		uint32_t wy = (yFp >> 8) & 0xFFu;
		uint32_t iwy = 256u - wy;
		const uint16_t* row0 = &smallBuffer[y * 160u];
		const uint16_t* row1 = row0 + 160u;

		uint32_t xFp = 0;
		for (int j = 0; j < 320; j++, xFp += xStepFp) {
			uint32_t x = xFp >> 16;
			uint32_t wx = (xFp >> 8) & 0xFFu;
			uint32_t iwx = 256u - wx;

			uint32_t top = (row0[x] * iwx + row0[x + 1] * wx) >> 8;
			uint32_t bot = (row1[x] * iwx + row1[x + 1] * wx) >> 8;
			uint16_t outVal = static_cast<uint16_t>((top * iwy + bot * wy) >> 8);

			//No alpha transparency, just write into the framebuffer
			if (!trans) {
				bigBuffer[offset] = outVal;
			}
			//We want to create a combined bigBuffer
			else {
				uint8_t idx = v2_core::map_raw_to_lut_index(outVal, minValue, maxValue, colorElements);
				uint8_t redT = colorMap[3 * idx];
				uint8_t greenT = colorMap[3 * idx + 1];
				uint8_t blueT = colorMap[3 * idx + 2];

				uint16_t vis = bigBuffer[offset];
				uint8_t redV = (vis & 0xF800u) >> 8;
				uint8_t greenV = (vis & 0x07E0u) >> 3;
				uint8_t blueV = (vis & 0x001Fu) << 3;

				uint8_t red = static_cast<uint8_t>((redT * invAlpha256 + redV * alpha256) >> 8);
				uint8_t green = static_cast<uint8_t>((greenT * invAlpha256 + greenV * alpha256) >> 8);
				uint8_t blue = static_cast<uint8_t>((blueT * invAlpha256 + blueV * alpha256) >> 8);

				bigBuffer[offset] = (((red & 248) | green >> 5) << 8) | ((green & 28) << 3 | blue >> 3);
			}

			offset++;
		}
	}
}

/* Clears the temperature points array */
void clearTempPoints() {
	//Go through the array
	for (byte i = 0; i < 96; i++) {
		//Set the index to zero
		tempPoints[i][0] = 0;
		//Set the value to zero
		tempPoints[i][1] = 0;
	}
}

/* Shows the temperatures over the smallBuffer on the screen */
void showTemperatures() {
	int16_t xpos, ypos;

	//Go through the array
	for (byte i = 0; i < 96; i++) {
		//Get index
		uint16_t index = tempPoints[i][0];

		//Check if the tempPoint is active
		if (index != 0) {
			//Index goes from 1 to max
			index -= 1;

			//Calculate x and y position
			calculatePointPos(&xpos, &ypos, index);

			//Draw the marker
			display_drawLine(xpos, ypos, xpos, ypos);

			//Calc x position for the text (keep clear of right screen edge)
			xpos -= 20;
			if (xpos < 0)
				xpos = 0;
			if (xpos > 256)
				xpos = 256;

			//Calc y position for the text
			ypos += 15;
			if (ypos > 229)
				ypos = 229;

			//Display the absolute temperature
			display_printNumF(calFunction(tempPoints[i][1]), 2, xpos, ypos);
		}
	}
}

/* Read the x and y coordinates when touch screen is pressed for Add Point */
void getTouchPos(uint16_t* x, uint16_t* y) {
	int iter = 0;
	TS_Point point;
	unsigned long tx = 0;
	unsigned long ty = 0;
	//Wait for touch press
	while (!touch_touched());
	//While touch pressed, iterate over readings
	while (touch_touched() == true) {
		point = touch_getPoint();
		if ((point.x >= 0) && (point.x <= 320) && (point.y >= 0)
			&& (point.y <= 240)) {
			tx += point.x;
			ty += point.y;
			iter++;
		}
	}
	*x = tx / iter;
	*y = ty / iter;
}

/* Function to add or remove a measurement point */
void tempPointFunction(bool remove) {
	uint16_t xpos, ypos;
	byte pos = 0;
	bool removed = false;

	//If remove points, check if there are some first
	if (remove) {
		//Go through the array
		for (byte i = 0; i < 96; i++) {
			if (tempPoints[i][0] != 0)
			{
				removed = true;
				break;
			}
		}
		//No points available to remove
		if (!removed) {
			showFullMessage((char*) "No points available", true);
			delay(1000);
			return;
		}
	}
	//If add points, check if we have space left
	else
	{
		//Go through the array
		byte i;
		for (i = 0; i < 96; i++) {
			if (tempPoints[i][0] == 0)
			{
				pos = i;
				break;
			}
		}
		//Maximum number of points added
		if (i == 96) {
			showFullMessage((char*) "Remove a point first", true);
			delay(1000);
			return;
		}
	}

redraw:
	//Safe delay
	delay(10);

	//Create thermal smallBuffer
	if (displayMode == displayMode_thermal)
		createThermalImg();
	//Create visual or combined smallBuffer
	else
		createVisCombImg();

	//Show it on the screen
	displayBuffer();

	//Set text color, font and background
	changeTextColor();
	display_setBackColor(VGA_TRANSPARENT);
	display_setFont(smallFont);
	//Show current temperature points
	showTemperatures();
	//Display title
	display_setFont(bigFont);
	display_print((char*) "Select position", CENTER, 210);

	//Get touched coordinates
	getTouchPos(&xpos, &ypos);

	//Divide through 2 to match array size
	xpos /= 2;
	ypos /= 2;

	//Remove point
	if (remove) {
		//Set marker to false
		removed = false;

		//Check for 10 pixels around the touch position
		for (uint16_t x = xpos - 10; x <= xpos + 10; x++) {
			for (uint16_t y = ypos - 10; y <= ypos + 10; y++) {
				//Calculate index number
				uint16_t index = x + (y * 160) + 1;
				//If index is valid
				if ((index >= 1) && (index <= 19200)) {
					//Check for all 96 points
					for (byte i = 0; i < 96; i++)
					{
						//We found a valid temperature point
						if (tempPoints[i][0] == index)
						{
							//Set to invalid
							tempPoints[i][0] = 0;
							//Reset value
							tempPoints[i][1] = 0;
							//Set markter to true
							removed = true;
						}
					}
				}
			}
		}
		//Show border
		drawMainMenuBorder();
		//Show removed message
		if (removed)
			showFullMessage((char*) "Point removed", true);
		//Invalid position, redraw
		else {
			showFullMessage((char*) "Invalid position", true);
			delay(1000);
			goto redraw;
		}
	}

	//Add point
	else {
		//Add index
		tempPoints[pos][0] = xpos + (ypos * 160) + 1;
		//Set raw value to zero
		tempPoints[pos][1] = 0;
		//Show border
		drawMainMenuBorder();
		//Show message
		showFullMessage((char*) "Point added", true);
	}

	//Wait some time
	delay(1000);
}

/* Go through the array of temperatures and find min and max temp */
void limitValues() {
	maxValue = 0;
	minValue = 65535;
	uint16_t temp;
	for (int i = 0; i < 19200; i++) {
		//Get value
		temp = smallBuffer[i];
		//Find maximum temp
		if (temp > maxValue)
			maxValue = temp;
		//Find minimum temp
		if (temp < minValue)
			minValue = temp;
	}
}

/* Get the colors for hot / cold mode selection */
void getHotColdColors(byte* red, byte* green, byte* blue) {
	switch (hotColdColor) {
		//White
	case hotColdColor_white:
		*red = 255;
		*green = 255;
		*blue = 255;
		break;
		//Black
	case hotColdColor_black:
		*red = 0;
		*green = 0;
		*blue = 0;
		break;
		//White
	case hotColdColor_red:
		*red = 255;
		*green = 0;
		*blue = 0;
		break;
		//White
	case hotColdColor_green:
		*red = 0;
		*green = 255;
		*blue = 0;
		break;
		//White
	case hotColdColor_blue:
		*red = 0;
		*green = 0;
		*blue = 255;
		break;
	}
}

/* Convert the lepton values to RGB565 colors using fast integer LUT */
void convertColors(bool small) {
	//Precompute hot/cold RGB565 color once outside the pixel loop
	byte hcRed = 0, hcGreen = 0, hcBlue = 0;
	bool useHot = (hotColdMode == hotColdMode_hot) && (calStatus != cal_warmup) && (displayMode != displayMode_combined);
	bool useCold = (hotColdMode == hotColdMode_cold) && (calStatus != cal_warmup) && (displayMode != displayMode_combined);
	uint16_t hotColdRawLevel = 0;
	uint16_t hotColdRgb565 = 0;
	if (useHot || useCold) {
		hotColdRawLevel = tempToRaw(hotColdLevel);
		getHotColdColors(&hcRed, &hcGreen, &hcBlue);
		hotColdRgb565 = (((hcRed & 248) | hcGreen >> 5) << 8) | ((hcGreen & 28) << 3 | hcBlue >> 3);
	}

	//Safe integer fixed-point scale (16.16) guarded against maxValue <= minValue
	uint32_t maxIdx = (colorElements > 1) ? static_cast<uint32_t>(colorElements - 1) : 0u;
	uint32_t span = (maxValue > minValue) ? static_cast<uint32_t>(maxValue - minValue) : 1u;
	uint32_t scaleFp = (maxIdx << 16) / span;

	//Size of the array & buffer
	int size;
	unsigned short* frameBuffer;
	//Teensy 3.6, not for preview
	if ((teensyVersion == teensyVersion_new) && (!small) && (hqRes)) {
		size = 76800;
		frameBuffer = bigBuffer;
	}
	//Teensy 3.1 / 3.2 or preview
	else {
		size = 19200;
		frameBuffer = smallBuffer;
	}

	uint16_t mn = minValue;
	uint16_t mx = maxValue;
	for (int i = 0; i < size; i++) {
		uint16_t value = frameBuffer[i];

		//Limit values
		if (value > mx)
			value = mx;
		if (value < mn)
			value = mn;

		if (useHot && (value >= hotColdRawLevel)) {
			frameBuffer[i] = hotColdRgb565;
		} else if (useCold && (value <= hotColdRawLevel)) {
			frameBuffer[i] = hotColdRgb565;
		} else {
			uint32_t idx = (static_cast<uint32_t>(value - mn) * scaleFp) >> 16;
			if (idx > maxIdx) idx = maxIdx;
			frameBuffer[i] = colorMap565[idx];
		}
	}
}

/* Refresh the position and value of the min / max value */
void refreshMinMax()
{
	//Reset values
	minTempVal = 65535;
	maxTempVal = 0;

	//Go through the smallBuffer
	for (int i = 0; i < 19200; i++)
	{
		//We found a new min
		if (smallBuffer[i] < minTempVal)
		{
			//Save position and value
			minTempPos = i;
			minTempVal = smallBuffer[i];
		}

		//We found a new max
		if (smallBuffer[i] > maxTempVal)
		{
			maxTempPos = i;
			maxTempVal = smallBuffer[i];
		}
	}
}

/* Refresh the temperature points*/
void refreshTempPoints() {
	//Go through the array
	for (byte i = 0; i < 96; i++) {
		//Get index
		uint16_t index = tempPoints[i][0];

		//Check if point is active
		if (index != 0) {
			//Index goes from 1 to max
			index -= 1;

			//Calculate x and y position
			uint16_t xpos = index % 160;
			uint16_t ypos = index / 160;

			//Update value
			tempPoints[i][1] = smallBuffer[xpos + (ypos * 160)];
		}
	}
}

/* Calculate the x and y position out of the pixel index */
void calculatePointPos(int16_t* xpos, int16_t* ypos, uint16_t pixelIndex) {
	//Get xpos and ypos
	*xpos = (pixelIndex % 160) * 2;
	*ypos = (pixelIndex / 160) * 2;

	//Limit position
	if (*ypos > 238)
		*ypos = 228;
	if (*xpos > 318)
		*xpos = 318;
}

/* Creates a thermal smallBuffer and stores it in the array */
void createThermalImg(bool small) {
	//Receive the temperatures over SPI
	lepton_getRawValues();

	//Get the spot temperature (and ambient temp once per frame)
	getSpotTemp();

	//Find min and max BEFORE compensating calibration so minValue/maxValue are always valid
	if ((autoMode) && (!limitsLocked))
		limitValues();

	//Compensate calibration with MLX90614 for non-radiometric Lepton
	if ((leptonVersion != leptonVersion_2_5_shutter) && (leptonVersion != leptonVersion_3_5_shutter))
		compensateCalib();

	//Refresh the temp points if required
	refreshTempPoints();

	//Find min / max position
	if (minMaxPoints != minMaxPoints_disabled)
		refreshMinMax();

	//If smallBuffer save, save the raw data
	if (imgSave == imgSave_create)
		saveRawData(true, saveFilename);

	//Apply low-pass filter
	if (filterType == filterType_box)
		boxFilter();
	else if (filterType == filterType_gaussian)
		gaussianFilter();

	//Teensy 3.6 - Resize to big buffer when HQRes and not preview
	if ((teensyVersion == teensyVersion_new) && (!small) && (hqRes))
		smallToBigBuffer();

	//Convert lepton data to RGB565 colors
	if (!videoSave)
		convertColors(small);
}

/* Create the visual or combined smallBuffer display */
void createVisCombImg() {
	//Capture new frame from camera
	camera_capture();

	//Receive the temperatures over SPI
	lepton_getRawValues();

	//Get the spot temperature (and ambient temp once per frame)
	getSpotTemp();

	//Find min and max BEFORE compensating calibration so minValue/maxValue are always valid
	if ((autoMode) && (!limitsLocked))
		limitValues();

	//Compensate calibration with MLX90614 for non-radiometric Lepton
	if ((leptonVersion != leptonVersion_2_5_shutter) && (leptonVersion != leptonVersion_3_5_shutter))
		compensateCalib();

	//Refresh the temp points if required
	refreshTempPoints();

	//Find min / max position
	if (minMaxPoints != minMaxPoints_disabled)
		refreshMinMax();

	//For 320x240 resolution, decompress visual image before thermal
	if ((teensyVersion == teensyVersion_new) && (hqRes))
	{
		//Fill array white
		for (uint32_t i = 0; i < 76800; i++)
			bigBuffer[i] = 65535;
		//Get image from cam
		camera_get(camera_stream);
		//Resize the image
		resizeImage();
	}

	//For combined only
	if (displayMode == displayMode_combined) {
		//Apply low-pass filter
		if (filterType == filterType_box)
			boxFilter();
		else if (filterType == filterType_gaussian)
			gaussianFilter();

		//Teensy 3.6 with HQRes - Resize to big buffer and create transparency
		if ((teensyVersion == teensyVersion_new) && (hqRes))
			smallToBigBuffer(true);
		//Teensy 3.1 / 3.2 - Convert raw values to RGB565
		else
			convertColors();
	}

	//For low resolution, decompress visual image after thermal image
	if ((teensyVersion == teensyVersion_old) || (!hqRes))
	{
		//Resize the thermal image
		resizeImage();
		//Fill the edges of the thermal image
		fillEdges();
		//Get the visual image and decompress it combined
		camera_get(camera_stream);
	}
}
