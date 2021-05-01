
#include "Wire.h"


#if defined(__IMXRT1062__)

//#include "debug/printf.h"

#define PINCONFIG (IOMUXC_PAD_ODE | IOMUXC_PAD_SRE | IOMUXC_PAD_DSE(4) | IOMUXC_PAD_SPEED(1) | IOMUXC_PAD_PKE | IOMUXC_PAD_PUE | IOMUXC_PAD_PUS(3))

void TwoWire::begin(void)
{
	// use 24 MHz clock
	CCM_CSCDR2 = (CCM_CSCDR2 & ~CCM_CSCDR2_LPI2C_CLK_PODF(63)) | CCM_CSCDR2_LPI2C_CLK_SEL;
	hardware.clock_gate_register |= hardware.clock_gate_mask;
	port->MCR = LPI2C_MCR_RST;
	setClock(100000);

	// Setup SDA register
	*(portControlRegister(hardware.sda_pins[sda_pin_index_].pin)) = PINCONFIG;
	*(portConfigRegister(hardware.sda_pins[sda_pin_index_].pin)) = hardware.sda_pins[sda_pin_index_].mux_val;
	if (hardware.sda_pins[sda_pin_index_].select_input_register) {
		*(hardware.sda_pins[sda_pin_index_].select_input_register) =  hardware.sda_pins[sda_pin_index_].select_val;
	}

	// setup SCL register
	*(portControlRegister(hardware.scl_pins[scl_pin_index_].pin)) = PINCONFIG;
	*(portConfigRegister(hardware.scl_pins[scl_pin_index_].pin)) = hardware.scl_pins[scl_pin_index_].mux_val;
	if (hardware.scl_pins[scl_pin_index_].select_input_register) {
		*(hardware.scl_pins[scl_pin_index_].select_input_register) =  hardware.scl_pins[scl_pin_index_].select_val;
	}
}

void TwoWire::begin(uint8_t address)
{
	// TODO: slave mode
}

void TwoWire::end()
{
}

void TwoWire::setSDA(uint8_t pin) {
	if (pin == hardware.sda_pins[sda_pin_index_].pin) return;
	uint32_t newindex=0;
	while (1) {
		uint32_t sda_pin = hardware.sda_pins[newindex].pin;
		if (sda_pin == 255) return;
		if (sda_pin == pin) break;
		if (++newindex >= sizeof(hardware.sda_pins)) return;
	}
	if ((hardware.clock_gate_register & hardware.clock_gate_mask)) {
		*(portConfigRegister(hardware.sda_pins[sda_pin_index_].pin)) = 5;	// hard to know what to go back to?

		// setup new one...
		*(portControlRegister(hardware.sda_pins[newindex].pin)) |= IOMUXC_PAD_PKE | IOMUXC_PAD_PUE | IOMUXC_PAD_PUS(3);
		*(portConfigRegister(hardware.sda_pins[newindex].pin)) = hardware.sda_pins[newindex].mux_val;
		if (hardware.sda_pins[newindex].select_input_register) {
			*(hardware.sda_pins[newindex].select_input_register) =  hardware.sda_pins[newindex].select_val;
		}
	}
	sda_pin_index_ = newindex;
}

void TwoWire::setSCL(uint8_t pin) {
	if (pin == hardware.scl_pins[scl_pin_index_].pin) return;
	uint32_t newindex=0;
	while (1) {
		uint32_t scl_pin = hardware.scl_pins[newindex].pin;
		if (scl_pin == 255) return;
		if (scl_pin == pin) break;
		if (++newindex >= sizeof(hardware.scl_pins)) return;
	}
	if ((hardware.clock_gate_register & hardware.clock_gate_mask)) {
		*(portConfigRegister(hardware.scl_pins[scl_pin_index_].pin)) = 5;	// hard to know what to go back to?

		// setup new one...
		*(portControlRegister(hardware.scl_pins[newindex].pin)) |= IOMUXC_PAD_PKE | IOMUXC_PAD_PUE | IOMUXC_PAD_PUS(3);
		*(portConfigRegister(hardware.scl_pins[newindex].pin)) = hardware.scl_pins[newindex].mux_val;
		if (hardware.scl_pins[newindex].select_input_register) {
			*(hardware.scl_pins[newindex].select_input_register) =  hardware.scl_pins[newindex].select_val;
		}
	}
	scl_pin_index_ = newindex;
}

