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
* https://github.com/maxritter/DIY-Thermocam
*
*/

/*################################# INCLUDES ##################################*/

#include <Arduino.h>
#include <globaldefines.h>
#include <globalvariables.h>
#include <SPI.h>
#include <hardware.h>
#include <EEPROM.h>
#include <display.h>

/*################# DATA TYPES, CONSTANTS & MACRO DEFINITIONS #################*/

#define ILI9341_SPICLOCK 25000000

#define ILI9341_TFTWIDTH 240
#define ILI9341_TFTHEIGHT 320

#define ILI9341_NOP 0x00
#define ILI9341_SWRESET 0x01
#define ILI9341_RDDID 0x04
#define ILI9341_RDDST 0x09

#define ILI9341_SLPIN 0x10
#define ILI9341_SLPOUT 0x11
#define ILI9341_PTLON 0x12
#define ILI9341_NORON 0x13

#define ILI9341_RDMODE 0x0A
#define ILI9341_RDMADCTL 0x0B
#define ILI9341_RDPIXFMT 0x0C
#define ILI9341_RDIMGFMT 0x0A
#define ILI9341_RDSELFDIAG 0x0F

#define ILI9341_INVOFF 0x20
#define ILI9341_INVON 0x21
#define ILI9341_GAMMASET 0x26
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON 0x29

#define ILI9341_CASET 0x2A
#define ILI9341_PASET 0x2B
#define ILI9341_RAMWR 0x2C
#define ILI9341_RAMRD 0x2E

#define ILI9341_PTLAR 0x30
#define ILI9341_MADCTL 0x36
#define ILI9341_PIXFMT 0x3A

#define ILI9341_FRMCTR1 0xB1
#define ILI9341_FRMCTR2 0xB2
#define ILI9341_FRMCTR3 0xB3
#define ILI9341_INVCTR 0xB4
#define ILI9341_DFUNCTR 0xB6

#define ILI9341_PWCTR1 0xC0
#define ILI9341_PWCTR2 0xC1
#define ILI9341_PWCTR3 0xC2
#define ILI9341_PWCTR4 0xC3
#define ILI9341_PWCTR5 0xC4
#define ILI9341_VMCTR1 0xC5
#define ILI9341_VMCTR2 0xC7

#define ILI9341_RDID1 0xDA
#define ILI9341_RDID2 0xDB
#define ILI9341_RDID3 0xDC
#define ILI9341_RDID4 0xDD

#define ILI9341_GMCTRP1 0xE0
#define ILI9341_GMCTRN1 0xE1

#define MADCTL_MY 0x80
#define MADCTL_MX 0x40
#define MADCTL_MV 0x20
#define MADCTL_ML 0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH 0x04

#define TCR_MASK (LPSPI_TCR_PCS(3) | LPSPI_TCR_FRAMESZ(31) | LPSPI_TCR_CONT | LPSPI_TCR_RXMSK)

#define swap(type, i, j) \
	{                    \
		type t = i;      \
		i = j;           \
		j = t;           \
	}
#define fontbyte(x) cfont.font[x]

struct current_font
{
	uint8_t *font;
	uint8_t x_size;
	uint8_t y_size;
	uint8_t offset;
	uint8_t numchars;
};

/*######################### STATIC DATA DECLARATIONS ##########################*/

static uint8_t rotation;
static current_font cfont;
static boolean transparent;
static byte fch, fcl, bch, bcl, orient;
static uint16_t imageX, imageY;

static const uint8_t init_commands[] = {
	4, 0xEF, 0x03, 0x80, 0x02,
	4, 0xCF, 0x00, 0XC1, 0X30,
	5, 0xED, 0x64, 0x03, 0X12, 0X81,
	4, 0xE8, 0x85, 0x00, 0x78,
	6, 0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02,
	2, 0xF7, 0x20,
	3, 0xEA, 0x00, 0x00,
	2, ILI9341_PWCTR1, 0x23,	   // Power control
	2, ILI9341_PWCTR2, 0x10,	   // Power control
	3, ILI9341_VMCTR1, 0x3e, 0x28, // VCM control
	2, ILI9341_VMCTR2, 0x86,	   // VCM control2
	2, ILI9341_MADCTL, 0x48,	   // Memory Access Control
	2, ILI9341_PIXFMT, 0x55,
	3, ILI9341_FRMCTR1, 0x00, 0x18,
	4, ILI9341_DFUNCTR, 0x08, 0x82, 0x27, // Display Function Control
	2, 0xF2, 0x00,						  // Gamma Function Disable
	2, ILI9341_GAMMASET, 0x01,			  // Gamma curve selected
	16, ILI9341_GMCTRP1, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
	0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00, // Set Gamma
	16, ILI9341_GMCTRN1, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
	0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F, // Set Gamma
	3, 0xb1, 0x00, 0x10,								  // FrameRate Control 119Hz
	0};

/*############################# PUBLIC VARIABLES ##############################*/

boolean display_writeToImage;
uint32_t display_cs_pinmask;
volatile uint32_t *display_cs_port;
uint32_t display_spi_tcr_current;
uint32_t display_dc_pinmask;
volatile uint32_t *display_dc_port;
uint8_t display_pending_rx_count;
uint32_t display_tcr_dc_assert;
uint32_t display_tcr_dc_not_assert;

/*######################## PUBLIC FUNCTION BODIES #############################*/

