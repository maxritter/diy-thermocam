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

#ifndef TwoWireKinetis_h
#define TwoWireKinetis_h

#if defined(__arm__) && defined(TEENSYDUINO) && (defined(__MKL26Z64__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__))

#include <Arduino.h>
#include <stdint.h>

#define BUFFER_LENGTH 32
#define WIRE_HAS_END 1


// Teensy LC
#if defined(__MKL26Z64__)
#define WIRE_IMPLEMENT_WIRE
//Wire1 consumes precious memory on Teensy LC
//#define WIRE_IMPLEMENT_WIRE1
#define WIRE_HAS_STOP_INTERRUPT

// Teensy 3.0
#elif defined(__MK20DX128__)
#define WIRE_IMPLEMENT_WIRE

// Teensy 3.1 & 3.2
#elif defined(__MK20DX256__)
#define WIRE_IMPLEMENT_WIRE
#define WIRE_IMPLEMENT_WIRE1

// Teensy 3.5
#elif defined(__MK64FX512__)
#define WIRE_IMPLEMENT_WIRE
#define WIRE_IMPLEMENT_WIRE1
#define WIRE_IMPLEMENT_WIRE2
#define WIRE_HAS_START_INTERRUPT
#define WIRE_HAS_STOP_INTERRUPT

// Teensy 3.6
#elif defined(__MK66FX1M0__)
#define WIRE_IMPLEMENT_WIRE
#define WIRE_IMPLEMENT_WIRE1
#define WIRE_IMPLEMENT_WIRE2
//Wire3 is seldom used on Teensy 3.6
//#define WIRE_IMPLEMENT_WIRE3
#define WIRE_HAS_START_INTERRUPT
#define WIRE_HAS_STOP_INTERRUPT

#endif


class TwoWire : public Stream
{
public:
	// Hardware description struct
	typedef struct {
		volatile uint32_t &clock_gate_register;
		uint32_t clock_gate_mask;
		uint8_t  sda_pin[5];
		uint8_t  sda_mux[5];
		uint8_t  scl_pin[5];
		uint8_t  scl_mux[5];
		IRQ_NUMBER_t irq;
	} I2C_Hardware_t;
	static const I2C_Hardware_t i2c0_hardware;
	static const I2C_Hardware_t i2c1_hardware;
	static const I2C_Hardware_t i2c2_hardware;
	static const I2C_Hardware_t i2c3_hardware;
public:
	constexpr TwoWire(uintptr_t port_addr, const I2C_Hardware_t &myhardware)
		: port_addr(port_addr), hardware(myhardware) {
	}
	void begin();
	void begin(uint8_t address);
	void begin(int address) {
		begin((uint8_t)address);
	}
	void end();
	void setClock(uint32_t frequency);
	void setSDA(uint8_t pin);
	void setSCL(uint8_t pin);
	void beginTransmission(uint8_t address) {
		txBuffer[0] = (address << 1);
		transmitting = 1;
		txBufferLength = 1;
	}
	void beginTransmission(int address) {
		beginTransmission((uint8_t)address);
	}
	uint8_t endTransmission(uint8_t sendStop);
	uint8_t endTransmission(void) {
		return endTransmission(1);
	}
	uint8_t requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop);
	uint8_t requestFrom(uint8_t address, uint8_t quantity) {
		return requestFrom(address, quantity, (uint8_t)1);
	}
	uint8_t requestFrom(int address, int quantity, int sendStop) {
		return requestFrom((uint8_t)address, (uint8_t)quantity,
			(uint8_t)(sendStop ? 1 : 0));
	}
	uint8_t requestFrom(int address, int quantity) {
		return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)1);
	}
	uint8_t requestFrom(uint8_t addr, uint8_t qty, uint32_t iaddr, uint8_t n, uint8_t stop);
	virtual size_t write(uint8_t data);
	virtual size_t write(const uint8_t *data, size_t quantity);
	virtual int available(void) {
		return rxBufferLength - rxBufferIndex;
	}
	virtual int read(void) {
		if (rxBufferIndex >= rxBufferLength) return -1;
		return rxBuffer[rxBufferIndex++];
	}
	virtual int peek(void) {
		if (rxBufferIndex >= rxBufferLength) return -1;
		return rxBuffer[rxBufferIndex];
	}
	virtual void flush(void) {
	}
	void onReceive(void (*function)(int numBytes)) {
		user_onReceive = function;
	}
	void onRequest(void (*function)(void)) {
		user_onRequest = function;
	}
	// send() for compatibility with very old sketches and libraries
	void send(uint8_t b) {
		write(b);
	}
	void send(uint8_t *s, uint8_t n) {
		write(s, n);
	}
	void send(int n) {
		write((uint8_t)n);
	}
	void send(char *s) {
		write(s);
	}
	uint8_t receive(void) {
		int c = read();
		if (c < 0) return 0;
		return c;
	}
	size_t write(unsigned long n) {
		return write((uint8_t)n);
	}
	size_t write(long n) {
		return write((uint8_t)n);
	}
	size_t write(unsigned int n) {
		return write((uint8_t)n);
	}
	size_t write(int n) {
		return write((uint8_t)n);
	}
	using Print::write;
