#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <display.h>

// Host 320x240 RGB565 framebuffer
uint16_t sim_fb[320 * 240];

boolean display_writeToImage = false;

struct current_font {
	uint8_t *font;
	uint8_t x_size;
	uint8_t y_size;
	uint8_t offset;
	uint8_t numchars;
};

static current_font cfont = {nullptr, 8, 12, 0x20, 95};
static boolean transparent = false;
static word fgColor = 0xFFFF;
static word bgColor = 0x0000;

static inline uint16_t* activeBuf() {
	return (display_writeToImage && bigBuffer) ? bigBuffer : sim_fb;
}

static inline void putPx(int x, int y, word color) {
	if (x >= 0 && x < 320 && y >= 0 && y < 240) {
		activeBuf()[y * 320 + x] = color;
	}
}

void display_waitDmaComplete() {}
void display_clrScr() { display_fillScr(0x0000); }
void display_clrXY() {}
void display_enterSleepMode() {}
void display_exitSleepMode() {}
byte display_InitLCD() { return 0; }
void display_init() {}
void display_setRotation(uint8_t) {}
uint8_t display_readcommand8(uint8_t, uint8_t) { return 0x55; }

void display_setColor(byte r, byte g, byte b) {
	fgColor = static_cast<word>(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}
void display_setColor(word color) { fgColor = color; }
word display_getColor() { return fgColor; }

void display_setBackColor(byte r, byte g, byte b) {
	bgColor = static_cast<word>(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
	transparent = false;
}
void display_setBackColor(uint32_t color) {
	if (color == VGA_TRANSPARENT) {
		transparent = true;
	} else {
		bgColor = static_cast<word>(color & 0xFFFFu);
		transparent = false;
	}
}
word display_getBackColor() { return bgColor; }

void display_setFont(const uint8_t* font) {
	cfont.font = const_cast<uint8_t*>(font);
	cfont.x_size = font[0];
	cfont.y_size = font[1];
	cfont.offset = font[2];
	cfont.numchars = font[3];
}
uint8_t* display_getFont() { return cfont.font; }
uint8_t display_getFontXsize() { return cfont.x_size; }
uint8_t display_getFontYsize() { return cfont.y_size; }
int display_getFontHeight() { return cfont.y_size; }

void display_drawPixel(int x, int y) { putPx(x, y, fgColor); }

void display_drawHLine(int x, int y, int l) {
	if (y < 0 || y >= 240 || l <= 0) return;
	if (x < 0) { l += x; x = 0; }
	if (x >= 320 || l <= 0) return;
	if (x + l > 320) l = 320 - x;
	uint16_t* dst = &activeBuf()[y * 320 + x];
	for (int i = 0; i < l; ++i) dst[i] = fgColor;
}

void display_drawVLine(int x, int y, int l) {
	if (x < 0 || x >= 320 || l <= 0) return;
	if (y < 0) { l += y; y = 0; }
	if (y >= 240 || l <= 0) return;
	if (y + l > 240) l = 240 - y;
	uint16_t* dst = &activeBuf()[y * 320 + x];
	for (int i = 0; i < l; ++i) {
		*dst = fgColor;
		dst += 320;
	}
}

void display_drawLine(int x1, int y1, int x2, int y2) {
	if (y1 == y2) {
		if (x1 > x2) std::swap(x1, x2);
		display_drawHLine(x1, y1, x2 - x1 + 1);
		return;
	}
	if (x1 == x2) {
		if (y1 > y2) std::swap(y1, y2);
		display_drawVLine(x1, y1, y2 - y1 + 1);
		return;
	}
	int dx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
	int dy = std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
	int col = x1, row = y1;
	if (dx < dy) {
		int t = -(dy >> 1);
		while (true) {
			putPx(col, row, fgColor);
			if (row == y2) return;
			row += sy;
			t += dx;
			if (t >= 0) { col += sx; t -= dy; }
		}
	} else {
		int t = -(dx >> 1);
		while (true) {
			putPx(col, row, fgColor);
			if (col == x2) return;
			col += sx;
			t += dy;
			if (t >= 0) { row += sy; t -= dx; }
		}
	}
}

void display_fillScr(word color) {
	uint16_t* dst = activeBuf();
	for (int i = 0; i < 320 * 240; ++i) dst[i] = color;
}

void display_fillScr(byte r, byte g, byte b) {
	word color = static_cast<word>(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
	display_fillScr(color);
}

void display_drawRect(int x1, int y1, int x2, int y2) {
	if (x1 > x2) std::swap(x1, x2);
	if (y1 > y2) std::swap(y1, y2);
	display_drawHLine(x1, y1, x2 - x1);
	display_drawHLine(x1, y2, x2 - x1);
	display_drawVLine(x1, y1, y2 - y1);
	display_drawVLine(x2, y1, y2 - y1);
}

void display_fillRect(int x1, int y1, int x2, int y2) {
	if (x1 > x2) std::swap(x1, x2);
	if (y1 > y2) std::swap(y1, y2);
	int w = x2 - x1;
	int h = y2 - y1;
	if (x1 >= 320 || y1 >= 240 || w <= 0 || h <= 0) return;
	if (x1 < 0) { w += x1; x1 = 0; }
	if (y1 < 0) { h += y1; y1 = 0; }
	if (w <= 0 || h <= 0) return;
	if (x1 + w > 320) w = 320 - x1;
	if (y1 + h > 240) h = 240 - y1;
	uint16_t* dst = activeBuf();
	for (int yy = y1; yy < y1 + h; ++yy) {
		uint16_t* row = &dst[yy * 320 + x1];
		for (int xx = 0; xx < w; ++xx) row[xx] = fgColor;
	}
}

void display_drawRoundRect(int x1, int y1, int x2, int y2) {
	if (x1 > x2) std::swap(x1, x2);
	if (y1 > y2) std::swap(y1, y2);
	if ((x2 - x1) > 4 && (y2 - y1) > 4) {
		display_drawPixel(x1 + 1, y1 + 1);
		display_drawPixel(x2 - 1, y1 + 1);
		display_drawPixel(x1 + 1, y2 - 1);
		display_drawPixel(x2 - 1, y2 - 1);
		display_drawHLine(x1 + 2, y1, x2 - x1 - 4);
		display_drawHLine(x1 + 2, y2, x2 - x1 - 4);
		display_drawVLine(x1, y1 + 2, y2 - y1 - 4);
		display_drawVLine(x2, y1 + 2, y2 - y1 - 4);
	}
}

void display_fillRoundRect(int x1, int y1, int x2, int y2) {
	if (x1 > x2) std::swap(x1, x2);
	if (y1 > y2) std::swap(y1, y2);
	if ((x2 - x1) > 4 && (y2 - y1) > 4) {
		for (int i = 0; i < ((y2 - y1) / 2) + 1; i++) {
			switch (i) {
			case 0:
				display_drawHLine(x1 + 2, y1 + i, x2 - x1 - 4);
				display_drawHLine(x1 + 2, y2 - i, x2 - x1 - 4);
				break;
			case 1:
				display_drawHLine(x1 + 1, y1 + i, x2 - x1 - 2);
				display_drawHLine(x1 + 1, y2 - i, x2 - x1 - 2);
				break;
			default:
				display_drawHLine(x1, y1 + i, x2 - x1);
				display_drawHLine(x1, y2 - i, x2 - x1);
				break;
			}
		}
	}
}

void display_drawCircle(int x, int y, int radius) {
	int f = 1 - radius;
	int ddF_x = 1;
	int ddF_y = -2 * radius;
	int x1 = 0;
	int y1 = radius;
	putPx(x, y + radius, fgColor);
	putPx(x, y - radius, fgColor);
	putPx(x + radius, y, fgColor);
	putPx(x - radius, y, fgColor);
	while (x1 < y1) {
		if (f >= 0) { y1--; ddF_y += 2; f += ddF_y; }
		x1++; ddF_x += 2; f += ddF_x;
		putPx(x + x1, y + y1, fgColor);
		putPx(x - x1, y + y1, fgColor);
		putPx(x + x1, y - y1, fgColor);
		putPx(x - x1, y - y1, fgColor);
		putPx(x + y1, y + x1, fgColor);
		putPx(x - y1, y + x1, fgColor);
		putPx(x + y1, y - x1, fgColor);
		putPx(x - y1, y - x1, fgColor);
	}
}

void display_fillCircle(int x, int y, int radius) {
	for (int y1 = -radius; y1 <= 0; y1++) {
		for (int x1 = -radius; x1 <= 0; x1++) {
			if (x1 * x1 + y1 * y1 <= radius * radius) {
				display_drawHLine(x + x1, y + y1, 2 * (-x1));
				display_drawHLine(x + x1, y - y1, 2 * (-x1));
				break;
			}
		}
	}
}

void display_printChar(byte c, int x, int y) {
	if (!cfont.font || c < cfont.offset || c >= cfont.offset + cfont.numchars) return;
	const int row_bytes = cfont.x_size / 8;
	word temp = static_cast<word>(((c - cfont.offset) * (row_bytes * cfont.y_size)) + 4);
	uint16_t* dst = activeBuf();
	for (word j = 0; j < cfont.y_size; j++) {
		int py = y + j;
		if (py >= 0 && py < 240) {
			uint16_t* row = &dst[py * 320];
			for (int zz = 0; zz < row_bytes; zz++) {
				byte ch = pgm_read_byte(&cfont.font[temp + zz]);
				int bx = x + (zz * 8);
				for (byte i = 0; i < 8; i++) {
					int px = bx + i;
					if (px >= 0 && px < 320) {
						if ((ch & (1u << (7 - i))) != 0)
							row[px] = fgColor;
						else if (!transparent)
							row[px] = bgColor;
					}
				}
			}
		}
		temp += row_bytes;
	}
}

void display_print(char* st, int x, int y, int) {
	if (!st) return;
	int stl = static_cast<int>(std::strlen(st));
	if (x == RIGHT) x = 320 - (stl * cfont.x_size);
	if (x == CENTER) x = (320 - (stl * cfont.x_size)) / 2;
	for (int i = 0; i < stl; i++) {
		display_printChar(static_cast<byte>(st[i]), x, y);
		x += cfont.x_size;
	}
}

void display_print(String st, int x, int y, int deg) {
	std::string copy = st.s;
	display_print(const_cast<char*>(copy.c_str()), x, y, deg);
}

void display_printC(String st, int x, int y, uint32_t color) {
	display_setColor(static_cast<word>(color & 0xFFFFu));
	display_print(st, x, y, 0);
}

void display_printNumI(long num, int x, int y, int length, char filler) {
	char buf[32];
	if (length > 0 && filler == '0') {
		std::snprintf(buf, sizeof(buf), "%0*ld", length, num);
	} else if (length > 0) {
		std::snprintf(buf, sizeof(buf), "%*ld", length, num);
	} else {
		std::snprintf(buf, sizeof(buf), "%ld", num);
	}
	display_print(buf, x, y);
}

void display_convertFloat(char* buf, double num, int width, byte prec) {
	dtostrf(num, static_cast<signed char>(width), prec, buf);
}

void display_printNumF(double num, byte dec, int x, int y, char divider, int length, char) {
	char st[32];
	std::snprintf(st, sizeof(st), "%*.*f", length, static_cast<int>(dec), num);
	if (divider != '.') {
		for (char* p = st; *p; ++p) if (*p == '.') *p = divider;
	}
	display_print(st, x, y);
}

int display_getStringWidth(char* str) {
	return str ? static_cast<int>(std::strlen(str)) * cfont.x_size : 0;
}

void display_drawBitmap(int x, int y, int w, int h, unsigned short* data) {
	uint16_t* dst = activeBuf();
	for (int yy = 0; yy < h; ++yy) {
		int py = y + yy;
		if (py < 0 || py >= 240) continue;
		for (int xx = 0; xx < w; ++xx) {
			int px = x + xx;
			if (px >= 0 && px < 320) dst[py * 320 + px] = data[yy * w + xx];
		}
	}
}

void display_writeRect2BPP(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* pixels, const uint16_t* palette) {
	const uint8_t* p = pixels;
	for (int yy = 0; yy < h; ++yy) {
		for (int xx = 0; xx < w; xx += 4) {
			uint8_t b = *p++;
			putPx(x + xx + 0, y + yy, palette[(b >> 6) & 3]);
			putPx(x + xx + 1, y + yy, palette[(b >> 4) & 3]);
			putPx(x + xx + 2, y + yy, palette[(b >> 2) & 3]);
			putPx(x + xx + 3, y + yy, palette[b & 3]);
		}
	}
}

void display_writeRect4BPP(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* pixels, const uint16_t* palette) {
	const uint8_t* p = pixels;
	for (int yy = 0; yy < h; ++yy) {
		for (int xx = 0; xx < w; xx += 2) {
			uint8_t b = *p++;
			putPx(x + xx + 0, y + yy, palette[(b >> 4) & 0xF]);
			putPx(x + xx + 1, y + yy, palette[b & 0xF]);
		}
	}
}

void display_blendRect(int x, int y, int w, int h, word color, uint8_t alpha_num_of_8) {
	if (!display_writeToImage || !bigBuffer || w <= 0 || h <= 0) return;
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > 320) w = 320 - x;
	if (y + h > 240) h = 240 - y;
	if (w <= 0 || h <= 0) return;
	uint32_t a = (alpha_num_of_8 > 8) ? 8u : alpha_num_of_8;
	uint32_t inv = 8u - a;
	uint32_t sr = (color >> 11) & 0x1Fu;
	uint32_t sg = (color >> 5) & 0x3Fu;
	uint32_t sb = color & 0x1Fu;
	for (int yy = y; yy < y + h; ++yy) {
		uint16_t* row = &bigBuffer[yy * 320 + x];
		for (int xx = 0; xx < w; ++xx) {
			uint32_t bg = row[xx];
			uint32_t r = (sr * a + ((bg >> 11) & 0x1Fu) * inv) >> 3;
			uint32_t g = (sg * a + ((bg >> 5) & 0x3Fu) * inv) >> 3;
			uint32_t b = (sb * a + (bg & 0x1Fu) * inv) >> 3;
			row[xx] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
		}
	}
}

void display_writeScreen(unsigned short* pcolors) {
	std::memcpy(sim_fb, pcolors, sizeof(sim_fb));
}