void display_begin_spi_transaction()
{
	SPI.beginTransaction(SPISettings(ILI9341_SPICLOCK, MSBFIRST, SPI_MODE0));
	if (!display_dc_port)
		display_spi_tcr_current = IMXRT_LPSPI4_S.TCR;
	if (display_cs_port)
		t4_direct_write_low(display_cs_port, display_cs_pinmask);
}
void display_end_spi_transaction()
{
	if (display_cs_port)
		t4_direct_write_high(display_cs_port, display_cs_pinmask);
	SPI.endTransaction();
}

void display_waitFifoNotFull()
{
	uint32_t tmp __attribute__((unused));
	do
	{
		if ((IMXRT_LPSPI4_S.RSR & LPSPI_RSR_RXEMPTY) == 0)
		{
			tmp = IMXRT_LPSPI4_S.RDR;
			if (display_pending_rx_count)
				display_pending_rx_count--;
		}
	} while ((IMXRT_LPSPI4_S.SR & LPSPI_SR_TDF) == 0);
}

void display_waitTransmitComplete()
{
	uint32_t tmp __attribute__((unused));

	while (display_pending_rx_count)
	{
		if ((IMXRT_LPSPI4_S.RSR & LPSPI_RSR_RXEMPTY) == 0)
		{
			tmp = IMXRT_LPSPI4_S.RDR;
			display_pending_rx_count--;
		}
	}
	IMXRT_LPSPI4_S.CR = LPSPI_CR_MEN | LPSPI_CR_RRF;
}

void display_maybeUpdateTCR(uint32_t requested_tcr_state)
{
	if ((display_spi_tcr_current & TCR_MASK) != requested_tcr_state)
	{
		bool dc_state_change = (display_spi_tcr_current & LPSPI_TCR_PCS(3)) != (requested_tcr_state & LPSPI_TCR_PCS(3));
		display_spi_tcr_current = (display_spi_tcr_current & ~TCR_MASK) | requested_tcr_state;
		if (!dc_state_change || !display_dc_pinmask)
		{
			while ((IMXRT_LPSPI4_S.FSR & 0x1f))
				;
			IMXRT_LPSPI4_S.TCR = display_spi_tcr_current;
		}

		else
		{
			display_waitTransmitComplete();
			if (requested_tcr_state & LPSPI_TCR_PCS(3))
				t4_direct_write_high(display_dc_port, display_dc_pinmask);
			else
				t4_direct_write_low(display_dc_port, display_dc_pinmask);
			IMXRT_LPSPI4_S.TCR = display_spi_tcr_current & ~(LPSPI_TCR_PCS(3) | LPSPI_TCR_CONT);
		}
	}
}

void display_writecommand_cont(uint8_t c)
{
	display_maybeUpdateTCR(display_tcr_dc_assert | LPSPI_TCR_FRAMESZ(7));
	IMXRT_LPSPI4_S.TDR = c;
	display_pending_rx_count++;
}

void display_writedata8_cont(uint8_t c)
{
	display_maybeUpdateTCR(display_tcr_dc_not_assert | LPSPI_TCR_FRAMESZ(7) | LPSPI_TCR_CONT);
	IMXRT_LPSPI4_S.TDR = c;
	display_pending_rx_count++;
	display_waitFifoNotFull();
}

void display_writedata16_cont(uint16_t d)
{
	display_maybeUpdateTCR(display_tcr_dc_not_assert | LPSPI_TCR_FRAMESZ(15) | LPSPI_TCR_CONT);
	IMXRT_LPSPI4_S.TDR = d;
	display_pending_rx_count++;
	display_waitFifoNotFull();
}

void display_writecommand_last(uint8_t c)
{
	display_maybeUpdateTCR(display_tcr_dc_assert | LPSPI_TCR_FRAMESZ(7));
	IMXRT_LPSPI4_S.TDR = c;
	display_pending_rx_count++;
	display_waitTransmitComplete();
}

void display_writedata8_last(uint8_t c)
{
	display_maybeUpdateTCR(display_tcr_dc_not_assert | LPSPI_TCR_FRAMESZ(7));
	IMXRT_LPSPI4_S.TDR = c;
	display_pending_rx_count++;
	display_waitTransmitComplete();
}

void display_writedata16_last(uint16_t d)
{
	display_maybeUpdateTCR(display_tcr_dc_not_assert | LPSPI_TCR_FRAMESZ(15));
	IMXRT_LPSPI4_S.TDR = d;
	display_pending_rx_count++;
	display_waitTransmitComplete();
}

void display_setAddr(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	display_writecommand_cont(ILI9341_CASET);
	display_writedata16_cont(x0);
	display_writedata16_cont(x1);
	display_writecommand_cont(ILI9341_PASET);
	display_writedata16_cont(y0);
	display_writedata16_cont(y1);
}

