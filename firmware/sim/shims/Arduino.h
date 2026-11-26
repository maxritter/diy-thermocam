#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <string>

using byte = uint8_t;
using word = uint16_t;
using boolean = bool;

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef DMAMEM
#define DMAMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif
#ifndef F_CPU_ACTUAL
#define F_CPU_ACTUAL 450000000u
#endif
#ifndef HIGH
#define HIGH 1
#endif
#ifndef LOW
#define LOW 0
#endif
#ifndef INPUT
#define INPUT 0
#endif
#ifndef OUTPUT
#define OUTPUT 1
#endif
#ifndef INPUT_PULLDOWN
#define INPUT_PULLDOWN 2
#endif
#ifndef A16
#define A16 40
#endif
#ifndef RISING
#define RISING 1
#endif
#ifndef FALLING
#define FALLING 2
#endif

struct String {
	std::string s;
	String() = default;
	String(const char* p) : s(p ? p : "") {}
	String(const std::string& str) : s(str) {}
	size_t length() const { return s.size(); }
	const char* c_str() const { return s.c_str(); }
	void toCharArray(char* buf, size_t bufsize) const {
		if (!buf || bufsize == 0) return;
		std::strncpy(buf, s.c_str(), bufsize - 1);
		buf[bufsize - 1] = '\0';
	}
};

uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
int digitalRead(int pin);
void digitalWrite(int pin, int val);
int analogRead(int pin);
void pinMode(int pin, int mode);
void noInterrupts();
void interrupts();
void attachInterrupt(int pin, void (*fn)(), int mode);
void detachInterrupt(int pin);
void yield();

inline char* itoa(int value, char* str, int base) {
	if (base == 16) std::snprintf(str, 16, "%x", value);
	else std::snprintf(str, 16, "%d", value);
	return str;
}

inline char* dtostrf(double val, signed char width, unsigned char prec, char* s) {
	std::snprintf(s, 26, "%*.*f", (int)width, (int)prec, val);
	return s;
}

struct SimSerial {
	void begin(unsigned long) {}
	int available() { return 0; }
	void clear() {}
};
inline SimSerial Serial;
