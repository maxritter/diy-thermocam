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
#if defined(__STM32F1__)
#include "SdSpi.h"
#define USE_STM32F1_DMAC 1
//------------------------------------------------------------------------------
/** Initialize the SPI bus.
 *
 * \param[in] chipSelectPin SD card chip select pin.
 */
void SdSpi::begin(uint8_t chipSelectPin) {
  pinMode(chipSelectPin, OUTPUT);
  digitalWrite(chipSelectPin, HIGH);
  SPI.begin();
}
//------------------------------------------------------------------------------
/** Set SPI options for access to SD/SDHC cards.
 *
 * \param[in] divisor SCK clock divider relative to the APB1 or APB2 clock.
 */
void SdSpi::beginTransaction(uint8_t divisor) {
#if ENABLE_SPI_TRANSACTIONS
  // Correct divisor will be set below.
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
#endif  // ENABLE_SPI_TRANSACTIONS
  uint32_t br;  // Baud rate control field in SPI_CR1.
  if (divisor <= 2) {
    br = SPI_CLOCK_DIV2;
  } else  if (divisor <= 4) {
    br = SPI_CLOCK_DIV4;
  } else  if (divisor <= 8) {
    br = SPI_CLOCK_DIV8;
  } else  if (divisor <= 16) {
    br = SPI_CLOCK_DIV16;
  } else  if (divisor <= 32) {
    br = SPI_CLOCK_DIV32;
  } else  if (divisor <= 64) {
    br = SPI_CLOCK_DIV64;
  } else  if (divisor <= 128) {
    br = SPI_CLOCK_DIV128;
  } else {
    br = SPI_CLOCK_DIV256;
  }
  SPI.setClockDivider(br);
#if !ENABLE_SPI_TRANSACTIONS
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
#endif  // !ENABLE_SPI_TRANSACTIONS
}
//------------------------------------------------------------------------------
/**
 * End SPI transaction.
 */
void SdSpi::endTransaction() {
#if ENABLE_SPI_TRANSACTIONS
  SPI.endTransaction();
#endif  // ENABLE_SPI_TRANSACTIONS
}
//------------------------------------------------------------------------------
/** Receive a byte.
 *
 * \return The byte.
 */
uint8_t SdSpi::receive() {
  return SPI.transfer(0XFF);
}
//------------------------------------------------------------------------------
/** Receive multiple bytes.
 *
 * \param[out] buf Buffer to receive the data.
 * \param[in] n Number of bytes to receive.
 *
 * \return Zero for no error or nonzero error code.
 */
uint8_t SdSpi::receive(uint8_t* buf, size_t n) {
  int rtn = 0;
#if USE_STM32F1_DMAC
  rtn = SPI.dmaTransfer(0, const_cast<uint8*>(buf), n);
#else  // USE_STM32F1_DMAC
//  SPI.read(buf, n);
  for (size_t i = 0; i < n; i++) {
    buf[i] = SPI.transfer(0XFF);
  }
#endif  // USE_STM32F1_DMAC
  return rtn;
}
//------------------------------------------------------------------------------
/** Send a byte.
 *
 * \param[in] b Byte to send
 */
void SdSpi::send(uint8_t b) {
  SPI.transfer(b);
}
//------------------------------------------------------------------------------
/** Send multiple bytes.
 *
 * \param[in] buf Buffer for data to be sent.
 * \param[in] n Number of bytes to send.
 */
void SdSpi::send(const uint8_t* buf , size_t n) {
#if USE_STM32F1_DMAC
  SPI.dmaSend(const_cast<uint8*>(buf), n);
#else  // #if USE_STM32F1_DMAC
  SPI.write(buf, n);
#endif  // USE_STM32F1_DMAC
}
#endif  // USE_NATIVE_STM32F1_SPI
