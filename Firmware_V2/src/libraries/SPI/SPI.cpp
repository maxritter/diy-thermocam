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
uint8_t SPIClass::_transferWriteFill = 0;


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

void SPIClass::transfer(const void * buf, void * retbuf, uint32_t count) {
	if (count == 0) return;

	const uint8_t *p = (const uint8_t *)buf;
	uint8_t *pret = (uint8_t *)retbuf;
	uint8_t in;

	uint8_t out = p ? *p++ : _transferWriteFill;
	SPDR = out;
	while (--count > 0) {
		if (p) {
			out = *p++;
		}
		while (!(SPSR & _BV(SPIF))) ;
		in = SPDR;
		SPDR = out;
		if (pret)*pret++ = in;
	}
	while (!(SPSR & _BV(SPIF))) ;
	in = SPDR;
	if (pret)*pret = in;
}


/**********************************************************/
/*     32 bit Teensy 3.x				  */
/**********************************************************/

#elif defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISK)
#if defined(KINETISK) && defined( SPI_HAS_TRANSFER_ASYNC)

#ifndef TRANSFER_COUNT_FIXED
inline void DMAChanneltransferCount(DMAChannel * dmac, unsigned int len) {
	// note does no validation of length...
	DMABaseClass::TCD_t *tcd = dmac->TCD;
	if (!(tcd->BITER & DMA_TCD_BITER_ELINK)) {
		tcd->BITER = len & 0x7fff;
	} else {
		tcd->BITER = (tcd->BITER & 0xFE00) | (len & 0x1ff);
	}
	tcd->CITER = tcd->BITER; 
}
#else 
inline void DMAChanneltransferCount(DMAChannel * dmac, unsigned int len) {
	dmac->transferCount(len);
}
#endif
#endif


#if defined(__MK20DX128__) || defined(__MK20DX256__)
#ifdef SPI_HAS_TRANSFER_ASYNC
void _spi_dma_rxISR0(void) {SPI.dma_rxisr();}
#else
void _spi_dma_rxISR0(void) {;}
#endif

const SPIClass::SPI_Hardware_t SPIClass::spi0_hardware = {
	SIM_SCGC6, SIM_SCGC6_SPI0, 4, IRQ_SPI0,
	32767, DMAMUX_SOURCE_SPI0_TX, DMAMUX_SOURCE_SPI0_RX,
	_spi_dma_rxISR0,
	12, 8,
	PORT_PCR_MUX(2), PORT_PCR_MUX(2),
	11, 7,
	PORT_PCR_DSE | PORT_PCR_MUX(2), PORT_PCR_MUX(2),
	13, 14,
	PORT_PCR_DSE | PORT_PCR_MUX(2), PORT_PCR_MUX(2),
	10, 2, 9, 6, 20, 23, 21, 22, 15,
	PORT_PCR_MUX(2),  PORT_PCR_MUX(2), PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),
	0x1, 0x1, 0x2, 0x2, 0x4, 0x4, 0x8, 0x8, 0x10
};
SPIClass SPI((uintptr_t)&KINETISK_SPI0, (uintptr_t)&SPIClass::spi0_hardware);

#elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
#ifdef SPI_HAS_TRANSFER_ASYNC
void _spi_dma_rxISR0(void) {SPI.dma_rxisr();}
void _spi_dma_rxISR1(void) {SPI1.dma_rxisr();}
void _spi_dma_rxISR2(void) {SPI2.dma_rxisr();}
#else
void _spi_dma_rxISR0(void) {;}
void _spi_dma_rxISR1(void) {;}
void _spi_dma_rxISR2(void) {;}
#endif
const SPIClass::SPI_Hardware_t SPIClass::spi0_hardware = {
	SIM_SCGC6, SIM_SCGC6_SPI0, 4, IRQ_SPI0,
	32767, DMAMUX_SOURCE_SPI0_TX, DMAMUX_SOURCE_SPI0_RX,
	_spi_dma_rxISR0,
	12, 8, 39, 255,
	PORT_PCR_MUX(2), PORT_PCR_MUX(2), PORT_PCR_MUX(2), 0,
	11, 7, 28, 255,
	PORT_PCR_MUX(2), PORT_PCR_MUX(2), PORT_PCR_MUX(2), 0,
	13, 14, 27,
	PORT_PCR_MUX(2), PORT_PCR_MUX(2), PORT_PCR_MUX(2),
	10, 2, 9, 6, 20, 23, 21, 22, 15, 26, 45,
	PORT_PCR_MUX(2),  PORT_PCR_MUX(2), PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),   PORT_PCR_MUX(2),   PORT_PCR_MUX(3),
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
	PORT_PCR_MUX(2), PORT_PCR_MUX(7), PORT_PCR_MUX(2), PORT_PCR_MUX(7),
	0, 21, 61, 59,
	PORT_PCR_MUX(2), PORT_PCR_MUX(7), PORT_PCR_MUX(7), PORT_PCR_MUX(2),
	32, 20, 60,
	PORT_PCR_MUX(2), PORT_PCR_MUX(7), PORT_PCR_MUX(2),
	6, 31, 58, 62, 63, 255, 255, 255, 255, 255, 255,
	PORT_PCR_MUX(7),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  0,  0,  0,  0,   0,   0,
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
	PORT_PCR_MUX(2), PORT_PCR_MUX(2), 0, 0,
	44, 52, 255, 255,
	PORT_PCR_MUX(2), PORT_PCR_MUX(2), 0, 0,
	46, 53, 255,
	PORT_PCR_MUX(2), PORT_PCR_MUX(2), 0,
	43, 54, 55, 255, 255, 255, 255, 255, 255, 255, 255,
	PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  PORT_PCR_MUX(2),  0,  0,  0,  0,  0,  0,   0,   0,
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
	*reg = hardware().mosi_mux[mosi_pin_index];
	reg = portConfigRegister(hardware().miso_pin[miso_pin_index]);
	*reg= hardware().miso_mux[miso_pin_index];
	reg = portConfigRegister(hardware().sck_pin[sck_pin_index]);
	*reg = hardware().sck_mux[sck_pin_index];
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
			*reg = hardware().cs_mux[i];
			return hardware().cs_mask[i];
		}
	}
	return 0;
}

void SPIClass::setMOSI(uint8_t pin)
{
	if (hardware_addr == (uintptr_t)&spi0_hardware) {
		SPCR.setMOSI_soft(pin);
	}
	if (pin != hardware().mosi_pin[mosi_pin_index]) {
		for (unsigned int i = 0; i < sizeof(hardware().mosi_pin); i++) {
			if  (pin == hardware().mosi_pin[i]) {
				if (hardware().clock_gate_register & hardware().clock_gate_mask) {
					volatile uint32_t *reg;
					reg = portConfigRegister(hardware().mosi_pin[mosi_pin_index]);
					*reg = 0;
					reg = portConfigRegister(hardware().mosi_pin[i]);
					*reg = hardware().mosi_mux[i];
				}	
				mosi_pin_index = i;
				return;
			}
		}
	}
}

void SPIClass::setMISO(uint8_t pin)
{
	if (hardware_addr == (uintptr_t)&spi0_hardware) {
		SPCR.setMISO_soft(pin);
	}
	if (pin != hardware().miso_pin[miso_pin_index]) {
		for (unsigned int i = 0; i < sizeof(hardware().miso_pin); i++) {
			if  (pin == hardware().miso_pin[i]) {
				if (hardware().clock_gate_register & hardware().clock_gate_mask) {
					volatile uint32_t *reg;
					reg = portConfigRegister(hardware().miso_pin[miso_pin_index]);
					*reg = 0;
					reg = portConfigRegister(hardware().miso_pin[i]);
					*reg = hardware().miso_mux[i];
				}	
				miso_pin_index = i;
				return;
			}
		}
	}
}

void SPIClass::setSCK(uint8_t pin)
{
	if (hardware_addr == (uintptr_t)&spi0_hardware) {
		SPCR.setSCK_soft(pin);
	}
	if (pin != hardware().sck_pin[sck_pin_index]) {
		for (unsigned int i = 0; i < sizeof(hardware().sck_pin); i++) {
			if  (pin == hardware().sck_pin[i]) {
				if (hardware().clock_gate_register & hardware().clock_gate_mask) {
					volatile uint32_t *reg;
					reg = portConfigRegister(hardware().sck_pin[sck_pin_index]);
					*reg = 0;
					reg = portConfigRegister(hardware().sck_pin[i]);
					*reg = hardware().sck_mux[i];
				}	
				sck_pin_index = i;
				return;
			}
		}
	}
}

