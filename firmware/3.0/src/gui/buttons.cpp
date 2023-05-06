/*
 *
 * Buttons - Touch buttons for the GUI
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
#include <buttons.h>

/*################# DATA TYPES, CONSTANTS & MACRO DEFINITIONS #################*/

#define BUTTON_DISABLED			0x0001
#define BUTTON_SYMBOL			0x0002
#define BUTTON_SYMBOL_REP_3X	0x0004
#define BUTTON_BITMAP			0x0008
#define BUTTON_NO_BORDER		0x0010
#define BUTTON_UNUSED			0x8000

typedef struct {
	uint16_t pos_x, pos_y, width, height;
	uint16_t flags;
	boolean largetouch;
	char *label;
	const uint8_t* data;
	const uint16_t* palette;
} button_type;

/*######################### STATIC DATA DECLARATIONS ##########################*/

//Store up to 20 buttons
static button_type buttons[20];
//Button attributes
static word buttons_colorText;
static word buttons_colorTextInactive;
static word buttons_colorBackGround;
static word buttons_colorBorder;
static word buttons_colorHilite;
//Button fonts
static uint8_t* buttons_fontText;
static uint8_t* buttons_fontSymbol;

/*######################## PUBLIC FUNCTION BODIES #############################*/

/* Add a text button */
int buttons_addButton(uint16_t x, uint16_t y, uint16_t width, uint16_t height, char *label, uint16_t flags, boolean largetouch)
{
	int btcnt = 0;

	while (((buttons[btcnt].flags & BUTTON_UNUSED) == 0)
			&& (btcnt < 20))
		btcnt++;

	if (btcnt == 20)
		return -1;
	else {
		buttons[btcnt].pos_x = x;
		buttons[btcnt].pos_y = y;
		buttons[btcnt].width = width;
		buttons[btcnt].height = height;
		buttons[btcnt].flags = flags;
		buttons[btcnt].label = label;
		buttons[btcnt].data = NULL;
		buttons[btcnt].palette = NULL;
		buttons[btcnt].largetouch = largetouch;
		return btcnt;
	}
}

/* Add a bitmap button */
int buttons_addButton(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* data, const uint16_t* palette, uint16_t flags)
{
	int btcnt = 0;

	while (((buttons[btcnt].flags & BUTTON_UNUSED) == 0) && (btcnt < 20))
		btcnt++;

	if (btcnt == 20)
		return -1;
	else
	{
		buttons[btcnt].pos_x = x;
		buttons[btcnt].pos_y = y;
		buttons[btcnt].width = width;
		buttons[btcnt].height = height;
		buttons[btcnt].flags = flags | BUTTON_BITMAP;
		buttons[btcnt].label = NULL;
		buttons[btcnt].data = data;
		buttons[btcnt].palette = palette;
		return btcnt;
	}
}

/* Draw a specific button */
void buttons_drawButton(int buttonID) {
	int text_x, text_y;
	uint8_t *_font_current = display_getFont();
	;
	word _current_color = display_getColor();
	word _current_back = display_getBackColor();

	if (buttons[buttonID].flags & BUTTON_BITMAP) {
		display_writeRect2BPP(buttons[buttonID].pos_x, buttons[buttonID].pos_y,
				buttons[buttonID].width, buttons[buttonID].height,
				buttons[buttonID].data, buttons[buttonID].palette);
		if (!(buttons[buttonID].flags & BUTTON_NO_BORDER)) {
			if ((buttons[buttonID].flags & BUTTON_DISABLED))
				display_setColor(buttons_colorTextInactive);
			else
				display_setColor(buttons_colorBorder);
			display_drawRoundRect(buttons[buttonID].pos_x, buttons[buttonID].pos_y,
					buttons[buttonID].pos_x + buttons[buttonID].width,
					buttons[buttonID].pos_y + buttons[buttonID].height);
			display_drawRoundRect(buttons[buttonID].pos_x - 1, buttons[buttonID].pos_y - 1,
					buttons[buttonID].pos_x + buttons[buttonID].width + 1,
					buttons[buttonID].pos_y + buttons[buttonID].height + 1);
		}
	}
	else {
		display_setColor(buttons_colorBackGround);
		display_fillRoundRect(buttons[buttonID].pos_x, buttons[buttonID].pos_y,
				buttons[buttonID].pos_x + buttons[buttonID].width,
				buttons[buttonID].pos_y + buttons[buttonID].height);
		display_setColor(buttons_colorBorder);
		display_drawRoundRect(buttons[buttonID].pos_x, buttons[buttonID].pos_y,
				buttons[buttonID].pos_x + buttons[buttonID].width,
				buttons[buttonID].pos_y + buttons[buttonID].height);
		display_drawRoundRect(buttons[buttonID].pos_x - 1, buttons[buttonID].pos_y - 1,
				buttons[buttonID].pos_x + buttons[buttonID].width + 1,
				buttons[buttonID].pos_y + buttons[buttonID].height + 1);
		if (buttons[buttonID].flags & BUTTON_DISABLED)
			display_setColor(buttons_colorTextInactive);
		else
			display_setColor(buttons_colorText);
		if (buttons[buttonID].flags & BUTTON_SYMBOL) {
			display_setFont(buttons_fontSymbol);
			text_x = (buttons[buttonID].width / 2) - (display_getFontXsize() / 2)
						+ buttons[buttonID].pos_x;
			text_y = (buttons[buttonID].height / 2)
						- (display_getFontYsize() / 2) + buttons[buttonID].pos_y;
		}
		else {
			display_setFont(buttons_fontText);
			text_x = ((buttons[buttonID].width / 2)
					- ((strlen(buttons[buttonID].label) * display_getFontXsize())
							/ 2)) + buttons[buttonID].pos_x;
			text_y = (buttons[buttonID].height / 2)
						- (display_getFontYsize() / 2) + buttons[buttonID].pos_y;
		}
		display_setBackColor(buttons_colorBackGround);
		display_print(buttons[buttonID].label, text_x, text_y);
		if ((buttons[buttonID].flags & BUTTON_SYMBOL)
				&& (buttons[buttonID].flags & BUTTON_SYMBOL_REP_3X)) {
			display_print(buttons[buttonID].label,
					text_x - display_getFontXsize(), text_y);
			display_print(buttons[buttonID].label,
					text_x + display_getFontXsize(), text_y);
		}
	}
	display_setFont(_font_current);
	display_setColor(_current_color);
	display_setBackColor(_current_back);
}

