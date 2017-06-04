/*
 * Copyright (c) 2010 by Cristian Maglie <c.maglie@bug.st>
 * SPI Master library for arduino.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include "SPI.h"
#include "pins_arduino.h"



/**********************************************************/
/*     8 bit AVR-based boards				  */
/**********************************************************/

#if defined(__AVR__)

SPIClass SPI;

uint8_t SPIClass::interruptMode = 0;
uint8_t SPIClass::interruptMask = 0;
uint8_t SPIClass::interruptSave = 0;
#ifdef SPI_TRANSACTION_MISMATCH_LED
uint8_t SPIClass::inTransactionFlag = 0;
#endif

void SPIClass::begin()
{
	// Set SS to high so a connected chip will be "deselected" by default
	digitalWrite(SS, HIGH);

	// When the SS pin is set as OUTPUT, it can be used as
	// a general purpose output port (it doesn't influence
	// SPI operations).
	pinMode(SS, OUTPUT);

	// Warning: if the SS pin ever becomes a LOW INPUT then SPI
	// automatically switches to Slave, so the data direction of
	// the SS pin MUST be kept as OUTPUT.
	SPCR |= _BV(MSTR);
	SPCR |= _BV(SPE);

	// Set direction register for SCK and MOSI pin.
	// MISO pin automatically overrides to INPUT.
	// By doing this AFTER enabling SPI, we avoid accidentally
	// clocking in a single bit since the lines go directly
	// from "input" to SPI control.
	// http://code.google.com/p/arduino/issues/detail?id=888
	pinMode(SCK, OUTPUT);
	pinMode(MOSI, OUTPUT);
}

void SPIClass::end() {
	SPCR &= ~_BV(SPE);
}

// mapping of interrupt numbers to bits within SPI_AVR_EIMSK
#if defined(__AVR_ATmega32U4__)
  #define SPI_INT0_MASK	 (1<<INT0)
  #define SPI_INT1_MASK	 (1<<INT1)
  #define SPI_INT2_MASK	 (1<<INT2)
  #define SPI_INT3_MASK	 (1<<INT3)
  #define SPI_INT4_MASK	 (1<<INT6)
#elif defined(__AVR_AT90USB646__) || defined(__AVR_AT90USB1286__)
  #define SPI_INT0_MASK	 (1<<INT0)
  #define SPI_INT1_MASK	 (1<<INT1)
  #define SPI_INT2_MASK	 (1<<INT2)
  #define SPI_INT3_MASK	 (1<<INT3)
  #define SPI_INT4_MASK	 (1<<INT4)
  #define SPI_INT5_MASK	 (1<<INT5)
  #define SPI_INT6_MASK	 (1<<INT6)
  #define SPI_INT7_MASK	 (1<<INT7)
#elif defined(EICRA) && defined(EICRB) && defined(EIMSK)
  #define SPI_INT0_MASK	 (1<<INT4)
  #define SPI_INT1_MASK	 (1<<INT5)
  #define SPI_INT2_MASK	 (1<<INT0)
  #define SPI_INT3_MASK	 (1<<INT1)
  #define SPI_INT4_MASK	 (1<<INT2)
  #define SPI_INT5_MASK	 (1<<INT3)
  #define SPI_INT6_MASK	 (1<<INT6)
  #define SPI_INT7_MASK	 (1<<INT7)
#else
  #ifdef INT0
  #define SPI_INT0_MASK	 (1<<INT0)
  #endif
  #ifdef INT1
  #define SPI_INT1_MASK	 (1<<INT1)
  #endif
  #ifdef INT2
  #define SPI_INT2_MASK	 (1<<INT2)
  #endif
#endif

