/* Arduino SdSpi Library
 * Copyright (C) 2016 by William Greiman
 *
 * STM32F1 code for Maple and Maple Mini support, 2015 by Victor Perez
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
#if defined(ESP8266)
#include "SdSpi.h"
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
 * \param[in] divisor SCK clock divider relative to the max SPI clock.
 */
void SdSpi::beginTransaction(uint8_t divisor) {
  const uint32_t F_SPI_MAX = 80000000;
#if ENABLE_SPI_TRANSACTIONS
  // Note: ESP8266 beginTransaction does not protect for interrupts.
  SPISettings settings(F_SPI_MAX/(divisor ? divisor : 1), MSBFIRST, SPI_MODE0);
  SPI.beginTransaction(settings);
#else  // ENABLE_SPI_TRANSACTIONS
  // Note: ESP8266 beginTransaction is the same as following code.
  SPI.setFrequency(F_SPI_MAX/(divisor ? divisor : 1));
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
#endif  // ENABLE_SPI_TRANSACTIONS
}
//------------------------------------------------------------------------------
void SdSpi::endTransaction() {
#if ENABLE_SPI_TRANSACTIONS
  // Note: endTransaction is an empty function on ESP8266.
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
  // Works without 32-bit alignment of buf.
  SPI.transferBytes(0, buf, n);
  return 0;
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
  // Adjust to 32-bit alignment.
  while ((reinterpret_cast<uintptr_t>(buf) & 0X3) && n) {
    SPI.transfer(*buf++);
    n--;
  }
  SPI.transferBytes(const_cast<uint8_t*>(buf), 0, n);
}
#endif  // defined(ESP8266)