void SPIClass::transfer(const void * buf, void * retbuf, size_t count)
{

	if (count == 0) return;
	if (!(port().CTAR0 & SPI_CTAR_LSBFE)) {
		// We are doing the standard MSB order
		const uint8_t *p_write = (const uint8_t *)buf;
		uint8_t *p_read = (uint8_t *)retbuf;
		size_t count_read = count;

		// Lets clear the reader queue
		port().MCR = SPI_MCR_MSTR | SPI_MCR_CLR_RXF | SPI_MCR_PCSIS(0x1F);

		uint32_t sr;

		// Now lets loop while we still have data to output
		if (count & 1) {
		    if (p_write) {
				if (count > 1)
					port().PUSHR = *p_write++ | SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0);
				else
					port().PUSHR = *p_write++ | SPI_PUSHR_CTAS(0);
			} else {
				if (count > 1)
					port().PUSHR = _transferWriteFill | SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0);
				else
					port().PUSHR = _transferWriteFill | SPI_PUSHR_CTAS(0);
			}
			count--;
		}

	    uint16_t w =  (uint16_t)(_transferWriteFill << 8) | _transferWriteFill;

		while (count > 0) {
			// Push out the next byte; 
		    if (p_write) {
		    	w = (*p_write++) << 8;
				w |= *p_write++;
		    }
		    uint16_t queue_full_status_mask = (hardware().queue_size-1) << 12;
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
						if (p_read) {
							*p_read++ = w;  // Read any pending RX bytes in
						} 
						count_read--;
					} else {
						if (p_read) {
							*p_read++ = w >> 8;
							*p_read++ = (w & 0xff);
						}
						count_read -= 2;
					}
				}
			} while ((sr & (15 << 12)) > queue_full_status_mask);

		}

		// now lets wait for all of the read bytes to be returned...
		while (count_read) {
			sr = port().SR;
			if (sr & 0xF0)  {
				uint16_t w = port().POPR;  // Read any pending RX bytes in
				if (count_read & 1) {
					if (p_read)
						*p_read++ = w;  // Read any pending RX bytes in
					count_read--;
				} else {
					if (p_read) {
						*p_read++ = w >> 8;
						*p_read++ = (w & 0xff);
					}
					count_read -= 2;
				}
			}
		}
	} else {
		// We are doing the less ofen LSB mode
		const uint8_t *p_write = (const uint8_t *)buf;
		uint8_t *p_read = (uint8_t *)retbuf;
		size_t count_read = count;

		// Lets clear the reader queue
		port().MCR = SPI_MCR_MSTR | SPI_MCR_CLR_RXF | SPI_MCR_PCSIS(0x1F);

		uint32_t sr;

		// Now lets loop while we still have data to output
		if (count & 1) {
		    if (p_write) {
				if (count > 1)
					port().PUSHR = *p_write++ | SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0);
				else
					port().PUSHR = *p_write++ | SPI_PUSHR_CTAS(0);
			} else {
				if (count > 1)
					port().PUSHR = _transferWriteFill | SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0);
				else
					port().PUSHR = _transferWriteFill | SPI_PUSHR_CTAS(0);
			}
			count--;
		}

	    uint16_t w = _transferWriteFill;

		while (count > 0) {
			// Push out the next byte; 
		    if (p_write) {
				w = *p_write++;
		    	w |= ((*p_write++) << 8);
		    }
		    uint16_t queue_full_status_mask = (hardware().queue_size-1) << 12;
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
						if (p_read) {
							*p_read++ = w;  // Read any pending RX bytes in
						} 
						count_read--;
					} else {
						if (p_read) {
							*p_read++ = (w & 0xff);
							*p_read++ = w >> 8;
						}
						count_read -= 2;
					}
				}
			} while ((sr & (15 << 12)) > queue_full_status_mask);

		}

		// now lets wait for all of the read bytes to be returned...
		while (count_read) {
			sr = port().SR;
			if (sr & 0xF0)  {
				uint16_t w = port().POPR;  // Read any pending RX bytes in
				if (count_read & 1) {
					if (p_read)
						*p_read++ = w;  // Read any pending RX bytes in
					count_read--;
				} else {
					if (p_read) {
						*p_read++ = (w & 0xff);
						*p_read++ = w >> 8;
					}
					count_read -= 2;
				}
			}
		}
	}
}
//=============================================================================
// ASYNCH Support
//=============================================================================
//=========================================================================
// Try Transfer using DMA.
//=========================================================================
#ifdef SPI_HAS_TRANSFER_ASYNC
static uint8_t bit_bucket;
#define dontInterruptAtCompletion(dmac) (dmac)->TCD->CSR &= ~DMA_TCD_CSR_INTMAJOR