void SPIClass::usingInterrupt(uint8_t interruptNumber)
{
	uint8_t stmp, mask;

	if (interruptMode > 1) return;

	stmp = SREG;
	noInterrupts();
	switch (interruptNumber) {
	#ifdef SPI_INT0_MASK
	case 0: mask = SPI_INT0_MASK; break;
	#endif
	#ifdef SPI_INT1_MASK
	case 1: mask = SPI_INT1_MASK; break;
	#endif
	#ifdef SPI_INT2_MASK
	case 2: mask = SPI_INT2_MASK; break;
	#endif
	#ifdef SPI_INT3_MASK
	case 3: mask = SPI_INT3_MASK; break;
	#endif
	#ifdef SPI_INT4_MASK
	case 4: mask = SPI_INT4_MASK; break;
	#endif
	#ifdef SPI_INT5_MASK
	case 5: mask = SPI_INT5_MASK; break;
	#endif
	#ifdef SPI_INT6_MASK
	case 6: mask = SPI_INT6_MASK; break;
	#endif
	#ifdef SPI_INT7_MASK
	case 7: mask = SPI_INT7_MASK; break;
	#endif
	default:
		interruptMode = 2;
		SREG = stmp;
		return;
	}
	interruptMode = 1;
	interruptMask |= mask;
	SREG = stmp;
}


/**********************************************************/
/*     32 bit Teensy 3.x				  */
/**********************************************************/

#elif defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISK)


#if defined(__MK20DX128__) || defined(__MK20DX256__)
void _spi_dma_rxISR0(void) {/*SPI.dma_rxisr();*/}
const SPIClass::SPI_Hardware_t SPIClass::spi0_hardware = {
	SIM_SCGC6, SIM_SCGC6_SPI0, 4, IRQ_SPI0,
	32767, DMAMUX_SOURCE_SPI0_TX, DMAMUX_SOURCE_SPI0_RX,
	_spi_dma_rxISR0,
	12, 8,
	2, 2,
	11, 7,
	2, 2,
	13, 14,
	2, 2,
	10, 2, 9, 6, 20, 23, 21, 22, 15,
	2,  2, 2,  2,  2,  2,  2,  2,  2,
	0x1, 0x1, 0x2, 0x2, 0x4, 0x4, 0x8, 0x8, 0x10
};
SPIClass SPI((uintptr_t)&KINETISK_SPI0, (uintptr_t)&SPIClass::spi0_hardware);

#elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
void _spi_dma_rxISR0(void) {/*SPI.dma_rxisr();*/}
void _spi_dma_rxISR1(void) {/*SPI1.dma_rxisr();*/}
void _spi_dma_rxISR2(void) {/*SPI2.dma_rxisr();*/}
const SPIClass::SPI_Hardware_t SPIClass::spi0_hardware = {
	SIM_SCGC6, SIM_SCGC6_SPI0, 4, IRQ_SPI0,
	32767, DMAMUX_SOURCE_SPI0_TX, DMAMUX_SOURCE_SPI0_RX,
	_spi_dma_rxISR0,
	12, 8, 39, 255,
	2, 2, 2, 0,
	11, 7, 28, 255,
	2, 2, 2, 0,
	13, 14, 27,
	2, 2, 2,
	10, 2, 9, 6, 20, 23, 21, 22, 15, 26, 45,
	2,  2, 2,  2,  2,  2,  2,  2,  2,   2,   3,
	0x1, 0x1, 0x2, 0x2, 0x4, 0x4, 0x8, 0x8, 0x10, 0x1, 0x20
};
const SPIClass::SPI_Hardware_t SPIClass::spi1_hardware = {
	SIM_SCGC6, SIM_SCGC6_SPI1, 1, IRQ_SPI1,
	#if defined(__MK66FX1M0__)
	32767, DMAMUX_SOURCE_SPI1_TX, DMAMUX_SOURCE_SPI1_RX,
	#else
	// T3.5 does not have good DMA support on 1 and 2
	511, 0, DMAMUX_SOURCE_SPI1,
	#endif
	_spi_dma_rxISR1,
	1, 5, 61, 59,
	2, 7, 2, 7,
	0, 21, 61, 59,
	2, 7, 7, 2,
	32, 20, 60,
	2, 7, 2,
	6, 31, 58, 62, 63, 255, 255, 255, 255, 255, 255,
	7,  2,  2,  2,  2,  0,  0,  0,  0,   0,   0,
	0x1, 0x1, 0x2, 0x1, 0x4, 0, 0, 0, 0, 0, 0
};
const SPIClass::SPI_Hardware_t SPIClass::spi2_hardware = {
	SIM_SCGC3, SIM_SCGC3_SPI2, 1, IRQ_SPI2,
	#if defined(__MK66FX1M0__)
	32767, DMAMUX_SOURCE_SPI2_TX, DMAMUX_SOURCE_SPI2_RX,
	#else
	// T3.5 does not have good DMA support on 1 and 2
	511, 0, DMAMUX_SOURCE_SPI2,
	#endif
	_spi_dma_rxISR2,
	45, 51, 255, 255,
	2, 2, 0, 0,
	44, 52, 255, 255,
	2, 2, 0, 0,
	46, 53, 255,
	2, 2, 0,
	43, 54, 55, 255, 255, 255, 255, 255, 255, 255, 255,
	2,  2,  2,  0,  0,  0,  0,  0,  0,   0,   0,
	0x1, 0x2, 0x1, 0, 0, 0, 0, 0, 0, 0, 0
};
SPIClass SPI((uintptr_t)&KINETISK_SPI0, (uintptr_t)&SPIClass::spi0_hardware);
SPIClass SPI1((uintptr_t)&KINETISK_SPI1, (uintptr_t)&SPIClass::spi1_hardware);
SPIClass SPI2((uintptr_t)&KINETISK_SPI2, (uintptr_t)&SPIClass::spi2_hardware);
#endif