/* Read 8-bit command from the screen */
uint8_t display_readcommand8(uint8_t c, uint8_t index)
{
	uint8_t r = 0;

	display_begin_spi_transaction();

	if (display_dc_port)
	{
		t4_direct_write_low(display_dc_port, display_dc_pinmask);
		IMXRT_LPSPI4_S.SR = LPSPI_SR_TCF | LPSPI_SR_FCF | LPSPI_SR_WCF;
		IMXRT_LPSPI4_S.TCR = LPSPI_TCR_FRAMESZ(7) | LPSPI_TCR_RXMSK | LPSPI_TCR_CONT;
		IMXRT_LPSPI4_S.TDR = 0xD9;
		while (!(IMXRT_LPSPI4_S.SR & LPSPI_SR_WCF))
			;

		t4_direct_write_high(display_dc_port, display_dc_pinmask);
		IMXRT_LPSPI4_S.SR = LPSPI_SR_TCF | LPSPI_SR_FCF | LPSPI_SR_WCF;
		IMXRT_LPSPI4_S.TDR = 0x10 + index;
		while (!(IMXRT_LPSPI4_S.SR & LPSPI_SR_WCF))
			;

		t4_direct_write_low(display_dc_port, display_dc_pinmask);
		IMXRT_LPSPI4_S.SR = LPSPI_SR_TCF | LPSPI_SR_FCF | LPSPI_SR_WCF;
		IMXRT_LPSPI4_S.TDR = c;
		while (!(IMXRT_LPSPI4_S.SR & LPSPI_SR_WCF))
			;

		t4_direct_write_high(display_dc_port, display_dc_pinmask);
		IMXRT_LPSPI4_S.SR = LPSPI_SR_TCF | LPSPI_SR_FCF | LPSPI_SR_WCF;
		IMXRT_LPSPI4_S.TCR = LPSPI_TCR_FRAMESZ(7);
		IMXRT_LPSPI4_S.TDR = 0x10 + index;
		while (!(IMXRT_LPSPI4_S.SR & LPSPI_SR_WCF))
			while (((IMXRT_LPSPI4_S.FSR >> 16) & 0x1F) == 0)

				r = IMXRT_LPSPI4_S.RDR;
	}

	display_end_spi_transaction();

	return r;
}

/* Set display rotation */
void display_setRotation(uint8_t m)
{
	display_begin_spi_transaction();
	display_writecommand_cont(ILI9341_MADCTL);
	rotation = m % 4;
	switch (rotation)
	{
	case 0:
		display_writedata8_last(MADCTL_MX | MADCTL_BGR);
		orient = LANDSCAPE;
		break;
	case 1:
		display_writedata8_last(MADCTL_MV | MADCTL_BGR);
		orient = PORTRAIT;
		break;
	case 2:
		display_writedata8_last(MADCTL_MY | MADCTL_BGR);
		orient = LANDSCAPE;
		break;
	case 3:
		display_writedata8_last(MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR);
		orient = PORTRAIT;
		break;
	}
	display_end_spi_transaction();
}

/* Init the hardware LCD */
byte display_InitLCD()
{
	display_pending_rx_count = 0;
	display_cs_port = portOutputRegister(pin_lcd_cs);
	display_cs_pinmask = digitalPinToBitMask(pin_lcd_cs);
	display_spi_tcr_current = IMXRT_LPSPI4_S.TCR;

	uint8_t dc_cs_index = SPI.setCS(pin_lcd_dc);
	display_dc_port = 0;
	display_dc_pinmask = 0;
	dc_cs_index--;
	display_tcr_dc_assert = LPSPI_TCR_PCS(dc_cs_index);
	display_tcr_dc_not_assert = LPSPI_TCR_PCS(3);

	display_maybeUpdateTCR(display_tcr_dc_not_assert | LPSPI_TCR_FRAMESZ(7));

	//Read the self-diagnostic flag
	byte diag = display_readcommand8(ILI9341_RDSELFDIAG);

	//Send the init commands
	display_begin_spi_transaction();
	const uint8_t *addr = init_commands;
	while (1)
	{
		uint8_t count = *addr++;
		if (count-- == 0)
			break;
		display_writecommand_cont(*addr++);
		while (count-- > 0)
		{
			display_writedata8_cont(*addr++);
		}
	}
	display_writecommand_last(ILI9341_SLPOUT);
	display_end_spi_transaction();

	//Wait a short time
	delay(120);

	//Turn the display on
	display_begin_spi_transaction();
	display_writecommand_last(ILI9341_DISPON);
	display_end_spi_transaction();

	//Init font & transparency
	cfont.font = 0;
	transparent = 0;

	//Set the display rotation
	display_setRotation(45);

	//Disable write to image
	display_writeToImage = 0;

	//Return the diagnostic info
	return diag;
}

/* Init the display module */
void display_init()
{
	byte count = 0;
	//Init the display
	byte check = display_InitLCD();

	//Status not okay, try again 10 times
	while ((check != 0x00) && (count < 10))
	{
		delay(10);
		check = display_InitLCD();
		count++;
	}
	//If it failed after 10 attemps, show diag
	if (check != 0x00)
		setDiagnostic(diag_display);

	//Read 180° rotation
	byte read = EEPROM.read(eeprom_rotationVert);
	if ((read == 0) || (read == 1))
		rotationVert = read;
	else
		rotationVert = 0;
}

/* Set the xy coordinates */
void display_setXY(word x1, word y1, word x2, word y2)
{
	if (orient == LANDSCAPE)
	{
		swap(word, x1, y1);
		swap(word, x2, y2);
		y1 = 239 - y1;
		y2 = 239 - y2;
		swap(word, y1, y2);
	}

	//Write to the display
	if (!display_writeToImage)
	{
		display_begin_spi_transaction();
		display_setAddr(x1, y1, x2, y2);
		display_writecommand_last(ILI9341_RAMWR); // write to RAM
		display_end_spi_transaction();
	}
	//Write to the image buffer
	else
	{
		imageX = x1;
		imageY = y1;
	}
}

