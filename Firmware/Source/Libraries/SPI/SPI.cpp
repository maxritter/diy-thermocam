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
/*     32 bit Teensy 3.0 and 3.1			  */
/**********************************************************/

#elif defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISK)


SPIClass SPI;

uint8_t SPIClass::interruptMasksUsed = 0;
uint32_t SPIClass::interruptMask[(NVIC_NUM_INTERRUPTS+31)/32];
uint32_t SPIClass::interruptSave[(NVIC_NUM_INTERRUPTS+31)/32];
#ifdef SPI_TRANSACTION_MISMATCH_LED
uint8_t SPIClass::inTransactionFlag = 0;
#endif

void SPIClass::begin()
{
	SIM_SCGC6 |= SIM_SCGC6_SPI0;
	SPI0_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
	SPI0_CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI0_CTAR1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI0_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
	SPCR.enable_pins(); // pins managed by SPCRemulation in avr_emulation.h
}

void SPIClass::end() {
	SPCR.disable_pins();
	SPI0_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
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

static void updateCTAR(uint32_t ctar)
{
	if (SPI0_CTAR0 != ctar) {
		uint32_t mcr = SPI0_MCR;
		if (mcr & SPI_MCR_MDIS) {
			SPI0_CTAR0 = ctar;
			SPI0_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
		} else {
			SPI0_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
			SPI0_CTAR0 = ctar;
			SPI0_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
			SPI0_MCR = mcr;
		}
	}
}

void SPIClass::setBitOrder(uint8_t bitOrder)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI0;
	uint32_t ctar = SPI0_CTAR0;
	if (bitOrder == LSBFIRST) {
		ctar |= SPI_CTAR_LSBFE;
	} else {
		ctar &= ~SPI_CTAR_LSBFE;
	}
	updateCTAR(ctar);
}

void SPIClass::setDataMode(uint8_t dataMode)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI0;

	// TODO: implement with native code

	SPCR = (SPCR & ~SPI_MODE_MASK) | dataMode;
}

void SPIClass::setClockDivider_noInline(uint32_t clk)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI0;
	uint32_t ctar = SPI0_CTAR0;
	ctar &= (SPI_CTAR_CPOL | SPI_CTAR_CPHA | SPI_CTAR_LSBFE);
	if (ctar & SPI_CTAR_CPHA) {
		clk = (clk & 0xFFFF0FFF) | ((clk & 0xF000) >> 4);
	}
	ctar |= clk;
	updateCTAR(ctar);
}