void SPIClass::begin()
{
	volatile uint32_t *reg;

	hardware().clock_gate_register |= hardware().clock_gate_mask;
	port().MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
	port().CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	port().CTAR1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	port().MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
	reg = portConfigRegister(hardware().mosi_pin[mosi_pin_index]);
	*reg = PORT_PCR_MUX(hardware().mosi_mux[mosi_pin_index]);
	reg = portConfigRegister(hardware().miso_pin[miso_pin_index]);
	*reg= PORT_PCR_MUX(hardware().miso_mux[miso_pin_index]);
	reg = portConfigRegister(hardware().sck_pin[sck_pin_index]);
	*reg = PORT_PCR_MUX(hardware().sck_mux[sck_pin_index]);
}

void SPIClass::end()
{
	volatile uint32_t *reg;

	reg = portConfigRegister(hardware().mosi_pin[mosi_pin_index]);
	*reg = 0;
	reg = portConfigRegister(hardware().miso_pin[miso_pin_index]);
	*reg = 0;
	reg = portConfigRegister(hardware().sck_pin[sck_pin_index]);
	*reg = 0;
	port().MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
}

void SPIClass::usingInterrupt(IRQ_NUMBER_t interruptName)
{
	uint32_t n = (uint32_t)interruptName;

	if (n >= NVIC_NUM_INTERRUPTS) return;

	//Serial.print("usingInterrupt ");
	//Serial.println(n);
	interruptMasksUsed |= (1 << (n >> 5));
	interruptMask[n >> 5] |= (1 << (n & 0x1F));
	//Serial.printf("interruptMasksUsed = %d\n", interruptMasksUsed);
	//Serial.printf("interruptMask[0] = %08X\n", interruptMask[0]);
	//Serial.printf("interruptMask[1] = %08X\n", interruptMask[1]);
	//Serial.printf("interruptMask[2] = %08X\n", interruptMask[2]);
}

void SPIClass::notUsingInterrupt(IRQ_NUMBER_t interruptName)
{
	uint32_t n = (uint32_t)interruptName;
	if (n >= NVIC_NUM_INTERRUPTS) return;
	interruptMask[n >> 5] &= ~(1 << (n & 0x1F));
	if (interruptMask[n >> 5] == 0) {
		interruptMasksUsed &= ~(1 << (n >> 5));
	}
}