private:
	KINETIS_I2C_t& port() { return (*(KINETIS_I2C_t *) port_addr); }
	uint8_t i2c_status(void) {
		return port().S;
	}
	void isr(void);
	bool wait_idle(void);
	uintptr_t port_addr;
	const I2C_Hardware_t &hardware;
	uint8_t rxBuffer[BUFFER_LENGTH] = {};
	uint8_t rxBufferIndex = 0;
	uint8_t rxBufferLength = 0;
	uint8_t txAddress = 0;
	uint8_t txBuffer[BUFFER_LENGTH+1] = {};
	uint8_t txBufferIndex = 0;
	uint8_t txBufferLength = 0;
	uint8_t transmitting = 0;
	uint8_t slave_mode = 0;
	uint8_t irqcount = 0;
	uint8_t sda_pin_index = 0;
	uint8_t scl_pin_index = 0;
	void onRequestService(void);
	void onReceiveService(uint8_t*, int);
	void (*user_onRequest)(void) = nullptr;
	void (*user_onReceive)(int) = nullptr;
	void sda_rising_isr(void);
	friend void i2c0_isr(void);
	friend void i2c1_isr(void);
	friend void i2c2_isr(void);
	friend void i2c3_isr(void);
	friend void sda_rising_isr0(void);
	friend void sda_rising_isr1(void);
};

#ifdef WIRE_IMPLEMENT_WIRE
extern TwoWire Wire;
#endif
#ifdef WIRE_IMPLEMENT_WIRE1
extern TwoWire Wire1;
#endif
#ifdef WIRE_IMPLEMENT_WIRE2
extern TwoWire Wire2;
#endif
#ifdef WIRE_IMPLEMENT_WIRE3
extern TwoWire Wire3;
#endif


