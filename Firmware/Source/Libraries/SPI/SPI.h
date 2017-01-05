/*
 * Copyright (c) 2010 by Cristian Maglie <c.maglie@bug.st>
 * Copyright (c) 2014 by Paul Stoffregen <paul@pjrc.com> (Transaction API)
 * Copyright (c) 2014 by Matthijs Kooijman <matthijs@stdin.nl> (SPISettings AVR)
 * SPI Master library for arduino.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#ifndef _SPI_H_INCLUDED
#define _SPI_H_INCLUDED

#include <Arduino.h>

// SPI_HAS_TRANSACTION means SPI has beginTransaction(), endTransaction(),
// usingInterrupt(), and SPISetting(clock, bitOrder, dataMode)
#define SPI_HAS_TRANSACTION 1

// Uncomment this line to add detection of mismatched begin/end transactions.
// A mismatch occurs if other libraries fail to use SPI.endTransaction() for
// each SPI.beginTransaction().  Connect a LED to this pin.  The LED will turn
// on if any mismatch is ever detected.
//#define SPI_TRANSACTION_MISMATCH_LED 5

#ifndef __SAM3X8E__
#ifndef LSBFIRST
#define LSBFIRST 0
#endif
#ifndef MSBFIRST
#define MSBFIRST 1
#endif
#endif

#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C

#define SPI_CLOCK_DIV4 0x00
#define SPI_CLOCK_DIV16 0x01
#define SPI_CLOCK_DIV64 0x02
#define SPI_CLOCK_DIV128 0x03
#define SPI_CLOCK_DIV2 0x04
#define SPI_CLOCK_DIV8 0x05
#define SPI_CLOCK_DIV32 0x06

#define SPI_MODE_MASK 0x0C  // CPOL = bit 3, CPHA = bit 2 on SPCR
#define SPI_CLOCK_MASK 0x03  // SPR1 = bit 1, SPR0 = bit 0 on SPCR
#define SPI_2XCLOCK_MASK 0x01  // SPI2X = bit 0 on SPSR


/**********************************************************/
/*     8 bit AVR-based boards				  */
/**********************************************************/

#if defined(__AVR__)

// define SPI_AVR_EIMSK for AVR boards with external interrupt pins
#if defined(EIMSK)
  #define SPI_AVR_EIMSK	 EIMSK
#elif defined(GICR)
  #define SPI_AVR_EIMSK	 GICR
#elif defined(GIMSK)
  #define SPI_AVR_EIMSK	 GIMSK
#endif

class SPISettings {
public:
	SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {
		if (__builtin_constant_p(clock)) {
			init_AlwaysInline(clock, bitOrder, dataMode);
		} else {
			init_MightInline(clock, bitOrder, dataMode);
		}
	}
	SPISettings() {
		init_AlwaysInline(4000000, MSBFIRST, SPI_MODE0);
	}
private:
	void init_MightInline(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {
		init_AlwaysInline(clock, bitOrder, dataMode);
	}
	void init_AlwaysInline(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
	  __attribute__((__always_inline__)) {
		// Clock settings are defined as follows. Note that this shows SPI2X
		// inverted, so the bits form increasing numbers. Also note that
		// fosc/64 appears twice
		// SPR1 SPR0 ~SPI2X Freq
		//   0	  0	0   fosc/2
		//   0	  0	1   fosc/4
		//   0	  1	0   fosc/8
		//   0	  1	1   fosc/16
		//   1	  0	0   fosc/32
		//   1	  0	1   fosc/64
		//   1	  1	0   fosc/64
		//   1	  1	1   fosc/128

		// We find the fastest clock that is less than or equal to the
		// given clock rate. The clock divider that results in clock_setting
		// is 2 ^^ (clock_div + 1). If nothing is slow enough, we'll use the
		// slowest (128 == 2 ^^ 7, so clock_div = 6).
		uint8_t clockDiv;

		// When the clock is known at compiletime, use this if-then-else
		// cascade, which the compiler knows how to completely optimize
		// away. When clock is not known, use a loop instead, which generates
		// shorter code.
		if (__builtin_constant_p(clock)) {
			if (clock >= F_CPU / 2) {
				clockDiv = 0;
			} else if (clock >= F_CPU / 4) {
				clockDiv = 1;
			} else if (clock >= F_CPU / 8) {
				clockDiv = 2;
			} else if (clock >= F_CPU / 16) {
				clockDiv = 3;
			} else if (clock >= F_CPU / 32) {
				clockDiv = 4;
			} else if (clock >= F_CPU / 64) {
				clockDiv = 5;
			} else {
				clockDiv = 6;
			}
		} else {
			uint32_t clockSetting = F_CPU / 2;
			clockDiv = 0;
			while (clockDiv < 6 && clock < clockSetting) {
				clockSetting /= 2;
				clockDiv++;
			}
		}

		// Compensate for the duplicate fosc/64
		if (clockDiv == 6)
		clockDiv = 7;

		// Invert the SPI2X bit
		clockDiv ^= 0x1;

		// Pack into the SPISettings class
		spcr = _BV(SPE) | _BV(MSTR) | ((bitOrder == LSBFIRST) ? _BV(DORD) : 0) |
			(dataMode & SPI_MODE_MASK) | ((clockDiv >> 1) & SPI_CLOCK_MASK);
		spsr = clockDiv & SPI_2XCLOCK_MASK;
	}
	uint8_t spcr;
	uint8_t spsr;
	friend class SPIClass;
};



class SPIClass {
public:
	// Initialize the SPI library
	static void begin();

	// If SPI is used from within an interrupt, this function registers
	// that interrupt with the SPI library, so beginTransaction() can
	// prevent conflicts.  The input interruptNumber is the number used
	// with attachInterrupt.  If SPI is used from a different interrupt
	// (eg, a timer), interruptNumber should be 255.
	static void usingInterrupt(uint8_t interruptNumber);

	// Before using SPI.transfer() or asserting chip select pins,
	// this function is used to gain exclusive access to the SPI bus
	// and configure the correct settings.
	inline static void beginTransaction(SPISettings settings) {
		if (interruptMode > 0) {
			#ifdef SPI_AVR_EIMSK
			if (interruptMode == 1) {
				interruptSave = SPI_AVR_EIMSK;
				SPI_AVR_EIMSK &= ~interruptMask;
			} else
			#endif
			{
				uint8_t tmp = SREG;
				cli();
				interruptSave = tmp;
			}
		}
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 1;
		#endif
		SPCR = settings.spcr;
		SPSR = settings.spsr;
	}

	// Write to the SPI bus (MOSI pin) and also receive (MISO pin)
	inline static uint8_t transfer(uint8_t data) {
		SPDR = data;
		asm volatile("nop");
		while (!(SPSR & _BV(SPIF))) ; // wait
		return SPDR;
	}
	inline static uint16_t transfer16(uint16_t data) {
		union { uint16_t val; struct { uint8_t lsb; uint8_t msb; }; } in, out;
		in.val = data;
		if ((SPCR & _BV(DORD))) {
			SPDR = in.lsb;
			asm volatile("nop");
			while (!(SPSR & _BV(SPIF))) ;
			out.lsb = SPDR;
			SPDR = in.msb;
			asm volatile("nop");
			while (!(SPSR & _BV(SPIF))) ;
			out.msb = SPDR;
		} else {
			SPDR = in.msb;
			asm volatile("nop");
			while (!(SPSR & _BV(SPIF))) ;
			out.msb = SPDR;
			SPDR = in.lsb;
			asm volatile("nop");
			while (!(SPSR & _BV(SPIF))) ;
			out.lsb = SPDR;
		}
		return out.val;
	}
	inline static void transfer(void *buf, size_t count) {
		if (count == 0) return;
		uint8_t *p = (uint8_t *)buf;
		SPDR = *p;
		while (--count > 0) {
			uint8_t out = *(p + 1);
			while (!(SPSR & _BV(SPIF))) ;
			uint8_t in = SPDR;
			SPDR = out;
			*p++ = in;
		}
		while (!(SPSR & _BV(SPIF))) ;
		*p = SPDR;
	}

	// After performing a group of transfers and releasing the chip select
	// signal, this function allows others to access the SPI bus
	inline static void endTransaction(void) {
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (!inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 0;
		#endif
		if (interruptMode > 0) {
			#ifdef SPI_AVR_EIMSK
			if (interruptMode == 1) {
				SPI_AVR_EIMSK = interruptSave;
			} else
			#endif
			{
				SREG = interruptSave;
			}
		}
	}

	// Disable the SPI bus
	static void end();

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	inline static void setBitOrder(uint8_t bitOrder) {
		if (bitOrder == LSBFIRST) SPCR |= _BV(DORD);
		else SPCR &= ~(_BV(DORD));
	}
	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	inline static void setDataMode(uint8_t dataMode) {
		SPCR = (SPCR & ~SPI_MODE_MASK) | dataMode;
	}
	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	inline static void setClockDivider(uint8_t clockDiv) {
		SPCR = (SPCR & ~SPI_CLOCK_MASK) | (clockDiv & SPI_CLOCK_MASK);
		SPSR = (SPSR & ~SPI_2XCLOCK_MASK) | ((clockDiv >> 2) & SPI_2XCLOCK_MASK);
	}
	// These undocumented functions should not be used.  SPI.transfer()
	// polls the hardware flag which is automatically cleared as the
	// AVR responds to SPI's interrupt
	inline static void attachInterrupt() { SPCR |= _BV(SPIE); }
	inline static void detachInterrupt() { SPCR &= ~_BV(SPIE); }

private:
	static uint8_t interruptMode; // 0=none, 1=mask, 2=global
	static uint8_t interruptMask; // which interrupts to mask
	static uint8_t interruptSave; // temp storage, to restore state
	#ifdef SPI_TRANSACTION_MISMATCH_LED
	static uint8_t inTransactionFlag;
	#endif
};



/**********************************************************/
/*     32 bit Teensy 3.0 and 3.1			  */
/**********************************************************/

#elif defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISK)

#define SPI_HAS_NOTUSINGINTERRUPT 1

class SPISettings {
public:
	SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {
		if (__builtin_constant_p(clock)) {
			init_AlwaysInline(clock, bitOrder, dataMode);
		} else {
			init_MightInline(clock, bitOrder, dataMode);
		}
	}
	SPISettings() {
		init_AlwaysInline(4000000, MSBFIRST, SPI_MODE0);
	}
private:
	void init_MightInline(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {
		init_AlwaysInline(clock, bitOrder, dataMode);
	}
	void init_AlwaysInline(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
	  __attribute__((__always_inline__)) {
		uint32_t t, c = SPI_CTAR_FMSZ(7);
		if (bitOrder == LSBFIRST) c |= SPI_CTAR_LSBFE;
		if (__builtin_constant_p(clock)) {
			if	  (clock >= F_BUS / 2) {
				t = SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_DBR
				  | SPI_CTAR_CSSCK(0);
			} else if (clock >= F_BUS / 3) {
				t = SPI_CTAR_PBR(1) | SPI_CTAR_BR(0) | SPI_CTAR_DBR
				  | SPI_CTAR_CSSCK(0);
			} else if (clock >= F_BUS / 4) {
				t = SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0);
			} else if (clock >= F_BUS / 5) {
				t = SPI_CTAR_PBR(2) | SPI_CTAR_BR(0) | SPI_CTAR_DBR
				  | SPI_CTAR_CSSCK(0);
			} else if (clock >= F_BUS / 6) {
				t = SPI_CTAR_PBR(1) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0);
			} else if (clock >= F_BUS / 8) {
				t = SPI_CTAR_PBR(0) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
			} else if (clock >= F_BUS / 10) {
				t = SPI_CTAR_PBR(2) | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0);
			} else if (clock >= F_BUS / 12) {
				t = SPI_CTAR_PBR(1) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
			} else if (clock >= F_BUS / 16) {
				t = SPI_CTAR_PBR(0) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2);
			} else if (clock >= F_BUS / 20) {
				t = SPI_CTAR_PBR(2) | SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(0);
			} else if (clock >= F_BUS / 24) {
				t = SPI_CTAR_PBR(1) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2);
			} else if (clock >= F_BUS / 32) {
				t = SPI_CTAR_PBR(0) | SPI_CTAR_BR(4) | SPI_CTAR_CSSCK(3);
			} else if (clock >= F_BUS / 40) {
				t = SPI_CTAR_PBR(2) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2);
			} else if (clock >= F_BUS / 56) {
				t = SPI_CTAR_PBR(3) | SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(2);
			} else if (clock >= F_BUS / 64) {
				t = SPI_CTAR_PBR(0) | SPI_CTAR_BR(5) | SPI_CTAR_CSSCK(4);
			} else if (clock >= F_BUS / 96) {
				t = SPI_CTAR_PBR(1) | SPI_CTAR_BR(5) | SPI_CTAR_CSSCK(4);
			} else if (clock >= F_BUS / 128) {
				t = SPI_CTAR_PBR(0) | SPI_CTAR_BR(6) | SPI_CTAR_CSSCK(5);
			} else if (clock >= F_BUS / 192) {
				t = SPI_CTAR_PBR(1) | SPI_CTAR_BR(6) | SPI_CTAR_CSSCK(5);
			} else if (clock >= F_BUS / 256) {
				t = SPI_CTAR_PBR(0) | SPI_CTAR_BR(7) | SPI_CTAR_CSSCK(6);
			} else if (clock >= F_BUS / 384) {
				t = SPI_CTAR_PBR(1) | SPI_CTAR_BR(7) | SPI_CTAR_CSSCK(6);
			} else if (clock >= F_BUS / 512) {
				t = SPI_CTAR_PBR(0) | SPI_CTAR_BR(8) | SPI_CTAR_CSSCK(7);
			} else if (clock >= F_BUS / 640) {
				t = SPI_CTAR_PBR(2) | SPI_CTAR_BR(7) | SPI_CTAR_CSSCK(6);
			} else {	 /* F_BUS / 768 */
				t = SPI_CTAR_PBR(1) | SPI_CTAR_BR(8) | SPI_CTAR_CSSCK(7);
			}
		} else {
			for (uint32_t i=0; i<23; i++) {
				t = ctar_clock_table[i];
				if (clock >= F_BUS / ctar_div_table[i]) break;
			}
		}
		if (dataMode & 0x08) {
			c |= SPI_CTAR_CPOL;
		}
		if (dataMode & 0x04) {
			c |= SPI_CTAR_CPHA;
			t = (t & 0xFFFF0FFF) | ((t & 0xF000) >> 4);
		}
		ctar = c | t;
	}
	static const uint16_t ctar_div_table[23];
	static const uint32_t ctar_clock_table[23];
	uint32_t ctar;
	friend class SPIClass;
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
	friend class SPI1Class;
	friend class SPI2Class;