const uint16_t SPISettings::ctar_div_table[23] = {
	2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40,
	56, 64, 96, 128, 192, 256, 384, 512, 640, 768
};
const uint32_t SPISettings::ctar_clock_table[23] = {
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_DBR | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(0) | SPI_CTAR_DBR | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(2) | SPI_CTAR_BR(0) | SPI_CTAR_DBR | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1),
	SPI_CTAR_PBR(2) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2),
	SPI_CTAR_PBR(2) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(0),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(4) | SPI_CTAR_CSSCK(3),
	SPI_CTAR_PBR(2) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2),
	SPI_CTAR_PBR(3) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(5) | SPI_CTAR_CSSCK(4),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(5) | SPI_CTAR_CSSCK(4),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(6) | SPI_CTAR_CSSCK(5),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(6) | SPI_CTAR_CSSCK(5),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(7) | SPI_CTAR_CSSCK(6),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(7) | SPI_CTAR_CSSCK(6),
	SPI_CTAR_PBR(0) | SPI_CTAR_BR(8) | SPI_CTAR_CSSCK(7),
	SPI_CTAR_PBR(2) | SPI_CTAR_BR(7) | SPI_CTAR_CSSCK(6),
	SPI_CTAR_PBR(1) | SPI_CTAR_BR(8) | SPI_CTAR_CSSCK(7)
};

void SPIClass::updateCTAR(uint32_t ctar)
{
	if (port().CTAR0 != ctar) {
		uint32_t mcr = port().MCR;
		if (mcr & SPI_MCR_MDIS) {
			port().CTAR0 = ctar;
			port().CTAR1 = ctar | SPI_CTAR_FMSZ(8);
		} else {
			port().MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
			port().CTAR0 = ctar;
			port().CTAR1 = ctar | SPI_CTAR_FMSZ(8);
			port().MCR = mcr;
		}
	}
}

void SPIClass::setBitOrder(uint8_t bitOrder)
{
	hardware().clock_gate_register |= hardware().clock_gate_mask;
	uint32_t ctar = port().CTAR0;
	if (bitOrder == LSBFIRST) {
		ctar |= SPI_CTAR_LSBFE;
	} else {
		ctar &= ~SPI_CTAR_LSBFE;
	}
	updateCTAR(ctar);
}

void SPIClass::setDataMode(uint8_t dataMode)
{
	hardware().clock_gate_register |= hardware().clock_gate_mask;
	//uint32_t ctar = port().CTAR0;

	// TODO: implement with native code
	//SPCR = (SPCR & ~SPI_MODE_MASK) | dataMode;
}

void SPIClass::setClockDivider_noInline(uint32_t clk)
{
	hardware().clock_gate_register |= hardware().clock_gate_mask;
	uint32_t ctar = port().CTAR0;
	ctar &= (SPI_CTAR_CPOL | SPI_CTAR_CPHA | SPI_CTAR_LSBFE);
	if (ctar & SPI_CTAR_CPHA) {
		clk = (clk & 0xFFFF0FFF) | ((clk & 0xF000) >> 4);
	}
	ctar |= clk;
	updateCTAR(ctar);
}

uint8_t SPIClass::pinIsChipSelect(uint8_t pin)
{
	for (unsigned int i = 0; i < sizeof(hardware().cs_pin); i++) {
		if (pin == hardware().cs_pin[i]) return hardware().cs_mask[i];
	}
	return 0;
}

bool SPIClass::pinIsChipSelect(uint8_t pin1, uint8_t pin2)
{
	uint8_t pin1_mask, pin2_mask;
	if ((pin1_mask = (uint8_t)pinIsChipSelect(pin1)) == 0) return false;
	if ((pin2_mask = (uint8_t)pinIsChipSelect(pin2)) == 0) return false;
	//Serial.printf("pinIsChipSelect %d %d %x %x\n\r", pin1, pin2, pin1_mask, pin2_mask);
	if ((pin1_mask & pin2_mask) != 0) return false;
	return true;
}

bool SPIClass::pinIsMOSI(uint8_t pin)
{
	for (unsigned int i = 0; i < sizeof(hardware().mosi_pin); i++) {
		if (pin == hardware().mosi_pin[i]) return true;
	}
	return false;
}

