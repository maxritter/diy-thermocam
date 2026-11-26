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
* https://github.com/maxritter/diy-thermocam
*
*/

#ifndef GUI_H
#define GUI_H

/*########################## PUBLIC PROCEDURES ################################*/

enum UiPillStyle : uint8_t {
	uiPill_secondary = 0,
	uiPill_primary = 1,
	uiPill_danger = 2
};

void bootScreen();
void changeTextColor();
void drawCenterElement(int element);
void drawMainMenuBorder();
void drawTitle(char* name, bool firstStart = false);
void floatToChar(char* buffer, float val);
void showDiagnostic();
void drawStatusScreen();
void showStatusScreen();
void showFullMessage(char* message, bool small = false);
void showModalCard(const char* title, const char* line1, const char* line2 = nullptr, const char* hint = nullptr);
void showTransMessage(char* msg);
void ui_beginScreen(const char* title, const char* rightBadge = nullptr, bool accentBadge = false);
void ui_drawPill(int x, int y, int w, int h, const char* label, UiPillStyle style = uiPill_secondary);
void ui_drawCard(int x, int y, int w, int h, const char* title, const char* subtitle = nullptr, bool active = false);
void ui_drawWideCard(int x, int y, int w, int h, const char* title, const char* badge = nullptr, bool active = false);
void ui_waitTouchRelease(uint32_t maxMs = 1000u);
void ui_waitTouchReleaseAndLockout(uint32_t lockoutMs = 250u);
bool ui_confirmDialog(const char* title, const char* headline, const char* line1, const char* line2, bool highlightYes = false, bool destructive = false);

#endif /* GUI_H */