#endif
};



class SPIClass {
public:
	// Initialize the SPI library
	static void begin();

	// If SPI is to used from within an interrupt, this function registers
	// that interrupt with the SPI library, so beginTransaction() can
	// prevent conflicts.  The input interruptNumber is the number used
	// with attachInterrupt.  If SPI is used from a different interrupt
	// (eg, a timer), interruptNumber should be 255.
	static void usingInterrupt(uint8_t n) {
		if (n == 3 || n == 4 || n == 24 || n == 33) {
			usingInterrupt(IRQ_PORTA);
		} else if (n == 0 || n == 1 || (n >= 16 && n <= 19) || n == 25 || n == 32) {
			usingInterrupt(IRQ_PORTB);
		} else if ((n >= 9 && n <= 13) || n == 15 || n == 22 || n == 23
		  || (n >= 27 && n <= 30)) {
			usingInterrupt(IRQ_PORTC);
		} else if (n == 2 || (n >= 5 && n <= 8) || n == 14 || n == 20 || n == 21) {
			usingInterrupt(IRQ_PORTD);
		} else if (n == 26 || n == 31) {
			usingInterrupt(IRQ_PORTE);
		}
	}
	static void usingInterrupt(IRQ_NUMBER_t interruptName);
	static void notUsingInterrupt(IRQ_NUMBER_t interruptName);