//=========================================================================
// Init the DMA channels
//=========================================================================
bool SPIClass::initDMAChannels() {
	// Allocate our channels. 
	_dmaTX = new DMAChannel();
	if (_dmaTX == nullptr) {
		return false;
	}

	_dmaRX = new DMAChannel();
	if (_dmaRX == nullptr) {
		delete _dmaTX; // release it
		_dmaTX = nullptr; 
		return false;
	}

	// Let's setup the RX chain
	_dmaRX->disable();
	_dmaRX->source((volatile uint8_t&)port().POPR);
	_dmaRX->disableOnCompletion();
	_dmaRX->triggerAtHardwareEvent(hardware().rx_dma_channel);
	_dmaRX->attachInterrupt(hardware().dma_rxisr);
	_dmaRX->interruptAtCompletion();

	// We may be using settings chain here so lets set it up. 
	// Now lets setup TX chain.  Note if trigger TX is not set
	// we need to have the RX do it for us.
	_dmaTX->disable();
	_dmaTX->destination((volatile uint8_t&)port().PUSHR);
	_dmaTX->disableOnCompletion();

	if (hardware().tx_dma_channel) {
		_dmaTX->triggerAtHardwareEvent(hardware().tx_dma_channel);
	} else {
//		Serial.printf("SPI InitDMA tx triger by RX: %x\n", (uint32_t)_dmaRX);
	    _dmaTX->triggerAtTransfersOf(*_dmaRX);
	}


	_dma_state = DMAState::idle;  // Should be first thing set!
	return true;
}

//=========================================================================
// Main Async Transfer function
//=========================================================================

bool SPIClass::transfer(const void *buf, void *retbuf, size_t count, EventResponderRef event_responder) {
	uint8_t dma_first_byte;
	if (_dma_state == DMAState::notAllocated) {
		if (!initDMAChannels())
			return false;
	}

	if (_dma_state == DMAState::active)
		return false; // already active

	event_responder.clearEvent();	// Make sure it is not set yet
	if (count < 2) {
		// Use non-async version to simplify cases...
		transfer(buf, retbuf, count);
		event_responder.triggerEvent();
		return true;
	}

	// Now handle the cases where the count > then how many we can output in one DMA request
	if (count > hardware().max_dma_count) {
		_dma_count_remaining = count - hardware().max_dma_count;
		count = hardware().max_dma_count;
	} else {
		_dma_count_remaining = 0;
	}

	// Now See if caller passed in a source buffer. 
	_dmaTX->TCD->ATTR_DST = 0;		// Make sure set for 8 bit mode
	uint8_t *write_data = (uint8_t*) buf;
	if (buf) {
		dma_first_byte = *write_data;
		_dmaTX->sourceBuffer((uint8_t*)write_data+1, count-1);  
		_dmaTX->TCD->SLAST = 0;	// Finish with it pointing to next location
	} else {
		dma_first_byte = _transferWriteFill;
		_dmaTX->source((uint8_t&)_transferWriteFill);   // maybe have setable value
		DMAChanneltransferCount(_dmaTX, count-1);
	}	
	if (retbuf) {
		// On T3.5 must handle SPI1/2 differently as only one DMA channel
		_dmaRX->TCD->ATTR_SRC = 0;		//Make sure set for 8 bit mode...
		_dmaRX->destinationBuffer((uint8_t*)retbuf, count);
		_dmaRX->TCD->DLASTSGA = 0;		// At end point after our bufffer
	} else {
			// Write  only mode
		_dmaRX->TCD->ATTR_SRC = 0;		//Make sure set for 8 bit mode...
		_dmaRX->destination((uint8_t&)bit_bucket);
		DMAChanneltransferCount(_dmaRX, count);
	}

	_dma_event_responder = &event_responder;
	// Now try to start it?
	// Setup DMA main object
	yield();
	port().MCR = SPI_MCR_MSTR | SPI_MCR_CLR_RXF | SPI_MCR_CLR_TXF | SPI_MCR_PCSIS(0x1F);

	port().SR = 0xFF0F0000;

	// Lets try to output the first byte to make sure that we are in 8 bit mode...
	port().PUSHR = dma_first_byte | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT;	

	if (hardware().tx_dma_channel) {
		port().RSER =  SPI_RSER_RFDF_RE | SPI_RSER_RFDF_DIRS | SPI_RSER_TFFF_RE | SPI_RSER_TFFF_DIRS;
		_dmaRX->enable();
		// Get the initial settings. 
		_dmaTX->enable();
	} else {
		//T3.5 SP1 and SPI2 - TX is not triggered by SPI but by RX...
		port().RSER =  SPI_RSER_RFDF_RE | SPI_RSER_RFDF_DIRS ;
	    _dmaTX->triggerAtTransfersOf(*_dmaRX);
		_dmaTX->enable();
		_dmaRX->enable();
	}

	_dma_state = DMAState::active;
	return true;
}