class TWBRemulation
{
public:
	inline TWBRemulation & operator = (int val) __attribute__((always_inline)) {
		if (val == 12 || val == ((F_CPU / 400000) - 16) / 2) { // 22, 52, 112
			I2C0_C1 = 0;
			#if F_BUS == 128000000
			I2C0_F = I2C_F_DIV320; // 400 kHz
			#elif F_BUS == 120000000
			I2C0_F = I2C_F_DIV288; // 416 kHz
			#elif F_BUS == 108000000
			I2C0_F = I2C_F_DIV256; // 422 kHz
			#elif F_BUS == 96000000
			I2C0_F = I2C_F_DIV240; // 400 kHz
			#elif F_BUS == 90000000
			I2C0_F = I2C_F_DIV224; // 402 kHz
			#elif F_BUS == 80000000
			I2C0_F = I2C_F_DIV192; // 416 kHz
			#elif F_BUS == 72000000
			I2C0_F = I2C_F_DIV192; // 375 kHz
			#elif F_BUS == 64000000
			I2C0_F = I2C_F_DIV160; // 400 kHz
			#elif F_BUS == 60000000
			I2C0_F = I2C_F_DIV144; // 416 kHz
			#elif F_BUS == 56000000
			I2C0_F = I2C_F_DIV144; // 389 kHz
			#elif F_BUS == 54000000
			I2C0_F = I2C_F_DIV128; // 422 kHz
			#elif F_BUS == 48000000
			I2C0_F = I2C_F_DIV112; // 400 kHz
			#elif F_BUS == 40000000
			I2C0_F = I2C_F_DIV96;  // 416 kHz
			#elif F_BUS == 36000000
			I2C0_F = I2C_F_DIV96;  // 375 kHz
			#elif F_BUS == 24000000
			I2C0_F = I2C_F_DIV64;  // 375 kHz
			#elif F_BUS == 16000000
			I2C0_F = I2C_F_DIV40;  // 400 kHz
			#elif F_BUS == 8000000
			I2C0_F = I2C_F_DIV20;  // 400 kHz
			#elif F_BUS == 4000000
			I2C0_F = I2C_F_DIV20;  // 200 kHz
			#elif F_BUS == 2000000
			I2C0_F = I2C_F_DIV20;  // 100 kHz
			#endif
			I2C0_C1 = I2C_C1_IICEN;
		} else if (val == 72 || val == ((F_CPU / 100000) - 16) / 2) { // 112, 232, 472
			I2C0_C1 = 0;
			#if F_BUS == 128000000
			I2C0_F = I2C_F_DIV1280; // 100 kHz
			#elif F_BUS == 120000000
			I2C0_F = I2C_F_DIV1152; // 104 kHz
			#elif F_BUS == 108000000
			I2C0_F = I2C_F_DIV1024; // 105 kHz
			#elif F_BUS == 96000000
			I2C0_F = I2C_F_DIV960; // 100 kHz
			#elif F_BUS == 90000000
			I2C0_F = I2C_F_DIV896; // 100 kHz
			#elif F_BUS == 80000000
			I2C0_F = I2C_F_DIV768; // 104 kHz
			#elif F_BUS == 72000000
			I2C0_F = I2C_F_DIV640; // 112 kHz
			#elif F_BUS == 64000000
			I2C0_F = I2C_F_DIV640; // 100 kHz
			#elif F_BUS == 60000000
			I2C0_F = I2C_F_DIV576; // 104 kHz
			#elif F_BUS == 56000000
			I2C0_F = I2C_F_DIV512; // 109 kHz
			#elif F_BUS == 54000000
			I2C0_F = I2C_F_DIV512; // 105 kHz
			#elif F_BUS == 48000000
			I2C0_F = I2C_F_DIV480; // 100 kHz
			#elif F_BUS == 40000000
			I2C0_F = I2C_F_DIV384; // 104 kHz
			#elif F_BUS == 36000000
			I2C0_F = I2C_F_DIV320; // 113 kHz
			#elif F_BUS == 24000000
			I2C0_F = I2C_F_DIV240; // 100 kHz
			#elif F_BUS == 16000000
			I2C0_F = I2C_F_DIV160; // 100 kHz
			#elif F_BUS == 8000000
			I2C0_F = I2C_F_DIV80; // 100 kHz
			#elif F_BUS == 4000000
			I2C0_F = I2C_F_DIV40; // 100 kHz
			#elif F_BUS == 2000000
			I2C0_F = I2C_F_DIV20; // 100 kHz
			#endif
			I2C0_C1 = I2C_C1_IICEN;
		}
		return *this;
	}
	inline operator int () const __attribute__((always_inline)) {
		#if F_BUS == 128000000
		if (I2C0_F == I2C_F_DIV320) return 12;
		#elif F_BUS == 120000000
		if (I2C0_F == I2C_F_DIV288) return 12;
		#elif F_BUS == 108000000
		if (I2C0_F == I2C_F_DIV256) return 12;
		#elif F_BUS == 96000000
		if (I2C0_F == I2C_F_DIV240) return 12;
		#elif F_BUS == 90000000
		if (I2C0_F == I2C_F_DIV224) return 12;
		#elif F_BUS == 80000000
		if (I2C0_F == I2C_F_DIV192) return 12;
		#elif F_BUS == 72000000
		if (I2C0_F == I2C_F_DIV192) return 12;
		#elif F_BUS == 64000000
		if (I2C0_F == I2C_F_DIV160) return 12;
		#elif F_BUS == 60000000
		if (I2C0_F == I2C_F_DIV144) return 12;
		#elif F_BUS == 56000000
		if (I2C0_F == I2C_F_DIV144) return 12;
		#elif F_BUS == 54000000
		if (I2C0_F == I2C_F_DIV128) return 12;
		#elif F_BUS == 48000000
		if (I2C0_F == I2C_F_DIV112) return 12;
		#elif F_BUS == 40000000
		if (I2C0_F == I2C_F_DIV96) return 12;
		#elif F_BUS == 36000000
		if (I2C0_F == I2C_F_DIV96) return 12;
		#elif F_BUS == 24000000
		if (I2C0_F == I2C_F_DIV64) return 12;
		#elif F_BUS == 16000000
		if (I2C0_F == I2C_F_DIV40) return 12;
		#elif F_BUS == 8000000
		if (I2C0_F == I2C_F_DIV20) return 12;
		#elif F_BUS == 4000000
		if (I2C0_F == I2C_F_DIV20) return 12;
		#endif
		return 72;
	}
};
extern TWBRemulation TWBR;

#endif
#endif
