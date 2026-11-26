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

	while ((btcnt < 20)
			&& ((buttons[btcnt].flags & BUTTON_UNUSED) == 0))
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

	while ((btcnt < 20) && ((buttons[btcnt].flags & BUTTON_UNUSED) == 0))
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

/* Draw 2px rounded border highlight/restore for a button */
static void drawButtonOutline(int buttonID, word borderColor) {
	if (buttonID < 0 || (buttons[buttonID].flags & BUTTON_NO_BORDER))
		return;
	const int x1 = buttons[buttonID].pos_x;
	const int y1 = buttons[buttonID].pos_y;
	const int x2 = x1 + buttons[buttonID].width;
	const int y2 = y1 + buttons[buttonID].height;
	display_setColor(borderColor);
	display_drawRoundRect(x1, y1, x2, y2);
	display_drawRoundRect(x1 - 1, y1 - 1, x2 + 1, y2 + 1);
}

/* Hit-test touch coordinates against all enabled buttons */
static int findPressedButton(int x, int y) {
	for (int i = 0; i < 20; i++) {
		if ((buttons[i].flags & (BUTTON_UNUSED | BUTTON_DISABLED)) != 0)
			continue;
		int xpos = buttons[i].pos_x;
		int ypos = buttons[i].pos_y;
		int width = buttons[i].width;
		int height = buttons[i].height;
		if (buttons[i].largetouch) {
			xpos -= 30;
			ypos -= 20;
			width += 60;
			height += 40;
		}
		if (x >= xpos && x <= (xpos + width) && y >= ypos && y <= (ypos + height))
			return i;
	}
	return -1;
}

/* Fill a button with active or inactive colors and redraw its centered label */
static void drawButtonState(int buttonID, word fillColor, word textColor) {
	const button_type& b = buttons[buttonID];
	display_setColor(fillColor);
	display_fillRoundRect(b.pos_x + 1, b.pos_y + 1,
			b.pos_x + b.width - 1, b.pos_y + b.height - 1);
	display_setFont(buttons_fontText);
	display_setColor(textColor);
	int text_x = (b.width / 2) - ((strlen(b.label) * display_getFontXsize()) / 2) + b.pos_x;
	int text_y = (b.height / 2) - (display_getFontYsize() / 2) + b.pos_y;
	display_setBackColor(fillColor);
	display_print(b.label, text_x, text_y);
}

/* Check which button is pressed */
int buttons_checkButtons(boolean timeout, boolean fast) {
	TS_Point p = touch_getPoint();
	word savedColor = display_getColor();
	int result = findPressedButton(p.x, p.y);

	drawButtonOutline(result, buttons_colorHilite);

	uint32_t waitMs = fast ? 50u : (timeout ? 150u : 1500u);
	uint32_t startMs = millis();
	while (touch_touched() && ((millis() - startMs) < waitMs)) {
		delay(5);
	}

	drawButtonOutline(result, buttons_colorBorder);
	display_setColor(savedColor);
	return result;
}

/* Set a specific button to active (thermal orange accent pill) */
void buttons_setActive(int buttonID) {
	drawButtonState(buttonID, UI_ACCENT, UI_ACCENT_FG);
}

/* Set a specific button to inactive */
void buttons_setInactive(int buttonID) {
	drawButtonState(buttonID, buttons_colorBackGround, buttons_colorText);
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

/* Init the buttons with modern 2026 obsidian + thermal orange palette */
void buttons_init() {
	buttons_deleteAllButtons();
	buttons_fontText = NULL;
	buttons_fontSymbol = NULL;
	buttons_setButtonColors(UI_TEXT, UI_TEXT_MUTED, UI_BORDER, UI_ACCENT, UI_SURFACE);
}
