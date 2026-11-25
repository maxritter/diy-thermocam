/*
*
* Display - ILI9341 SPI Display Module
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

#ifndef DISPLAY_H
#define DISPLAY_H

/*################# PUBLIC CONSTANTS, VARIABLES & DATA TYPES ##################*/

#define LEFT 0
#define RIGHT 9999
#define CENTER 9998

#define PORTRAIT 0
#define LANDSCAPE 1

#define VGA_BLACK		0x0000
#define VGA_WHITE		0xFFFF
#define VGA_RED			0xF800
#define VGA_GREEN		0x0400
#define VGA_BLUE		0x001F
#define VGA_SILVER		0xC618
#define VGA_GRAY		0x8410
#define VGA_MAROON		0x8000
#define VGA_YELLOW		0xFE40
#define VGA_OLIVE		0x8400
#define VGA_LIME		0x07E0
#define VGA_AQUA		0xBE7F
#define VGA_TEAL		0x0410
#define VGA_NAVY		0x0010
#define VGA_FUCHSIA		0xF81F
#define VGA_PURPLE		0x8010
#define VGA_TRANSPARENT	0xFFFFFFFF

// Modern 2026 Dark-Slate + Thermal Amber UI palette (RGB565)
#define UI_BG_DARK      0x08A5  // #0F172A deep slate
#define UI_SURFACE      0x1947  // #1E293B slate panel
#define UI_ELEVATED     0x21A9  // #263449 button surface
#define UI_BORDER       0x3A6C  // #3B4D66 subtle border
#define UI_TEXT         0xF7DF  // #F8FAFC crisp white
#define UI_TEXT_MUTED   0x9517  // #94A3B8 muted slate
#define UI_ACCENT       0xFB82  // #F97316 thermal amber/orange
#define UI_ACCENT_DARK  0xC2C1  // #C2410C deep amber
#define UI_OK           0x262B  // #22C55E green
#define UI_ERR          0xEA28  // #EF4444 red

struct propFont
{
	byte charCode;
	int adjYOffset;
	int width;
	int height;
	int xOffset;
	int xDelta;
	byte* dataPtr;
};

extern boolean display_writeToImage;

/*########################## PUBLIC PROCEDURES ################################*/

void display_clrScr();
void display_clrXY();
void display_convertFloat(char* buf, double num, int width, byte prec);
void display_drawBitmap(int x, int y, int w, int h, unsigned short *data);
void display_drawCircle(int x, int y, int radius);
void display_drawHLine(int x, int y, int l);
void display_drawLine(int x1, int y1, int x2, int y2);
void display_drawPixel(int x, int y);
void display_drawRect(int x1, int y1, int x2, int y2);
void display_drawRoundRect(int x1, int y1, int x2, int y2);
void display_drawVLine(int x, int y, int l);
void display_enterSleepMode();
void display_exitSleepMode();
void display_fillCircle(int x, int y, int radius);
void display_fillRect(int x1, int y1, int x2, int y2);
void display_fillRoundRect(int x1, int y1, int x2, int y2);
void display_fillScr(word color);
void display_fillScr(byte r, byte g, byte b);
word display_getBackColor();
boolean display_getCharPtr(byte c, propFont& fontChar);
word display_getColor();
int display_getFontHeight();
uint8_t* display_getFont();
uint8_t display_getFontXsize();
uint8_t display_getFontYsize();
int display_getStringWidth(char* str);
byte display_InitLCD();
void display_init();
void display_LCD_Write_DATA(char VH, char VL);
void display_printChar(byte c, int x, int y);
void display_printC(String st, int x, int y, uint32_t color = VGA_BLACK);
void display_printNumF(double num, byte dec, int x, int y, char divider = '.', int length = 0, char filler = ' ');
void display_printNumI(long num, int x, int y, int length = 0, char filler = ' ');
int display_printProportionalChar(byte c, int x, int y);
void display_print(char* st, int x, int y, int deg = 0);
void display_print(String st, int x, int y, int deg = 0);
uint8_t display_readcommand8(uint8_t c, uint8_t index = 0);
void display_rotateChar(byte c, int x, int y, int pos, int deg);
int display_rotatePropChar(byte c, int x, int y, int offset, int deg);
void display_setAddr(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void display_setBackColor(byte r, byte g, byte b);
void display_setBackColor(uint32_t color);
void display_setColor(byte r, byte g, byte b);
void display_setColor(word color);
void display_setFont(const uint8_t* font);
void display_setPixel(word color);
void display_setRotation(uint8_t m);
void display_setXY(word x1, word y1, word x2, word y2);
void display_waitFifoEmpty();
void display_waitFifoNotFull();
void display_maybeUpdateTCR(uint32_t requested_tcr_state);
void display_waitTransmitComplete();
void display_writecommand_cont(uint8_t c);
void display_writecommand_last(uint8_t c);
void display_writedata16_cont(uint16_t d);
void display_writedata16_last(uint16_t d);
void display_writedata8_cont(uint8_t c);
void display_writedata8_last(uint8_t c);
void display_writeRect2BPP(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* pixels, const uint16_t* palette);
void display_writeRect4BPP(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* pixels, const uint16_t* palette);
void display_writeScreen(unsigned short *pcolors);
void display_waitDmaComplete();
void display_blendRect(int x, int y, int w, int h, word color, uint8_t alpha_num_of_8 = 5);

#endif /* DISPLAY_H */
