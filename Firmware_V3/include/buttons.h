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
* https://github.com/maxritter/DIY-Thermocam
*
*/

#ifndef BUTTONS_H
#define BUTTONS_H

/*########################## PUBLIC PROCEDURES ################################*/

int buttons_addButton(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* data, const uint16_t* palette, uint16_t flags = 0);
int buttons_addButton(uint16_t x, uint16_t y, uint16_t width, uint16_t height, char *label, uint16_t flags = 0, boolean largetouch = 0);
boolean buttons_buttonEnabled(int buttonID);
int buttons_checkButtons(boolean timeout = 0, boolean fast = 0);
void buttons_deleteAllButtons();
void buttons_deleteButton(int buttonID);
void buttons_disableButton(int buttonID, boolean redraw = 0);
void buttons_drawButtons();
void buttons_drawButton(int buttonID);
void buttons_enableButton(int buttonID, boolean redraw = 0);
void buttons_init();
void buttons_relabelButton(int buttonID, char *label, boolean redraw = 0);
void buttons_setActive(int buttonID);
void buttons_setButtonColors(word atxt, word iatxt, word brd, word brdhi, word back);
void buttons_setInactive(int buttonID);
void buttons_setSymbolFont(const uint8_t* font);
void buttons_setTextFont(const uint8_t* font);

#endif /* BUTTONS_H */