bool SPIClass::pinIsMISO(uint8_t pin)
{
	for (unsigned int i = 0; i < sizeof(hardware().miso_pin); i++) {
		if (pin == hardware().miso_pin[i]) return true;
	}
	return false;
}

bool SPIClass::pinIsSCK(uint8_t pin)
{
	for (unsigned int i = 0; i < sizeof(hardware().sck_pin); i++) {
		if (pin == hardware().sck_pin[i]) return true;
	}
	return false;
}

// setCS() is not intended for use from normal Arduino programs/sketches.
uint8_t SPIClass::setCS(uint8_t pin)
{
	for (unsigned int i = 0; i < sizeof(hardware().cs_pin); i++) {
		if (pin == hardware().cs_pin[i]) {
			volatile uint32_t *reg = portConfigRegister(pin);
			*reg = PORT_PCR_MUX(hardware().cs_mux[i]);
			return hardware().cs_mask[i];
		}
	}
	return 0;
}

void SPIClass::setMOSI(uint8_t pin)
{
	if (pin != hardware().mosi_pin[mosi_pin_index]) {
		for (unsigned int i = 0; i < sizeof(hardware().mosi_pin); i++) {
			if  (pin == hardware().mosi_pin[i]) {
				mosi_pin_index = i;
				return;
			}
		}
	}
}

void SPIClass::setMISO(uint8_t pin)
{
	if (pin != hardware().miso_pin[miso_pin_index]) {
		for (unsigned int i = 0; i < sizeof(hardware().miso_pin); i++) {
			if  (pin == hardware().miso_pin[i]) {
				miso_pin_index = i;
				return;
			}
		}
	}
}

void SPIClass::setSCK(uint8_t pin)
{
	if (pin != hardware().sck_pin[sck_pin_index]) {
		for (unsigned int i = 0; i < sizeof(hardware().sck_pin); i++) {
			if  (pin == hardware().sck_pin[i]) {
				sck_pin_index = i;
				return;
			}
		}
	}
}

void SPIClass::transfer(void *buf, size_t count)
{
	if (count == 0) return;
	uint8_t *p_write = (uint8_t *)buf;
	uint8_t *p_read = p_write;
	size_t count_read = count;
	bool lsbfirst = (port().CTAR0 & SPI_CTAR_LSBFE) ? true : false;
	uint32_t sr, full_mask;

	// Lets clear the reader queue
	port().MCR = SPI_MCR_MSTR | SPI_MCR_CLR_RXF | SPI_MCR_PCSIS(0x1F);

	// Now lets loop while we still have data to output
	if (count & 1) {
		if (count > 1)
			port().PUSHR = *p_write++ | SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0);
		else
			port().PUSHR = *p_write++ | SPI_PUSHR_CTAS(0);
		count--;
	}

	full_mask = (hardware().queue_size-1) << 12;
	while (count > 0) {
		// Push out the next byte
		uint16_t w = (*p_write++) << 8;
		w |= *p_write++;
		if (lsbfirst) w = __builtin_bswap16(w);
		if (count == 2)
			port().PUSHR = w | SPI_PUSHR_CTAS(1);
		else	
			port().PUSHR = w | SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1);
		count -= 2; // how many bytes to output.
		// Make sure queue is not full before pushing next byte out
		do {
			sr = port().SR;
			if (sr & 0xF0)  {
				uint16_t w = port().POPR;  // Read any pending RX bytes in
				if (count_read & 1) {
					*p_read++ = w;  // Read any pending RX bytes in
					count_read--;
				} else {
					if (lsbfirst) w = __builtin_bswap16(w);
					*p_read++ = w >> 8;
					*p_read++ = (w & 0xff);
					count_read -= 2;
				}
			}
		} while ((sr & (15 << 12)) > full_mask);
	}

	// now lets wait for all of the read bytes to be returned...
	while (count_read) {
		sr = port().SR;
		if (sr & 0xF0)  {
			uint16_t w = port().POPR;  // Read any pending RX bytes in
			if (count_read & 1) {
				*p_read++ = w;  // Read any pending RX bytes in
				count_read--;
			} else {
				if (lsbfirst) w = __builtin_bswap16(w);
				*p_read++ = w >> 8;
				*p_read++ = (w & 0xff);
				count_read -= 2;
			}
		}
	}
}


