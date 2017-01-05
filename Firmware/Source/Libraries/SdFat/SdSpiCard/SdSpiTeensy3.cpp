/* Arduino SdSpi Library
 * Copyright (C) 2013 by William Greiman
 *
 * This file is part of the Arduino SdSpi Library
 *
 * This Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Arduino SdSpi Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include "SdSpi.h"
#if defined(__arm__) && defined(CORE_TEENSY)
// SPI definitions
#include "kinetis.h"
#ifdef KINETISK
// use 16-bit frame if SPI_USE_8BIT_FRAME is zero
#define SPI_USE_8BIT_FRAME 0
// Limit initial fifo to three entries to avoid fifo overrun
#define SPI_INITIAL_FIFO_DEPTH 3
// define some symbols that are not in mk20dx128.h
#ifndef SPI_SR_RXCTR
#define SPI_SR_RXCTR 0XF0
#endif  // SPI_SR_RXCTR
#ifndef SPI_PUSHR_CONT
#define SPI_PUSHR_CONT 0X80000000
#endif   // SPI_PUSHR_CONT
#ifndef SPI_PUSHR_CTAS
#define SPI_PUSHR_CTAS(n) (((n) & 7) << 28)
#endif  // SPI_PUSHR_CTAS
//------------------------------------------------------------------------------
/**
 * initialize SPI pins
 */
void SdSpi::begin(uint8_t chipSelectPin) {
  pinMode(chipSelectPin, OUTPUT);
  digitalWrite(chipSelectPin, HIGH);
  SIM_SCGC6 |= SIM_SCGC6_SPI0;
}
//------------------------------------------------------------------------------
/**
 * Initialize hardware SPI
 *
 */