/* Draw all buttons */
void buttons_drawButtons() {
	for (int i = 0; i < 20; i++) {
		if ((buttons[i].flags & BUTTON_UNUSED) == 0)
			buttons_drawButton(i);
	}
}

/* Enable a specific button */
void buttons_enableButton(int buttonID, boolean redraw) {
	if (!(buttons[buttonID].flags & BUTTON_UNUSED)) {
		buttons[buttonID].flags = buttons[buttonID].flags & ~BUTTON_DISABLED;
		if (redraw)
			buttons_drawButton(buttonID);
	}
}

/* Disable a specific button */
void buttons_disableButton(int buttonID, boolean redraw) {
	if (!(buttons[buttonID].flags & BUTTON_UNUSED)) {
		buttons[buttonID].flags = buttons[buttonID].flags | BUTTON_DISABLED;
		if (redraw)
			buttons_drawButton(buttonID);
	}
}

/* Relabel a specific button */
void buttons_relabelButton(int buttonID, char *label, boolean redraw) {
	if (!(buttons[buttonID].flags & BUTTON_UNUSED)) {
		buttons[buttonID].label = label;
		if (redraw)
			buttons_drawButton(buttonID);
	}
}

/* Check if the button is enabled */
boolean buttons_buttonEnabled(int buttonID) {
	return !(buttons[buttonID].flags & BUTTON_DISABLED);
}

/* Delete a specific button */
void buttons_deleteButton(int buttonID) {
	if (!(buttons[buttonID].flags & BUTTON_UNUSED))
		buttons[buttonID].flags = BUTTON_UNUSED;
}

/* Delete all buttons */
void buttons_deleteAllButtons() {
	for (int i = 0; i < 20; i++) {
		buttons[i].pos_x = 0;
		buttons[i].pos_y = 0;
		buttons[i].width = 0;
		buttons[i].height = 0;
		buttons[i].flags = BUTTON_UNUSED;
		buttons[i].label = (char*) "";
	}
}