//-------------------------------------------------------------------------
// DMA RX ISR
//-------------------------------------------------------------------------
void SPIClass::dma_rxisr(void) {
	_dmaRX->clearInterrupt();
	_dmaTX->clearComplete();
	_dmaRX->clearComplete();

	uint8_t should_reenable_tx = true;	// should we re-enable TX maybe not if count will be 0...
	if (_dma_count_remaining) {
		// What do I need to do to start it back up again...
		// We will use the BITR/CITR from RX as TX may have prefed some stuff
		if (_dma_count_remaining > hardware().max_dma_count) {
			_dma_count_remaining -= hardware().max_dma_count;
		} else {
			DMAChanneltransferCount(_dmaTX, _dma_count_remaining-1);
			DMAChanneltransferCount(_dmaRX, _dma_count_remaining);
			if (_dma_count_remaining == 1) should_reenable_tx = false;

			_dma_count_remaining = 0;
		}
		// In some cases we need to again start the TX manually to get it to work...
		if (_dmaTX->TCD->SADDR == &_transferWriteFill) {
			if (port().CTAR0  & SPI_CTAR_FMSZ(8)) {
				port().PUSHR = (_transferWriteFill | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT);
			} else  {
				port().PUSHR = (_transferWriteFill | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT);
			}
		} else {
			if (port().CTAR0  & SPI_CTAR_FMSZ(8)) {
				// 16 bit mode
				uint16_t w = *((uint16_t*)_dmaTX->TCD->SADDR);
				_dmaTX->TCD->SADDR = (volatile uint8_t*)(_dmaTX->TCD->SADDR) + 2;
				port().PUSHR = (w | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT);
			} else  {
				uint8_t w = *((uint8_t*)_dmaTX->TCD->SADDR);
				_dmaTX->TCD->SADDR = (volatile uint8_t*)(_dmaTX->TCD->SADDR) + 1;
				port().PUSHR = (w | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT);
			}
		}
		_dmaRX->enable();
		if (should_reenable_tx)
			_dmaTX->enable();
	} else {

		port().RSER = 0;
		//port().MCR = SPI_MCR_MSTR | SPI_MCR_CLR_RXF | SPI_MCR_PCSIS(0x1F);  // clear out the queue
		port().SR = 0xFF0F0000;
		port().CTAR0  &= ~(SPI_CTAR_FMSZ(8)); 	// Hack restore back to 8 bits

		_dma_state = DMAState::completed;   // set back to 1 in case our call wants to start up dma again
		_dma_event_responder->triggerEvent();

	}
}
#endif // SPI_HAS_TRANSFER_ASYNC


/**********************************************************/
/*     32 bit Teensy-LC                                   */
/**********************************************************/

#elif defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISL)