	// Before using SPI.transfer() or asserting chip select pins,
	// this function is used to gain exclusive access to the SPI bus
	// and configure the correct settings.
	inline static void beginTransaction(SPISettings settings) {
		if (interruptMasksUsed) {
			__disable_irq();
			if (interruptMasksUsed & 0x01) {
				interruptSave[0] = NVIC_ICER0 & interruptMask[0];
				NVIC_ICER0 = interruptSave[0];
			}
			#if NVIC_NUM_INTERRUPTS > 32
			if (interruptMasksUsed & 0x02) {
				interruptSave[1] = NVIC_ICER1 & interruptMask[1];
				NVIC_ICER1 = interruptSave[1];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 64 && defined(NVIC_ISER2)
			if (interruptMasksUsed & 0x04) {
				interruptSave[2] = NVIC_ICER2 & interruptMask[2];
				NVIC_ICER2 = interruptSave[2];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 96 && defined(NVIC_ISER3)
			if (interruptMasksUsed & 0x08) {
				interruptSave[3] = NVIC_ICER3 & interruptMask[3];
				NVIC_ICER3 = interruptSave[3];
			}
			#endif
			__enable_irq();
		}
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 1;
		#endif
		if (SPI0_CTAR0 != settings.ctar) {
			SPI0_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
			SPI0_CTAR0 = settings.ctar;
			SPI0_CTAR1 = settings.ctar| SPI_CTAR_FMSZ(8);
			SPI0_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
		}
	}

	// Write to the SPI bus (MOSI pin) and also receive (MISO pin)
	inline static uint8_t transfer(uint8_t data) {
		SPI0_SR = SPI_SR_TCF;
		SPI0_PUSHR = data;
		while (!(SPI0_SR & SPI_SR_TCF)) ; // wait
		return SPI0_POPR;
	}
	inline static uint16_t transfer16(uint16_t data) {
		SPI0_SR = SPI_SR_TCF;
		SPI0_PUSHR = data | SPI_PUSHR_CTAS(1);
		while (!(SPI0_SR & SPI_SR_TCF)) ; // wait
		return SPI0_POPR;
	}
	inline static void transfer(void *buf, size_t count) {
		if (count == 0) return;
		uint8_t *p = (uint8_t *)buf;
		SPDR = *p;
		while (--count > 0) {
			uint8_t out = *(p + 1);
			while (!(SPSR & _BV(SPIF))) ;
			uint8_t in = SPDR;
			SPDR = out;
			*p++ = in;
		}
		while (!(SPSR & _BV(SPIF))) ;
		*p = SPDR;
	}

	// After performing a group of transfers and releasing the chip select
	// signal, this function allows others to access the SPI bus
	inline static void endTransaction(void) {
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (!inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 0;
		#endif
		if (interruptMasksUsed) {
			if (interruptMasksUsed & 0x01) {
				NVIC_ISER0 = interruptSave[0];
			}
			#if NVIC_NUM_INTERRUPTS > 32
			if (interruptMasksUsed & 0x02) {
				NVIC_ISER1 = interruptSave[1];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 64 && defined(NVIC_ISER2)
			if (interruptMasksUsed & 0x04) {
				NVIC_ISER2 = interruptSave[2];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 96 && defined(NVIC_ISER3)
			if (interruptMasksUsed & 0x08) {
				NVIC_ISER3 = interruptSave[3];
			}
			#endif
		}
	}

	// Disable the SPI bus
	static void end();

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	static void setBitOrder(uint8_t bitOrder);

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	static void setDataMode(uint8_t dataMode);

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	inline static void setClockDivider(uint8_t clockDiv) {
		if (clockDiv == SPI_CLOCK_DIV2) {
			setClockDivider_noInline(SPISettings(12000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV4) {
			setClockDivider_noInline(SPISettings(4000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV8) {
			setClockDivider_noInline(SPISettings(2000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV16) {
			setClockDivider_noInline(SPISettings(1000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV32) {
			setClockDivider_noInline(SPISettings(500000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV64) {
			setClockDivider_noInline(SPISettings(250000, MSBFIRST, SPI_MODE0).ctar);
		} else { /* clockDiv == SPI_CLOCK_DIV128 */
			setClockDivider_noInline(SPISettings(125000, MSBFIRST, SPI_MODE0).ctar);
		}
	}
	static void setClockDivider_noInline(uint32_t clk);

	// These undocumented functions should not be used.  SPI.transfer()
	// polls the hardware flag which is automatically cleared as the
	// AVR responds to SPI's interrupt
	inline static void attachInterrupt() { }
	inline static void detachInterrupt() { }

	// Teensy 3.x can use alternate pins for these 3 SPI signals.
	inline static void setMOSI(uint8_t pin) __attribute__((always_inline)) {
		SPCR.setMOSI(pin);
	}
	inline static void setMISO(uint8_t pin) __attribute__((always_inline)) {
		SPCR.setMISO(pin);
	}
	inline static void setSCK(uint8_t pin) __attribute__((always_inline)) {
		SPCR.setSCK(pin);
	}
	// return true if "pin" has special chip select capability
	static uint8_t pinIsChipSelect(uint8_t pin);
	// return true if both pin1 and pin2 have independent chip select capability
	static bool pinIsChipSelect(uint8_t pin1, uint8_t pin2);
	// configure a pin for chip select and return its SPI_MCR_PCSIS bitmask
	static uint8_t setCS(uint8_t pin);

private:
	static uint8_t interruptMasksUsed;
	static uint32_t interruptMask[(NVIC_NUM_INTERRUPTS+31)/32];
	static uint32_t interruptSave[(NVIC_NUM_INTERRUPTS+31)/32];
	#ifdef SPI_TRANSACTION_MISMATCH_LED
	static uint8_t inTransactionFlag;
	#endif
};


/**********************************************************/
/*     Teensy 3.5 and 3.6 have SPI1 as well				  */
/**********************************************************/
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
class SPI1Class {
public:
	// Initialize the SPI library
	static void begin();

	// If SPI is to used from within an interrupt, this function registers
	// that interrupt with the SPI library, so beginTransaction() can
	// prevent conflicts.  The input interruptNumber is the number used
	// with attachInterrupt.  If SPI is used from a different interrupt
	// (eg, a timer), interruptNumber should be 255.
	static void usingInterrupt(uint8_t n) {
		if (n == 3 || n == 4 || n == 24 || n == 33) {
			usingInterrupt(IRQ_PORTA);
		} else if (n == 0 || n == 1 || (n >= 16 && n <= 19) || n == 25 || n == 32) {
			usingInterrupt(IRQ_PORTB);
		} else if ((n >= 9 && n <= 13) || n == 15 || n == 22 || n == 23
		  || (n >= 27 && n <= 30)) {
			usingInterrupt(IRQ_PORTC);
		} else if (n == 2 || (n >= 5 && n <= 8) || n == 14 || n == 20 || n == 21) {
			usingInterrupt(IRQ_PORTD);
		} else if (n == 26 || n == 31) {
			usingInterrupt(IRQ_PORTE);
		}
	}
	static void usingInterrupt(IRQ_NUMBER_t interruptName);
	static void notUsingInterrupt(IRQ_NUMBER_t interruptName);

	// Before using SPI.transfer() or asserting chip select pins,
	// this function is used to gain exclusive access to the SPI bus
	// and configure the correct settings.
	inline static void beginTransaction(SPISettings settings) {
		if (interruptMasksUsed) {
			__disable_irq();
			if (interruptMasksUsed & 0x01) {
				interruptSave[0] = NVIC_ICER0 & interruptMask[0];
				NVIC_ICER0 = interruptSave[0];
			}
			#if NVIC_NUM_INTERRUPTS > 32
			if (interruptMasksUsed & 0x02) {
				interruptSave[1] = NVIC_ICER1 & interruptMask[1];
				NVIC_ICER1 = interruptSave[1];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 64 && defined(NVIC_ISER2)
			if (interruptMasksUsed & 0x04) {
				interruptSave[2] = NVIC_ICER2 & interruptMask[2];
				NVIC_ICER2 = interruptSave[2];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 96 && defined(NVIC_ISER3)
			if (interruptMasksUsed & 0x08) {
				interruptSave[3] = NVIC_ICER3 & interruptMask[3];
				NVIC_ICER3 = interruptSave[3];
			}
			#endif
			__enable_irq();
		}
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 1;
		#endif
		if (SPI1_CTAR0 != settings.ctar) {
			SPI1_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
			SPI1_CTAR0 = settings.ctar;
			SPI1_CTAR1 = settings.ctar| SPI_CTAR_FMSZ(8);
			SPI1_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
		}
	}

	// Write to the SPI bus (MOSI pin) and also receive (MISO pin)
	inline static uint8_t transfer(uint8_t data) {
		SPI1_SR = SPI_SR_TCF;
		SPI1_PUSHR = data;
		while (!(SPI1_SR & SPI_SR_TCF)) ; // wait
		return SPI1_POPR;
	}
	inline static uint16_t transfer16(uint16_t data) {
		SPI1_SR = SPI_SR_TCF;
		SPI1_PUSHR = data | SPI_PUSHR_CTAS(1);
		while (!(SPI1_SR & SPI_SR_TCF)) ; // wait
		return SPI1_POPR;
	}
	inline static void transfer(void *buf, size_t count) {
		uint8_t *p = (uint8_t *)buf;
		while (count--) {
			*p = transfer(*p);
			p++;
		}
	}

	// After performing a group of transfers and releasing the chip select
	// signal, this function allows others to access the SPI bus
	inline static void endTransaction(void) {
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (!inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 0;
		#endif
		if (interruptMasksUsed) {
			if (interruptMasksUsed & 0x01) {
				NVIC_ISER0 = interruptSave[0];
			}
			#if NVIC_NUM_INTERRUPTS > 32
			if (interruptMasksUsed & 0x02) {
				NVIC_ISER1 = interruptSave[1];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 64 && defined(NVIC_ISER2)
			if (interruptMasksUsed & 0x04) {
				NVIC_ISER2 = interruptSave[2];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 96 && defined(NVIC_ISER3)
			if (interruptMasksUsed & 0x08) {
				NVIC_ISER3 = interruptSave[3];
			}
			#endif
		}
	}

	// Disable the SPI bus
	static void end();

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	static void setBitOrder(uint8_t bitOrder);

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	static void setDataMode(uint8_t dataMode);

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	inline static void setClockDivider(uint8_t clockDiv) {
		if (clockDiv == SPI_CLOCK_DIV2) {
			setClockDivider_noInline(SPISettings(12000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV4) {
			setClockDivider_noInline(SPISettings(4000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV8) {
			setClockDivider_noInline(SPISettings(2000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV16) {
			setClockDivider_noInline(SPISettings(1000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV32) {
			setClockDivider_noInline(SPISettings(500000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV64) {
			setClockDivider_noInline(SPISettings(250000, MSBFIRST, SPI_MODE0).ctar);
		} else { /* clockDiv == SPI_CLOCK_DIV128 */
			setClockDivider_noInline(SPISettings(125000, MSBFIRST, SPI_MODE0).ctar);
		}
	}
	static void setClockDivider_noInline(uint32_t clk);

	// These undocumented functions should not be used.  SPI.transfer()
	// polls the hardware flag which is automatically cleared as the
	// AVR responds to SPI's interrupt
	inline static void attachInterrupt() { }
	inline static void detachInterrupt() { }

	// Teensy 3.x can use alternate pins for these 3 SPI signals.
	inline static void setMOSI(uint8_t pin) __attribute__((always_inline)) {
		SPCR1.setMOSI(pin);
	}
	inline static void setMISO(uint8_t pin) __attribute__((always_inline)) {
		SPCR1.setMISO(pin);
	}
	inline static void setSCK(uint8_t pin) __attribute__((always_inline)) {
		SPCR1.setSCK(pin);
	}
	// return true if "pin" has special chip select capability
	static uint8_t pinIsChipSelect(uint8_t pin);
	// return true if both pin1 and pin2 have independent chip select capability
	static bool pinIsChipSelect(uint8_t pin1, uint8_t pin2);
	// configure a pin for chip select and return its SPI_MCR_PCSIS bitmask
	static uint8_t setCS(uint8_t pin);

private:
	static uint8_t interruptMasksUsed;
	static uint32_t interruptMask[(NVIC_NUM_INTERRUPTS+31)/32];
	static uint32_t interruptSave[(NVIC_NUM_INTERRUPTS+31)/32];
	#ifdef SPI_TRANSACTION_MISMATCH_LED
	static uint8_t inTransactionFlag;
	#endif
};
class SPI2Class {
public:
	// Initialize the SPI library
	static void begin();

	// If SPI is to used from within an interrupt, this function registers
	// that interrupt with the SPI library, so beginTransaction() can
	// prevent conflicts.  The input interruptNumber is the number used
	// with attachInterrupt.  If SPI is used from a different interrupt
	// (eg, a timer), interruptNumber should be 255.
	static void usingInterrupt(uint8_t n) {
		if (n == 3 || n == 4 || n == 24 || n == 33) {
			usingInterrupt(IRQ_PORTA);
		} else if (n == 0 || n == 1 || (n >= 16 && n <= 19) || n == 25 || n == 32) {
			usingInterrupt(IRQ_PORTB);
		} else if ((n >= 9 && n <= 13) || n == 15 || n == 22 || n == 23
		  || (n >= 27 && n <= 30)) {
			usingInterrupt(IRQ_PORTC);
		} else if (n == 2 || (n >= 5 && n <= 8) || n == 14 || n == 20 || n == 21) {
			usingInterrupt(IRQ_PORTD);
		} else if (n == 26 || n == 31) {
			usingInterrupt(IRQ_PORTE);
		}
	}
	static void usingInterrupt(IRQ_NUMBER_t interruptName);
	static void notUsingInterrupt(IRQ_NUMBER_t interruptName);

	// Before using SPI.transfer() or asserting chip select pins,
	// this function is used to gain exclusive access to the SPI bus
	// and configure the correct settings.
	inline static void beginTransaction(SPISettings settings) {
		if (interruptMasksUsed) {
			__disable_irq();
			if (interruptMasksUsed & 0x01) {
				interruptSave[0] = NVIC_ICER0 & interruptMask[0];
				NVIC_ICER0 = interruptSave[0];
			}
			#if NVIC_NUM_INTERRUPTS > 32
			if (interruptMasksUsed & 0x02) {
				interruptSave[1] = NVIC_ICER1 & interruptMask[1];
				NVIC_ICER1 = interruptSave[1];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 64 && defined(NVIC_ISER2)
			if (interruptMasksUsed & 0x04) {
				interruptSave[2] = NVIC_ICER2 & interruptMask[2];
				NVIC_ICER2 = interruptSave[2];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 96 && defined(NVIC_ISER3)
			if (interruptMasksUsed & 0x08) {
				interruptSave[3] = NVIC_ICER3 & interruptMask[3];
				NVIC_ICER3 = interruptSave[3];
			}
			#endif
			__enable_irq();
		}
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 1;
		#endif
		if (SPI2_CTAR0 != settings.ctar) {
			SPI2_MCR = SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
			SPI2_CTAR0 = settings.ctar;
			SPI2_CTAR1 = settings.ctar| SPI_CTAR_FMSZ(8);
			SPI2_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
		}
	}

	// Write to the SPI bus (MOSI pin) and also receive (MISO pin)
	inline static uint8_t transfer(uint8_t data) {
		SPI2_SR = SPI_SR_TCF;
		SPI2_PUSHR = data;
		while (!(SPI2_SR & SPI_SR_TCF)) ; // wait
		return SPI2_POPR;
	}
	inline static uint16_t transfer16(uint16_t data) {
		SPI2_SR = SPI_SR_TCF;
		SPI2_PUSHR = data | SPI_PUSHR_CTAS(1);
		while (!(SPI2_SR & SPI_SR_TCF)) ; // wait
		return SPI2_POPR;
	}
	inline static void transfer(void *buf, size_t count) {
		uint8_t *p = (uint8_t *)buf;
		while (count--) {
			*p = transfer(*p);
			p++;
		}
	}

	// After performing a group of transfers and releasing the chip select
	// signal, this function allows others to access the SPI bus
	inline static void endTransaction(void) {
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (!inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 0;
		#endif
		if (interruptMasksUsed) {
			if (interruptMasksUsed & 0x01) {
				NVIC_ISER0 = interruptSave[0];
			}
			#if NVIC_NUM_INTERRUPTS > 32
			if (interruptMasksUsed & 0x02) {
				NVIC_ISER1 = interruptSave[1];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 64 && defined(NVIC_ISER2)
			if (interruptMasksUsed & 0x04) {
				NVIC_ISER2 = interruptSave[2];
			}
			#endif
			#if NVIC_NUM_INTERRUPTS > 96 && defined(NVIC_ISER3)
			if (interruptMasksUsed & 0x08) {
				NVIC_ISER3 = interruptSave[3];
			}
			#endif
		}
	}

	// Disable the SPI bus
	static void end();

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	static void setBitOrder(uint8_t bitOrder);

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	static void setDataMode(uint8_t dataMode);

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	inline static void setClockDivider(uint8_t clockDiv) {
		if (clockDiv == SPI_CLOCK_DIV2) {
			setClockDivider_noInline(SPISettings(12000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV4) {
			setClockDivider_noInline(SPISettings(4000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV8) {
			setClockDivider_noInline(SPISettings(2000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV16) {
			setClockDivider_noInline(SPISettings(1000000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV32) {
			setClockDivider_noInline(SPISettings(500000, MSBFIRST, SPI_MODE0).ctar);
		} else if (clockDiv == SPI_CLOCK_DIV64) {
			setClockDivider_noInline(SPISettings(250000, MSBFIRST, SPI_MODE0).ctar);
		} else { /* clockDiv == SPI_CLOCK_DIV128 */
			setClockDivider_noInline(SPISettings(125000, MSBFIRST, SPI_MODE0).ctar);
		}
	}
	static void setClockDivider_noInline(uint32_t clk);

	// These undocumented functions should not be used.  SPI.transfer()
	// polls the hardware flag which is automatically cleared as the
	// AVR responds to SPI's interrupt
	inline static void attachInterrupt() { }
	inline static void detachInterrupt() { }

	// Teensy 3.x can use alternate pins for these 3 SPI signals.
	inline static void setMOSI(uint8_t pin) __attribute__((always_inline)) {
		SPCR2.setMOSI(pin);
	}
	inline static void setMISO(uint8_t pin) __attribute__((always_inline)) {
		SPCR2.setMISO(pin);
	}
	inline static void setSCK(uint8_t pin) __attribute__((always_inline)) {
		SPCR2.setSCK(pin);
	}
	// return true if "pin" has special chip select capability
	static uint8_t pinIsChipSelect(uint8_t pin);
	// return true if both pin1 and pin2 have independent chip select capability
	static bool pinIsChipSelect(uint8_t pin1, uint8_t pin2);
	// configure a pin for chip select and return its SPI_MCR_PCSIS bitmask
	static uint8_t setCS(uint8_t pin);

private:
	static uint8_t interruptMasksUsed;
	static uint32_t interruptMask[(NVIC_NUM_INTERRUPTS+31)/32];
	static uint32_t interruptSave[(NVIC_NUM_INTERRUPTS+31)/32];
	#ifdef SPI_TRANSACTION_MISMATCH_LED
	static uint8_t inTransactionFlag;
	#endif
};
#endif









/**********************************************************/
/*     32 bit Teensy-LC					  */
/**********************************************************/

#elif defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISL)

class SPISettings {
public:
	SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {
		if (__builtin_constant_p(clock)) {
			init_AlwaysInline(clock, bitOrder, dataMode);
		} else {
			init_MightInline(clock, bitOrder, dataMode);
		}
	}
	SPISettings() {
		init_AlwaysInline(4000000, MSBFIRST, SPI_MODE0);
	}
private:
	void init_MightInline(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {
		init_AlwaysInline(clock, bitOrder, dataMode);
	}
	void init_AlwaysInline(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
	  __attribute__((__always_inline__)) {
		uint8_t c = SPI_C1_MSTR | SPI_C1_SPE;
		if (dataMode & 0x04) c |= SPI_C1_CPHA;
		if (dataMode & 0x08) c |= SPI_C1_CPOL;
		if (bitOrder == LSBFIRST) c |= SPI_C1_LSBFE;
		c1 = c;
		if (__builtin_constant_p(clock)) {
			  if	  (clock >= F_BUS /   2) { c = SPI_BR_SPPR(0) | SPI_BR_SPR(0);
			} else if (clock >= F_BUS /   4) { c = SPI_BR_SPPR(1) | SPI_BR_SPR(0);
			} else if (clock >= F_BUS /   6) { c = SPI_BR_SPPR(2) | SPI_BR_SPR(0);
			} else if (clock >= F_BUS /   8) { c = SPI_BR_SPPR(3) | SPI_BR_SPR(0);
			} else if (clock >= F_BUS /  10) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(0);
			} else if (clock >= F_BUS /  12) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(0);
			} else if (clock >= F_BUS /  14) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(0);
			} else if (clock >= F_BUS /  16) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(0);
			} else if (clock >= F_BUS /  20) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(1);
			} else if (clock >= F_BUS /  24) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(1);
			} else if (clock >= F_BUS /  28) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(1);
			} else if (clock >= F_BUS /  32) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(1);
			} else if (clock >= F_BUS /  40) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(2);
			} else if (clock >= F_BUS /  48) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(2);
			} else if (clock >= F_BUS /  56) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(2);
			} else if (clock >= F_BUS /  64) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(2);
			} else if (clock >= F_BUS /  80) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(3);
			} else if (clock >= F_BUS /  96) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(3);
			} else if (clock >= F_BUS / 112) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(3);
			} else if (clock >= F_BUS / 128) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(3);
			} else if (clock >= F_BUS / 160) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(4);
			} else if (clock >= F_BUS / 192) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(4);
			} else if (clock >= F_BUS / 224) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(4);
			} else if (clock >= F_BUS / 256) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(4);
			} else if (clock >= F_BUS / 320) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(5);
			} else if (clock >= F_BUS / 384) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(5);
			} else if (clock >= F_BUS / 448) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(5);
			} else if (clock >= F_BUS / 512) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(5);
			} else if (clock >= F_BUS / 640) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(6);
			} else      /* F_BUS / 768 */    { c = SPI_BR_SPPR(5) | SPI_BR_SPR(6);
			}
		} else {
			for (uint32_t i=0; i<30; i++) {
				c = br_clock_table[i];
				if (clock >= F_BUS / br_div_table[i]) break;
			}
		}
		br0 = c;
		if (__builtin_constant_p(clock)) {
			  if	  (clock >= (F_PLL/2) /   2) { c = SPI_BR_SPPR(0) | SPI_BR_SPR(0);
			} else if (clock >= (F_PLL/2) /   4) { c = SPI_BR_SPPR(1) | SPI_BR_SPR(0);
			} else if (clock >= (F_PLL/2) /   6) { c = SPI_BR_SPPR(2) | SPI_BR_SPR(0);
			} else if (clock >= (F_PLL/2) /   8) { c = SPI_BR_SPPR(3) | SPI_BR_SPR(0);
			} else if (clock >= (F_PLL/2) /  10) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(0);
			} else if (clock >= (F_PLL/2) /  12) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(0);
			} else if (clock >= (F_PLL/2) /  14) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(0);
			} else if (clock >= (F_PLL/2) /  16) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(0);
			} else if (clock >= (F_PLL/2) /  20) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(1);
			} else if (clock >= (F_PLL/2) /  24) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(1);
			} else if (clock >= (F_PLL/2) /  28) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(1);
			} else if (clock >= (F_PLL/2) /  32) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(1);
			} else if (clock >= (F_PLL/2) /  40) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(2);
			} else if (clock >= (F_PLL/2) /  48) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(2);
			} else if (clock >= (F_PLL/2) /  56) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(2);
			} else if (clock >= (F_PLL/2) /  64) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(2);
			} else if (clock >= (F_PLL/2) /  80) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(3);
			} else if (clock >= (F_PLL/2) /  96) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(3);
			} else if (clock >= (F_PLL/2) / 112) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(3);
			} else if (clock >= (F_PLL/2) / 128) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(3);
			} else if (clock >= (F_PLL/2) / 160) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(4);
			} else if (clock >= (F_PLL/2) / 192) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(4);
			} else if (clock >= (F_PLL/2) / 224) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(4);
			} else if (clock >= (F_PLL/2) / 256) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(4);
			} else if (clock >= (F_PLL/2) / 320) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(5);
			} else if (clock >= (F_PLL/2) / 384) { c = SPI_BR_SPPR(5) | SPI_BR_SPR(5);
			} else if (clock >= (F_PLL/2) / 448) { c = SPI_BR_SPPR(6) | SPI_BR_SPR(5);
			} else if (clock >= (F_PLL/2) / 512) { c = SPI_BR_SPPR(7) | SPI_BR_SPR(5);
			} else if (clock >= (F_PLL/2) / 640) { c = SPI_BR_SPPR(4) | SPI_BR_SPR(6);
			} else      /* (F_PLL/2) / 768 */    { c = SPI_BR_SPPR(5) | SPI_BR_SPR(6);
			}
		} else {
			for (uint32_t i=0; i<30; i++) {
				c = br_clock_table[i];
				if (clock >= (F_PLL/2) / br_div_table[i]) break;
			}
		}
		br1 = c;
	}
	static const uint8_t  br_clock_table[30];
	static const uint16_t br_div_table[30];
	uint8_t c1, br0, br1;
	friend class SPIClass;
	friend class SPI1Class;
};