uint8_t SPIClass::pinIsChipSelect(uint8_t pin)
{
	switch (pin) {
	  case 10: return 0x01; // PTC4
	  case 2:  return 0x01; // PTD0
	  case 9:  return 0x02; // PTC3
	  case 6:  return 0x02; // PTD4
	  case 20: return 0x04; // PTD5
	  case 23: return 0x04; // PTC2
	  case 21: return 0x08; // PTD6
	  case 22: return 0x08; // PTC1
	  case 15: return 0x10; // PTC0
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
	  case 26: return 0x01;
	  case 45: return 0x20; // CS5
#endif
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

uint8_t SPIClass::setCS(uint8_t pin)
{
	switch (pin) {
	  case 10: CORE_PIN10_CONFIG = PORT_PCR_MUX(2); return 0x01; // PTC4
	  case 2:  CORE_PIN2_CONFIG  = PORT_PCR_MUX(2); return 0x01; // PTD0
	  case 9:  CORE_PIN9_CONFIG  = PORT_PCR_MUX(2); return 0x02; // PTC3
	  case 6:  CORE_PIN6_CONFIG  = PORT_PCR_MUX(2); return 0x02; // PTD4
	  case 20: CORE_PIN20_CONFIG = PORT_PCR_MUX(2); return 0x04; // PTD5
	  case 23: CORE_PIN23_CONFIG = PORT_PCR_MUX(2); return 0x04; // PTC2
	  case 21: CORE_PIN21_CONFIG = PORT_PCR_MUX(2); return 0x08; // PTD6
	  case 22: CORE_PIN22_CONFIG = PORT_PCR_MUX(2); return 0x08; // PTC1
	  case 15: CORE_PIN15_CONFIG = PORT_PCR_MUX(2); return 0x10; // PTC0
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
	  case 26: CORE_PIN26_CONFIG = PORT_PCR_MUX(2);return 0x01;
	  case 45: CORE_PIN45_CONFIG = PORT_PCR_MUX(3);return 0x20;
#endif
	}
	return 0;
}



/**********************************************************/
/*     32 bit Teensy-3.5/3.6                              */
/**********************************************************/
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
SPI1Class SPI1;

uint8_t SPI1Class::interruptMasksUsed = 0;
uint32_t SPI1Class::interruptMask[(NVIC_NUM_INTERRUPTS+31)/32];
uint32_t SPI1Class::interruptSave[(NVIC_NUM_INTERRUPTS+31)/32];
#ifdef SPI_TRANSACTION_MISMATCH_LED
uint8_t SPI1Class::inTransactionFlag = 0;
#endif

void SPI1Class::begin()
{
	SIM_SCGC6 |= SIM_SCGC6_SPI1;
	SPI1_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
	SPI1_CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI1_CTAR1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI1_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
	SPCR1.enable_pins(); // pins managed by SPCRemulation in avr_emulation.h
}

void SPI1Class::end() {
	SPCR1.disable_pins();
	SPI1_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
}

void SPI1Class::usingInterrupt(IRQ_NUMBER_t interruptName)
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

void SPI1Class::notUsingInterrupt(IRQ_NUMBER_t interruptName)
{
	uint32_t n = (uint32_t)interruptName;
	if (n >= NVIC_NUM_INTERRUPTS) return;
	interruptMask[n >> 5] &= ~(1 << (n & 0x1F));
	if (interruptMask[n >> 5] == 0) {
		interruptMasksUsed &= ~(1 << (n >> 5));
	}
}


static void updateCTAR1(uint32_t ctar)
{
	if (SPI1_CTAR0 != ctar) {
		uint32_t mcr = SPI1_MCR;
		if (mcr & SPI_MCR_MDIS) {
			SPI1_CTAR0 = ctar;
			SPI1_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
		} else {
			SPI1_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
			SPI1_CTAR0 = ctar;
			SPI1_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
			SPI1_MCR = mcr;
		}
	}
}

void SPI1Class::setBitOrder(uint8_t bitOrder)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI1;
	uint32_t ctar = SPI1_CTAR0;
	if (bitOrder == LSBFIRST) {
		ctar |= SPI_CTAR_LSBFE;
	} else {
		ctar &= ~SPI_CTAR_LSBFE;
	}
	updateCTAR1(ctar);
}

void SPI1Class::setDataMode(uint8_t dataMode)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI1;

	// TODO: implement with native code
	SPCR1 = (SPCR1 & ~SPI_MODE_MASK) | dataMode;
}

void SPI1Class::setClockDivider_noInline(uint32_t clk)
{
	SIM_SCGC6 |= SIM_SCGC6_SPI1;
	uint32_t ctar = SPI1_CTAR0;
	ctar &= (SPI_CTAR_CPOL | SPI_CTAR_CPHA | SPI_CTAR_LSBFE);
	if (ctar & SPI_CTAR_CPHA) {
		clk = (clk & 0xFFFF0FFF) | ((clk & 0xF000) >> 4);
	}
	ctar |= clk;
	updateCTAR1(ctar);
}

uint8_t SPI1Class::pinIsChipSelect(uint8_t pin)
{
	switch (pin) {
	  case 6:  return 0x01; // CS0
	  case 31: return 0x01; // CS0
	  case 58: return 0x02;	//CS1
	  case 62: return 0x01;	//CS0
	  case 63: return 0x04;	//CS2
	}
	return 0;
}