#ifdef SPI_HAS_TRANSFER_ASYNC
void _spi_dma_rxISR0(void) {SPI.dma_isr();}
void _spi_dma_rxISR1(void) {SPI1.dma_isr();}
#else
void _spi_dma_rxISR0(void) {;}
void _spi_dma_rxISR1(void) {;}
#endif

const SPIClass::SPI_Hardware_t SPIClass::spi0_hardware = {
	SIM_SCGC4, SIM_SCGC4_SPI0,
	0, // BR index 0
	DMAMUX_SOURCE_SPI0_TX, DMAMUX_SOURCE_SPI0_RX, _spi_dma_rxISR0,
	12, 8,
	PORT_PCR_MUX(2), PORT_PCR_MUX(2),
	11, 7,
	 PORT_PCR_DSE | PORT_PCR_MUX(2), PORT_PCR_MUX(2),
	13, 14,
	 PORT_PCR_DSE | PORT_PCR_MUX(2), PORT_PCR_MUX(2),
	10, 2,
	PORT_PCR_MUX(2),  PORT_PCR_MUX(2),
	0x1, 0x1
};
SPIClass SPI((uintptr_t)&KINETISL_SPI0, (uintptr_t)&SPIClass::spi0_hardware);

const SPIClass::SPI_Hardware_t SPIClass::spi1_hardware = {
	SIM_SCGC4, SIM_SCGC4_SPI1,
	1, // BR index 1 in SPI Settings
	DMAMUX_SOURCE_SPI1_TX, DMAMUX_SOURCE_SPI1_RX, _spi_dma_rxISR1,
	1, 5,
	PORT_PCR_MUX(2), PORT_PCR_MUX(2),
	0, 21,
	PORT_PCR_MUX(2), PORT_PCR_MUX(2),
	20, 255,
	PORT_PCR_MUX(2), 0,
	6, 255,
	PORT_PCR_MUX(2),  0,
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
	*reg = hardware().mosi_mux[mosi_pin_index];
	reg = portConfigRegister(hardware().miso_pin[miso_pin_index]);
	*reg = hardware().miso_mux[miso_pin_index];
	reg = portConfigRegister(hardware().sck_pin[sck_pin_index]);
	*reg = hardware().sck_mux[sck_pin_index];
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
				if (hardware().clock_gate_register & hardware().clock_gate_mask) {
					volatile uint32_t *reg;
					reg = portConfigRegister(hardware().mosi_pin[mosi_pin_index]);
					*reg = 0;
					reg = portConfigRegister(hardware().mosi_pin[i]);
					*reg = hardware().mosi_mux[i];
				}	
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
				if (hardware().clock_gate_register & hardware().clock_gate_mask) {
					volatile uint32_t *reg;
					reg = portConfigRegister(hardware().miso_pin[miso_pin_index]);
					*reg = 0;
					reg = portConfigRegister(hardware().miso_pin[i]);
					*reg = hardware().miso_mux[i];
				}	
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
				if (hardware().clock_gate_register & hardware().clock_gate_mask) {
					volatile uint32_t *reg;
					reg = portConfigRegister(hardware().sck_pin[sck_pin_index]);
					*reg = 0;
					reg = portConfigRegister(hardware().sck_pin[i]);
					*reg = hardware().sck_mux[i];
				}	
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
			*reg = hardware().cs_mux[i];
			return hardware().cs_mask[i];
		}
	}
	return 0;
}

void SPIClass::transfer(const void * buf, void * retbuf, size_t count) {
	if (count == 0) return;
	const uint8_t *p = (const uint8_t *)buf;
	uint8_t *pret = (uint8_t *)retbuf;
	uint8_t in;

	while (!(port().S & SPI_S_SPTEF)) ; // wait
	uint8_t out = p ? *p++ : _transferWriteFill;
	port().DL = out;
	while (--count > 0) {
		if (p) {
			out = *p++;
		}
		while (!(port().S & SPI_S_SPTEF)) ; // wait
		__disable_irq();
		port().DL = out;
		while (!(port().S & SPI_S_SPRF)) ; // wait
		in = port().DL;
		__enable_irq();
		if (pret)*pret++ = in;
	}
	while (!(port().S & SPI_S_SPRF)) ; // wait
	in = port().DL;
	if (pret)*pret = in;
}
//=============================================================================
// ASYNCH Support
//=============================================================================
//=========================================================================
// Try Transfer using DMA.
//=========================================================================
#ifdef SPI_HAS_TRANSFER_ASYNC
static uint8_t      _dma_dummy_rx;