class SPIClass {
public:
	// Initialize the SPI library
	static void begin();

	// If SPI is to used from within an interrupt, this function registers
	// that interrupt with the SPI library, so beginTransaction() can
	// prevent conflicts.  The input interruptNumber is the number used
	// with attachInterrupt.  If SPI is used from a different interrupt
	// (eg, a timer), interruptNumber should be 255.
	static void usingInterrupt(uint8_t n) {
		if (n == 3 || n == 4) {
			usingInterrupt(IRQ_PORTA);
		} else if ((n >= 2 && n <= 15) || (n >= 20 && n <= 23)) {
			usingInterrupt(IRQ_PORTCD);
		}
	}
	static void usingInterrupt(IRQ_NUMBER_t interruptName) {
		uint32_t n = (uint32_t)interruptName;
		if (n < NVIC_NUM_INTERRUPTS) interruptMask |= (1 << n);
	}
	static void notUsingInterrupt(IRQ_NUMBER_t interruptName) {
		uint32_t n = (uint32_t)interruptName;
		if (n < NVIC_NUM_INTERRUPTS) interruptMask &= ~(1 << n);
	}

	// Before using SPI.transfer() or asserting chip select pins,
	// this function is used to gain exclusive access to the SPI bus
	// and configure the correct settings.
	inline static void beginTransaction(SPISettings settings) {
		if (interruptMask) {
			__disable_irq();
			interruptSave = NVIC_ICER0 & interruptMask;
			NVIC_ICER0 = interruptSave;
			__enable_irq();
		}
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 1;
		#endif
		SPI0_C1 = settings.c1;
		SPI0_BR = settings.br0;
	}

