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
#include <perf.h>
#include <tc_thermal_core.h>
#include <create.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Filter a 160x120 smallBuffer with 3x3 integer gaussian kernel */
void gaussianFilter()
{
	uint32_t t0 = micros();
	tc_core::filter_gaussian_3x3(smallBuffer, tc_core::SMALL_W, tc_core::SMALL_H);
	g_perf.filter_us = micros() - t0;
}

/* Filter a 160x120 smallBuffer with 3x3 integer box kernel */
void boxFilter()
{
	uint32_t t0 = micros();
	tc_core::filter_box_3x3(smallBuffer, tc_core::SMALL_W, tc_core::SMALL_H);
	g_perf.filter_us = micros() - t0;
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

/* Write the smallBuffer to the bigBuffer using fast integer clamped Catmull-Rom 2x */
void smallToBigBuffer()
{
	uint32_t t0 = micros();
	tc_core::upscale_sharp_2x(smallBuffer, bigBuffer);
	g_perf.scale_us = micros() - t0;
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

/* Go through the array of temperatures and find min and max temp (fused single pass) */
void limitValues()
{
	tc_core::MinMaxStats st{};
	tc_core::scan_min_max(smallBuffer, 19200, st);
	minValue = st.min_val;
	maxValue = st.max_val;
	minTempVal = st.min_val;
	minTempPos = st.min_pos;
	maxTempVal = st.max_val;
	maxTempPos = st.max_pos;
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

/* Convert the lepton values to RGB565 colors using precomputed LUT & divide-by-zero guard */
void convertColors(bool small)
{
	uint32_t t0 = micros();
	uint16_t hotColdRaw = 0;
	uint16_t hotColdRgb = 0;
	if (hotColdMode != hotColdMode_disabled) {
		hotColdRaw = tempToRaw(hotColdLevel);
		byte r = 0, g = 0, b = 0;
		getHotColdColors(&r, &g, &b);
		hotColdRgb = tc_core::pack_rgb565(r, g, b);
	}
	unsigned short* frameBuffer = small ? smallBuffer : bigBuffer;
	size_t count = small ? tc_core::SMALL_PIXELS : tc_core::BIG_PIXELS;
	tc_core::convert_colors_u16(
		frameBuffer, count, minValue, maxValue, colorLut,
		static_cast<uint16_t>(colorElements), hotColdMode, hotColdRaw, hotColdRgb);
	g_perf.colorize_us = micros() - t0;
}

/* Refresh the position and value of the min / max value */
void refreshMinMax()
{
	tc_core::MinMaxStats st{};
	tc_core::scan_min_max(smallBuffer, tc_core::SMALL_PIXELS, st);
	minTempPos = st.min_pos;
	minTempVal = st.min_val;
	maxTempPos = st.max_pos;
	maxTempVal = st.max_val;
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

/* Creates a thermal frame and stores it in the buffer */
void createThermalImg(bool small)
{
	//Acquire double-buffered Lepton frame
	lepton_acquireFrame(true);

	//Stats stage: spotmeter + temp points + fused min/max scan
	uint32_t t_stats = micros();
	getSpotTemp();
	refreshTempPoints();
	if (autoMode && !limitsLocked) {
		limitValues(); // also populates minTempPos/maxTempPos in one pass!
	} else if (minMaxPoints != minMaxPoints_disabled) {
		refreshMinMax();
	}
	g_perf.stats_us = micros() - t_stats;

	//If image save, save the raw .DAT data before display filtering
	if (imgSave == imgSave_create)
		saveRawData(true, saveFilename);

	//Apply low-pass filter
	g_perf.filter_us = 0;
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