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
		delay(5);
	//While touch pressed, iterate over readings (clamped strictly to 0..319, 0..239)
	uint32_t t0 = millis();
	while (touch_touched() == true && (millis() - t0) < 2000u)
	{
		point = touch_getPoint();
		if ((point.x >= 0) && (point.x <= 320) && (point.y >= 0) && (point.y <= 240))
		{
			uint16_t px = (point.x >= 320) ? 319 : static_cast<uint16_t>(point.x);
			uint16_t py = (point.y >= 240) ? 239 : static_cast<uint16_t>(point.y);
			tx += px;
			ty += py;
			iter++;
		}
	}
	if (iter > 0)
	{
		*x = static_cast<uint16_t>(tx / iter);
		*y = static_cast<uint16_t>(ty / iter);
	}
	else
	{
		*x = 160;
		*y = 120;
	}
	if (*x > 319) *x = 319;
	if (*y > 239) *y = 239;
}

static bool hasActiveTempPoints()
{
	for (byte i = 0; i < 96; i++)
	{
		if (tempPoints[i][0] != 0)
			return true;
	}
	return false;
}

static int findFreeTempPointSlot()
{
	for (int i = 0; i < 96; i++)
	{
		if (tempPoints[i][0] == 0)
			return i;
	}
	return -1;
}

static bool removeTempPointsNear(uint16_t cx, uint16_t cy)
{
	bool removed = false;
	for (byte i = 0; i < 96; i++)
	{
		uint16_t idx = tempPoints[i][0];
		if (idx < 1 || idx > tc_core::SMALL_PIXELS)
			continue;
		int px = static_cast<int>((idx - 1u) % 160u);
		int py = static_cast<int>((idx - 1u) / 160u);
		if (abs(px - static_cast<int>(cx)) <= 10 && abs(py - static_cast<int>(cy)) <= 10)
		{
			tempPoints[i][0] = 0;
			tempPoints[i][1] = 0;
			removed = true;
		}
	}
	return removed;
}

/* Function to add or remove a measurement point */
void tempPointFunction(bool remove)
{
	int freeSlot = 0;
	if (remove)
	{
		if (!hasActiveTempPoints())
		{
			showFullMessage((char *)"No points available", true);
			delay(1000);
			return;
		}
	}
	else
	{
		freeSlot = findFreeTempPointSlot();
		if (freeSlot < 0)
		{
			showFullMessage((char *)"Remove a point first", true);
			delay(1000);
			return;
		}
	}

	while (true)
	{
		lepton_startFrame();
		createThermalImg();
		displayBuffer();

		changeTextColor();
		display_setBackColor(VGA_TRANSPARENT);
		display_setFont(smallFont);
		showTemperatures();
		display_setFont(bigFont);
		display_print((char *)"Select position", CENTER, 210);

		uint16_t xpos = 0, ypos = 0;
		getTouchPos(&xpos, &ypos);
		xpos = (xpos / 2u > 159u) ? 159u : static_cast<uint16_t>(xpos / 2u);
		ypos = (ypos / 2u > 119u) ? 119u : static_cast<uint16_t>(ypos / 2u);

		drawMainMenuBorder();
		if (remove)
		{
			if (removeTempPointsNear(xpos, ypos))
			{
				showFullMessage((char *)"Point removed", true);
				delay(1000);
				return;
			}
			showFullMessage((char *)"Invalid position", true);
			delay(1000);
		}
		else
		{
			tempPoints[freeSlot][0] = static_cast<uint16_t>(xpos + (ypos * 160u) + 1u);
			tempPoints[freeSlot][1] = 0;
			showFullMessage((char *)"Point added", true);
			delay(1000);
			return;
		}
	}
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
			//Index goes from 1 to max (bounds-checked against smallBuffer[19200])
			index -= 1;
			if (index < tc_core::SMALL_PIXELS)
			{
				tempPoints[i][1] = smallBuffer[index];
			}
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

/* Acquire, compute stats, save PRE-FILTER raw video .DAT (if requested), then filter/scale/colorize */
void createThermalImgVideo(bool saveRawNow, char* dirName, uint32_t folderFrames)
{
	//Acquire triple-buffered Lepton frame
	lepton_acquireFrame(true);

	//Stats stage: spotmeter + temp points + fused min/max scan
	uint32_t t_stats = micros();
	getSpotTemp();
	refreshTempPoints();
	if (autoMode && !limitsLocked) {
		limitValues();
	} else if (minMaxPoints != minMaxPoints_disabled) {
		refreshMinMax();
	}
	g_perf.stats_us = micros() - t_stats;

	//Save raw .DAT frame BEFORE display filtering (100% consistent with stills, zero double-filtering!)
	if (saveRawNow && dirName) {
		saveRawData(false, dirName, folderFrames);
	}

	//Apply low-pass filter for live preview only
	g_perf.filter_us = 0;
	if (filterType == filterType_box)
		boxFilter();
	else if (filterType == filterType_gaussian)
		gaussianFilter();

	//Upscale & colorize for live preview
	smallToBigBuffer();
	convertColors(false);
}