	// Write to the SPI bus (MOSI pin) and also receive (MISO pin)
	inline static uint8_t transfer(uint8_t data) {
		SPI0_DL = data;
		while (!(SPI0_S & SPI_S_SPRF)) ; // wait
		return SPI0_DL;
	}
	inline static uint16_t transfer16(uint16_t data) {
		SPI0_C2 = SPI_C2_SPIMODE;
		SPI0_S;
		SPI0_DL = data;
		SPI0_DH = data >> 8;
		while (!(SPI0_S & SPI_S_SPRF)) ; // wait
		uint16_t r = SPI0_DL | (SPI0_DH << 8);
		SPI0_C2 = 0;
		SPI0_S;
		return r;
	}
	inline static void transfer(void *buf, size_t count) {
		if (count == 0) return;
		uint8_t *p = (uint8_t *)buf;
		while (!(SPI0_S & SPI_S_SPTEF)) ; // wait
		SPI0_DL = *p;
		while (--count > 0) {
			uint8_t out = *(p + 1);
			while (!(SPI0_S & SPI_S_SPTEF)) ; // wait
			__disable_irq();
			SPI0_DL = out;
			while (!(SPI0_S & SPI_S_SPRF)) ; // wait
			uint8_t in = SPI0_DL;
			__enable_irq();
			*p++ = in;
		}
		while (!(SPI0_S & SPI_S_SPRF)) ; // wait
		*p = SPDR;
	}