bool TwoWire::force_clock()
{
	bool ret = false;
	uint32_t sda_pin = hardware.sda_pins[sda_pin_index_].pin;
	uint32_t scl_pin = hardware.scl_pins[scl_pin_index_].pin;
	uint32_t sda_mask = digitalPinToBitMask(sda_pin);
	uint32_t scl_mask = digitalPinToBitMask(scl_pin);
	// take control of pins with GPIO
	*portConfigRegister(sda_pin) = 5 | 0x10;
	*portSetRegister(sda_pin) = sda_mask;
	*portModeRegister(sda_pin) |= sda_mask;
	*portConfigRegister(scl_pin) = 5 | 0x10;
	*portSetRegister(scl_pin) = scl_mask;
	*portModeRegister(scl_pin) |= scl_mask;
	delayMicroseconds(10);
	for (int i=0; i < 9; i++) {
		if ((*portInputRegister(sda_pin) & sda_mask)
		  && (*portInputRegister(scl_pin) & scl_mask)) {
			// success, both pins are high
			ret = true;
			break;
		}
		*portClearRegister(scl_pin) = scl_mask;
		delayMicroseconds(5);
		*portSetRegister(scl_pin) = scl_mask;
		delayMicroseconds(5);
	}
	// return control of pins to I2C
	*(portConfigRegister(sda_pin)) = hardware.sda_pins[sda_pin_index_].mux_val;
	*(portConfigRegister(scl_pin)) = hardware.scl_pins[scl_pin_index_].mux_val;
	return ret;
}

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




// 2      BBF = Bus Busy Flag
// 1      MBF = Master Busy Flag
//   40   DMF = Data Match Flag
//   20   PLTF = Pin Low Timeout Flag
//   10   FEF = FIFO Error Flag
//   08   ALF = Arbitration Lost Flag
//   04   NDF = NACK Detect Flag
//   02   SDF = STOP Detect Flag
//   01   EPF = End Packet Flag
//      2 RDF = Receive Data Flag
//      1 TDF = Transmit Data Flag

bool TwoWire::wait_idle()
{
	elapsedMillis timeout = 0;
	while (1) {
		uint32_t status = port->MSR; // pg 2899 & 2892
		if (!(status & LPI2C_MSR_BBF)) break; // bus is available
		if (status & LPI2C_MSR_MBF) break; // we already have bus control
		if (timeout > 16) {
			//Serial.printf("timeout waiting for idle, MSR = %x\n", status);
			if (force_clock()) break;
			//Serial.printf("unable to get control of I2C bus\n");
			return false;
		}
	}
	port->MSR = 0x00007F00; // clear all prior flags
	return true;
}