/**********************************************************/
/*     32 bit Teensy-LC                                   */
/**********************************************************/

#elif defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISL)

void _spi_dma_rxISR0(void) {/*SPI.dma_rxisr();*/}
const SPIClass::SPI_Hardware_t SPIClass::spi0_hardware = {
	SIM_SCGC4, SIM_SCGC4_SPI0,
	0, // BR index 0
	DMAMUX_SOURCE_SPI0_TX, DMAMUX_SOURCE_SPI0_RX, _spi_dma_rxISR0,
	12, 8,
	2, 2,
	11, 7,
	2, 2,
	13, 14,
	2, 2,
	10, 2,
	2,  2,
	0x1, 0x1
};
SPIClass SPI((uintptr_t)&KINETISL_SPI0, (uintptr_t)&SPIClass::spi0_hardware);

void _spi_dma_rxISR1(void) {/*SPI1.dma_rxisr();*/}
const SPIClass::SPI_Hardware_t SPIClass::spi1_hardware = {
	SIM_SCGC4, SIM_SCGC4_SPI1,
	1, // BR index 1 in SPI Settings
	DMAMUX_SOURCE_SPI1_TX, DMAMUX_SOURCE_SPI1_RX, _spi_dma_rxISR1,
	1, 5,
	2, 2,
	0, 21,
	2, 2,
	20, 255,
	2, 0,
	6, 255,
	2,  0,
	0x1, 0
};
SPIClass SPI1((uintptr_t)&KINETISL_SPI1, (uintptr_t)&SPIClass::spi1_hardware);


void SPIClass::begin()
{
	volatile uint32_t *reg;

	hardware().clock_gate_register |= hardware().clock_gate_mask;
	port().C1 = SPI_C1_SPE | SPI_C1_MSTR;
	port().C2 = 0;
	uint8_t tmp __attribute__((unused)) = port().S;
	reg = portConfigRegister(hardware().mosi_pin[mosi_pin_index]);
	*reg = PORT_PCR_MUX(hardware().mosi_mux[mosi_pin_index]);
	reg = portConfigRegister(hardware().miso_pin[miso_pin_index]);
	*reg = PORT_PCR_MUX(hardware().miso_mux[miso_pin_index]);
	reg = portConfigRegister(hardware().sck_pin[sck_pin_index]);
	*reg = PORT_PCR_MUX(hardware().sck_mux[sck_pin_index]);
}

void SPIClass::end() {
	volatile uint32_t *reg;

	reg = portConfigRegister(hardware().mosi_pin[mosi_pin_index]);
	*reg = 0;
	reg = portConfigRegister(hardware().miso_pin[miso_pin_index]);
	*reg = 0;
	reg = portConfigRegister(hardware().sck_pin[sck_pin_index]);
	*reg = 0;
	port().C1 = 0;
}

const uint16_t SPISettings::br_div_table[30] = {
	2, 4, 6, 8, 10, 12, 14, 16, 20, 24,
	28, 32, 40, 48, 56, 64, 80, 96, 112, 128,
	160, 192, 224, 256, 320, 384, 448, 512, 640, 768,
};