	// After performing a group of transfers and releasing the chip select
	// signal, this function allows others to access the SPI bus
	inline static void endTransaction(void) {
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (!inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 0;
		#endif
		if (interruptMask) {
			NVIC_ISER0 = interruptSave;
		}
	}

	// Disable the SPI bus
	static void end();

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	static void setBitOrder(uint8_t bitOrder) {
		uint8_t c = SPI0_C1 | SPI_C1_SPE;
		if (bitOrder == LSBFIRST) c |= SPI_C1_LSBFE;
		else c &= ~SPI_C1_LSBFE;
		SPI0_C1 = c;
	}

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	static void setDataMode(uint8_t dataMode) {
		uint8_t c = SPI0_C1 | SPI_C1_SPE;
		if (dataMode & 0x04) c |= SPI_C1_CPHA;
		else c &= ~SPI_C1_CPHA;
		if (dataMode & 0x08) c |= SPI_C1_CPOL;
		else c &= ~SPI_C1_CPOL;
		SPI0_C1 = c;
	}

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	inline static void setClockDivider(uint8_t clockDiv) {
		if (clockDiv == SPI_CLOCK_DIV2) {
			SPI0_BR = (SPISettings(12000000, MSBFIRST, SPI_MODE0).br0);
		} else if (clockDiv == SPI_CLOCK_DIV4) {
			SPI0_BR = (SPISettings(4000000, MSBFIRST, SPI_MODE0).br0);
		} else if (clockDiv == SPI_CLOCK_DIV8) {
			SPI0_BR = (SPISettings(2000000, MSBFIRST, SPI_MODE0).br0);
		} else if (clockDiv == SPI_CLOCK_DIV16) {
			SPI0_BR = (SPISettings(1000000, MSBFIRST, SPI_MODE0).br0);
		} else if (clockDiv == SPI_CLOCK_DIV32) {
			SPI0_BR = (SPISettings(500000, MSBFIRST, SPI_MODE0).br0);
		} else if (clockDiv == SPI_CLOCK_DIV64) {
			SPI0_BR = (SPISettings(250000, MSBFIRST, SPI_MODE0).br0);
		} else { /* clockDiv == SPI_CLOCK_DIV128 */
			SPI0_BR = (SPISettings(125000, MSBFIRST, SPI_MODE0).br0);
		}
	}

	// These undocumented functions should not be used.  SPI.transfer()
	// polls the hardware flag which is automatically cleared as the
	// AVR responds to SPI's interrupt
	inline static void attachInterrupt() { }
	inline static void detachInterrupt() { }

	// Teensy LC can use alternate pins for these 3 SPI signals.
	inline static void setMOSI(uint8_t pin) __attribute__((always_inline)) {
		SPCR.setMOSI(pin);
	}
	inline static void setMISO(uint8_t pin) __attribute__((always_inline)) {
		SPCR.setMISO(pin);
	}
	inline static void setSCK(uint8_t pin) __attribute__((always_inline)) {
		SPCR.setSCK(pin);
	}
	// return true if "pin" has special chip select capability
	static bool pinIsChipSelect(uint8_t pin) { return (pin == 10 || pin == 2); }
	// return true if both pin1 and pin2 have independent chip select capability
	static bool pinIsChipSelect(uint8_t pin1, uint8_t pin2) { return false; }
	// configure a pin for chip select and return its SPI_MCR_PCSIS bitmask
	static uint8_t setCS(uint8_t pin);

private:
	static uint32_t interruptMask;
	static uint32_t interruptSave;
	#ifdef SPI_TRANSACTION_MISMATCH_LED
	static uint8_t inTransactionFlag;
	#endif
};



class SPI1Class {
public:
	// Initialize the SPI library
	static void begin();

	// If SPI is to used from within an interrupt, this function registers
	// that interrupt with the SPI library, so beginTransaction() can
	// prevent conflicts.  The input interruptNumber is the number used
	// with attachInterrupt.  If SPI is used from a different interrupt
	// (eg, a timer), interruptNumber should be 255.
	static void usingInterrupt(uint8_t n) {
		if (n == 3 || n == 4) {
			usingInterrupt(IRQ_PORTA);
		} else if ((n >= 2 && n <= 15) || (n >= 20 && n <= 23)) {
			usingInterrupt(IRQ_PORTCD);
		}
	}
	static void usingInterrupt(IRQ_NUMBER_t interruptName) {
		uint32_t n = (uint32_t)interruptName;
		if (n < NVIC_NUM_INTERRUPTS) interruptMask |= (1 << n);
	}
	static void notUsingInterrupt(IRQ_NUMBER_t interruptName) {
		uint32_t n = (uint32_t)interruptName;
		if (n < NVIC_NUM_INTERRUPTS) interruptMask &= ~(1 << n);
	}

	// Before using SPI.transfer() or asserting chip select pins,
	// this function is used to gain exclusive access to the SPI bus
	// and configure the correct settings.
	inline static void beginTransaction(SPISettings settings) {
		if (interruptMask) {
			__disable_irq();
			interruptSave = NVIC_ICER0 & interruptMask;
			NVIC_ICER0 = interruptSave;
			__enable_irq();
		}
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 1;
		#endif
		SPI1_C1 = settings.c1;
		SPI1_BR = settings.br1;
	}

	// Write to the SPI bus (MOSI pin) and also receive (MISO pin)
	inline static uint8_t transfer(uint8_t data) {
		SPI1_DL = data;
		while (!(SPI1_S & SPI_S_SPRF)) ; // wait
		return SPI1_DL;
	}
	inline static uint16_t transfer16(uint16_t data) {
		SPI1_C2 = SPI_C2_SPIMODE;
		SPI1_S;
		SPI1_DL = data;
		SPI1_DH = data >> 8;
		while (!(SPI1_S & SPI_S_SPRF)) ; // wait
		uint16_t r = SPI1_DL | (SPI1_DH << 8);
		SPI1_C2 = 0;
		SPI1_S;
		return r;
	}
	inline static void transfer(void *buf, size_t count) {
		if (count == 0) return;
		uint8_t *p = (uint8_t *)buf;
		while (!(SPI1_S & SPI_S_SPTEF)) ; // wait
		SPI1_DL = *p;
		while (--count > 0) {
			uint8_t out = *(p + 1);
			while (!(SPI1_S & SPI_S_SPTEF)) ; // wait
			__disable_irq();
			SPI1_DL = out;
			while (!(SPI1_S & SPI_S_SPRF)) ; // wait
			uint8_t in = SPI1_DL;
			__enable_irq();
			*p++ = in;
		}
		while (!(SPI1_S & SPI_S_SPRF)) ; // wait
		*p = SPDR;
	}