void SdSpi::beginTransaction(uint8_t sckDivisor) {
  uint32_t ctar, ctar0, ctar1;
#if ENABLE_SPI_TRANSACTIONS
  SPI.beginTransaction(SPISettings());
#endif  // #if ENABLE_SPI_TRANSACTIONS
  if (sckDivisor <= 2) {
    // 1/2 speed
    ctar = SPI_CTAR_DBR | SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0);
  } else if (sckDivisor <= 4) {
    // 1/4 speed
    ctar = SPI_CTAR_BR(0) | SPI_CTAR_CSSCK(0);
  } else if (sckDivisor <= 8) {
    // 1/8 speed
    ctar = SPI_CTAR_BR(1) | SPI_CTAR_CSSCK(1);
  } else if (sckDivisor <= 12) {
    // 1/12 speed
    ctar = SPI_CTAR_BR(2) | SPI_CTAR_CSSCK(2);
  } else if (sckDivisor <= 16) {
    // 1/16 speed
    ctar = SPI_CTAR_BR(3) | SPI_CTAR_CSSCK(3);
  } else if (sckDivisor <= 32) {
    // 1/32 speed
    ctar = SPI_CTAR_PBR(1) | SPI_CTAR_BR(4) | SPI_CTAR_CSSCK(4);
  } else if (sckDivisor <= 64) {
    // 1/64 speed
    ctar = SPI_CTAR_PBR(1) | SPI_CTAR_BR(5) | SPI_CTAR_CSSCK(5);
  } else {
    // 1/128 speed
    ctar = SPI_CTAR_PBR(1) | SPI_CTAR_BR(6) | SPI_CTAR_CSSCK(6);
  }
  // CTAR0 - 8 bit transfer
  ctar0 = ctar | SPI_CTAR_FMSZ(7);

  // CTAR1 - 16 bit transfer
  ctar1 = ctar | SPI_CTAR_FMSZ(15);

  if (SPI0_CTAR0 != ctar0 || SPI0_CTAR1 != ctar1) {
    SPI0_MCR = SPI_MCR_MSTR | SPI_MCR_MDIS | SPI_MCR_HALT | SPI_MCR_PCSIS(0x1F);
    SPI0_CTAR0 = ctar0;
    SPI0_CTAR1 = ctar1;
  }
  SPI0_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
  CORE_PIN11_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
  CORE_PIN12_CONFIG = PORT_PCR_MUX(2);
  CORE_PIN13_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
}
//------------------------------------------------------------------------------
/** SPI receive a byte */
uint8_t SdSpi::receive() {
  SPI0_MCR |= SPI_MCR_CLR_RXF;
  SPI0_SR = SPI_SR_TCF;
  SPI0_PUSHR = 0xFF;
  while (!(SPI0_SR & SPI_SR_TCF)) {}
  return SPI0_POPR;
}
//------------------------------------------------------------------------------
/** SPI receive multiple bytes */
uint8_t SdSpi::receive(uint8_t* buf, size_t n) {
  // clear any data in RX FIFO
  SPI0_MCR = SPI_MCR_MSTR | SPI_MCR_CLR_RXF | SPI_MCR_PCSIS(0x1F);
#if SPI_USE_8BIT_FRAME
  // initial number of bytes to push into TX FIFO
  int nf = n < SPI_INITIAL_FIFO_DEPTH ? n : SPI_INITIAL_FIFO_DEPTH;
  for (int i = 0; i < nf; i++) {
    SPI0_PUSHR = 0XFF;
  }
  // limit for pushing dummy data into TX FIFO
  uint8_t* limit = buf + n - nf;
  while (buf < limit) {
    while (!(SPI0_SR & SPI_SR_RXCTR)) {}
    SPI0_PUSHR = 0XFF;
    *buf++ = SPI0_POPR;
  }
  // limit for rest of RX data
  limit += nf;
  while (buf < limit) {
    while (!(SPI0_SR & SPI_SR_RXCTR)) {}
    *buf++ = SPI0_POPR;
  }
#else  // SPI_USE_8BIT_FRAME
  // use 16 bit frame to avoid TD delay between frames
  // get one byte if n is odd
  if (n & 1) {
    *buf++ = receive();
    n--;
  }
  // initial number of words to push into TX FIFO
  int nf = n/2 < SPI_INITIAL_FIFO_DEPTH ? n/2 : SPI_INITIAL_FIFO_DEPTH;
  for (int i = 0; i < nf; i++) {
    SPI0_PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1) | 0XFFFF;
  }
  uint8_t* limit = buf + n - 2*nf;
  while (buf < limit) {
    while (!(SPI0_SR & SPI_SR_RXCTR)) {}
    SPI0_PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1) | 0XFFFF;
    uint16_t w = SPI0_POPR;
    *buf++ = w >> 8;
    *buf++ = w & 0XFF;
  }
  // limit for rest of RX data
  limit += 2*nf;
  while (buf < limit) {
    while (!(SPI0_SR & SPI_SR_RXCTR)) {}
    uint16_t w = SPI0_POPR;
    *buf++ = w >> 8;
    *buf++ = w & 0XFF;
  }
#endif  // SPI_USE_8BIT_FRAME
  return 0;
}
//------------------------------------------------------------------------------
/** SPI send a byte */
void SdSpi::send(uint8_t b) {
  SPI0_MCR |= SPI_MCR_CLR_RXF;
  SPI0_SR = SPI_SR_TCF;
  SPI0_PUSHR = b;
  while (!(SPI0_SR & SPI_SR_TCF)) {}
}
//------------------------------------------------------------------------------
/** SPI send multiple bytes */
void SdSpi::send(const uint8_t* buf , size_t n) {
  // clear any data in RX FIFO
  SPI0_MCR = SPI_MCR_MSTR | SPI_MCR_CLR_RXF | SPI_MCR_PCSIS(0x1F);
#if SPI_USE_8BIT_FRAME
  // initial number of bytes to push into TX FIFO
  int nf = n < SPI_INITIAL_FIFO_DEPTH ? n : SPI_INITIAL_FIFO_DEPTH;
  // limit for pushing data into TX fifo
  const uint8_t* limit = buf + n;
  for (int i = 0; i < nf; i++) {
    SPI0_PUSHR = *buf++;
  }
  // write data to TX FIFO
  while (buf < limit) {
    while (!(SPI0_SR & SPI_SR_RXCTR)) {}
    SPI0_PUSHR = *buf++;
    SPI0_POPR;
  }
  // wait for data to be sent
  while (nf) {
    while (!(SPI0_SR & SPI_SR_RXCTR)) {}
    SPI0_POPR;
    nf--;
  }
#else  // SPI_USE_8BIT_FRAME
  // use 16 bit frame to avoid TD delay between frames
  // send one byte if n is odd
  if (n & 1) {
    send(*buf++);
    n--;
  }
  // initial number of words to push into TX FIFO
  int nf = n/2 < SPI_INITIAL_FIFO_DEPTH ? n/2 : SPI_INITIAL_FIFO_DEPTH;
  // limit for pushing data into TX fifo
  const uint8_t* limit = buf + n;
  for (int i = 0; i < nf; i++) {
    uint16_t w = (*buf++) << 8;
    w |= *buf++;
    SPI0_PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1) | w;
  }
  // write data to TX FIFO
  while (buf < limit) {
    uint16_t w = *buf++ << 8;
    w |= *buf++;
    while (!(SPI0_SR & SPI_SR_RXCTR)) {}
    SPI0_PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1) | w;
    SPI0_POPR;
  }
  // wait for data to be sent
  while (nf) {
    while (!(SPI0_SR & SPI_SR_RXCTR)) {}
    SPI0_POPR;
    nf--;
  }