const uint8_t SPISettings::br_clock_table[30] = {
	SPI_BR_SPPR(0) | SPI_BR_SPR(0),
	SPI_BR_SPPR(1) | SPI_BR_SPR(0),
	SPI_BR_SPPR(2) | SPI_BR_SPR(0),
	SPI_BR_SPPR(3) | SPI_BR_SPR(0),
	SPI_BR_SPPR(4) | SPI_BR_SPR(0),
	SPI_BR_SPPR(5) | SPI_BR_SPR(0),
	SPI_BR_SPPR(6) | SPI_BR_SPR(0),
	SPI_BR_SPPR(7) | SPI_BR_SPR(0),
	SPI_BR_SPPR(4) | SPI_BR_SPR(1),
	SPI_BR_SPPR(5) | SPI_BR_SPR(1),
	SPI_BR_SPPR(6) | SPI_BR_SPR(1),
	SPI_BR_SPPR(7) | SPI_BR_SPR(1),
	SPI_BR_SPPR(4) | SPI_BR_SPR(2),
	SPI_BR_SPPR(5) | SPI_BR_SPR(2),
	SPI_BR_SPPR(6) | SPI_BR_SPR(2),
	SPI_BR_SPPR(7) | SPI_BR_SPR(2),
	SPI_BR_SPPR(4) | SPI_BR_SPR(3),
	SPI_BR_SPPR(5) | SPI_BR_SPR(3),
	SPI_BR_SPPR(6) | SPI_BR_SPR(3),
	SPI_BR_SPPR(7) | SPI_BR_SPR(3),
	SPI_BR_SPPR(4) | SPI_BR_SPR(4),
	SPI_BR_SPPR(5) | SPI_BR_SPR(4),
	SPI_BR_SPPR(6) | SPI_BR_SPR(4),
	SPI_BR_SPPR(7) | SPI_BR_SPR(4),
	SPI_BR_SPPR(4) | SPI_BR_SPR(5),
	SPI_BR_SPPR(5) | SPI_BR_SPR(5),
	SPI_BR_SPPR(6) | SPI_BR_SPR(5),
	SPI_BR_SPPR(7) | SPI_BR_SPR(5),
	SPI_BR_SPPR(4) | SPI_BR_SPR(6),
	SPI_BR_SPPR(5) | SPI_BR_SPR(6)
};

void SPIClass::setMOSI(uint8_t pin)
{
	if (pin != hardware().mosi_pin[mosi_pin_index]) {
		for (unsigned int i = 0; i < sizeof(hardware().mosi_pin); i++) {
			if (pin == hardware().mosi_pin[i] ) {
				mosi_pin_index = i;
				return;
			}
		}
	}
}

void SPIClass::setMISO(uint8_t pin)
{
	if (pin != hardware().miso_pin[miso_pin_index]) {
		for (unsigned int i = 0; i < sizeof(hardware().miso_pin); i++) {
			if (pin == hardware().miso_pin[i] ) {
				miso_pin_index = i;
				return;
			}
		}
	}
}

void SPIClass::setSCK(uint8_t pin)
{
	if (pin != hardware().sck_pin[sck_pin_index]) {
		for (unsigned int i = 0; i < sizeof(hardware().sck_pin); i++) {
			if (pin == hardware().sck_pin[i] ) {
				sck_pin_index = i;
				return;
			}
		}
	}
}

bool SPIClass::pinIsChipSelect(uint8_t pin)
{
	for (unsigned int i = 0; i < sizeof(hardware().cs_pin); i++) {
		if (pin == hardware().cs_pin[i]) return hardware().cs_mask[i];
	}
	return 0;
}

bool SPIClass::pinIsMOSI(uint8_t pin)
{
	for (unsigned int i = 0; i < sizeof(hardware().mosi_pin); i++) {
		if (pin == hardware().mosi_pin[i]) return true;
	}
	return false;
}

bool SPIClass::pinIsMISO(uint8_t pin)
{
	for (unsigned int i = 0; i < sizeof(hardware().miso_pin); i++) {
		if (pin == hardware().miso_pin[i]) return true;
	}
	return false;
}

bool SPIClass::pinIsSCK(uint8_t pin)
{
	for (unsigned int i = 0; i < sizeof(hardware().sck_pin); i++) {
		if (pin == hardware().sck_pin[i]) return true;
	}
	return false;
}

// setCS() is not intended for use from normal Arduino programs/sketches.
uint8_t SPIClass::setCS(uint8_t pin)
{
	for (unsigned int i = 0; i < sizeof(hardware().cs_pin); i++) {
		if  (pin == hardware().cs_pin[i]) {
			volatile uint32_t *reg = portConfigRegister(pin);
			*reg = PORT_PCR_MUX(hardware().cs_mux[i]);
			return hardware().cs_mask[i];
		}
	}
	return 0;
}




#endif