bool SPI1Class::pinIsChipSelect(uint8_t pin1, uint8_t pin2)
{
	uint8_t pin1_mask, pin2_mask;
	if ((pin1_mask = (uint8_t)pinIsChipSelect(pin1)) == 0) return false;
	if ((pin2_mask = (uint8_t)pinIsChipSelect(pin2)) == 0) return false;
	//Serial.printf("pinIsChipSelect %d %d %x %x\n\r", pin1, pin2, pin1_mask, pin2_mask);
	if ((pin1_mask & pin2_mask) != 0) return false;
	return true;
}

uint8_t SPI1Class::setCS(uint8_t pin)
{
	switch (pin) {
	  case 6:  CORE_PIN6_CONFIG  = PORT_PCR_MUX(7); return 0x01; // PTD4
	  case 31: CORE_PIN31_CONFIG = PORT_PCR_MUX(2); return 0x01; // PTD5
	  case 58: CORE_PIN58_CONFIG = PORT_PCR_MUX(2); return 0x02;	//CS1
	  case 62: CORE_PIN62_CONFIG = PORT_PCR_MUX(2); return 0x01;	//CS0
	  case 63: CORE_PIN63_CONFIG = PORT_PCR_MUX(2); return 0x04;	//CS2
	}
	return 0;
}

//  SPI2 Class
SPI2Class SPI2;

uint8_t SPI2Class::interruptMasksUsed = 0;
uint32_t SPI2Class::interruptMask[(NVIC_NUM_INTERRUPTS+31)/32];
uint32_t SPI2Class::interruptSave[(NVIC_NUM_INTERRUPTS+31)/32];
#ifdef SPI_TRANSACTION_MISMATCH_LED
uint8_t SPI2Class::inTransactionFlag = 0;
#endif

void SPI2Class::begin()
{
	SIM_SCGC3 |= SIM_SCGC3_SPI2;
	SPI2_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
	SPI2_CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI2_CTAR1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
	SPI2_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
	SPCR2.enable_pins(); // pins managed by SPCRemulation in avr_emulation.h
}

void SPI2Class::end() {
	SPCR2.disable_pins();
	SPI2_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
}

void SPI2Class::usingInterrupt(IRQ_NUMBER_t interruptName)
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

void SPI2Class::notUsingInterrupt(IRQ_NUMBER_t interruptName)
{
	uint32_t n = (uint32_t)interruptName;
	if (n >= NVIC_NUM_INTERRUPTS) return;
	interruptMask[n >> 5] &= ~(1 << (n & 0x1F));
	if (interruptMask[n >> 5] == 0) {
		interruptMasksUsed &= ~(1 << (n >> 5));
	}
}


static void updateCTAR2(uint32_t ctar)
{
	if (SPI2_CTAR0 != ctar) {
		uint32_t mcr = SPI2_MCR;
		if (mcr & SPI_MCR_MDIS) {
			SPI2_CTAR0 = ctar;
			SPI2_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
		} else {
			SPI2_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
			SPI2_CTAR0 = ctar;
			SPI2_CTAR1 = ctar | SPI_CTAR_FMSZ(8);
			SPI2_MCR = mcr;
		}
	}
}

void SPI2Class::setBitOrder(uint8_t bitOrder)
{
	SIM_SCGC3 |= SIM_SCGC3_SPI2;
	uint32_t ctar = SPI2_CTAR0;
	if (bitOrder == LSBFIRST) {
		ctar |= SPI_CTAR_LSBFE;
	} else {
		ctar &= ~SPI_CTAR_LSBFE;
	}
	updateCTAR2(ctar);
}

void SPI2Class::setDataMode(uint8_t dataMode)
{
	SIM_SCGC3 |= SIM_SCGC3_SPI2;

	// TODO: implement with native code
	SPCR2 = (SPCR2 & ~SPI_MODE_MASK) | dataMode;
}

