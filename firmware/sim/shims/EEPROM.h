#pragma once
#include <cstdint>

struct SimEEPROM {
	uint8_t mem[512] = {};
	uint8_t read(int idx) const { return (idx >= 0 && idx < 512) ? mem[idx] : 0; }
	void write(int idx, uint8_t v) { if (idx >= 0 && idx < 512) mem[idx] = v; }
};
extern SimEEPROM EEPROM;