#endif  // SPI_USE_8BIT_FRAME
}
#else  // KINETISK
//==============================================================================
// Use standard SPI library if not KINETISK
/**
 * Initialize SPI pins.
 */
void SdSpi::begin(uint8_t chipSelectPin) {
  pinMode(chipSelectPin, OUTPUT);
  digitalWrite(chipSelectPin, HIGH);
  SPI.begin();
}
/** Set SPI options for access to SD/SDHC cards.
 *
 * \param[in] divisor SCK clock divider relative to the system clock.
 */
void SdSpi::beginTransaction(uint8_t divisor) {
#if ENABLE_SPI_TRANSACTIONS
  SPI.beginTransaction(SPISettings());
#else  // #if ENABLE_SPI_TRANSACTIONS
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
#endif  // #if ENABLE_SPI_TRANSACTIONS
#ifndef SPI_CLOCK_DIV128
  SPI.setClockDivider(divisor);
#else  // SPI_CLOCK_DIV128
  int v;
  if (divisor <= 2) {
    v = SPI_CLOCK_DIV2;
  } else  if (divisor <= 4) {
    v = SPI_CLOCK_DIV4;
  } else  if (divisor <= 8) {
    v = SPI_CLOCK_DIV8;
  } else  if (divisor <= 16) {
    v = SPI_CLOCK_DIV16;
  } else  if (divisor <= 32) {
    v = SPI_CLOCK_DIV32;
  } else  if (divisor <= 64) {
    v = SPI_CLOCK_DIV64;
  } else {
    v = SPI_CLOCK_DIV128;
  }
  SPI.setClockDivider(v);
#endif  // SPI_CLOCK_DIV128
}
/** Receive a byte.
 *
 * \return The byte.
 */
uint8_t SdSpi::receive() {
  return SPI.transfer(0XFF);
}
/** Receive multiple bytes.
 *
 * \param[out] buf Buffer to receive the data.
 * \param[in] n Number of bytes to receive.
 *
 * \return Zero for no error or nonzero error code.
 */
uint8_t SdSpi::receive(uint8_t* buf, size_t n) {
  for (size_t i = 0; i < n; i++) {
    buf[i] = SPI.transfer(0XFF);
  }
  return 0;
}
/** Send a byte.
 *
 * \param[in] b Byte to send
 */
void SdSpi::send(uint8_t b) {
  SPI.transfer(b);
}
/** Send multiple bytes.
 *
 * \param[in] buf Buffer for data to be sent.
 * \param[in] n Number of bytes to send.
 */
void SdSpi::send(const uint8_t* buf , size_t n) {
  for (size_t i = 0; i < n; i++) {
    SPI.transfer(buf[i]);
  }
}
#endif  // KINETISK
//------------------------------------------------------------------------------
void SdSpi::endTransaction() {
#if ENABLE_SPI_TRANSACTIONS
  SPI.endTransaction();
#endif  // ENABLE_SPI_TRANSACTIONS
}
#endif  // defined(__arm__) && defined(CORE_TEENSY)