/* Check which button is pressed */
int buttons_checkButtons(boolean timeout, boolean fast) {
	TS_Point p = touch_getPoint();
	int x = p.x;
	int y = p.y;
	int result = -1;
	word _current_color = display_getColor();
	int xpos, ypos, width, height;
	for (int i = 0; i < 20; i++) {
		xpos = buttons[i].pos_x;
		ypos = buttons[i].pos_y;
		width = buttons[i].width;
		height = buttons[i].height;
		if (buttons[i].largetouch) {
			xpos -= 30;
			ypos -= 20;
			width += 60;
			height += 40;
		}
		if (((buttons[i].flags & BUTTON_UNUSED) == 0)
				&& ((buttons[i].flags & BUTTON_DISABLED) == 0)
				&& (result == -1)) {
			if ((x >= xpos)
					&& (x <= (xpos + width))
					&& (y >= ypos)
					&& (y <= (ypos + height)))
				result = i;
		}
	}
	if (result != -1) {
		if (!(buttons[result].flags & BUTTON_NO_BORDER)) {
			display_setColor(buttons_colorHilite);
			if (buttons[result].flags & BUTTON_BITMAP)
				display_drawRoundRect(buttons[result].pos_x,
						buttons[result].pos_y,
						buttons[result].pos_x + buttons[result].width,
						buttons[result].pos_y + buttons[result].height);
			else
				display_drawRoundRect(buttons[result].pos_x,
						buttons[result].pos_y,
						buttons[result].pos_x + buttons[result].width,
						buttons[result].pos_y + buttons[result].height);
			display_drawRoundRect(buttons[result].pos_x - 1,
					buttons[result].pos_y - 1,
					buttons[result].pos_x + buttons[result].width + 1,
					buttons[result].pos_y + buttons[result].height + 1);
		}
	}
	if (fast) {
		long time = millis();
		while ((touch_touched() == 1)
				&& ((millis() - time) < 50)) {
		};
	}
	else if (timeout) {
		long time = millis();
		while ((touch_touched() == 1)
				&& ((millis() - time) < 150)) {
		};
	}
	else {
		while (touch_touched() == 1) {
		};
	}
	if (result != -1) {
		if (!(buttons[result].flags & BUTTON_NO_BORDER)) {
			display_setColor(buttons_colorBorder);
			if (buttons[result].flags & BUTTON_BITMAP)
				display_drawRoundRect(buttons[result].pos_x,
						buttons[result].pos_y,
						buttons[result].pos_x + buttons[result].width,
						buttons[result].pos_y + buttons[result].height);
			else
				display_drawRoundRect(buttons[result].pos_x,
						buttons[result].pos_y,
						buttons[result].pos_x + buttons[result].width,
						buttons[result].pos_y + buttons[result].height);
			display_drawRoundRect(buttons[result].pos_x - 1,
					buttons[result].pos_y - 1,
					buttons[result].pos_x + buttons[result].width + 1,
					buttons[result].pos_y + buttons[result].height + 1);
		}
	}
	display_setColor(_current_color);
	return result;
}

/* Set a specific button to active */
void buttons_setActive(int buttonID) {
	int text_x, text_y;
	display_setColor(VGA_AQUA);
	display_fillRect(buttons[buttonID].pos_x + 3, buttons[buttonID].pos_y + 3,
			buttons[buttonID].pos_x + buttons[buttonID].width - 3,
			buttons[buttonID].pos_y + buttons[buttonID].height - 3);
	display_setFont(buttons_fontText);
	display_setColor(buttons_colorText);
	text_x = ((buttons[buttonID].width / 2)
			- ((strlen(buttons[buttonID].label) * display_getFontXsize()) / 2))
				+ buttons[buttonID].pos_x;
	text_y = (buttons[buttonID].height / 2) - (display_getFontYsize() / 2)
				+ buttons[buttonID].pos_y;
	display_setBackColor(VGA_AQUA);
	display_print(buttons[buttonID].label, text_x, text_y);
}

/* Set a specific button to inactive */
void buttons_setInactive(int buttonID) {
	int text_x, text_y;
	display_setColor(buttons_colorBackGround);
	display_fillRect(buttons[buttonID].pos_x + 3, buttons[buttonID].pos_y + 3,
			buttons[buttonID].pos_x + buttons[buttonID].width - 3,
			buttons[buttonID].pos_y + buttons[buttonID].height - 3);
	display_setFont(buttons_fontText);
	display_setColor(buttons_colorText);
	text_x = ((buttons[buttonID].width / 2)
			- ((strlen(buttons[buttonID].label) * display_getFontXsize()) / 2))
				+ buttons[buttonID].pos_x;
	text_y = (buttons[buttonID].height / 2) - (display_getFontYsize() / 2)
				+ buttons[buttonID].pos_y;
	display_setBackColor(buttons_colorBackGround);
	display_print(buttons[buttonID].label, text_x, text_y);
}

/* Set the text font of all buttons */
void buttons_setTextFont(const uint8_t* font) {
	buttons_fontText = (uint8_t*) font;
}

/* Set the symbol font of all buttons */
void buttons_setSymbolFont(const uint8_t* font) {
	buttons_fontSymbol = (uint8_t*) font;
}

/* Set the buttons color */
void buttons_setButtonColors(word atxt, word iatxt, word brd, word brdhi,
		word back) {
	buttons_colorText = atxt;
	buttons_colorTextInactive = iatxt;
	buttons_colorBackGround = back;
	buttons_colorBorder = brd;
	buttons_colorHilite = brdhi;
}

/* Init the buttons */
void buttons_init() {
	buttons_deleteAllButtons();
	buttons_colorText = VGA_WHITE;
	buttons_colorTextInactive = VGA_GRAY;
	buttons_colorBackGround = VGA_BLUE;
	buttons_colorBorder = VGA_BLACK;
	buttons_colorHilite = VGA_BLUE;
	buttons_fontText = NULL;
	buttons_fontSymbol = NULL;
	buttons_setButtonColors(VGA_BLACK, VGA_BLACK, VGA_BLACK, VGA_BLUE,
			VGA_WHITE);
}
