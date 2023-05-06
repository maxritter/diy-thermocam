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
* https://github.com/maxritter/diy-thermocam
*
*/

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>
#include <touchscreen.h>
#include <gui.h>
#include <fonts.h>
#include <hardware.h>
#include <temperature.h>
#include <lepton.h>
#include <save.h>
#include <create.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Filter a 160x120 smallBuffer with 3x3 gaussian kernel */
void gaussianFilter()
{
	byte gaussianKernel[3][3] = {
		{1, 2, 1},
		{2, 4, 2},
		{1, 2, 1}};
	long sum;

	for (int y = 1; y < 119; y++)
	{
		for (int x = 1; x < 159; x++)
		{
			sum = 0;
			for (int k = -1; k <= 1; k++)
			{
				for (int j = -1; j <= 1; j++)
				{
					sum += gaussianKernel[j + 1][k + 1] * smallBuffer[(y - j) * 160 + (x - k)];
				}
			}
			smallBuffer[(y * 160) + x] = (unsigned short)(sum / 16.0);
		}
	}
}

/* Filter a 160x120 smallBuffer with a 3x3 box kernel */
void boxFilter()
{
	byte boxKernel[3][3] = {
		{1, 1, 1},
		{1, 1, 1},
		{1, 1, 1}};
	long sum;

	for (int y = 1; y < 119; y++)
	{
		for (int x = 1; x < 159; x++)
		{
			sum = 0;
			for (int k = -1; k <= 1; k++)
			{
				for (int j = -1; j <= 1; j++)
				{
					sum += boxKernel[j + 1][k + 1] * smallBuffer[(y - j) * 160 + (x - k)];
				}
			}
			smallBuffer[(y * 160) + x] = (unsigned short)(sum / 9.0);
		}
	}
}

//Resize the pixels of thermal smallBuffer
void resizePixels(unsigned short *pixels, int w1, int h1, int w2, int h2)
{
	//Calculate the ratio
	int x_ratio = (int)((w1 << 16) / w2) + 1;
	int y_ratio = (int)((h1 << 16) / h2) + 1;
	int x2, y2;
	for (int i = 0; i < h2; i++)
	{
		for (int j = 0; j < w2; j++)
		{
			x2 = ((j * x_ratio) >> 16);
			y2 = ((i * y_ratio) >> 16);
			pixels[(i * w1) + j] = pixels[(y2 * w1) + x2];
		}
	}
	//Set the other pixels to zero
	for (int j = 0; j < h2; j++)
	{
		for (int i = w2; i < w1; i++)
		{
			pixels[i + (j * w1)] = 65535;
		}
	}
	for (int j = h2; j < h1; j++)
	{
		for (int i = 0; i < w1; i++)
		{
			pixels[i + (j * w1)] = 65535;
		}
	}
}

/* Write the smallBuffer to the bigBuffer by resizing */
void smallToBigBuffer()
{
	unsigned short A, B, C, D, outVal;
	int x, y, index;
	float x_ratio = 159.0 / 320;
	float y_ratio = 119.0 / 240;
	float x_diff, y_diff;

	int offset = 0;
	for (int i = 0; i < 240; i++)
	{
		for (int j = 0; j < 320; j++)
		{

			x = (int)(x_ratio * j);
			y = (int)(y_ratio * i);
			x_diff = (x_ratio * j) - x;
			y_diff = (y_ratio * i) - y;
			index = y * 160 + x;

			A = smallBuffer[index];
			B = smallBuffer[index + 1];
			C = smallBuffer[index + 160];
			D = smallBuffer[index + 160 + 1];

			outVal = (unsigned short)(A * (1 - x_diff) * (1 - y_diff) + B * (x_diff) * (1 - y_diff) +
									  C * (y_diff) * (1 - x_diff) + D * (x_diff * y_diff));

			bigBuffer[offset] = outVal;

			//Raise counter
			offset++;
		}
	}
}

/* Clears the temperature points array */
void clearTempPoints()
{
	//Go through the array
	for (byte i = 0; i < 96; i++)
	{
		//Set the index to zero
		tempPoints[i][0] = 0;
		//Set the value to zero
		tempPoints[i][1] = 0;
	}
}

/* Shows the temperatures over the smallBuffer on the screen */
void showTemperatures()
{
	int16_t xpos, ypos;

	//Go through the array
	for (byte i = 0; i < 96; i++)
	{
		//Get index
		uint16_t index = tempPoints[i][0];

		//Check if the tempPoint is active
		if (index != 0)
		{
			//Index goes from 1 to max
			index -= 1;

			//Calculate x and y position
			calculatePointPos(&xpos, &ypos, index);

			//Draw the marker
			display_drawLine(xpos, ypos, xpos, ypos);

			//Calc x position for the text
			xpos -= 20;
			if (xpos < 0)
				xpos = 0;
			if (xpos > 279)
				xpos = 279;

			//Calc y position for the text
			ypos += 15;
			if (ypos > 229)
				ypos = 229;

			//Display the absolute temperature
			display_printNumF(rawToTemp(tempPoints[i][1]), 2, xpos, ypos);
		}
	}
}

/* Read the x and y coordinates when touch screen is pressed for Add Point */
void getTouchPos(uint16_t *x, uint16_t *y)
{
	int iter = 0;
	TS_Point point;
	unsigned long tx = 0;
	unsigned long ty = 0;
	//Wait for touch press
	while (!touch_touched())
		;
	//While touch pressed, iterate over readings
	while (touch_touched() == true)
	{
		point = touch_getPoint();
		if ((point.x >= 0) && (point.x <= 320) && (point.y >= 0) && (point.y <= 240))
		{
			tx += point.x;
			ty += point.y;
			iter++;
		}
	}
	*x = tx / iter;
	*y = ty / iter;
}