uint8_t TwoWire::endTransmission(uint8_t sendStop)
{
	uint32_t tx_len = txBufferLength;
	if (!tx_len) return 4; // no address for transmit
	if (!wait_idle()) return 4;
	uint32_t tx_index = 0; // 0=start, 1=addr, 2-(N-1)=data, N=stop
	elapsedMillis timeout = 0;
	while (1) {
		// transmit stuff, if we haven't already
		if (tx_index <= tx_len) {
			uint32_t fifo_used = port->MFSR & 0x07; // pg 2914
			while (fifo_used < 4) {
				if (tx_index == 0) {
					port->MTDR = LPI2C_MTDR_CMD_START | txBuffer[0];
					tx_index = 1;
				} else if (tx_index < tx_len) {
					port->MTDR = LPI2C_MTDR_CMD_TRANSMIT | txBuffer[tx_index++];
				} else {
					if (sendStop) port->MTDR = LPI2C_MTDR_CMD_STOP;
					tx_index++;
					break;
				}
				fifo_used++;
			}
		}
		// monitor status
		uint32_t status = port->MSR; // pg 2884 & 2891
		if (status & LPI2C_MSR_ALF) {
			port->MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF; // clear FIFOs
			return 4; // we lost bus arbitration to another master
		}
		if (status & LPI2C_MSR_NDF) {
			port->MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF; // clear FIFOs
			port->MTDR = LPI2C_MTDR_CMD_STOP;
			return 2; // NACK (assume address, TODO: how to tell address from data)
		}
		if ((status & LPI2C_MSR_PLTF) || timeout > 50) {
			port->MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF; // clear FIFOs
			port->MTDR = LPI2C_MTDR_CMD_STOP; // try to send a stop
			return 4; // clock stretched too long or generic timeout
		}
		// are we done yet?
		if (tx_index > tx_len) {
			uint32_t tx_fifo = port->MFSR & 0x07;
			if (tx_fifo == 0 && ((status & LPI2C_MSR_SDF) || !sendStop)) {
				return 0;
			}
		}
		yield();
	}
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t length, uint8_t sendStop)
{
	if (!wait_idle()) return 4;
	address = (address & 0x7F) << 1;
	if (length < 1) length = 1;
	if (length > 255) length = 255;
	rxBufferIndex = 0;
	rxBufferLength = 0;
	uint32_t tx_state = 0; // 0=begin, 1=start, 2=data, 3=stop
	elapsedMillis timeout = 0;
	while (1) {
		// transmit stuff, if we haven't already
		if (tx_state < 3) {
			uint32_t tx_fifo = port->MFSR & 0x07; // pg 2914
			while (tx_fifo < 4 && tx_state < 3) {
				if (tx_state == 0) {
					port->MTDR = LPI2C_MTDR_CMD_START | 1 | address;
				} else if (tx_state == 1) {
					port->MTDR = LPI2C_MTDR_CMD_RECEIVE | (length - 1);
				} else {
					if (sendStop) port->MTDR = LPI2C_MTDR_CMD_STOP;
				}
				tx_state++;
				tx_fifo--;
			}
		}
		// receive stuff
		if (rxBufferLength < sizeof(rxBuffer)) {
			uint32_t rx_fifo = (port->MFSR >> 16) & 0x07;
			while (rx_fifo > 0 && rxBufferLength < sizeof(rxBuffer)) {
				rxBuffer[rxBufferLength++] = port->MRDR;
				rx_fifo--;
			}
		}
		// monitor status, check for error conditions
		uint32_t status = port->MSR; // pg 2884 & 2891
		if (status & LPI2C_MSR_ALF) {
			port->MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF; // clear FIFOs
			break;
		}
		if ((status & LPI2C_MSR_NDF) || (status & LPI2C_MSR_PLTF) || timeout > 50) {
			port->MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF; // clear FIFOs
			port->MTDR = LPI2C_MTDR_CMD_STOP; // try to send a stop
			break;
		}
		// are we done yet?
		if (rxBufferLength >= length && tx_state >= 3) {
			uint32_t tx_fifo = port->MFSR & 0x07;
			if (tx_fifo == 0 && ((status & LPI2C_MSR_SDF) || !sendStop)) {
				break;
			}
		}
		yield();
	}
	uint32_t rx_fifo = (port->MFSR >> 16) & 0x07;
	if (rx_fifo > 0) port->MCR |= LPI2C_MCR_RRF;
	return rxBufferLength;
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



PROGMEM
constexpr TwoWire::I2C_Hardware_t TwoWire::i2c1_hardware = {
	CCM_CCGR2, CCM_CCGR2_LPI2C1(CCM_CCGR_ON),
		{{18, 3 | 0x10, &IOMUXC_LPI2C1_SDA_SELECT_INPUT, 1}, {0xff, 0xff, nullptr, 0}},
		{{19, 3 | 0x10, &IOMUXC_LPI2C1_SCL_SELECT_INPUT, 1}, {0xff, 0xff, nullptr, 0}},
	IRQ_LPI2C1
};
TwoWire Wire(&IMXRT_LPI2C1, TwoWire::i2c1_hardware);

PROGMEM
constexpr TwoWire::I2C_Hardware_t TwoWire::i2c3_hardware = {
	CCM_CCGR2, CCM_CCGR2_LPI2C3(CCM_CCGR_ON),
		{{17, 1 | 0x10, &IOMUXC_LPI2C3_SDA_SELECT_INPUT, 2}, {36, 2 | 0x10, &IOMUXC_LPI2C3_SDA_SELECT_INPUT, 1}},
		{{16, 1 | 0x10, &IOMUXC_LPI2C3_SCL_SELECT_INPUT, 2}, {37, 2 | 0x10, &IOMUXC_LPI2C3_SCL_SELECT_INPUT, 1}},
	IRQ_LPI2C3
};
TwoWire Wire1(&IMXRT_LPI2C3, TwoWire::i2c3_hardware);

PROGMEM
constexpr TwoWire::I2C_Hardware_t TwoWire::i2c4_hardware = {
	CCM_CCGR6, CCM_CCGR6_LPI2C4_SERIAL(CCM_CCGR_ON),
		{{25, 0 | 0x10, &IOMUXC_LPI2C4_SDA_SELECT_INPUT, 1}, {0xff, 0xff, nullptr, 0}},
		{{24, 0 | 0x10, &IOMUXC_LPI2C4_SCL_SELECT_INPUT, 1}, {0xff, 0xff, nullptr, 0}},
	IRQ_LPI2C4
};
TwoWire Wire2(&IMXRT_LPI2C4, TwoWire::i2c4_hardware);




// Timeout if a device stretches SCL this long, in microseconds
#define CLOCK_STRETCH_TIMEOUT 15000


void TwoWire::setClock(uint32_t frequency)
{
	port->MCR = 0;
	if (frequency < 400000) {
		// 100 kHz
		port->MCCR0 = LPI2C_MCCR0_CLKHI(55) | LPI2C_MCCR0_CLKLO(59) |
			LPI2C_MCCR0_DATAVD(25) | LPI2C_MCCR0_SETHOLD(40);
		port->MCFGR1 = LPI2C_MCFGR1_PRESCALE(1);
		port->MCFGR2 = LPI2C_MCFGR2_FILTSDA(5) | LPI2C_MCFGR2_FILTSCL(5) |
			LPI2C_MCFGR2_BUSIDLE(3000); // idle timeout 250 us
		port->MCFGR3 = LPI2C_MCFGR3_PINLOW(CLOCK_STRETCH_TIMEOUT * 12 / 256 + 1);
	} else if (frequency < 1000000) {
		// 400 kHz
		port->MCCR0 = LPI2C_MCCR0_CLKHI(26) | LPI2C_MCCR0_CLKLO(28) |
			LPI2C_MCCR0_DATAVD(12) | LPI2C_MCCR0_SETHOLD(18);
		port->MCFGR1 = LPI2C_MCFGR1_PRESCALE(0);
		port->MCFGR2 = LPI2C_MCFGR2_FILTSDA(2) | LPI2C_MCFGR2_FILTSCL(2) |
			LPI2C_MCFGR2_BUSIDLE(3600); // idle timeout 150 us
		port->MCFGR3 = LPI2C_MCFGR3_PINLOW(CLOCK_STRETCH_TIMEOUT * 24 / 256 + 1);
	} else {
		// 1 MHz
		port->MCCR0 = LPI2C_MCCR0_CLKHI(9) | LPI2C_MCCR0_CLKLO(10) |
			LPI2C_MCCR0_DATAVD(4) | LPI2C_MCCR0_SETHOLD(7);
		port->MCFGR1 = LPI2C_MCFGR1_PRESCALE(0);
		port->MCFGR2 = LPI2C_MCFGR2_FILTSDA(1) | LPI2C_MCFGR2_FILTSCL(1) |
			LPI2C_MCFGR2_BUSIDLE(2400); // idle timeout 100 us
		port->MCFGR3 = LPI2C_MCFGR3_PINLOW(CLOCK_STRETCH_TIMEOUT * 24 / 256 + 1);
	}
	port->MCCR1 = port->MCCR0;
	port->MCFGR0 = 0;
	port->MFCR = LPI2C_MFCR_RXWATER(1) | LPI2C_MFCR_TXWATER(1);
	port->MCR = LPI2C_MCR_MEN;
}

#endif