	// After performing a group of transfers and releasing the chip select
	// signal, this function allows others to access the SPI bus
	inline static void endTransaction(void) {
		#ifdef SPI_TRANSACTION_MISMATCH_LED
		if (!inTransactionFlag) {
			pinMode(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
			digitalWrite(SPI_TRANSACTION_MISMATCH_LED, HIGH);
		}
		inTransactionFlag = 0;
		#endif
		if (interruptMask) {
			NVIC_ISER0 = interruptSave;
		}
	}

	// Disable the SPI bus
	static void end();

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	static void setBitOrder(uint8_t bitOrder) {
		uint8_t c = SPI1_C1 | SPI_C1_SPE;
		if (bitOrder == LSBFIRST) c |= SPI_C1_LSBFE;
		else c &= ~SPI_C1_LSBFE;
		SPI1_C1 = c;
	}

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	static void setDataMode(uint8_t dataMode) {
		uint8_t c = SPI1_C1 | SPI_C1_SPE;
		if (dataMode & 0x04) c |= SPI_C1_CPHA;
		else c &= ~SPI_C1_CPHA;
		if (dataMode & 0x08) c |= SPI_C1_CPOL;
		else c &= ~SPI_C1_CPOL;
		SPI1_C1 = c;
	}

	// This function is deprecated.	 New applications should use
	// beginTransaction() to configure SPI settings.
	inline static void setClockDivider(uint8_t clockDiv) {
		if (clockDiv == SPI_CLOCK_DIV2) {
			SPI1_BR = (SPISettings(12000000, MSBFIRST, SPI_MODE0).br1);
		} else if (clockDiv == SPI_CLOCK_DIV4) {
			SPI1_BR = (SPISettings(4000000, MSBFIRST, SPI_MODE0).br1);
		} else if (clockDiv == SPI_CLOCK_DIV8) {
			SPI1_BR = (SPISettings(2000000, MSBFIRST, SPI_MODE0).br1);
		} else if (clockDiv == SPI_CLOCK_DIV16) {
			SPI1_BR = (SPISettings(1000000, MSBFIRST, SPI_MODE0).br1);
		} else if (clockDiv == SPI_CLOCK_DIV32) {
			SPI1_BR = (SPISettings(500000, MSBFIRST, SPI_MODE0).br1);
		} else if (clockDiv == SPI_CLOCK_DIV64) {
			SPI1_BR = (SPISettings(250000, MSBFIRST, SPI_MODE0).br1);
		} else { /* clockDiv == SPI_CLOCK_DIV128 */
			SPI1_BR = (SPISettings(125000, MSBFIRST, SPI_MODE0).br1);
		}
	}

	// These undocumented functions should not be used.  SPI.transfer()
	// polls the hardware flag which is automatically cleared as the
	// AVR responds to SPI's interrupt
	inline static void attachInterrupt() { }
	inline static void detachInterrupt() { }

	// Teensy LC can use alternate pins for these 3 SPI signals.
	inline static void setMOSI(uint8_t pin) __attribute__((always_inline)) {
		SPCR1.setMOSI(pin);
	}
	inline static void setMISO(uint8_t pin) __attribute__((always_inline)) {
		SPCR1.setMISO(pin);
	}
	inline static void setSCK(uint8_t pin) __attribute__((always_inline)) {
		SPCR1.setSCK(pin);
	}
	// return true if "pin" has special chip select capability
	static bool pinIsChipSelect(uint8_t pin) { return (pin == 6); }
	// return true if both pin1 and pin2 have independent chip select capability
	static bool pinIsChipSelect(uint8_t pin1, uint8_t pin2) { return false; }
	// configure a pin for chip select and return its SPI_MCR_PCSIS bitmask
	static uint8_t setCS(uint8_t pin);

private:
	static uint32_t interruptMask;
	static uint32_t interruptSave;
	#ifdef SPI_TRANSACTION_MISMATCH_LED
	static uint8_t inTransactionFlag;
	#endif
};
















/**********************************************************/
/*     32 bit Arduino Due				  */
/**********************************************************/

#elif defined(__arm__) && defined(__SAM3X8E__)

#undef SPI_MODE0
#undef SPI_MODE1
#undef SPI_MODE2
#undef SPI_MODE3
#define SPI_MODE0 0x02
#define SPI_MODE1 0x00
#define SPI_MODE2 0x03
#define SPI_MODE3 0x01

#undef SPI_CLOCK_DIV2
#undef SPI_CLOCK_DIV4
#undef SPI_CLOCK_DIV8
#undef SPI_CLOCK_DIV16
#undef SPI_CLOCK_DIV32
#undef SPI_CLOCK_DIV64
#undef SPI_CLOCK_DIV128
#define SPI_CLOCK_DIV2	 11
#define SPI_CLOCK_DIV4	 21
#define SPI_CLOCK_DIV8	 42
#define SPI_CLOCK_DIV16	 84
#define SPI_CLOCK_DIV32	 168
#define SPI_CLOCK_DIV64	 255
#define SPI_CLOCK_DIV128 255

enum SPITransferMode {
	SPI_CONTINUE,
	SPI_LAST
};

class SPISettings {
public:
	SPISettings(uint32_t clock, BitOrder bitOrder, uint8_t dataMode) {
		if (__builtin_constant_p(clock)) {
			init_AlwaysInline(clock, bitOrder, dataMode);
		} else {
			init_MightInline(clock, bitOrder, dataMode);
		}
	}
	SPISettings() {
		init_AlwaysInline(4000000, MSBFIRST, SPI_MODE0);
	}
private:
	void init_MightInline(uint32_t clock, BitOrder bitOrder, uint8_t dataMode) {
		init_AlwaysInline(clock, bitOrder, dataMode);
	}
	void init_AlwaysInline(uint32_t clock, BitOrder bitOrder, uint8_t dataMode)
	  __attribute__((__always_inline__)) {
		uint8_t div;
		border = bitOrder;
		if (__builtin_constant_p(clock)) {
			if      (clock >= F_CPU /   2) div =   2;
			else if (clock >= F_CPU /   3) div =   3;
			else if (clock >= F_CPU /   4) div =   4;
			else if (clock >= F_CPU /   5) div =   5;
			else if (clock >= F_CPU /   6) div =   6;
			else if (clock >= F_CPU /   7) div =   7;
			else if (clock >= F_CPU /   8) div =   8;
			else if (clock >= F_CPU /   9) div =   9;
			else if (clock >= F_CPU /  10) div =  10;
			else if (clock >= F_CPU /  11) div =  11;
			else if (clock >= F_CPU /  12) div =  12;
			else if (clock >= F_CPU /  13) div =  13;
			else if (clock >= F_CPU /  14) div =  14;
			else if (clock >= F_CPU /  15) div =  15;
			else if (clock >= F_CPU /  16) div =  16;
			else if (clock >= F_CPU /  17) div =  17;
			else if (clock >= F_CPU /  18) div =  18;
			else if (clock >= F_CPU /  19) div =  19;
			else if (clock >= F_CPU /  20) div =  20;
			else if (clock >= F_CPU /  21) div =  21;
			else if (clock >= F_CPU /  22) div =  22;
			else if (clock >= F_CPU /  23) div =  23;
			else if (clock >= F_CPU /  24) div =  24;
			else if (clock >= F_CPU /  25) div =  25;
			else if (clock >= F_CPU /  26) div =  26;
			else if (clock >= F_CPU /  27) div =  27;
			else if (clock >= F_CPU /  28) div =  28;
			else if (clock >= F_CPU /  29) div =  29;
			else if (clock >= F_CPU /  30) div =  30;
			else if (clock >= F_CPU /  31) div =  31;
			else if (clock >= F_CPU /  32) div =  32;
			else if (clock >= F_CPU /  33) div =  33;
			else if (clock >= F_CPU /  34) div =  34;
			else if (clock >= F_CPU /  35) div =  35;
			else if (clock >= F_CPU /  36) div =  36;
			else if (clock >= F_CPU /  37) div =  37;
			else if (clock >= F_CPU /  38) div =  38;
			else if (clock >= F_CPU /  39) div =  39;
			else if (clock >= F_CPU /  40) div =  40;
			else if (clock >= F_CPU /  41) div =  41;
			else if (clock >= F_CPU /  42) div =  42;
			else if (clock >= F_CPU /  43) div =  43;
			else if (clock >= F_CPU /  44) div =  44;
			else if (clock >= F_CPU /  45) div =  45;
			else if (clock >= F_CPU /  46) div =  46;
			else if (clock >= F_CPU /  47) div =  47;
			else if (clock >= F_CPU /  48) div =  48;
			else if (clock >= F_CPU /  49) div =  49;
			else if (clock >= F_CPU /  50) div =  50;
			else if (clock >= F_CPU /  51) div =  51;
			else if (clock >= F_CPU /  52) div =  52;
			else if (clock >= F_CPU /  53) div =  53;
			else if (clock >= F_CPU /  54) div =  54;
			else if (clock >= F_CPU /  55) div =  55;
			else if (clock >= F_CPU /  56) div =  56;
			else if (clock >= F_CPU /  57) div =  57;
			else if (clock >= F_CPU /  58) div =  58;
			else if (clock >= F_CPU /  59) div =  59;
			else if (clock >= F_CPU /  60) div =  60;
			else if (clock >= F_CPU /  61) div =  61;
			else if (clock >= F_CPU /  62) div =  62;
			else if (clock >= F_CPU /  63) div =  63;
			else if (clock >= F_CPU /  64) div =  64;
			else if (clock >= F_CPU /  65) div =  65;
			else if (clock >= F_CPU /  66) div =  66;
			else if (clock >= F_CPU /  67) div =  67;
			else if (clock >= F_CPU /  68) div =  68;
			else if (clock >= F_CPU /  69) div =  69;
			else if (clock >= F_CPU /  70) div =  70;
			else if (clock >= F_CPU /  71) div =  71;
			else if (clock >= F_CPU /  72) div =  72;
			else if (clock >= F_CPU /  73) div =  73;
			else if (clock >= F_CPU /  74) div =  74;
			else if (clock >= F_CPU /  75) div =  75;
			else if (clock >= F_CPU /  76) div =  76;
			else if (clock >= F_CPU /  77) div =  77;
			else if (clock >= F_CPU /  78) div =  78;
			else if (clock >= F_CPU /  79) div =  79;
			else if (clock >= F_CPU /  80) div =  80;
			else if (clock >= F_CPU /  81) div =  81;
			else if (clock >= F_CPU /  82) div =  82;
			else if (clock >= F_CPU /  83) div =  83;
			else if (clock >= F_CPU /  84) div =  84;
			else if (clock >= F_CPU /  85) div =  85;
			else if (clock >= F_CPU /  86) div =  86;
			else if (clock >= F_CPU /  87) div =  87;
			else if (clock >= F_CPU /  88) div =  88;
			else if (clock >= F_CPU /  89) div =  89;
			else if (clock >= F_CPU /  90) div =  90;
			else if (clock >= F_CPU /  91) div =  91;
			else if (clock >= F_CPU /  92) div =  92;
			else if (clock >= F_CPU /  93) div =  93;
			else if (clock >= F_CPU /  94) div =  94;
			else if (clock >= F_CPU /  95) div =  95;
			else if (clock >= F_CPU /  96) div =  96;
			else if (clock >= F_CPU /  97) div =  97;
			else if (clock >= F_CPU /  98) div =  98;
			else if (clock >= F_CPU /  99) div =  99;
			else if (clock >= F_CPU / 100) div = 100;
			else if (clock >= F_CPU / 101) div = 101;
			else if (clock >= F_CPU / 102) div = 102;
			else if (clock >= F_CPU / 103) div = 103;
			else if (clock >= F_CPU / 104) div = 104;
			else if (clock >= F_CPU / 105) div = 105;
			else if (clock >= F_CPU / 106) div = 106;
			else if (clock >= F_CPU / 107) div = 107;
			else if (clock >= F_CPU / 108) div = 108;
			else if (clock >= F_CPU / 109) div = 109;
			else if (clock >= F_CPU / 110) div = 110;
			else if (clock >= F_CPU / 111) div = 111;
			else if (clock >= F_CPU / 112) div = 112;
			else if (clock >= F_CPU / 113) div = 113;
			else if (clock >= F_CPU / 114) div = 114;
			else if (clock >= F_CPU / 115) div = 115;
			else if (clock >= F_CPU / 116) div = 116;
			else if (clock >= F_CPU / 117) div = 117;
			else if (clock >= F_CPU / 118) div = 118;
			else if (clock >= F_CPU / 119) div = 119;
			else if (clock >= F_CPU / 120) div = 120;
			else if (clock >= F_CPU / 121) div = 121;
			else if (clock >= F_CPU / 122) div = 122;
			else if (clock >= F_CPU / 123) div = 123;
			else if (clock >= F_CPU / 124) div = 124;
			else if (clock >= F_CPU / 125) div = 125;
			else if (clock >= F_CPU / 126) div = 126;
			else if (clock >= F_CPU / 127) div = 127;
			else if (clock >= F_CPU / 128) div = 128;
			else if (clock >= F_CPU / 129) div = 129;
			else if (clock >= F_CPU / 130) div = 130;
			else if (clock >= F_CPU / 131) div = 131;
			else if (clock >= F_CPU / 132) div = 132;
			else if (clock >= F_CPU / 133) div = 133;
			else if (clock >= F_CPU / 134) div = 134;
			else if (clock >= F_CPU / 135) div = 135;
			else if (clock >= F_CPU / 136) div = 136;
			else if (clock >= F_CPU / 137) div = 137;
			else if (clock >= F_CPU / 138) div = 138;
			else if (clock >= F_CPU / 139) div = 139;
			else if (clock >= F_CPU / 140) div = 140;
			else if (clock >= F_CPU / 141) div = 141;
			else if (clock >= F_CPU / 142) div = 142;
			else if (clock >= F_CPU / 143) div = 143;
			else if (clock >= F_CPU / 144) div = 144;
			else if (clock >= F_CPU / 145) div = 145;
			else if (clock >= F_CPU / 146) div = 146;
			else if (clock >= F_CPU / 147) div = 147;
			else if (clock >= F_CPU / 148) div = 148;
			else if (clock >= F_CPU / 149) div = 149;
			else if (clock >= F_CPU / 150) div = 150;
			else if (clock >= F_CPU / 151) div = 151;
			else if (clock >= F_CPU / 152) div = 152;
			else if (clock >= F_CPU / 153) div = 153;
			else if (clock >= F_CPU / 154) div = 154;
			else if (clock >= F_CPU / 155) div = 155;
			else if (clock >= F_CPU / 156) div = 156;
			else if (clock >= F_CPU / 157) div = 157;
			else if (clock >= F_CPU / 158) div = 158;
			else if (clock >= F_CPU / 159) div = 159;
			else if (clock >= F_CPU / 160) div = 160;
			else if (clock >= F_CPU / 161) div = 161;
			else if (clock >= F_CPU / 162) div = 162;
			else if (clock >= F_CPU / 163) div = 163;
			else if (clock >= F_CPU / 164) div = 164;
			else if (clock >= F_CPU / 165) div = 165;
			else if (clock >= F_CPU / 166) div = 166;
			else if (clock >= F_CPU / 167) div = 167;
			else if (clock >= F_CPU / 168) div = 168;
			else if (clock >= F_CPU / 169) div = 169;
			else if (clock >= F_CPU / 170) div = 170;
			else if (clock >= F_CPU / 171) div = 171;
			else if (clock >= F_CPU / 172) div = 172;
			else if (clock >= F_CPU / 173) div = 173;
			else if (clock >= F_CPU / 174) div = 174;
			else if (clock >= F_CPU / 175) div = 175;
			else if (clock >= F_CPU / 176) div = 176;
			else if (clock >= F_CPU / 177) div = 177;
			else if (clock >= F_CPU / 178) div = 178;
			else if (clock >= F_CPU / 179) div = 179;
			else if (clock >= F_CPU / 180) div = 180;
			else if (clock >= F_CPU / 181) div = 181;
			else if (clock >= F_CPU / 182) div = 182;
			else if (clock >= F_CPU / 183) div = 183;
			else if (clock >= F_CPU / 184) div = 184;
			else if (clock >= F_CPU / 185) div = 185;
			else if (clock >= F_CPU / 186) div = 186;
			else if (clock >= F_CPU / 187) div = 187;
			else if (clock >= F_CPU / 188) div = 188;
			else if (clock >= F_CPU / 189) div = 189;
			else if (clock >= F_CPU / 190) div = 190;
			else if (clock >= F_CPU / 191) div = 191;
			else if (clock >= F_CPU / 192) div = 192;
			else if (clock >= F_CPU / 193) div = 193;
			else if (clock >= F_CPU / 194) div = 194;
			else if (clock >= F_CPU / 195) div = 195;
			else if (clock >= F_CPU / 196) div = 196;
			else if (clock >= F_CPU / 197) div = 197;
			else if (clock >= F_CPU / 198) div = 198;
			else if (clock >= F_CPU / 199) div = 199;
			else if (clock >= F_CPU / 200) div = 200;
			else if (clock >= F_CPU / 201) div = 201;
			else if (clock >= F_CPU / 202) div = 202;
			else if (clock >= F_CPU / 203) div = 203;
			else if (clock >= F_CPU / 204) div = 204;
			else if (clock >= F_CPU / 205) div = 205;
			else if (clock >= F_CPU / 206) div = 206;
			else if (clock >= F_CPU / 207) div = 207;
			else if (clock >= F_CPU / 208) div = 208;
			else if (clock >= F_CPU / 209) div = 209;
			else if (clock >= F_CPU / 210) div = 210;
			else if (clock >= F_CPU / 211) div = 211;
			else if (clock >= F_CPU / 212) div = 212;
			else if (clock >= F_CPU / 213) div = 213;
			else if (clock >= F_CPU / 214) div = 214;
			else if (clock >= F_CPU / 215) div = 215;
			else if (clock >= F_CPU / 216) div = 216;
			else if (clock >= F_CPU / 217) div = 217;
			else if (clock >= F_CPU / 218) div = 218;
			else if (clock >= F_CPU / 219) div = 219;
			else if (clock >= F_CPU / 220) div = 220;
			else if (clock >= F_CPU / 221) div = 221;
			else if (clock >= F_CPU / 222) div = 222;
			else if (clock >= F_CPU / 223) div = 223;
			else if (clock >= F_CPU / 224) div = 224;
			else if (clock >= F_CPU / 225) div = 225;
			else if (clock >= F_CPU / 226) div = 226;
			else if (clock >= F_CPU / 227) div = 227;
			else if (clock >= F_CPU / 228) div = 228;
			else if (clock >= F_CPU / 229) div = 229;
			else if (clock >= F_CPU / 230) div = 230;
			else if (clock >= F_CPU / 231) div = 231;
			else if (clock >= F_CPU / 232) div = 232;
			else if (clock >= F_CPU / 233) div = 233;
			else if (clock >= F_CPU / 234) div = 234;
			else if (clock >= F_CPU / 235) div = 235;
			else if (clock >= F_CPU / 236) div = 236;
			else if (clock >= F_CPU / 237) div = 237;
			else if (clock >= F_CPU / 238) div = 238;
			else if (clock >= F_CPU / 239) div = 239;
			else if (clock >= F_CPU / 240) div = 240;
			else if (clock >= F_CPU / 241) div = 241;
			else if (clock >= F_CPU / 242) div = 242;
			else if (clock >= F_CPU / 243) div = 243;
			else if (clock >= F_CPU / 244) div = 244;
			else if (clock >= F_CPU / 245) div = 245;
			else if (clock >= F_CPU / 246) div = 246;
			else if (clock >= F_CPU / 247) div = 247;
			else if (clock >= F_CPU / 248) div = 248;
			else if (clock >= F_CPU / 249) div = 249;
			else if (clock >= F_CPU / 250) div = 250;
			else if (clock >= F_CPU / 251) div = 251;
			else if (clock >= F_CPU / 252) div = 252;
			else if (clock >= F_CPU / 253) div = 253;
			else if (clock >= F_CPU / 254) div = 254;
			else /*  clock >= F_CPU / 255 */ div = 255;
			/*
			#! /usr/bin/perl
			for ($i=2; $i<256; $i++) {
			  printf "\t\t\telse if (clock >= F_CPU / %3d) div = %3d;\n", $i, $i;
			}
			*/
		} else {
			for (div=2; div<255; div++) {
				if (clock >= F_CPU / div) break;
			}
		}
		config = (dataMode & 3) | SPI_CSR_CSAAT | SPI_CSR_SCBR(div) | SPI_CSR_DLYBCT(1);
	}
	uint32_t config;
	BitOrder border;
	friend class SPIClass;
};



class SPIClass {
  public:
	SPIClass(Spi *_spi, uint32_t _id, void(*_initCb)(void));