/* Clear the xy coordinates */
void display_clrXY()
{
	if (orient == PORTRAIT)
		display_setXY(0, 0, 319, 239);
	else
		display_setXY(0, 0, 239, 319);
}

/* Clear the screen */
void display_clrScr()
{
	display_setXY(0, 0, 239, 319);
}

/* Draw a pixel */
void display_drawPixel(int x, int y)
{
	//Out of borders, return
	if ((x < 0) || (x >= 320) || (y < 0) || (y >= 240))
		return;
	//Send pixel coordinates and color to screen
	display_begin_spi_transaction();
	display_setAddr(x, y, x, y);
	display_writecommand_cont(ILI9341_RAMWR);
	display_writedata16_last(fch << 8 | fcl);
	display_end_spi_transaction();
}

/* Draw a horizontal line */
void display_drawHLine(int x, int y, int l)
{
	//Clipping
	if ((x >= 320) || (y >= 240))
		return;
	if ((x + l - 1) >= 320)
		l = 320 - x;

	display_begin_spi_transaction();
	display_setAddr(x, y, x + l - 1, y);
	display_writecommand_cont(ILI9341_RAMWR);
	word color = (fch << 8 | fcl);
	while (l-- > 1)
	{
		display_writedata16_cont(color);
	}
	display_writedata16_last(color);
	display_end_spi_transaction();
}

/* Draw a vertical line */
void display_drawVLine(int x, int y, int l)
{
	//Clipping
	if ((x >= 320) || (y >= 240))
		return;
	if ((y + l - 1) >= 240)
		l = 240 - y;

	display_begin_spi_transaction();
	display_setAddr(x, y, x, y + l - 1);
	display_writecommand_cont(ILI9341_RAMWR);
	word color = (fch << 8 | fcl);
	while (l-- > 1)
	{
		display_writedata16_cont(color);
	}
	display_writedata16_last(color);
	display_end_spi_transaction();
}

/* Set a specific pixel in that color */
void display_setPixel(word color)
{
	uint32_t pos;

	//Write to display
	if (!display_writeToImage)
	{
		display_begin_spi_transaction();
		display_writedata16_last(color);
		display_end_spi_transaction();
	}
	//Write to buffer directly
	else
	{
		pos = ((imageY)*320) + imageX;
		if (pos < 76800)
			bigBuffer[pos] = color;
	}
}

/* Write the data to the LCD */
void display_LCD_Write_DATA(char VH, char VL)
{
	display_setPixel((VH << 8) | VL);
}

/* Draw a line */
void display_drawLine(int x1, int y1, int x2, int y2)
{
	//Write to screen
	if ((y1 == y2) && (!display_writeToImage))
		display_drawHLine(x1, y1, x2 - x1);
	else if ((x1 == x2) && (!display_writeToImage))
		display_drawVLine(x1, y1, y2 - y1);
	//Write to image buffer directly
	else
	{
		unsigned int dx = (x2 > x1 ? x2 - x1 : x1 - x2);
		short xstep = x2 > x1 ? 1 : -1;
		unsigned int dy = (y2 > y1 ? y2 - y1 : y1 - y2);
		short ystep = y2 > y1 ? 1 : -1;
		int col = x1, row = y1;

		if (dx < dy)
		{
			int t = -(dy >> 1);
			while (1)
			{
				display_setXY(col, row, col, row);
				display_LCD_Write_DATA(fch, fcl);
				if (row == y2)
					return;
				row += ystep;
				t += dx;
				if (t >= 0)
				{
					col += xstep;
					t -= dy;
				}
			}
		}
		else
		{
			int t = -(dx >> 1);
			while (1)
			{
				display_setXY(col, row, col, row);
				display_LCD_Write_DATA(fch, fcl);
				if (col == x2)
					return;
				col += xstep;
				t += dy;
				if (t >= 0)
				{
					row += ystep;
					t -= dx;
				}
			}
		}
	}
	display_clrXY();
}

/* Fill the screen by RGB565 color */
void display_fillScr(word color)
{
	int x = 0;
	int y = 0;
	display_begin_spi_transaction();
	display_setAddr(x, y, x + 319, y + 239);
	display_writecommand_cont(ILI9341_RAMWR);
	for (y = 240; y > 0; y--)
	{
		for (x = 320; x > 1; x--)
		{
			display_writedata16_cont(color);
		}
		display_writedata16_last(color);
	}
	display_end_spi_transaction();
}

/* Fill the screen by separate RGB value */
void display_fillScr(byte r, byte g, byte b)
{
	word color = ((r & 248) << 8 | (g & 252) << 3 | (b & 248) >> 3);
	display_fillScr(color);
}

/* Draw an empty rectangle */
void display_drawRect(int x1, int y1, int x2, int y2)
{
	if (x1 > x2)
	{
		swap(int, x1, x2);
	}
	if (y1 > y2)
	{
		swap(int, y1, y2);
	}

	display_drawHLine(x1, y1, x2 - x1);
	display_drawHLine(x1, y2, x2 - x1);
	display_drawVLine(x1, y1, y2 - y1);
	display_drawVLine(x2, y1, y2 - y1);
}