/* Function to add or remove a measurement point */
void tempPointFunction(bool remove)
{
	uint16_t xpos, ypos;
	byte pos = 0;
	bool removed = false;

	//If remove points, check if there are some first
	if (remove)
	{
		//Go through the array
		for (byte i = 0; i < 96; i++)
		{
			if (tempPoints[i][0] != 0)
			{
				removed = true;
				break;
			}
		}
		//No points available to remove
		if (!removed)
		{
			showFullMessage((char *)"No points available", true);
			delay(1000);
			return;
		}
	}
	//If add points, check if we have space left
	else
	{
		//Go through the array
		byte i;
		for (i = 0; i < 96; i++)
		{
			if (tempPoints[i][0] == 0)
			{
				pos = i;
				break;
			}
		}
		//Maximum number of points added
		if (i == 96)
		{
			showFullMessage((char *)"Remove a point first", true);
			delay(1000);
			return;
		}
	}

redraw:
	//Create thermal small buffer
	lepton_startFrame();
	createThermalImg();

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
	display_print((char *)"Select position", CENTER, 210);

	//Get touched coordinates
	getTouchPos(&xpos, &ypos);

	//Divide through 2 to match array size
	xpos /= 2;
	ypos /= 2;

	//Remove point
	if (remove)
	{
		//Set marker to false
		removed = false;

		//Check for 10 pixels around the touch position
		for (uint16_t x = xpos - 10; x <= xpos + 10; x++)
		{
			for (uint16_t y = ypos - 10; y <= ypos + 10; y++)
			{
				//Calculate index number
				uint16_t index = x + (y * 160) + 1;
				//If index is valid
				if ((index >= 1) && (index <= 19200))
				{
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
			showFullMessage((char *)"Point removed", true);
		//Invalid position, redraw
		else
		{
			showFullMessage((char *)"Invalid position", true);
			delay(1000);
			goto redraw;
		}
	}

	//Add point
	else
	{
		//Add index
		tempPoints[pos][0] = xpos + (ypos * 160) + 1;
		//Set raw value to zero
		tempPoints[pos][1] = 0;
		//Show border
		drawMainMenuBorder();
		//Show message
		showFullMessage((char *)"Point added", true);
	}

	//Wait some time
	delay(1000);
}

/* Go through the array of temperatures and find min and max temp */
void limitValues()
{
	maxValue = 0;
	minValue = 65535;
	uint16_t temp;
	for (int i = 0; i < 19200; i++)
	{
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
void getHotColdColors(byte *red, byte *green, byte *blue)
{
	switch (hotColdColor)
	{
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

/* Convert the lepton values to RGB colors */
void convertColors(bool small)
{
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;
	uint16_t value;

	//Calculate the scale
	float scale = (colorElements - 1.0) / (maxValue - minValue);

	//For hot and cold mode, calculate rawlevel
	float hotColdRawLevel = 0.0;
	if (hotColdMode != hotColdMode_disabled)
		hotColdRawLevel = tempToRaw(hotColdLevel);

	//Size of the array & buffer
	int size;
	unsigned short* frameBuffer;
	if (!small) {
		size = 76800;
		frameBuffer = bigBuffer;
	}
	else {
		size = 19200;
		frameBuffer = smallBuffer;
	}


	//Repeat for 160x120 data
	for (int i = 0; i < size; i++)
	{

		value = frameBuffer[i];

		//Limit values
		if (value > maxValue)
			value = maxValue;
		if (value < minValue)
			value = minValue;

		//Hot
		if ((hotColdMode == hotColdMode_hot) && (value >= hotColdRawLevel))
			getHotColdColors(&red, &green, &blue);
		//Cold
		else if ((hotColdMode == hotColdMode_cold) && (value <= hotColdRawLevel))
			getHotColdColors(&red, &green, &blue);

		//Apply colorscheme
		else
		{
			value = (value - minValue) * scale;
			red = colorMap[3 * value];
			green = colorMap[3 * value + 1];
			blue = colorMap[3 * value + 2];
		}

		//Convert to RGB565
		frameBuffer[i] = (((red & 248) | green >> 5) << 8) | ((green & 28) << 3 | blue >> 3);
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
void refreshTempPoints()
{
	//Go through the array
	for (byte i = 0; i < 96; i++)
	{
		//Get index
		uint16_t index = tempPoints[i][0];

		//Check if point is active
		if (index != 0)
		{
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
void calculatePointPos(int16_t *xpos, int16_t *ypos, uint16_t pixelIndex)
{
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
void createThermalImg(bool small)
{
	//Get Lepton frame
	if (small)
		lepton_startFrame();
	lepton_getFrame();

	//Get the spot temperature
	getSpotTemp();

	//Refresh the temp points if required
	refreshTempPoints();

	//Find min / max position
	if (minMaxPoints != minMaxPoints_disabled)
		refreshMinMax();

	//Find min and max if not in manual mode and limits not locked
	if ((autoMode) && (!limitsLocked))
		limitValues();

	//If smallBuffer save, save the raw data
	if (imgSave == imgSave_create)
		saveRawData(true, saveFilename);

	//Apply low-pass filter
	if (filterType == filterType_box)
		boxFilter();
	else if (filterType == filterType_gaussian)
		gaussianFilter();

	//Resize to big buffer when not preview
	if (!small)
		smallToBigBuffer();

	//Convert lepton data to RGB565 colors
	if (!videoSave)
		convertColors(small);
}