void SPI2Class::setClockDivider_noInline(uint32_t clk)
{
	SIM_SCGC3 |= SIM_SCGC3_SPI2;
	uint32_t ctar = SPI2_CTAR0;
	ctar &= (SPI_CTAR_CPOL | SPI_CTAR_CPHA | SPI_CTAR_LSBFE);
	if (ctar & SPI_CTAR_CPHA) {
		clk = (clk & 0xFFFF0FFF) | ((clk & 0xF000) >> 4);
	}
	ctar |= clk;
	updateCTAR2(ctar);
}

uint8_t SPI2Class::pinIsChipSelect(uint8_t pin)
{
	switch (pin) {
	  case 43:  return 0x01; // CS0
	  case 54: return 0x02; // CS1
	  case 55:  return 0x01; // CS0
	}
	return 0;
}

bool SPI2Class::pinIsChipSelect(uint8_t pin1, uint8_t pin2)
{
	uint8_t pin1_mask, pin2_mask;
	if ((pin1_mask = (uint8_t)pinIsChipSelect(pin1)) == 0) return false;
	if ((pin2_mask = (uint8_t)pinIsChipSelect(pin2)) == 0) return false;
	//Serial.printf("pinIsChipSelect %d %d %x %x\n\r", pin1, pin2, pin1_mask, pin2_mask);
	if ((pin1_mask & pin2_mask) != 0) return false;
	return true;
}

uint8_t SPI2Class::setCS(uint8_t pin)
{
	switch (pin) {
	  case 43: CORE_PIN43_CONFIG = PORT_PCR_MUX(2); return 0x01; // CS0
	  case 54: CORE_PIN54_CONFIG = PORT_PCR_MUX(2); return 0x02; // CS1
	  case 55: CORE_PIN55_CONFIG = PORT_PCR_MUX(2); return 0x01; // CS0
	}
	return 0;
}


#endif

/**********************************************************/
/*     32 bit Teensy-LC                                   */
/**********************************************************/

#elif defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISL)

SPIClass SPI;
SPI1Class SPI1;

uint32_t SPIClass::interruptMask = 0;
uint32_t SPIClass::interruptSave = 0;
uint32_t SPI1Class::interruptMask = 0;
uint32_t SPI1Class::interruptSave = 0;
#ifdef SPI_TRANSACTION_MISMATCH_LED
uint8_t SPIClass::inTransactionFlag = 0;
uint8_t SPI1Class::inTransactionFlag = 0;
#endif

void SPIClass::begin()
{
	SIM_SCGC4 |= SIM_SCGC4_SPI0;
	SPI0_C1 = SPI_C1_SPE | SPI_C1_MSTR;
	SPI0_C2 = 0;
	uint8_t tmp __attribute__((unused)) = SPI0_S;
	SPCR.enable_pins(); // pins managed by SPCRemulation in avr_emulation.h
}

