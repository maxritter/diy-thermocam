/* LittleFS for Teensy
 * Copyright (c) 2020, Paul Stoffregen, paul@pjrc.com
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
#include <FS.h>
#include <SPI.h>
#include <LittleFS.h>
#include "littlefs/lfs.h"

#include <algorithm>
#include <inttypes.h>
#include <SPI.h>

// Bits in LBA for BB LUT
#define BBLUT_STATUS_ENABLED (1 << 15)
#define BBLUT_STATUS_INVALID (1 << 14)
#define BBLUT_STATUS_MASK    (BBLUT_STATUS_ENABLED | BBLUT_STATUS_INVALID)

//////////////////////////////////////////////////////
// Some useful defs and macros
#define LINEAR_TO_COLUMNECC(laddr) ((laddr) % PAGE_ECCSIZE)
#define LINEAR_TO_COLUMN(laddr) ((laddr) % pageSize)
#define LINEAR_TO_PAGE(laddr) ((laddr) / pageSize)
#define LINEAR_TO_PAGEECC(laddr) ((laddr) / PAGE_ECCSIZE)
#define LINEAR_TO_BLOCK(laddr) (LINEAR_TO_PAGE(laddr) / PAGES_PER_BLOCK)
#define BLOCK_TO_PAGE(block) ((block) * PAGES_PER_BLOCK)
#define BLOCK_TO_LINEAR(block) (BLOCK_TO_PAGE(block) * pageSize)

//////////////////////////////////////////////////////
//Chip ID
#define W25N01	0xEFAA21
#define W25N02	0xEFAA22
#define W25M02	0xEFBB21


//Geometry
#define sectors_w25n0x              1024
#define pagesPerSector_w25n0x         64
#define pageSize              		2048
#define sectorSize					pagesPerSector_w25n0x * pageSize
#define totalSize_w25n0x		 	sectorSize * sectors_w25n0x
#define pagesPerDie					65534

// Device size parameters
#define PAGE_SIZE			2048
#define PAGES_PER_BLOCK		64
#define BLOCKS_PER_DIE		1024

#define reservedBBMBlocks	24


class LittleFS_SPINAND : public LittleFS
{
public:
	LittleFS_SPINAND() {
		port = nullptr;
	}
	bool begin(uint8_t cspin, SPIClass &spiport=SPI);
	uint8_t readECC(uint32_t address, uint8_t *data, int length);
	void readBBLUT(uint16_t *LBA, uint16_t *PBA, uint8_t *linkStatus);
	bool lowLevelFormat(char progressChar);
	uint8_t addBBLUT(uint32_t block_address);  //temporary for testing
  
private:
	int read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size);
	int prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size);
	int erase(lfs_block_t block);
	int wait(uint32_t microseconds);
	static int static_read(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, void *buffer, lfs_size_t size) {
		//Serial.printf("  flash rd: block=%d, offset=%d, size=%d\n", block, offset, size);
		return ((LittleFS_SPINAND *)(c->context))->read(block, offset, buffer, size);
	}
	static int static_prog(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, const void *buffer, lfs_size_t size) {
		//Serial.printf("  flash wr: block=%d, offset=%d, size=%d\n", block, offset, size);
		return ((LittleFS_SPINAND *)(c->context))->prog(block, offset, buffer, size);
	}
	static int static_erase(const struct lfs_config *c, lfs_block_t block) {
		//Serial.printf("  flash er: block=%d\n", block);
		return ((LittleFS_SPINAND *)(c->context))->erase(block);
	}
	static int static_sync(const struct lfs_config *c) {
		return 0;
	}
  bool isReady();
  bool writeEnable();
  void eraseSector(uint32_t address);
  void writeStatusRegister(uint8_t reg, uint8_t data);
  uint8_t readStatusRegister(uint16_t reg, bool dump);
  void loadPage(uint32_t address);

  void deviceReset();
  
	SPIClass *port;
	uint8_t pin;
	uint8_t addrbits;
	uint32_t progtime;
	uint32_t erasetime;
	uint32_t chipsize;
	uint32_t blocksize;
	
private:
  uint8_t die = 0;      //die = 0: use first 1GB die PA[16], die = 1: use second 1GB die PA[16].
  uint8_t dies = 0;		//used for W25M02
  uint32_t capacityID ;   // capacity
  uint32_t deviceID;

  uint16_t eccSize = 64;
  uint16_t PAGE_ECCSIZE = 2112;

};


#if defined(__IMXRT1062__)
class LittleFS_QPINAND : public LittleFS
{
public:
	LittleFS_QPINAND() { }
	bool begin();
    bool deviceErase();
	uint8_t readECC(uint32_t targetPage, uint8_t *buf, int size);
	void readBBLUT(uint16_t *LBA, uint16_t *PBA, uint8_t *linkStatus);
	bool lowLevelFormat(char progressChar);
	uint8_t addBBLUT(uint32_t block_address);  //temporary for testing
	
private:
	int read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size);
	int prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size);
	int erase(lfs_block_t block);
	int wait(uint32_t microseconds);
	static int static_read(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, void *buffer, lfs_size_t size) {
		//Serial.printf(".....  flash rd: block=%d, offset=%d, size=%d\n", block, offset, size);
		return ((LittleFS_QPINAND *)(c->context))->read(block, offset, buffer, size);
	}
	static int static_prog(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, const void *buffer, lfs_size_t size) {
		//Serial.printf(".....  flash wr: block=%d, offset=%d, size=%d\n", block, offset, size);
		return ((LittleFS_QPINAND *)(c->context))->prog(block, offset, buffer, size);
	}
	static int static_erase(const struct lfs_config *c, lfs_block_t block) {
		//Serial.printf(".....  flash er: block=%d\n", block);
		return ((LittleFS_QPINAND *)(c->context))->erase(block);
	}
	static int static_sync(const struct lfs_config *c) {
		return 0;
	}
  bool isReady();
  bool writeEnable();
  void deviceReset();
  void eraseSector(uint32_t address);
  void writeStatusRegister(uint8_t reg, uint8_t data);
  uint8_t readStatusRegister(uint16_t reg, bool dump);
  
	uint8_t addrbits;
	uint32_t progtime;
	uint32_t erasetime;
	uint32_t chipsize;
	uint32_t blocksize;
	
private:
  uint8_t die = 0;      //die = 0: use first 1GB die, die = 1: use second 1GB die.
  uint8_t dies;
  uint32_t capacityID ;   // capacity
  uint32_t deviceID;

  uint16_t eccSize = 64;
  uint16_t PAGE_ECCSIZE = 2112;

};

#endif