	byte transfer(uint8_t _data, SPITransferMode _mode = SPI_LAST) { return transfer(BOARD_SPI_DEFAULT_SS, _data, _mode); }
	byte transfer(byte _channel, uint8_t _data, SPITransferMode _mode = SPI_LAST);

	// Transaction Functions
	void usingInterrupt(uint8_t interruptNumber);
	void beginTransaction(uint8_t pin, SPISettings settings);
	void beginTransaction(SPISettings settings) {
		beginTransaction(BOARD_SPI_DEFAULT_SS, settings);
	}
	void endTransaction(void);

	// SPI Configuration methods
	void attachInterrupt(void);
	void detachInterrupt(void);

	void begin(void);
	void end(void);

	// Attach/Detach pin to/from SPI controller
	void begin(uint8_t _pin);
	void end(uint8_t _pin);

	// These methods sets a parameter on a single pin
	void setBitOrder(uint8_t _pin, BitOrder);
	void setDataMode(uint8_t _pin, uint8_t);
	void setClockDivider(uint8_t _pin, uint8_t);

	// These methods sets the same parameters but on default pin BOARD_SPI_DEFAULT_SS
	void setBitOrder(BitOrder _order) { setBitOrder(BOARD_SPI_DEFAULT_SS, _order); };
	void setDataMode(uint8_t _mode) { setDataMode(BOARD_SPI_DEFAULT_SS, _mode); };
	void setClockDivider(uint8_t _div) { setClockDivider(BOARD_SPI_DEFAULT_SS, _div); };

  private:
	void init();

	Spi *spi;
	uint32_t id;
	BitOrder bitOrder[SPI_CHANNELS_NUM];
	uint32_t divider[SPI_CHANNELS_NUM];
	uint32_t mode[SPI_CHANNELS_NUM];
	void (*initCb)(void);
	bool initialized;
	uint8_t interruptMode;  // 0=none, 1=mask, 2=global
	uint8_t interruptMask;  // bits 0:3=pin change
	uint8_t interruptSave;  // temp storage, to restore state
};






#endif



extern SPIClass SPI;
#if defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISL)
extern SPI1Class SPI1;
#endif
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
extern SPI1Class SPI1;
extern SPI2Class SPI2;
#endif
#endif