void SPIClass::end() {
	SPCR.disable_pins();
	SPI0_C1 = 0;
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

uint8_t SPIClass::setCS(uint8_t pin)
{
	switch (pin) {
	  case 10: CORE_PIN10_CONFIG = PORT_PCR_MUX(2); return 0x01; // PTC4
	  case 2:  CORE_PIN2_CONFIG  = PORT_PCR_MUX(2); return 0x01; // PTD0
	}
	return 0;
}

void SPI1Class::begin()
{
	SIM_SCGC4 |= SIM_SCGC4_SPI1;
	SPI1_C1 = SPI_C1_SPE | SPI_C1_MSTR;
	SPI1_C2 = 0;
	uint8_t tmp __attribute__((unused)) = SPI1_S;
	SPCR1.enable_pins(); // pins managed by SPCRemulation in avr_emulation.h
}

void SPI1Class::end() {
	SPCR1.disable_pins();
	SPI1_C1 = 0;
}

uint8_t SPI1Class::setCS(uint8_t pin)
{
	switch (pin) {
	  case 6:  CORE_PIN6_CONFIG = PORT_PCR_MUX(2); return 0x01; // PTD4
	}
	return 0;
}




/**********************************************************/
/*     32 bit Arduino Due				  */
/**********************************************************/

#elif defined(__arm__) && defined(__SAM3X8E__)

#include "SPI.h"


SPIClass::SPIClass(Spi *_spi, uint32_t _id, void(*_initCb)(void)) :
	spi(_spi), id(_id), initCb(_initCb), initialized(false)
{
	// Empty
}

void SPIClass::begin() {
	init();
	// NPCS control is left to the user

	// Default speed set to 4Mhz
	setClockDivider(BOARD_SPI_DEFAULT_SS, 21);
	setDataMode(BOARD_SPI_DEFAULT_SS, SPI_MODE0);
	setBitOrder(BOARD_SPI_DEFAULT_SS, MSBFIRST);
}

void SPIClass::begin(uint8_t _pin) {
	init();

	uint32_t spiPin = BOARD_PIN_TO_SPI_PIN(_pin);
	PIO_Configure(
		g_APinDescription[spiPin].pPort,
		g_APinDescription[spiPin].ulPinType,
		g_APinDescription[spiPin].ulPin,
		g_APinDescription[spiPin].ulPinConfiguration);

	// Default speed set to 4Mhz
	setClockDivider(_pin, 21);
	setDataMode(_pin, SPI_MODE0);
	setBitOrder(_pin, MSBFIRST);
}

void SPIClass::init() {
	if (initialized)
		return;
	interruptMode = 0;
	interruptMask = 0;
	interruptSave = 0;
	initCb();
	SPI_Configure(spi, id, SPI_MR_MSTR | SPI_MR_PS | SPI_MR_MODFDIS);
	SPI_Enable(spi);
	initialized = true;
}

#ifndef interruptsStatus
#define interruptsStatus() __interruptsStatus()
static inline unsigned char __interruptsStatus(void) __attribute__((always_inline, unused));
static inline unsigned char __interruptsStatus(void) {
  unsigned int primask;
  asm volatile ("mrs %0, primask" : "=r" (primask));
  if (primask) return 0;
  return 1;
}
#endif

void SPIClass::usingInterrupt(uint8_t interruptNumber)
{
	uint8_t irestore;

	irestore = interruptsStatus();
	noInterrupts();
	if (interruptMode < 2) {
		if (interruptNumber > NUM_DIGITAL_PINS) {
			interruptMode = 2;
		} else {
			uint8_t imask = interruptMask;
			Pio *pio = g_APinDescription[interruptNumber].pPort;
			if (pio == PIOA) {
				imask |= 1;
			} else if (pio == PIOB) {
				imask |= 2;
			} else if (pio == PIOC) {
				imask |= 4;
			} else if (pio == PIOD) {
				imask |= 8;
			}
			interruptMask = imask;
			interruptMode = 1;
		}
	}
	if (irestore) interrupts();
}

void SPIClass::beginTransaction(uint8_t pin, SPISettings settings)
{
	if (interruptMode > 0) {
		if (interruptMode == 1) {
			uint8_t imask = interruptMask;
			if (imask & 1) NVIC_DisableIRQ(PIOA_IRQn);
			if (imask & 2) NVIC_DisableIRQ(PIOB_IRQn);
			if (imask & 4) NVIC_DisableIRQ(PIOC_IRQn);
			if (imask & 8) NVIC_DisableIRQ(PIOD_IRQn);
		} else {
			interruptSave = interruptsStatus();
			noInterrupts();
		}
	}
	uint32_t ch = BOARD_PIN_TO_SPI_CHANNEL(pin);
	bitOrder[ch] = settings.border;
	SPI_ConfigureNPCS(spi, ch, settings.config);
}

void SPIClass::endTransaction(void)
{
	if (interruptMode > 0) {
		if (interruptMode == 1) {
			uint8_t imask = interruptMask;
			if (imask & 1) NVIC_EnableIRQ(PIOA_IRQn);
			if (imask & 2) NVIC_EnableIRQ(PIOB_IRQn);
			if (imask & 4) NVIC_EnableIRQ(PIOC_IRQn);
			if (imask & 8) NVIC_EnableIRQ(PIOD_IRQn);
		} else {
			if (interruptSave) interrupts();
		}
	}
}

void SPIClass::end(uint8_t _pin) {
	uint32_t spiPin = BOARD_PIN_TO_SPI_PIN(_pin);
	// Setting the pin as INPUT will disconnect it from SPI peripheral
	pinMode(spiPin, INPUT);
}

void SPIClass::end() {
	SPI_Disable(spi);
	initialized = false;
}

void SPIClass::setBitOrder(uint8_t _pin, BitOrder _bitOrder) {
	uint32_t ch = BOARD_PIN_TO_SPI_CHANNEL(_pin);
	bitOrder[ch] = _bitOrder;
}

void SPIClass::setDataMode(uint8_t _pin, uint8_t _mode) {
	uint32_t ch = BOARD_PIN_TO_SPI_CHANNEL(_pin);
	mode[ch] = _mode | SPI_CSR_CSAAT;
	// SPI_CSR_DLYBCT(1) keeps CS enabled for 32 MCLK after a completed
	// transfer. Some device needs that for working properly.
	SPI_ConfigureNPCS(spi, ch, mode[ch] | SPI_CSR_SCBR(divider[ch]) | SPI_CSR_DLYBCT(1));
}

void SPIClass::setClockDivider(uint8_t _pin, uint8_t _divider) {
	uint32_t ch = BOARD_PIN_TO_SPI_CHANNEL(_pin);
	divider[ch] = _divider;
	// SPI_CSR_DLYBCT(1) keeps CS enabled for 32 MCLK after a completed
	// transfer. Some device needs that for working properly.
	SPI_ConfigureNPCS(spi, ch, mode[ch] | SPI_CSR_SCBR(divider[ch]) | SPI_CSR_DLYBCT(1));
}

byte SPIClass::transfer(byte _pin, uint8_t _data, SPITransferMode _mode) {
	uint32_t ch = BOARD_PIN_TO_SPI_CHANNEL(_pin);
	// Reverse bit order
	if (bitOrder[ch] == LSBFIRST)
		_data = __REV(__RBIT(_data));
	uint32_t d = _data | SPI_PCS(ch);
	if (_mode == SPI_LAST)
		d |= SPI_TDR_LASTXFER;

	// SPI_Write(spi, _channel, _data);
    while ((spi->SPI_SR & SPI_SR_TDRE) == 0)
	;
    spi->SPI_TDR = d;

    // return SPI_Read(spi);
    while ((spi->SPI_SR & SPI_SR_RDRF) == 0)
	;
    d = spi->SPI_RDR;
	// Reverse bit order
	if (bitOrder[ch] == LSBFIRST)
		d = __REV(__RBIT(d));
    return d & 0xFF;
}

void SPIClass::attachInterrupt(void) {
	// Should be enableInterrupt()
}

void SPIClass::detachInterrupt(void) {
	// Should be disableInterrupt()
}

#if SPI_INTERFACES_COUNT > 0
static void SPI_0_Init(void) {
	PIO_Configure(
			g_APinDescription[PIN_SPI_MOSI].pPort,
			g_APinDescription[PIN_SPI_MOSI].ulPinType,
			g_APinDescription[PIN_SPI_MOSI].ulPin,
			g_APinDescription[PIN_SPI_MOSI].ulPinConfiguration);
	PIO_Configure(
			g_APinDescription[PIN_SPI_MISO].pPort,
			g_APinDescription[PIN_SPI_MISO].ulPinType,
			g_APinDescription[PIN_SPI_MISO].ulPin,
			g_APinDescription[PIN_SPI_MISO].ulPinConfiguration);
	PIO_Configure(
			g_APinDescription[PIN_SPI_SCK].pPort,
			g_APinDescription[PIN_SPI_SCK].ulPinType,
			g_APinDescription[PIN_SPI_SCK].ulPin,
			g_APinDescription[PIN_SPI_SCK].ulPinConfiguration);
}

SPIClass SPI(SPI_INTERFACE, SPI_INTERFACE_ID, SPI_0_Init);
#endif





#endif


