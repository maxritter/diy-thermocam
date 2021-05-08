/*
*
* GUI - Main Methods to lcd the Graphical-User-Interface
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
#include <touchscreen.h>
#include <fonts.h>
#include <bitmaps.h>
#include <buttons.h>
#include <hardware.h>
#include <gui.h>

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Converts a given float to char array */
void floatToChar(char *buffer, float val)
{
	int units = val;
	int hundredths = val * 100;
	hundredths = abs(hundredths % 100);
	sprintf(buffer, "%d.%02d", units, hundredths);
}

/* Sets the text color to the right one */
void changeTextColor()
{
	//Red
	if (textColor == textColor_red)
		display_setColor(VGA_RED);
	//Black
	else if (textColor == textColor_black)
		display_setColor(VGA_BLACK);
	//Green
	else if (textColor == textColor_green)
		display_setColor(VGA_GREEN);
	//Blue
	else if (textColor == textColor_blue)
		display_setColor(VGA_BLUE);
	//White
	else
		display_setColor(VGA_WHITE);
}

/* Shows a full screen message */
void showFullMessage(char *message, bool small)
{
	//Fill screen complete
	if (!small)
		display_fillScr(200, 200, 200);

	//Make a round corner around it
	else
	{
		display_setColor(200, 200, 200);
		display_fillRoundRect(6, 6, 314, 234);
	}

	//Display the text
	display_setFont(smallFont);
	display_setBackColor(200, 200, 200);
	display_setColor(VGA_BLACK);
	display_print(message, CENTER, 110);
}

/* Shows a transparent message in live mode */
void showTransMessage(char *msg)
{
	//Set text color
	changeTextColor();
	//Set background transparent
	display_setBackColor(VGA_TRANSPARENT);
	//Display to screen in big font
	display_setFont(bigFont);
	//Display higher if spot is enabled
	if (spotEnabled)
		display_print(msg, CENTER, 70);
	else
		display_print(msg, CENTER, 110);
	//Switch back to small font
	display_setFont(smallFont);
	//Wait some time to read the text
	delay(1000);
}

/* Draw a BigFont Text in the center of a menu*/
void drawCenterElement(int element)
{
	display_setFont(bigFont);
	display_setColor(VGA_BLACK);
	display_setBackColor(200, 200, 200);
	display_printNumI(element, CENTER, 80, 2, '0');
	display_setFont(smallFont);
}

/* Draws the border for the main menu */
void drawMainMenuBorder()
{
	display_setColor(VGA_BLACK);
	display_fillRoundRect(5, 5, 315, 235);
	display_fillRoundRect(4, 4, 316, 236);
}

/* Draw a title on the screen */
void drawTitle(char *name, bool firstStart)
{
	if (firstStart)
		display_fillScr(200, 200, 200);
	else
	{
		display_setColor(200, 200, 200);
		display_fillRoundRect(6, 6, 314, 234);
	}
	display_setFont(bigFont);
	display_setBackColor(200, 200, 200);
	display_setColor(VGA_BLACK);
	display_print(name, CENTER, 25);
	display_setFont(smallFont);
}

/* Shows the hadware diagnostics */
void showDiagnostic()
{
	//Display title & background
	display_fillScr(200, 200, 200);
	display_setFont(bigFont);
	display_setBackColor(200, 200, 200);
	display_setColor(VGA_BLUE);
	display_print((char *)"Self-diagnostic", CENTER, 10);

	//Change text color and font
	display_setFont(smallFont);
	display_setColor(VGA_BLACK);

	//Display hardware module names
	display_print((char *)"Display", 50, 50);
	display_print((char *)"Touch", 50, 85);
	display_print((char *)"Battery Gauge", 50, 120);
	display_print((char *)"FLIR Lepton", 50, 155);
	display_print((char *)"SD card", 50, 190);

	//Check display SPI
	if (checkDiagnostic(diag_display))
		display_print((char *)"OK    ", 220, 50);
	else
		display_print((char *)"Failed", 220, 50);

	//Check touch SPI
	if (checkDiagnostic(diag_touch))
		display_print((char *)"OK    ", 220, 85);
	else
		display_print((char *)"Failed", 220, 85);

	//Check battery gauge
	if (checkDiagnostic(diag_bat))
		display_print((char *)"OK    ", 220, 120);
	else
		display_print((char *)"Failed", 220, 120);

	//Check FLIR Lepton
	if (checkDiagnostic(diag_lepton))
		display_print((char *)"OK    ", 220, 155);
	else
		display_print((char *)"Failed", 220, 155);

	//Check sd card
	if (checkDiagnostic(diag_sd))
		display_print((char *)"OK    ", 220, 190);
	else
		display_print((char *)"Failed", 220, 190);

	//Show hint
	display_print((char *)"Touch to continue", CENTER, 220);

	//Wait until touch
	while (!touch_touched());
	showFullMessage((char*) "Trying to continue boot, may freeze..");
}

/* Draw the Boot screen */
void bootScreen()
{
	//Set rotation
	setDisplayRotation();

	//Init the buttons
	buttons_init();

	//Set Fonts
	buttons_setTextFont((uint8_t *)smallFont);
	display_setFont(smallFont);

	//Draw Screen
	display_fillScr(255, 255, 255);
	display_setFont(bigFont);
	display_setBackColor(255, 255, 255);
	display_setColor(VGA_BLACK);

	//Show the logo and boot text
	display_writeRect4BPP(90, 35, 140, 149, logoBitmap, logoColors);
	display_print((char *)"Booting", CENTER, 194);
	display_setFont(smallFont);

	//Show hardware version
	display_print((char *)"DIY-Thermocam V3", CENTER, 10);

	//Display version
	display_print(versionString, CENTER, 220);

	//Wait some time
	delay(2000);
}