/* Fill a rectangle */
void display_fillRect(int x1, int y1, int x2, int y2)
{
	if (x1 > x2)
	{
		swap(int, x1, x2);
	}
	if (y1 > y2)
	{
		swap(int, y1, y2);
	}

	int w = x2 - x1;
	int h = y2 - y1;

	//Clipping
	if ((x1 >= 320) || (y1 >= 240))
		return;
	if ((x1 + w - 1) >= 320)
		w = 320 - x1;
	if ((y1 + h - 1) >= 240)
		h = 240 - y1;

	//Send to display
	display_begin_spi_transaction();
	display_setAddr(x1, y1, x1 + w - 1, y1 + h - 1);
	display_writecommand_cont(ILI9341_RAMWR);
	word color = (fch << 8 | fcl);
	for (int y = h; y > 0; y--)
	{
		for (int x = w; x > 1; x--)
		{
			display_writedata16_cont(color);
		}
		display_writedata16_last(color);
	}
	display_end_spi_transaction();
}

/* Draw an empty round rectangle */
void display_drawRoundRect(int x1, int y1, int x2, int y2)
{
	if (x1 > x2)
	{
		swap(int, x1, x2);
	}
	if (y1 > y2)
	{
		swap(int, y1, y2);
	}
	if ((x2 - x1) > 4 && (y2 - y1) > 4)
	{
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

/* Fill a round rectangle */
void display_fillRoundRect(int x1, int y1, int x2, int y2)
{
	if (x1 > x2)
	{
		swap(int, x1, x2);
	}
	if (y1 > y2)
	{
		swap(int, y1, y2);
	}

	if ((x2 - x1) > 4 && (y2 - y1) > 4)
	{
		for (int i = 0; i < ((y2 - y1) / 2) + 1; i++)
		{
			switch (i)
			{
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
			}
		}
	}
}

/* Draw an empty circle */
void display_drawCircle(int x, int y, int radius)
{
	int f = 1 - radius;
	int ddF_x = 1;
	int ddF_y = -2 * radius;
	int x1 = 0;
	int y1 = radius;

	display_setXY(x, y + radius, x, y + radius);
	display_LCD_Write_DATA(fch, fcl);
	display_setXY(x, y - radius, x, y - radius);
	display_LCD_Write_DATA(fch, fcl);
	display_setXY(x + radius, y, x + radius, y);
	display_LCD_Write_DATA(fch, fcl);
	display_setXY(x - radius, y, x - radius, y);
	display_LCD_Write_DATA(fch, fcl);

	while (x1 < y1)
	{
		if (f >= 0)
		{
			y1--;
			ddF_y += 2;
			f += ddF_y;
		}
		x1++;
		ddF_x += 2;
		f += ddF_x;
		display_setXY(x + x1, y + y1, x + x1, y + y1);
		display_LCD_Write_DATA(fch, fcl);
		display_setXY(x - x1, y + y1, x - x1, y + y1);
		display_LCD_Write_DATA(fch, fcl);
		display_setXY(x + x1, y - y1, x + x1, y - y1);
		display_LCD_Write_DATA(fch, fcl);
		display_setXY(x - x1, y - y1, x - x1, y - y1);
		display_LCD_Write_DATA(fch, fcl);
		display_setXY(x + y1, y + x1, x + y1, y + x1);
		display_LCD_Write_DATA(fch, fcl);
		display_setXY(x - y1, y + x1, x - y1, y + x1);
		display_LCD_Write_DATA(fch, fcl);
		display_setXY(x + y1, y - x1, x + y1, y - x1);
		display_LCD_Write_DATA(fch, fcl);
		display_setXY(x - y1, y - x1, x - y1, y - x1);
		display_LCD_Write_DATA(fch, fcl);
	}
	display_clrXY();
}

/* Fill a circle */
void display_fillCircle(int x, int y, int radius)
{
	for (int y1 = -radius; y1 <= 0; y1++)
	{
		for (int x1 = -radius; x1 <= 0; x1++)
		{
			if (x1 * x1 + y1 * y1 <= radius * radius)
			{
				display_drawHLine(x + x1, y + y1, 2 * (-x1));
				display_drawHLine(x + x1, y - y1, 2 * (-x1));
				break;
			}
		}
	}
}

/* Set color to separate RGB values */
void display_setColor(byte r, byte g, byte b)
{
	fch = ((r & 248) | g >> 5);
	fcl = ((g & 28) << 3 | b >> 3);
}

/* Set color to RGB565 color */
void display_setColor(word color)
{
	fch = byte(color >> 8);
	fcl = byte(color & 0xFF);
}

/* Get current RGB565 color */
word display_getColor()
{
	return (fch << 8) | fcl;
}

/* Set back color to separate RGB value */
void display_setBackColor(byte r, byte g, byte b)
{
	bch = ((r & 248) | g >> 5);
	bcl = ((g & 28) << 3 | b >> 3);
	transparent = 0;
}

/* Set back color to RGB565 value */
void display_setBackColor(uint32_t color)
{
	if (color == VGA_TRANSPARENT)
		transparent = 1;
	else
	{
		bch = byte(color >> 8);
		bcl = byte(color & 0xFF);
		transparent = 0;
	}
}

/* Get back color as RGB565 value */
word display_getBackColor()
{
	return (bch << 8) | bcl;
}

/* Print a specific char */
void display_printChar(byte c, int x, int y)
{
	byte i, ch;
	word j;
	word temp;

	//Not transparent
	if (!transparent)
	{
		if (orient == PORTRAIT)
		{
			display_setXY(x, y, x + cfont.x_size - 1, y + cfont.y_size - 1);

			temp = ((c - cfont.offset) * ((cfont.x_size / 8) * cfont.y_size)) + 4;
			for (j = 0; j < ((cfont.x_size / 8) * cfont.y_size); j++)
			{
				ch = pgm_read_byte(&cfont.font[temp]);
				for (i = 0; i < 8; i++)
				{
					if ((ch & (1 << (7 - i))) != 0)
					{
						display_setPixel((fch << 8) | fcl);
					}
					else
					{
						display_setPixel((bch << 8) | bcl);
					}
				}
				temp++;
			}
		}
		else
		{
			temp = ((c - cfont.offset) * ((cfont.x_size / 8) * cfont.y_size)) + 4;

			for (j = 0; j < ((cfont.x_size / 8) * cfont.y_size);
				 j += (cfont.x_size / 8))
			{
				display_setXY(x, y + (j / (cfont.x_size / 8)), x + cfont.x_size - 1,
							  y + (j / (cfont.x_size / 8)));
				for (int zz = (cfont.x_size / 8) - 1; zz >= 0; zz--)
				{
					ch = pgm_read_byte(&cfont.font[temp + zz]);
					for (i = 0; i < 8; i++)
					{
						if ((ch & (1 << i)) != 0)
						{
							display_setPixel((fch << 8) | fcl);
						}
						else
						{
							display_setPixel((bch << 8) | bcl);
						}
					}
				}
				temp += (cfont.x_size / 8);
			}
		}
	}

	//Transparent
	else
	{
		temp = ((c - cfont.offset) * ((cfont.x_size / 8) * cfont.y_size)) + 4;
		for (j = 0; j < cfont.y_size; j++)
		{
			for (int zz = 0; zz < (cfont.x_size / 8); zz++)
			{
				ch = pgm_read_byte(&cfont.font[temp + zz]);
				for (i = 0; i < 8; i++)
				{
					display_setXY(x + i + (zz * 8), y + j, x + i + (zz * 8) + 1,
								  y + j + 1);

					if ((ch & (1 << (7 - i))) != 0)
					{
						display_setPixel((fch << 8) | fcl);
					}
				}
			}
			temp += (cfont.x_size / 8);
		}
	}

	display_clrXY();
}

/* Get the font height */
int display_getFontHeight()
{
	return (cfont.y_size);
}

/* Return the Glyph data for an individual character in the font*/
boolean display_getCharPtr(byte c, propFont &fontChar)
{
	byte *tempPtr = cfont.font + 4; // point at data

	do
	{
		fontChar.charCode = pgm_read_byte(tempPtr++);
		fontChar.adjYOffset = pgm_read_byte(tempPtr++);
		fontChar.width = pgm_read_byte(tempPtr++);
		fontChar.height = pgm_read_byte(tempPtr++);
		fontChar.xOffset = pgm_read_byte(tempPtr++);
		fontChar.xOffset = fontChar.xOffset < 0x80 ? fontChar.xOffset : (0x100 - fontChar.xOffset);
		fontChar.xDelta = pgm_read_byte(tempPtr++);
		if (c != fontChar.charCode && fontChar.charCode != 0xFF)
		{
			if (fontChar.width != 0)
			{
				// packed bits
				tempPtr += (((fontChar.width * fontChar.height) - 1) / 8) + 1;
			}
		}
	} while (c != fontChar.charCode && fontChar.charCode != 0xFF);

	fontChar.dataPtr = tempPtr;

	return (fontChar.charCode != 0xFF);
}

/* Print a proportional char */
int display_printProportionalChar(byte c, int x, int y)
{
	byte i, j;
	byte ch = 0;
	byte *tempPtr;

	propFont fontChar;
	if (!display_getCharPtr(c, fontChar))
	{
		return 0;
	}

	word fcolor = display_getColor();
	if (!transparent)
	{
		int fontHeight = display_getFontHeight();
		display_setColor(display_getBackColor());
		display_fillRect(x, y, x + fontChar.xDelta + 1, y + fontHeight);
		display_setColor(fcolor);
	}

	tempPtr = fontChar.dataPtr;

	if (fontChar.width != 0)
	{
		byte mask = 0x80;
		for (j = 0; j < fontChar.height; j++)
		{
			for (i = 0; i < fontChar.width; i++)
			{
				if (((i + (j * fontChar.width)) % 8) == 0)
				{
					mask = 0x80;
					ch = pgm_read_byte(tempPtr++);
				}

				if ((ch & mask) != 0)
				{
					display_setXY(x + fontChar.xOffset + i, y + j + fontChar.adjYOffset,
								  x + fontChar.xOffset + i, y + j + fontChar.adjYOffset);
					display_setPixel(fcolor);
				}

				mask >>= 1;
			}
		}
	}
	return fontChar.xDelta;
}

/* Rotate a proportional char */
int display_rotatePropChar(byte c, int x, int y, int offset, int deg)
{
	propFont fontChar;

	if (!display_getCharPtr(c, fontChar))
	{
		return 0;
	}

	byte ch = 0;
	byte *tempPtr = fontChar.dataPtr;
	double radian = deg * 0.0175;

	word fcolor = display_getColor();

	if (fontChar.width != 0)
	{
		byte mask = 0x80;
		float cos_radian = cos(radian);
		float sin_radian = sin(radian);
		for (int j = 0; j < fontChar.height; j++)
		{
			for (int i = 0; i < fontChar.width; i++)
			{
				if (((i + (j * fontChar.width)) % 8) == 0)
				{
					mask = 0x80;
					ch = pgm_read_byte(tempPtr++);
				}

				int newX = x + ((offset + i) * cos_radian - (j + fontChar.adjYOffset) * sin_radian);
				int newY = y + ((j + fontChar.adjYOffset) * cos_radian + (offset + i) * sin_radian);
				if ((ch & mask) != 0)
				{
					display_setXY(newX, newY, newX, newY);
					display_setPixel(fcolor);
				}
				else
				{
					if (!transparent)
					{
						display_setXY(newX, newY, newX, newY);
						display_setPixel(display_getBackColor());
					}
				}
				mask >>= 1;
			}
		}
	}

	display_clrXY();

	return fontChar.xDelta;
}

/* Rotate a char on the display */
void display_rotateChar(byte c, int x, int y, int pos, int deg)
{
	byte i, j, ch;
	word temp;
	int newx, newy;
	double radian;
	radian = deg * 0.0175;

	temp = ((c - cfont.offset) * ((cfont.x_size / 8) * cfont.y_size)) + 4;
	for (j = 0; j < cfont.y_size; j++)
	{
		for (int zz = 0; zz < (cfont.x_size / 8); zz++)
		{
			ch = pgm_read_byte(&cfont.font[temp + zz]);
			for (i = 0; i < 8; i++)
			{
				newx = x + (((i + (zz * 8) + (pos * cfont.x_size)) * cos(radian)) - ((j)*sin(radian)));
				newy = y + (((j)*cos(radian)) + ((i + (zz * 8) + (pos * cfont.x_size)) * sin(radian)));

				display_setXY(newx, newy, newx + 1, newy + 1);

				if ((ch & (1 << (7 - i))) != 0)
				{
					display_setPixel((fch << 8) | fcl);
				}
				else
				{
					if (!transparent)
						display_setPixel((bch << 8) | bcl);
				}
			}
		}
		temp += (cfont.x_size / 8);
	}
	display_clrXY();
}

/* Print char array on the display */
void display_print(char *st, int x, int y, int deg)
{
	int stl, i;
	stl = strlen(st);

	if (orient == PORTRAIT)
	{
		if (x == RIGHT)
			x = 320 - (stl * cfont.x_size);
		if (x == CENTER)
			x = (320 - (stl * cfont.x_size)) / 2;
	}
	else
	{
		if (x == RIGHT)
			x = 240 - (stl * cfont.x_size);
		if (x == CENTER)
			x = (240 - (stl * cfont.x_size)) / 2;
	}

	int offset = 0;
	for (i = 0; i < stl; i++)
	{
		if (deg == 0)
		{
			if (cfont.x_size == 0)
				x += display_printProportionalChar(*st++, x, y) + 1;
			else
			{
				display_printChar(*st++, x, y);
				x += cfont.x_size;
			}
		}
		else
		{
			if (cfont.x_size == 0)
				offset += display_rotatePropChar(*st++, x, y, offset, deg);
			else
				display_rotateChar(*st++, x, y, i, deg);
		}
	}
}

/* Print a rotated string on the display */
void display_print(String st, int x, int y, int deg)
{
	char buf[st.length() + 1];
	st.toCharArray(buf, st.length() + 1);
	display_print(buf, x, y, deg);
}

/* Print string on the display */
void display_printC(String st, int x, int y, uint32_t color)
{
	char buf[st.length() + 1];
	display_setColor(color);
	st.toCharArray(buf, st.length() + 1);
	display_print(buf, x, y, 0);
}

/* Print an integer */
void display_printNumI(long num, int x, int y, int length, char filler)
{
	char buf[25];
	char st[27];
	boolean neg = 0;
	int c = 0, f = 0;

	if (num == 0)
	{
		if (length != 0)
		{
			for (c = 0; c < (length - 1); c++)
				st[c] = filler;
			st[c] = 48;
			st[c + 1] = 0;
		}
		else
		{
			st[0] = 48;
			st[1] = 0;
		}
	}
	else
	{
		if (num < 0)
		{
			neg = 1;
			num = -num;
		}

		while (num > 0)
		{
			buf[c] = 48 + (num % 10);
			c++;
			num = (num - (num % 10)) / 10;
		}
		buf[c] = 0;

		if (neg)
		{
			st[0] = 45;
		}

		if (length > (c + neg))
		{
			for (int i = 0; i < (length - c - neg); i++)
			{
				st[i + neg] = filler;
				f++;
			}
		}

		for (int i = 0; i < c; i++)
		{
			st[i + neg + f] = buf[c - i - 1];
		}
		st[c + neg + f] = 0;
	}

	display_print(st, x, y);
}

/* Helper method to convert a float*/
void display_convertFloat(char *buf, double num, int width, byte prec)
{
	dtostrf(num, width, prec, buf);
}

/* Print a float */
void display_printNumF(double num, byte dec, int x, int y, char divider, int length, char filler)
{
	char st[27];
	boolean neg = 0;

	if (dec < 1)
		dec = 1;
	else if (dec > 5)
		dec = 5;

	if (num < 0)
		neg = 1;
	display_convertFloat(st, num, length, dec);
	if (divider != '.')
	{
		for (uint16_t i = 0; i < sizeof(st); i++)
			if (st[i] == '.')
				st[i] = divider;
	}

	if (filler != ' ')
	{
		if (neg)
		{
			st[0] = '-';
			for (uint16_t i = 1; i < sizeof(st); i++)
				if ((st[i] == ' ') || (st[i] == '-'))
					st[i] = filler;
		}
		else
		{
			for (uint16_t i = 0; i < sizeof(st); i++)
				if (st[i] == ' ')
					st[i] = filler;
		}
	}

	display_print(st, x, y);
}

/* Set a specific font */
void display_setFont(const uint8_t *font)
{
	cfont.font = (uint8_t *)font;
	cfont.x_size = fontbyte(0);
	cfont.y_size = fontbyte(1);
	cfont.offset = fontbyte(2);
	cfont.numchars = fontbyte(3);
}

/* Get the current font */
uint8_t *display_getFont()
{
	return cfont.font;
}

/* Get the x size of the current font */
uint8_t display_getFontXsize()
{
	return cfont.x_size;
}

/* Get the y size of the current font */
uint8_t display_getFontYsize()
{
	return cfont.y_size;
}

/* Draw a bitmap on the screen */
void display_drawBitmap(int x, int y, int w, int h, unsigned short *data)
{
	display_begin_spi_transaction();
	display_setAddr(x, y, x + w - 1, y + h - 1);
	display_writecommand_cont(ILI9341_RAMWR);

	for (y = h; y > 0; y--)
	{
		for (x = w; x > 1; x--)
		{
			display_writedata16_cont(*data++);
		}
		display_writedata16_last(*data++);
	}

	display_end_spi_transaction();
}

/* Write a paletted bitmap with 2BPP */
void display_writeRect2BPP(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *pixels, const uint16_t *palette)
{
	display_begin_spi_transaction();
	display_setAddr(x, y, x + w - 1, y + h - 1);
	display_writecommand_cont(ILI9341_RAMWR);
	for (y = h; y > 0; y--)
	{
		for (x = w; x > 4; x -= 4)
		{
			display_writedata16_cont(palette[((*pixels) >> 6) & 0x3]);
			display_writedata16_cont(palette[((*pixels) >> 4) & 0x3]);
			display_writedata16_cont(palette[((*pixels) >> 2) & 0x3]);
			display_writedata16_cont(palette[(*pixels++) & 0x3]);
		}
		display_writedata16_cont(palette[((*pixels) >> 6) & 0x3]);
		display_writedata16_cont(palette[((*pixels) >> 4) & 0x3]);
		display_writedata16_cont(palette[((*pixels) >> 2) & 0x3]);
		display_writedata16_last(palette[(*pixels++) & 0x3]);
	}
	display_end_spi_transaction();
}

/* Write a paletted bitmap with 4BPP */
void display_writeRect4BPP(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *pixels, const uint16_t *palette)
{
	display_begin_spi_transaction();
	display_setAddr(x, y, x + w - 1, y + h - 1);
	display_writecommand_cont(ILI9341_RAMWR);
	for (y = h; y > 0; y--)
	{
		for (x = w; x > 2; x -= 2)
		{
			display_writedata16_cont(palette[((*pixels) >> 4) & 0xF]);
			display_writedata16_cont(palette[(*pixels++) & 0xF]);
		}
		display_writedata16_cont(palette[((*pixels) >> 4) & 0xF]);
		display_writedata16_last(palette[(*pixels++) & 0xF]);
	}
	display_end_spi_transaction();
}

/* Returns the string width in pixels */
int display_getStringWidth(char *str)
{
	//Fixed font width
	if (cfont.x_size != 0)
	{
		return (strlen(str) * cfont.x_size);
	}

	//Calculate the string width
	int strWidth = 0;
	while (*str != 0)
	{
		propFont fontChar;
		boolean found = display_getCharPtr(*str, fontChar);

		if (found && *str == fontChar.charCode)
		{
			strWidth += fontChar.xDelta + 1;
		}

		str++;
	}

	return strWidth;
}

/* Enter sleep mode */
void display_enterSleepMode()
{
	display_begin_spi_transaction();
	display_writecommand_last(ILI9341_SLPIN);
	display_end_spi_transaction();
}

/* Exit sleep mode */
void display_exitSleepMode()
{
	display_begin_spi_transaction();
	display_writecommand_last(ILI9341_SLPOUT);
	display_end_spi_transaction();
}

/* Write 320x240 RGB565 data to the screen */
void display_writeScreen(unsigned short *pcolors)
{	
	displayUpdated = true;
	display_begin_spi_transaction();
	display_setAddr(0, 0, 319, 239);
	display_writecommand_cont(ILI9341_RAMWR);

	uint16_t *pfbtft_end = &pcolors[(ILI9341_TFTWIDTH * ILI9341_TFTHEIGHT) - 1];
	uint16_t *pftbft = pcolors;
	while (pftbft < pfbtft_end)
	{
		display_writedata16_cont(*pftbft++);
	}
	display_writedata16_last(*pftbft);
	display_end_spi_transaction();
	displayUpdated = false;
}