void SPIClass::dma_isr(void) {
	//  Serial.println("_spi_dma_rxISR");
	_dmaRX->clearInterrupt();
	port().C2 = 0;
	uint8_t tmp __attribute__((unused)) = port().S;
	_dmaTX->clearComplete();
	_dmaRX->clearComplete();

	_dma_state = DMAState::completed;   // set back to 1 in case our call wants to start up dma again
	_dma_event_responder->triggerEvent();
}

bool SPIClass::initDMAChannels() {
	//Serial.println("First dma call"); Serial.flush();
	_dmaTX = new DMAChannel();
	if (_dmaTX == nullptr) {
		return false;
	}

	_dmaTX->disable();
	_dmaTX->destination((volatile uint8_t&)port().DL);
	_dmaTX->disableOnCompletion();
	_dmaTX->triggerAtHardwareEvent(hardware().tx_dma_channel);


	_dmaRX = new DMAChannel();
	if (_dmaRX == NULL) {
		delete _dmaTX;
		_dmaRX = nullptr;
		return false;
	}
	_dmaRX->disable();
	_dmaRX->source((volatile uint8_t&)port().DL);
	_dmaRX->disableOnCompletion();
	_dmaRX->triggerAtHardwareEvent(hardware().rx_dma_channel);
	_dmaRX->attachInterrupt(hardware().dma_isr);
	_dmaRX->interruptAtCompletion();

	_dma_state = DMAState::idle;  // Should be first thing set!
	//Serial.println("end First dma call");
	return true;
}

//=========================================================================
// Main Async Transfer function
//=========================================================================
bool SPIClass::transfer(const void *buf, void *retbuf, size_t count, EventResponderRef event_responder) {
	if (_dma_state == DMAState::notAllocated) {
		if (!initDMAChannels()) {
			return false;
		}
	}
  
	if (_dma_state == DMAState::active)
		return false; // already active

	event_responder.clearEvent();	// Make sure it is not set yet

	if (count < 2) {
		// Use non-async version to simplify cases...
		transfer(buf, retbuf, count);
		event_responder.triggerEvent();
		return true;
	}
	//_dmaTX->destination((volatile uint8_t&)port().DL);
	//_dmaRX->source((volatile uint8_t&)port().DL);
	_dmaTX->CFG->DCR = (_dmaTX->CFG->DCR & ~DMA_DCR_DSIZE(3)) | DMA_DCR_DSIZE(1);
	_dmaRX->CFG->DCR = (_dmaRX->CFG->DCR & ~DMA_DCR_SSIZE(3)) | DMA_DCR_SSIZE(1);  // 8 bit transfer

	// Now see if the user passed in TX buffer to send.
	uint8_t first_char;
	if (buf) {
		uint8_t *data_out = (uint8_t*)buf;
		first_char = *data_out++;
		_dmaTX->sourceBuffer(data_out, count-1);
	} else {
		first_char = (_transferWriteFill & 0xff);
		_dmaTX->source((uint8_t&)_transferWriteFill);   // maybe have setable value
		_dmaTX->transferCount(count-1);
	}

	if (retbuf) {
		_dmaRX->destinationBuffer((uint8_t*)retbuf, count);
	} else {
		_dmaRX->destination(_dma_dummy_rx);    // NULL ?
		_dmaRX->transferCount(count);
	}

	_dma_event_responder = &event_responder;

	//Serial.println("Before DMA C2");
	// Try pushing the first character
    while (!(port().S & SPI_S_SPTEF));
    port().DL = first_char;

	port().C2 |= SPI_C2_TXDMAE | SPI_C2_RXDMAE;

	// Now  make sure SPI is enabled. 
	port().C1 |= SPI_C1_SPE;
      
    _dmaRX->enable();
    _dmaTX->enable();
    _dma_state = DMAState::active;
    return true;
}
#endif //SPI_HAS_TRANSFER_ASYNC

#endif
