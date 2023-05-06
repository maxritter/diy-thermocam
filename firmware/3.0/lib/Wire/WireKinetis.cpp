/* Wire Library for Teensy LC & 3.X
 * Copyright (c) 2014-2017, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this I2C library was funded by PJRC.COM, LLC by sales of
 * Teensy and related products.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Arduino.h>
#include "Wire.h"

#if defined(__arm__) && defined(TEENSYDUINO) && (defined(__MKL26Z64__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__))

#include "kinetis.h"
#include <string.h> // for memcpy
#include "core_pins.h"
#include "Wire.h"

// undefine these, so we can't accidentally access the hardware directly.
#undef I2C0_A1
#undef I2C0_F
#undef I2C0_C1
#undef I2C0_S
#undef I2C0_D
#undef I2C0_C2
#undef I2C0_FLT
#undef I2C0_RA
#undef I2C0_SMB
#undef I2C0_A2
#undef I2C0_SLTH
#undef I2C0_SLTL

void sda_rising_isr0(void);
void sda_rising_isr1(void);

void TwoWire::begin(void)
{
	//serial_begin(BAUD2DIV(115200));
	//serial_print("\nWire Begin\n");

	rxBufferIndex = 0;
	rxBufferLength = 0;
	txBufferIndex = 0;
	txBufferLength = 0;
	transmitting = 0;
	user_onRequest = NULL;
	user_onReceive = NULL;
	slave_mode = 0;
	hardware.clock_gate_register |= hardware.clock_gate_mask;
	port().C1 = 0;
	// On Teensy 3.0 external pullup resistors *MUST* be used
	// the PORT_PCR_PE bit is ignored when in I2C mode
	// I2C will not work at all without pullup resistors
	// It might seem like setting PORT_PCR_PE & PORT_PCR_PS
	// would enable pullup resistors.  However, there seems
	// to be a bug in chip while I2C is enabled, where setting
	// those causes the port to be driven strongly high.
	uint32_t mux;
	volatile uint32_t *reg;
	reg = portConfigRegister(hardware.sda_pin[sda_pin_index]);
	mux = PORT_PCR_MUX(hardware.sda_mux[sda_pin_index]);
	*reg = mux|PORT_PCR_ODE|PORT_PCR_SRE|PORT_PCR_DSE;
	reg = portConfigRegister(hardware.scl_pin[scl_pin_index]);
	mux = PORT_PCR_MUX(hardware.scl_mux[scl_pin_index]);
	*reg = mux|PORT_PCR_ODE|PORT_PCR_SRE|PORT_PCR_DSE;
	setClock(100000);
	port().C2 = I2C_C2_HDRS;
	port().C1 = I2C_C1_IICEN;
	//pinMode(3, OUTPUT);
	//pinMode(4, OUTPUT);
}

void TwoWire::setClock(uint32_t frequency)
{
	if (!(hardware.clock_gate_register & hardware.clock_gate_mask)) return;

#if F_BUS == 128000000
	if (frequency < 400000) {
		port().F = I2C_F_DIV1280; // 100 kHz
	} else if (frequency < 1000000) {
		port().F = I2C_F_DIV320; // 400 kHz
	} else {
		port().F = I2C_F_DIV128; // 1 MHz
	}
	port().FLT = 4;
#elif F_BUS == 120000000
	if (frequency < 400000) {
		port().F = I2C_F_DIV1152; // 104 kHz
	} else if (frequency < 1000000) {
		port().F = I2C_F_DIV288; // 416 kHz
	} else {
		port().F = I2C_F_DIV128; // 0.94 MHz
	}
	port().FLT = 4;
#elif F_BUS == 108000000
	if (frequency < 400000) {
		port().F = I2C_F_DIV1024; // 105 kHz
	} else if (frequency < 1000000) {
		port().F = I2C_F_DIV256; //  422 kHz
	} else {
		port().F = I2C_F_DIV112; // 0.96 MHz
	}
	port().FLT = 4;
#elif F_BUS == 96000000
	if (frequency < 400000) {
		port().F = I2C_F_DIV960; // 100 kHz
	} else if (frequency < 1000000) {
		port().F = I2C_F_DIV240; // 400 kHz
	} else {
		port().F = I2C_F_DIV96; // 1.0 MHz
	}
	port().FLT = 4;
#elif F_BUS == 90000000
	if (frequency < 400000) {
		port().F = I2C_F_DIV896; // 100 kHz
	} else if (frequency < 1000000) {
		port().F = I2C_F_DIV224; // 402 kHz
	} else {
		port().F = I2C_F_DIV88; // 1.02 MHz
	}
	port().FLT = 4;
#elif F_BUS == 80000000
	if (frequency < 400000) {
		port().F = I2C_F_DIV768; // 104 kHz
	} else if (frequency < 1000000) {
		port().F = I2C_F_DIV192; // 416 kHz
	} else {
		port().F = I2C_F_DIV80; // 1.0 MHz
	}
	port().FLT = 4;
#elif F_BUS == 72000000
	if (frequency < 400000) {
		port().F = I2C_F_DIV640; // 112 kHz
	} else if (frequency < 1000000) {
		port().F = I2C_F_DIV192; // 375 kHz
	} else {
		port().F = I2C_F_DIV72; // 1.0 MHz
	}
	port().FLT = 4;
#elif F_BUS == 64000000
	if (frequency < 400000) {
		port().F = I2C_F_DIV640; // 100 kHz
	} else if (frequency < 1000000) {
		port().F = I2C_F_DIV160; // 400 kHz
	} else {
		port().F = I2C_F_DIV64; // 1.0 MHz
	}
	port().FLT = 4;
#elif F_BUS == 60000000
	if (frequency < 400000) {
		port().F = 0x2C;	// 104 kHz
	} else if (frequency < 1000000) {
		port().F = 0x1C; // 416 kHz
	} else {
		port().F = 0x12; // 938 kHz
	}
	port().FLT = 4;
#elif F_BUS == 56000000
	if (frequency < 400000) {
		port().F = 0x2B;	// 109 kHz
	} else if (frequency < 1000000) {
		port().F = 0x1C; // 389 kHz
	} else {
		port().F = 0x0E; // 1 MHz
	}
	port().FLT = 4;
#elif F_BUS == 54000000
	if (frequency < 400000) {
		port().F = I2C_F_DIV512;	// 105 kHz
	} else if (frequency < 1000000) {
		port().F = I2C_F_DIV128; // 422 kHz
	} else {
		port().F = I2C_F_DIV56; // 0.96 MHz
	}
	port().FLT = 4;
#elif F_BUS == 48000000
	if (frequency < 400000) {
		port().F = 0x27;	// 100 kHz
	} else if (frequency < 1000000) {
		port().F = 0x1A; // 400 kHz
	} else {
		port().F = 0x0D; // 1 MHz
	}
	port().FLT = 4;
#elif F_BUS == 40000000
	if (frequency < 400000) {
		port().F = 0x29;	// 104 kHz
	} else if (frequency < 1000000) {
		port().F = 0x19; // 416 kHz
	} else {
		port().F = 0x0B; // 1 MHz
	}
	port().FLT = 3;
#elif F_BUS == 36000000
	if (frequency < 400000) {
		port().F = 0x28;	// 113 kHz
	} else if (frequency < 1000000) {
		port().F = 0x19; // 375 kHz
	} else {
		port().F = 0x0A; // 1 MHz
	}
	port().FLT = 3;
#elif F_BUS == 24000000
	if (frequency < 400000) {
		port().F = 0x1F; // 100 kHz
	} else if (frequency < 1000000) {
		port().F = 0x12; // 375 kHz
	} else {
		port().F = 0x02; // 1 MHz
	}
	port().FLT = 2;
#elif F_BUS == 16000000
	if (frequency < 400000) {
		port().F = 0x20; // 100 kHz
	} else if (frequency < 1000000) {
		port().F = 0x07; // 400 kHz
	} else {
		port().F = 0x00; // 800 kHz
	}
	port().FLT = 1;
#elif F_BUS == 8000000
	if (frequency < 400000) {
		port().F = 0x14; // 100 kHz
	} else {
		port().F = 0x00; // 400 kHz
	}
	port().FLT = 1;
#elif F_BUS == 4000000
	if (frequency < 400000) {
		port().F = 0x07; // 100 kHz
	} else {
		port().F = 0x00; // 200 kHz
	}
	port().FLT = 1;
#elif F_BUS == 2000000
	port().F = 0x00; // 100 kHz
	port().FLT = 1;
#else
#error "F_BUS must be 128, 120, 108, 96, 90, 80, 72, 64, 60, 56, 54, 48, 40, 36, 24, 16, 8, 4 or 2 MHz"
#endif
}

void TwoWire::setSDA(uint8_t pin)
{
	if (pin == hardware.sda_pin[sda_pin_index]) return;
	uint32_t newindex=0;
	while (1) {
		uint32_t sda_pin = hardware.sda_pin[newindex];
		if (sda_pin == 255) return;
		if (sda_pin == pin) break;
		if (++newindex >= sizeof(hardware.sda_pin)) return;
	}
	if ((hardware.clock_gate_register & hardware.clock_gate_mask)) {
		volatile uint32_t *reg;
		reg = portConfigRegister(hardware.sda_pin[sda_pin_index]);
		*reg = 0;
		reg = portConfigRegister(hardware.sda_pin[newindex]);
		uint32_t mux = PORT_PCR_MUX(hardware.sda_mux[newindex]);
		*reg = mux|PORT_PCR_ODE|PORT_PCR_SRE|PORT_PCR_DSE;
	}
	sda_pin_index = newindex;
}

void TwoWire::setSCL(uint8_t pin)
{
	if (pin == hardware.scl_pin[scl_pin_index]) return;
	uint32_t newindex=0;
	while (1) {
		uint32_t scl_pin = hardware.scl_pin[newindex];
		if (scl_pin == 255) return;
		if (scl_pin == pin) break;
		if (++newindex >= sizeof(hardware.scl_pin)) return;
	}
	if ((hardware.clock_gate_register & hardware.clock_gate_mask)) {
		volatile uint32_t *reg;
		reg = portConfigRegister(hardware.scl_pin[scl_pin_index]);
		*reg = 0;
		reg = portConfigRegister(hardware.scl_pin[newindex]);
		uint32_t mux = PORT_PCR_MUX(hardware.scl_mux[newindex]);
		*reg = mux|PORT_PCR_ODE|PORT_PCR_SRE|PORT_PCR_DSE;
	}
	scl_pin_index = newindex;
}

void TwoWire::begin(uint8_t address)
{
	begin();
	port().A1 = address << 1;
	slave_mode = 1;
	port().C1 = I2C_C1_IICEN | I2C_C1_IICIE;
	NVIC_ENABLE_IRQ(hardware.irq);
}

void TwoWire::end()
{
	if (!(hardware.clock_gate_register & hardware.clock_gate_mask)) return;
	NVIC_DISABLE_IRQ(hardware.irq);
	// TODO: should this try to create a stop condition??
	port().C1 = 0;
	volatile uint32_t *reg;
	reg = portConfigRegister(hardware.scl_pin[scl_pin_index]);
	*reg = 0;
	reg = portConfigRegister(hardware.sda_pin[sda_pin_index]);
	*reg = 0;
	hardware.clock_gate_register &= ~hardware.clock_gate_mask;
}


void TwoWire::isr(void)
{
	uint8_t status, c1, data;
	static uint8_t receiving=0;

	status = port().S;
	//serial_print(".");
	if (status & I2C_S_ARBL) {
		// Arbitration Lost
		port().S = I2C_S_ARBL;
		//serial_print("a");
		if (receiving && rxBufferLength > 0) {
			// TODO: does this detect the STOP condition in slave receive mode?


		}
		if (!(status & I2C_S_IAAS)) return;
	}
	if (status & I2C_S_IAAS) {
		//serial_print("\n");
		// Addressed As A Slave
		if (status & I2C_S_SRW) {
			//serial_print("T");
			// Begin Slave Transmit
			receiving = 0;
			txBufferLength = 0;
			if (user_onRequest != NULL) {
				user_onRequest();
			}
			if (txBufferLength == 0) {
				// is this correct, transmitting a single zero
				// when we should send nothing?  Arduino's AVR
				// implementation does this, but is it ok?
				txBufferLength = 1;
				txBuffer[0] = 0;
			}
			port().C1 = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_TX;
			port().D = txBuffer[0];
			txBufferIndex = 1;
		} else {
			// Begin Slave Receive
			//serial_print("R");
			receiving = 1;
			rxBufferLength = 0;
			port().C1 = I2C_C1_IICEN | I2C_C1_IICIE;
			data = port().D;
		}
		port().S = I2C_S_IICIF;
		return;
	}
	#if defined(WIRE_HAS_STOP_INTERRUPT)
	c1 = port().FLT;
	if ((c1 & I2C_FLT_STOPF) && (c1 & I2C_FLT_STOPIE)) {
		port().FLT = c1 & ~I2C_FLT_STOPIE;
		if (user_onReceive != NULL) {
			rxBufferIndex = 0;
			user_onReceive(rxBufferLength);
		}
	}
	#endif
	c1 = port().C1;
	if (c1 & I2C_C1_TX) {
		// Continue Slave Transmit
		//serial_print("t");
		if ((status & I2C_S_RXAK) == 0) {
			//serial_print(".");
			// Master ACK'd previous byte
			if (txBufferIndex < txBufferLength) {
				port().D = txBuffer[txBufferIndex++];
			} else {
				port().D = 0;
			}
			port().C1 = I2C_C1_IICEN | I2C_C1_IICIE | I2C_C1_TX;
		} else {
			//serial_print("*");
			// Master did not ACK previous byte
			port().C1 = I2C_C1_IICEN | I2C_C1_IICIE;
			data = port().D;
		}
	} else {
		// Continue Slave Receive
		irqcount = 0;
		#ifdef WIRE_HAS_STOP_INTERRUPT
		port().FLT |= I2C_FLT_STOPIE;
		#else
		#if defined(WIRE_IMPLEMENT_WIRE) && !defined(WIRE_IMPLEMENT_WIRE1)
		attachInterrupt(hardware.sda_pin[sda_pin_index], sda_rising_isr0, RISING);
		#elif !defined(WIRE_IMPLEMENT_WIRE) && defined(WIRE_IMPLEMENT_WIRE1)
		attachInterrupt(hardware.sda_pin[sda_pin_index], sda_rising_isr1, RISING);
		#elif defined(WIRE_IMPLEMENT_WIRE) && defined(WIRE_IMPLEMENT_WIRE1)
		if (this == &Wire) {
			attachInterrupt(hardware.sda_pin[sda_pin_index], sda_rising_isr0, RISING);
		} else if (this == &Wire1) {
			attachInterrupt(hardware.sda_pin[sda_pin_index], sda_rising_isr1, RISING);
		}
		#endif
		#endif // WIRE_HAS_STOP_INTERRUPT
		//digitalWriteFast(4, HIGH);
		data = port().D;
		//serial_phex(data);
		if (rxBufferLength < BUFFER_LENGTH && receiving) {
			rxBuffer[rxBufferLength++] = data;
		}
		//digitalWriteFast(4, LOW);
	}
	port().S = I2C_S_IICIF;
}


// Detects the stop condition that terminates a slave receive transfer.
// Sadly, the I2C in older Kinetis K series lacks the stop detect interrupt
// This pin change interrupt hack is needed to detect the stop condition
#if !defined(WIRE_HAS_STOP_INTERRUPT)

#if defined(WIRE_IMPLEMENT_WIRE)
void sda_rising_isr0(void)
{
	Wire.sda_rising_isr();
}
#endif
#if defined(WIRE_IMPLEMENT_WIRE1)
void sda_rising_isr1(void)
{
	Wire1.sda_rising_isr();
}
#endif

void TwoWire::sda_rising_isr(void)
{
	//digitalWrite(3, HIGH);
	if (!(port().S & I2C_S_BUSY)) {
		detachInterrupt(hardware.sda_pin[sda_pin_index]);
		if (user_onReceive != NULL) {
			rxBufferIndex = 0;
			user_onReceive(rxBufferLength);
		}
		//delayMicroseconds(100);
	} else {
		if (++irqcount >= 2 || !slave_mode) {
			detachInterrupt(hardware.sda_pin[sda_pin_index]);
		}
	}
	//digitalWrite(3, LOW);
}
#endif // !WIRE_HAS_STOP_INTERRUPT


// Chapter 44: Inter-Integrated Circuit (I2C) - Page 1012
//  I2C0_A1      // I2C Address Register 1
//  I2C0_F       // I2C Frequency Divider register
//  I2C0_C1      // I2C Control Register 1
//  I2C0_S       // I2C Status register
//  I2C0_D       // I2C Data I/O register
//  I2C0_C2      // I2C Control Register 2
//  I2C0_FLT     // I2C Programmable Input Glitch Filter register

size_t TwoWire::write(uint8_t data)
{
	if (transmitting || slave_mode) {
		if (txBufferLength >= BUFFER_LENGTH+1) {
			setWriteError();
			return 0;
		}
		txBuffer[txBufferLength++] = data;
		return 1;
	}
	return 0;
}

size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
	if (transmitting || slave_mode) {
		size_t avail = BUFFER_LENGTH+1 - txBufferLength;
		if (quantity > avail) {
			quantity = avail;
			setWriteError();
		}
		memcpy(txBuffer + txBufferLength, data, quantity);
		txBufferLength += quantity;
		return quantity;
	}
	return 0;
}

bool TwoWire::wait_idle(void)
{
	bool reset=false;
	uint32_t wait_begin = millis();

	//Serial.print("busy:");
	while (i2c_status() & I2C_S_BUSY) {
		//Serial.write('.') ;
		uint32_t waited = millis() - wait_begin;
#if 1
		if (waited > 15 && !reset) {
			reset = true;
			//Serial.println("attempt forced reset");
			uint8_t sda_pin = hardware.sda_pin[sda_pin_index];
			pinMode(sda_pin, INPUT_DISABLE);
			uint8_t scl_pin = hardware.scl_pin[sda_pin_index];
			pinMode(scl_pin, OUTPUT);
			for (int i=0; i < 9; i++) {
				digitalWrite(scl_pin, LOW);
				delayMicroseconds(5);
				digitalWrite(scl_pin, HIGH);
				delayMicroseconds(5);
			}
			uint32_t mux;
			volatile uint32_t *reg;
			reg = portConfigRegister(hardware.sda_pin[sda_pin_index]);
			mux = PORT_PCR_MUX(hardware.sda_mux[sda_pin_index]);
			*reg = mux|PORT_PCR_ODE|PORT_PCR_SRE|PORT_PCR_DSE;
			reg = portConfigRegister(hardware.scl_pin[scl_pin_index]);
			mux = PORT_PCR_MUX(hardware.scl_mux[scl_pin_index]);
			*reg = mux|PORT_PCR_ODE|PORT_PCR_SRE|PORT_PCR_DSE;
			delayMicroseconds(10);
			continue;
		}
#endif
		if (waited > 16) {
			// bus stuck busy too long
			port().C1 = 0;
			port().C1 = I2C_C1_IICEN;
			//Serial.println("abort");
			//return 4; // timeout waiting for bus
			return false;
		}
	}
	return true;
}

uint8_t TwoWire::endTransmission(uint8_t sendStop)
{
	uint8_t i, status, ret=0;
	uint32_t wait_begin;

	// clear the status flags
	port().S = I2C_S_IICIF | I2C_S_ARBL;
	// now take control of the bus...
	if (port().C1 & I2C_C1_MST) {
		// we are already the bus master, so send a repeated start
		//Serial.print("rstart:");
		port().C1 = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_RSTA | I2C_C1_TX;
	} else {
		// we are not currently the bus master, so wait for bus ready
		if (!wait_idle()) {
			//Serial.printf("endTransmission err1\n");
			return 4; // timeout waiting for bus
		}
		// become the bus master in transmit mode (send start)
		slave_mode = 0;
		port().C1 = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX;
	}
	// wait until start condition establishes control of the bus
	wait_begin = millis();
	while (1) {
		status = i2c_status();
		if ((status & I2C_S_BUSY)) break;
		//Serial.write('*') ;
		if (millis() - wait_begin > 4) {
			port().C1 = 0;
			port().C1 = I2C_C1_IICEN;
			//Serial.println("abort2");
			//Serial.printf("endTransmission err2\n");
			return 4; // error generating start condition
		}
	}
	// transmit the address and data
	for (i=0; i < txBufferLength; i++) {
		port().D = txBuffer[i];
		//Serial.write('^');
		wait_begin = millis();
		while (1) {
			status = i2c_status();
			if ((status & I2C_S_IICIF)) break;
			if (!(status & I2C_S_BUSY)) break;
			if (millis() - wait_begin > 5) {
				port().C1 = 0;
				port().C1 = I2C_C1_IICEN;
				//Serial.println("abort3");
				//Serial.printf("endTransmission err3\n");
				return 4; // clock stretch too long
			}
		}
		port().S = I2C_S_IICIF;
		//Serial.write('$');
		status = i2c_status();
		if ((status & I2C_S_ARBL)) {
			// we lost bus arbitration to another master
			// TODO: what is the proper thing to do here??
			//Serial.printf(" c1=%02X ", port().C1);
			port().C1 = I2C_C1_IICEN;
			//Serial.printf("endTransmission err4\n");
			ret = 4; // 4:other error
			break;
		}
		if (!(status & I2C_S_BUSY)) {
			// suddenly lost control of the bus!
			port().C1 = I2C_C1_IICEN;
			//Serial.printf("endTransmission err5\n");
			ret = 4; // 4:other error
			break;
		}
		if (status & I2C_S_RXAK) {
			// the slave device did not acknowledge
			if (i == 0) {
				//Serial.printf("endTransmission err6\n");
				ret = 2; // 2:received NACK on transmit of address
			} else {
				//Serial.printf("endTransmission err7\n");
				ret = 3; // 3:received NACK on transmit of data 
			}
			sendStop = 1;
			break;
		}
	}
	if (sendStop) {
		// send the stop condition
		port().C1 = I2C_C1_IICEN;
		// TODO: do we wait for this somehow?
	}
	transmitting = 0;
	//Serial.print(" ret=");
	//Serial.println(ret);
	return ret;
}


uint8_t TwoWire::requestFrom(uint8_t address, uint8_t length, uint8_t sendStop)
{
	uint8_t tmp __attribute__((unused));
	uint8_t status, count=0;
	uint32_t wait_begin;

	rxBufferIndex = 0;
	rxBufferLength = 0;
	//serial_print("requestFrom\n");
	// clear the status flags
	port().S = I2C_S_IICIF | I2C_S_ARBL;
	// now take control of the bus...
	if (port().C1 & I2C_C1_MST) {
		// we are already the bus master, so send a repeated start
		port().C1 = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_RSTA | I2C_C1_TX;
	} else {
		// we are not currently the bus master, so wait for bus ready
		if (!wait_idle()) {
			//Serial.printf("requestFrom err1\n");
			return 0; // timeout waiting for bus
		}
		// become the bus master in transmit mode (send start)
		slave_mode = 0;
		port().C1 = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX;
	}

	// wait until start condition establishes control of the bus
	wait_begin = millis();
	while (1) {
		status = i2c_status();
		if ((status & I2C_S_BUSY)) break;
		if (millis() - wait_begin > 4) {
			port().C1 = 0;
			port().C1 = I2C_C1_IICEN;
			//Serial.printf("requestFrom err2\n");
			return 0; // error generating start condition
		}
	}
	// send the address
	port().D = (address << 1) | 1;
	wait_begin = millis();
	while (!(port().S & I2C_S_IICIF)) {
		if (millis() - wait_begin > 5) {
			port().C1 = 0;
			port().C1 = I2C_C1_IICEN;
			//Serial.printf("requestFrom err3\n");
			return 0; // clock stretch too long (during address)
		}
	}
	port().S = I2C_S_IICIF;
	status = i2c_status();
	if ((status & I2C_S_RXAK) || (status & I2C_S_ARBL)) {
		// the slave device did not acknowledge
		// or we lost bus arbitration to another master
		port().C1 = I2C_C1_IICEN;
		//Serial.printf("requestFrom err4\n");
		return 0;
	}
	if (length == 0) {
		// TODO: does anybody really do zero length reads?
		// if so, does this code really work?
		port().C1 = I2C_C1_IICEN | (sendStop ? 0 : I2C_C1_MST);
		//Serial.printf("requestFrom err5\n");
		return 0;
	} else if (length == 1) {
		port().C1 = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TXAK;
	} else {
		port().C1 = I2C_C1_IICEN | I2C_C1_MST;
	}
	tmp = port().D; // initiate the first receive
	//delayMicroseconds(250);
	while (length > 1) {
		wait_begin = millis();
		while (!(port().S & I2C_S_IICIF)) {
			if (millis() - wait_begin > 5) {
				port().C1 = 0;
				port().C1 = I2C_C1_IICEN;
				rxBufferLength = count;
				//Serial.printf("requestFrom err6\n");
				return count; // clock stretch too long (during data)
			}
		}
		port().S = I2C_S_IICIF;
		status = port().S;
		if ((status & I2C_S_ARBL)) {
			// we lost bus arbitration to another master
			// or suddenly lost control of the bus!
			// TODO: what is the proper thing to do here??
			//Serial.printf("requestFrom err7a\n");
			return count;
		}
		if (!(status & I2C_S_BUSY)) {
			// we lost bus arbitration to another master
			// or suddenly lost control of the bus!
			// TODO: what is the proper thing to do here??
			//Serial.printf("requestFrom err7b\n");
			return count;
		}
		length--;
		if (length == 1) port().C1 = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TXAK;
		if (count < BUFFER_LENGTH) {
			rxBuffer[count++] = port().D;
		} else {
			tmp = port().D;
		}
	}
	wait_begin = millis();
	while (!(port().S & I2C_S_IICIF)) {
		if (millis() - wait_begin > 5) {
			port().C1 = 0;
			port().C1 = I2C_C1_IICEN;
			rxBufferLength = count;
			//Serial.printf("requestFrom err8\n");
			return count; // clock stretch too long (during data)
		}
	}
	port().S = I2C_S_IICIF;
	status = port().S;
	if ((status & I2C_S_ARBL)) {
		// we lost bus arbitration to another master
		// or suddenly lost control of the bus!
		// TODO: what is the proper thing to do here??
		//digitalWriteFast(13, HIGH);
		port().S = I2C_S_ARBL;
		delayMicroseconds(5);
		port().C1 &= ~I2C_C1_TXAK;
		//Serial.printf("requestFrom err9a\n");
		return count;
	}
	if (!(status & I2C_S_BUSY)) {
		// we lost bus arbitration to another master
		// or suddenly lost control of the bus!
		// TODO: what is the proper thing to do here??
		//Serial.printf("requestFrom err9b\n");
		return count;
	}
	port().C1 = I2C_C1_IICEN | I2C_C1_MST | I2C_C1_TX;
	if (count < BUFFER_LENGTH) {
		rxBuffer[count++] = port().D;
	} else {
		tmp = port().D;
	}
#if F_CPU > 120000000
	__asm__("nop");
	__asm__("nop");
	__asm__("nop");
#endif
	if (sendStop) port().C1 = I2C_C1_IICEN;
	rxBufferLength = count;
	return count;
}

uint8_t TwoWire::requestFrom(uint8_t addr, uint8_t qty, uint32_t iaddr, uint8_t n, uint8_t stop)
{
	if (n > 0) {
		union { uint32_t ul; uint8_t b[4]; } iaddress;
		iaddress.ul = iaddr;
		beginTransmission(addr);
		if (n > 3) n = 3;
		do {
			n = n - 1;
			write(iaddress.b[n]);
		} while (n > 0);
		endTransmission(false);
	}
	if (qty > BUFFER_LENGTH) qty = BUFFER_LENGTH;
	return requestFrom(addr, qty, stop);
}

// for compatibility with examples that directly call this AVR-specific function
// https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout/wiring-and-test
// https://forum.pjrc.com/threads/44922-Undefined-reference-to-twi_writeTo
extern "C"
uint8_t twi_writeTo(uint8_t address, uint8_t* data, uint8_t length, uint8_t wait, uint8_t sendStop)
{
	if (!wait) return 4;
	Wire.beginTransmission(address);
	while (length) {
		Wire.write(*data++);
		length--;
	}
	return Wire.endTransmission(sendStop);
}

constexpr TwoWire::I2C_Hardware_t TwoWire::i2c0_hardware = {
	SIM_SCGC4, SIM_SCGC4_I2C0,
#if defined(__MKL26Z64__) || defined(__MK20DX128__) || defined(__MK20DX256__)
	18, 17, 255, 255, 255,
	2, 2, 0, 0, 0,
	19, 16, 255, 255, 255,
	2, 2, 0, 0, 0,
#elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
	18, 17, 34, 8, 48,
	2, 2, 5, 7, 2,
	19, 16, 33, 7, 47,
	2, 2, 5, 7, 2,
#endif
	IRQ_I2C0
};

#if defined(__MKL26Z64__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
constexpr TwoWire::I2C_Hardware_t TwoWire::i2c1_hardware = {
	SIM_SCGC4, SIM_SCGC4_I2C1,
#if defined(__MKL26Z64__)
	23, 255, 255, 255, 255,
	2, 0, 0, 0, 0,
	22, 255, 255, 255, 255,
	2, 0, 0, 0, 0,
#elif defined(__MK20DX256__)
	30, 255, 255, 255, 255,
	2, 0, 0, 0, 0,
	29, 255, 255, 255, 255,
	2, 0, 0, 0, 0,
#elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
	38, 58, 255, 255, 255,
	2, 6, 0, 0, 0,
	37, 59, 255, 255, 255,
	2, 6, 0, 0, 0,
#endif
	IRQ_I2C1
};
#endif

#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
constexpr TwoWire::I2C_Hardware_t TwoWire::i2c2_hardware = {
	SIM_SCGC1, SIM_SCGC1_I2C2,
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
	4, 255, 255, 255, 255,
	5, 0, 0, 0, 0,
	3, 26, 255, 255, 255,
	5, 5, 0, 0, 0,
#endif
	IRQ_I2C2
};
#endif

#if defined(__MK66FX1M0__)
constexpr TwoWire::I2C_Hardware_t TwoWire::i2c3_hardware = {
	SIM_SCGC1, SIM_SCGC1_I2C3,
#if defined(__MK66FX1M0__)
	56, 255, 255, 255, 255,
	2, 0, 0, 0, 0,
	57, 255, 255, 255, 255,
	2, 0, 0, 0, 0,
#endif
	IRQ_I2C3
};
#endif

// Helper to transform a non-constant expression of the form
// &(*(KINETIS_I2C_t *)0x40066000)
// into a compile time constant.
#define MAKE_CONST(x) (__builtin_constant_p(x) ? (x) : (x))

#ifdef WIRE_IMPLEMENT_WIRE
constexpr uintptr_t i2c0_addr = uintptr_t(MAKE_CONST(&KINETIS_I2C0));
TwoWire Wire(i2c0_addr, TwoWire::i2c0_hardware);
void i2c0_isr(void) { Wire.isr(); }
#endif
#ifdef WIRE_IMPLEMENT_WIRE1
constexpr uintptr_t i2c1_addr = uintptr_t(MAKE_CONST(&KINETIS_I2C1));
TwoWire Wire1(i2c1_addr, TwoWire::i2c1_hardware);
void i2c1_isr(void) { Wire1.isr(); }
#endif
#ifdef WIRE_IMPLEMENT_WIRE2
constexpr uintptr_t i2c2_addr = uintptr_t(MAKE_CONST(&KINETIS_I2C2));
TwoWire Wire2(i2c2_addr, TwoWire::i2c2_hardware);
void i2c2_isr(void) { Wire2.isr(); }
#endif
#ifdef WIRE_IMPLEMENT_WIRE3
constexpr uintptr_t i2c3_addr = uintptr_t(MAKE_CONST(&KINETIS_I2C3));
TwoWire Wire3(i2c3_addr, TwoWire::i2c3_hardware);
void i2c3_isr(void) { Wire3.isr(); }
#endif


#endif // __arm__ && TEENSYDUINO
