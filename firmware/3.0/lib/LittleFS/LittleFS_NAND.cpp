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
#include <LittleFS.h>

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



#define SPICONFIG_NAND   SPISettings(55000000, MSBFIRST, SPI_MODE0)


PROGMEM static const struct chipinfo {
	uint8_t id[3];
	uint8_t addrbits;	// number of address bits, 24 or 32
	uint16_t progsize;	// page size for programming, in bytes
	uint32_t erasesize;	// sector size for erasing, in bytes
	uint32_t chipsize;	// total number of bytes in the chip
	uint32_t progtime;	// maximum microseconds to wait for page programming
	uint32_t erasetime;	// maximum microseconds to wait for sector erase
} known_chips[] = {
	//NAND
	//{{0xEF, 0xAA, 0x21}, 24, 2048, 131072, 134217728,   2000, 15000},  //Winbond W25N01G
	//Upper 24 blocks * 128KB/block will be used for bad block replacement area
	//so reducing total chip size: 134217728 - 24*131072
	{{0xEF, 0xAA, 0x21}, 24, 2048, 131072, 131596288,   2000, 15000},  //Winbond W25N01G
	//{{0xEF, 0xAA, 0x22}, 24, 2048, 131072, 134217728*2, 2000, 15000},  //Winbond W25N02G
	{{0xEF, 0xAA, 0x22}, 24, 2048, 131072, 265289728, 2000, 15000},  //Winbond W25N02G
	{{0xEF, 0xBB, 0x21}, 24, 2048, 131072, 265289728, 2000, 15000},  //Winbond W25M02
};

volatile uint32_t currentPage     = UINT32_MAX;
volatile uint32_t currentPageRead = UINT32_MAX;


static const struct chipinfo * chip_lookup(const uint8_t *id)
{
	const unsigned int numchips = sizeof(known_chips) / sizeof(struct chipinfo);
	for (unsigned int i=0; i < numchips; i++) {
		const uint8_t *chip = known_chips[i].id;
		if (id[0] == chip[0] && id[1] == chip[1] && id[2] == chip[2]) {
			return known_chips + i;
		}
	}
	return nullptr;
}


FLASHMEM
bool LittleFS_SPINAND::begin(uint8_t cspin, SPIClass &spiport)
{
	pin = cspin;
	port = &spiport;

	//Serial.println("flash begin");
	configured = false;
	digitalWrite(pin, HIGH);
	pinMode(pin, OUTPUT);
	port->begin();

	uint8_t buf[5] = {0x9F, 0, 0, 0, 0};
	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
	port->transfer(buf, 5);
	digitalWrite(pin, HIGH);
	port->endTransaction();

	//Serial.printf("Flash ID: %02X %02X %02X\n", buf[2], buf[3], buf[4]);
	const struct chipinfo *info = chip_lookup(buf+2);
	if (!info) return false;
	//Serial.printf("Flash size is %.2f Mbyte\n", (float)info->chipsize / 1048576.0f);
	
	//capacityID = buf[3];   //W25N01G has 1 die, W25N02G had 2 dies
	deviceID = (buf[2] << 16) | (buf[3] << 8) | (buf[4]);
	//Serial.printf("Device ID: 0x%6X\n", deviceID);
	
	if(deviceID == W25N01) { 
		die = 1;
		eccSize = 64;
		PAGE_ECCSIZE = 2112;
	} else if(deviceID == W25N02) {
		die = 1;
		eccSize = 128;
		PAGE_ECCSIZE = 2176;
	} else if(deviceID == W25M02) {
		dies = 2;
		eccSize = 64;
		PAGE_ECCSIZE = 2112;
	}

		
  //uint8_t status;
  // No protection, WP-E off, WP-E prevents use of IO2.  PROT_REG(0xAO), PROT_CLEAR(0)
  writeStatusRegister(0xA0, 0);
  readStatusRegister(0xA0, false);

  // Buffered read mode (BUF = 1), ECC enabled (ECC = 1), 0xB0(0xB0), ECC_ENABLE((1 << 4)), ReadMode((1 << 3))
  writeStatusRegister(0xB0, (1 << 4) | (1 << 3));
  readStatusRegister(0xB0, false);

	memset(&lfs, 0, sizeof(lfs));
	memset(&config, 0, sizeof(config));
	config.context = (void *)this;
	config.read = &static_read;
	config.prog = &static_prog;
	config.erase = &static_erase;
	config.sync = &static_sync;
	config.read_size = info->progsize;
	config.prog_size = info->progsize;
	config.block_size = info->erasesize;
	config.block_count = info->chipsize / info->erasesize;
	config.block_cycles = 400;
	config.cache_size = info->progsize;
	config.lookahead_size = info->progsize;
	config.name_max = LFS_NAME_MAX;
	addrbits = info->addrbits;
	progtime = info->progtime;
	erasetime = info->erasetime;
	configured = true;
	blocksize = info->erasesize;
	chipsize = info->chipsize;

	//Serial.println("attempting to mount existing media");
	if (lfs_mount(&lfs, &config) < 0) {
		//Serial.println("couldn't mount media, attemping to format");
		if (lfs_format(&lfs, &config) < 0) {
			//Serial.println("format failed :(");
			port = nullptr;
			return false;
		}
		//Serial.println("attempting to mount freshly formatted media");
		if (lfs_mount(&lfs, &config) < 0) {
			//Serial.println("mount after format failed :(");
			port = nullptr;
			return false;
		}
	}
	mounted = true;
	//Serial.println("success");
	return true;
}


static void printtbuf(const void *buf, unsigned int len) __attribute__((unused));
static void printtbuf(const void *buf, unsigned int len)
{
	//const uint8_t *p = (const uint8_t *)buf;
	//Serial.print("    ");
	//while (len--) Serial.printf("%02X ", *p++);
	//Serial.println();
}

static bool blockIsBlank(struct lfs_config *config, lfs_block_t block, void *readBuf, bool full=true );
static bool blockIsBlank(struct lfs_config *config, lfs_block_t block, void *readBuf, bool full)
{
	if (!readBuf) return false;
	for (lfs_off_t offset=0; offset < config->block_size; offset += config->read_size) {
		memset(readBuf, 0, config->read_size);
		config->read(config, block, offset, readBuf, config->read_size);
		const uint8_t *buf = (uint8_t *)readBuf;
		//printtbuf(buf, 20);
		for (unsigned int i=0; i < config->read_size; i++) {
			if (buf[i] != 0xFF) return false;
		}
		if ( !full )
			return true; // first bytes read as 0xFF
	}
	return true; // all bytes read as 0xFF
}

int LittleFS_SPINAND::read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size)
{
	if (!port) return LFS_ERR_IO;
	const uint32_t addr = block * config.block_size + offset;
	
	if(deviceID == W25N01){
		uint16_t targetPage = LINEAR_TO_PAGE(addr);
		if(currentPageRead != targetPage){
		  loadPage(addr);
		  currentPageRead = targetPage;
		}
	} else {
		loadPage(addr);
	}
	
	wait(progtime);
	
	uint16_t column = LINEAR_TO_COLUMN(addr);

	uint8_t cmd[4];
	cmd[0] = 0x03;  //0x03, READ Data
	cmd[1] = column >> 8; 
	cmd[2] = column;
	cmd[3] = 0;
    
  	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
	port->transfer(cmd, 4);
	port->transfer(buf, size);
    digitalWrite(pin, HIGH);
    port->endTransaction();

	wait(progtime);

	// Check ECC
	uint8_t statReg = readStatusRegister(0xC0, false);
	uint8_t eccCode = (((statReg) & ((1 << 5)|(1 << 4))) >> 4);
	
	wait(progtime);
	
	switch (eccCode) {
	case 0: // Successful read, no ECC correction
	  break;
	case 1: // Successful read with ECC correction
	  //Serial.printf("Successful read with ECC correction (addr, code): %x, %x\n", addr, eccCode);
	case 2: // Uncorrectable ECC in a single page
	  //Serial.printf("Uncorrectable ECC in a single page (addr, code): %x, %x\n", addr, eccCode);
	case 3: // Uncorrectable ECC in multiple pages
	  addBBLUT(LINEAR_TO_BLOCK(addr));
	  //deviceReset();
	  //Serial.printf("Uncorrectable ECC in a multipe pages (addr, code): %x, %x\n", addr, eccCode);
	  break;
	}

	//printtbuf(buf, 20);
	return 0;
}

int LittleFS_SPINAND::prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size)
{
	if (!port) return LFS_ERR_IO;
	
	uint8_t cmd1[4], die_select;

	const uint32_t address = block * config.block_size + offset;
	
	//Program Data Load
	uint16_t columnAddress = LINEAR_TO_COLUMN(address);
	uint32_t pageAddress = LINEAR_TO_PAGE(address);

	if(deviceID == W25M02 || deviceID  == W25N02) {
		if(pageAddress > pagesPerDie) {
			//pageAddress -= pagesPerDie;
			die_select = 1;
			cmd1[1] = 1;
		} else {
			cmd1[1] = 0;
			die_select = 0;
		}
	} else {
		cmd1[1] = 0;
	}
	
	if(deviceID == W25M02) {
		//issue Select Die command before issuing a page load
		port->beginTransaction(SPICONFIG_NAND);
		digitalWrite(pin, LOW);
		port -> transfer(0xC2);
		port -> transfer(die_select);
		digitalWrite(pin, HIGH);
		port->endTransaction();
		if(pageAddress > pagesPerDie)
			pageAddress -= pagesPerDie;		//W25M02 has 2 separate W25N01 dies addressed individually
		cmd1[1] = 0;						//dummy block for write is 0.
		wait(progtime);
	} 
	
	writeEnable();   //sets the WEL in Status Reg to 1 (bit 2)
	
	uint8_t cmd[3];
	cmd[0] = 0x02;  //program data load, 0x02,  write data to the data buffer
	cmd[1] = columnAddress >> 8; 
	cmd[2] = columnAddress;

	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
	port->transfer(cmd, 3);
	port->transfer(buf, nullptr, size); 
	digitalWrite(pin, HIGH);
	port->endTransaction();

	wait(progtime);
	//uint8_t status = readStatusRegister(0xA0, false );  //0xA0 - status register
	//if ((status &  (1 << 3)) == 1)   //Status Program Fail
	//	Serial.println( "Programed Status: FAILED" );

	//Program Execute, 0x10
	cmd1[0] = 0x10;   //Program Execute, write from data buffer to physical memory page sepc
	//cmd1[1] defined above
	cmd1[2] = pageAddress >> 8; 
	cmd1[3] = pageAddress;
	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
	port->transfer(cmd1, 4);
	digitalWrite(pin, HIGH);
	port->endTransaction();

	return wait(progtime);
}

int LittleFS_SPINAND::erase(lfs_block_t block)
{
	if (!port) return LFS_ERR_IO;
	
	const uint32_t addr = block * config.block_size;
	
	void *buffer = malloc(config.read_size);
	if ( buffer != nullptr) {
		if ( blockIsBlank(&config, block, buffer)) {
			free(buffer);
			return 0; // Already formatted exit no wait
		}
		free(buffer);
	}

	eraseSector(addr);
	return 	wait(erasetime);
}
 
bool LittleFS_SPINAND::isReady()
{
	uint8_t val;
  	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
    port->transfer(0x05);  //0x05 - read status register
    port->transfer(0xC0);
    val = port->transfer(0x00);
    digitalWrite(pin, HIGH);
    port->endTransaction();
  return ((val & (1 << 0)) == 0);
}

int LittleFS_SPINAND::wait(uint32_t microseconds)
{
	elapsedMicros usec = 0;
	while (1) {
		if (isReady()) break;
		if (usec > microseconds) return LFS_ERR_IO; // timeout
		yield();
	}
	//Serial.printf("  waited %u us\n", (unsigned int)usec);
	return 0; // success
}

 
 bool LittleFS_SPINAND::writeEnable()
 {
   uint8_t status;
  	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
    port->transfer(0x06);  //Write Enable 0x06
    digitalWrite(pin, HIGH);
    port->endTransaction();
    wait(progtime);
	
  status = readStatusRegister(0xC0, false);
  return status & (0x02);
 }


void LittleFS_SPINAND::eraseSector(uint32_t address)
{
	uint32_t pageAddr = LINEAR_TO_PAGE(address) ;

	//if(pageAddr > pagesPerDie) pageAddr -= pagesPerDie;

	uint8_t cmd[4];
	uint8_t die_select = 0;

	if(deviceID == W25M02 || deviceID  == W25N02) {
		if(pageAddr > pagesPerDie) {
			cmd[1] = 1;
			die_select = 1;
		} else {
			cmd[1] = 0;
			die_select = 0;
		}
	} else {
		cmd[1] = 0;
	}

	if(deviceID == W25M02) {
		//setup new LUT to issue Select Die command before issuing a page load
		port->beginTransaction(SPICONFIG_NAND);
		digitalWrite(pin, LOW);
		port -> transfer(0xC2);   //die select
		port -> transfer(die_select);
		digitalWrite(pin, HIGH);
		port->endTransaction();
		if(pageAddr > pagesPerDie)
			pageAddr -= pagesPerDie;		//W25M02 has 2 separate W25N01 dies addressed individually
		cmd[1] = 0;						//dummy block for write is 0.
		
		wait(progtime);
	} 

    cmd[0] = 0xD8;   //Block erase, 0xD8
    cmd[2] = pageAddr >> 8;
    cmd[3] = pageAddr;

	writeEnable();
	
	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
	port->transfer(cmd, 4);
    digitalWrite(pin, HIGH);
    port->endTransaction();
  
    //uint16_t status = readStatusRegister(0x05,false);
    //if ((status &  (1 << 2)) == 1)   //Status erase Fail
	//		Serial.println( "erase Status: FAILED ");
}


void LittleFS_SPINAND::writeStatusRegister(uint8_t reg, uint8_t data)
{
	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
    port->transfer(0x01);  //0x01 - write status register
    port->transfer(reg);
    port->transfer(data);
    digitalWrite(pin, HIGH);
    port->endTransaction();

}


uint8_t LittleFS_SPINAND::readStatusRegister(uint16_t reg, bool dump)
{
  uint8_t val;
  	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
    port->transfer(0x05);  //0x05 - read status register
    port->transfer(reg);
    val = port->transfer(0x00);
    digitalWrite(pin, HIGH);
    port->endTransaction();

  if(dump) {
    //Serial.printf("Status of reg 0x%x: \n", reg);
    //Serial.printf("(HEX: ) 0x%02X, (Binary: )", val);
    //Serial.println(val, BIN);
    //Serial.println();
  }
  
  return val;

}



////////////////////////////////////////////////////////////
void LittleFS_SPINAND::loadPage(uint32_t address)
{
    uint32_t targetPage = LINEAR_TO_PAGE(address);
  
    uint8_t cmd[4], die_select;
	
	if(targetPage > pagesPerDie) {
		die_select = 1;
		cmd[1] = 1;
	} else {
		die_select = 0;
		cmd[1] = 0;
	}

	if(deviceID == W25M02) {
		//setup new LUT to issue Select Die command before issuing a page load
		port->beginTransaction(SPICONFIG_NAND);
		digitalWrite(pin, LOW);
		port -> transfer(0xC2);
		port -> transfer(die_select);
		digitalWrite(pin, HIGH);
		port->endTransaction();
		if(targetPage > pagesPerDie)
			targetPage -= pagesPerDie;		//W25M02 has 2 separate W25N01 dies addressed individually
		cmd[1] = 0;						//dummy block for write is 0.
		wait(progtime);		
	} 

    cmd[0] = 0x13;   //Page Data Read
	//cmd[1] defined above
    cmd[2] = targetPage >> 8; 
    cmd[3] = targetPage;

   	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
	port->transfer(cmd, 4);
    digitalWrite(pin, HIGH);
    port->endTransaction();
   
}
  
uint8_t LittleFS_SPINAND::readECC(uint32_t targetPage, uint8_t *data, int length)
{

    uint16_t column = LINEAR_TO_COLUMNECC(targetPage*eccSize);
	targetPage = LINEAR_TO_PAGEECC(targetPage*eccSize);
	
    uint8_t cmd[4], die_select;
	
	if(targetPage > pagesPerDie) {
		die_select = 1;
		cmd[1] = 1;
	} else {
		die_select = 0;
		cmd[1] = 0;
	}

	if(deviceID == W25M02) {
		//setup new LUT to issue Select Die command before issuing a page load
		port->beginTransaction(SPICONFIG_NAND);
		digitalWrite(pin, LOW);
		port -> transfer(0xC2);
		port -> transfer(die_select);
		digitalWrite(pin, HIGH);
		port->endTransaction();
		if(targetPage > pagesPerDie)
			targetPage -= pagesPerDie;		//W25M02 has 2 separate W25N01 dies addressed individually
		cmd[1] = 0;						//dummy block for write is 0.
	} 

    cmd[0] = 0x13;   //Page Data Read
	//cmd[1] defined above
    cmd[2] = targetPage >> 8; 
    cmd[3] = targetPage;

   	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
	port->transfer(cmd, 4);
    digitalWrite(pin, HIGH);
    port->endTransaction();

	wait(progtime);
	
	uint8_t cmd1[4];
	cmd1[0] = 0x03;  //0x03, READ Data
	cmd1[1] = 0; 
	cmd1[2] = column >> 8;
	cmd1[3] = column;
    
  	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
	port->transfer(cmd1, 4);
	port->transfer(data, length);
    digitalWrite(pin, HIGH);
    port->endTransaction();

	wait(progtime);

	  // Check ECC
	  uint8_t statReg = readStatusRegister(0xC0, false);
	  uint8_t eccCode = (((statReg) & ((1 << 5)|(1 << 4))) >> 4);

	  switch (eccCode) {
	  case 0: // Successful read, no ECC correction
		break;
	  case 1: // Successful read with ECC correction
	  case 2: // Uncorrectable ECC in a single page
	  case 3: // Uncorrectable ECC in multiple pages
		//addError(address, eccCode);
		//Serial.printf("ECC Error (addr, code): %x, %x\n", address, eccCode);
		addBBLUT(LINEAR_TO_BLOCK(targetPage*eccSize));
		//deviceReset();
		break;
	}

	//printtbuf(buf, 20);
	return eccCode;
}

void LittleFS_SPINAND::readBBLUT(uint16_t *LBA, uint16_t *PBA, uint8_t *linkStatus)
{
    //uint16_t LBA, PBA;
    //uint16_t temp;
    //uint16_t openEntries = 0;
	//BBLUT_TABLE_ENTRY_COUNT     20
	//BBLUT_TABLE_ENTRY_SIZE      4  // in bytes
	
    uint8_t data[20 * 4];

  	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
	port->transfer(0xA5);  //Read BBM_LUT 0xA5
	port->transfer(0);
  for(uint32_t i = 0; i < (80); i++) {
    data[i] = 	port->transfer(0);
  }  
    digitalWrite(pin, HIGH);
    port->endTransaction();

	//See page 33 of the reference manual for W25N01G
    //Serial.println("Status of the links");
    for(int i = 0, offset = 0 ; i < 20 ; i++, offset += 4) {
      LBA[i] = data[offset+ 0] << 8 | data[offset+ 1];
      PBA[i] =  data[offset+ 2] << 8 | data[offset+ 3]; 
      
	  if (LBA[i] == 0x0000) {
		linkStatus[i] = 0;
        //openEntries++;
      } else  {
        //Serial.printf("\tEntry: %d: Logical BA - %d, Physical BA - %d\n", i, LBA[i], PBA[i]);
        linkStatus[i] = (uint8_t) (LBA[i] >> 14);
        //if(linkStatus[i] == 3) Serial.println("\t    This link is enabled and its a Valid Link!");
        //if(linkStatus[i] == 4) Serial.println("\t    This link was enabled but its not valid any more!");
        //if(linkStatus[i] == 1) Serial.println("\t    Not Applicable!");
      }
	  
    }
    //Serial.printf("OpenEntries: %d\n", openEntries);
}

uint8_t LittleFS_SPINAND::addBBLUT(uint32_t block_address)
{
  if(deviceID == W25N01) {
	//check BBLUT FULL
	uint8_t lutFull = 0;
	lutFull = readStatusRegister(0xC0, false);
	
	if(lutFull & (1 << 6)) {
		Serial.printf("Lut Full!!!!");
		exit(1);
	}
	
	//Read BBLUT
	uint16_t LBA[20], PBA[20], openEntries = 0;
	uint8_t  LUT_STATUS[20];
	uint8_t firstOpenEntry = 0;
	
	readBBLUT(LBA, PBA, LUT_STATUS);
	
	Serial.println("Status of the links");
	for(uint16_t i = 0; i < 20; i++){
		if(LUT_STATUS[i] > 0) {
			Serial.printf("\tEntry: %d: Logical BA - %d, Physical BA - %d\n", i, LBA[i], PBA[i]);
			LUT_STATUS[i] = (uint8_t) (LBA[i] >> 14);
			if(LUT_STATUS[i] == 3) Serial.println("\t    This link is enabled and its a Valid Link!");
			if(LUT_STATUS[i] == 4) Serial.println("\t    This link was enabled but its not valid any more!");
			if(LUT_STATUS[i] == 1) Serial.println("\t    Not Applicable!");
		} else {
			openEntries++;
		}
	} Serial.printf("OpenEntries: %d\n", openEntries);
	
	
	//Need to determine if the address is already in the list and what the first open entry is.
	for(uint16_t i = 0; i < 20; i++){
		if(LUT_STATUS[i] > 0) {
			if(LBA[i] == block_address) {
				Serial.printf("Address: %d, already in BBLUT!\n", block_address);
				return 0;
			}
		}
	}
	
	firstOpenEntry = 20 - openEntries;	
	Serial.printf("First Open Entry: %d\n", firstOpenEntry);
	
	//Write BBLUT with next sequential block
	#ifdef LATER
	uint8_t cmd[5];
	
	uint16_t pba, lba;
	pba = block_address;
	lba = LINEAR_TO_BLOCK((firstOpenEntry+1)*blocksize + chipsize);
	Serial.printf("PBA: %d, LBA: %d\n", pba, lba);
	
	cmd[0] = 0xA1;
	cmd[1] = pba >> 8;
	cmd[2] = pba;
	cmd[3] = lba >> 8;
	cmd[4] = lba;
	
	//port->beginTransaction(SPICONFIG_NAND);
	//digitalWrite(pin, LOW);
	//port->transfer(cmd, 5);
    //digitalWrite(pin, HIGH);
    //port->endTransaction();
	#endif
	wait(progtime);
	
  }
	return 0;
}

void LittleFS_SPINAND::deviceReset()
{

	port->beginTransaction(SPICONFIG_NAND);
	digitalWrite(pin, LOW);
    port->transfer(0xFF);
    digitalWrite(pin, HIGH);
    port->endTransaction();
  
  wait(500000);

  // No protection, WP-E off, WP-E prevents use of IO2.  PROT_REG(0xAO), PROT_CLEAR(0)
  writeStatusRegister(0xA0, 0);
  readStatusRegister(0xA0, false);

  // Buffered read mode (BUF = 1), ECC enabled (ECC = 1), 0xB0(0xB0), ECC_ENABLE((1 << 4)), ReadMode((1 << 3))
  writeStatusRegister(0xB0, (1 << 4) | (1 << 3));
  readStatusRegister(0xB0, false);

}

bool LittleFS_SPINAND::lowLevelFormat(char progressChar, Print* pr)
{
	uint32_t eraseAddr;
	bool val;
	val = LittleFS::lowLevelFormat(progressChar, pr);
	
	for(uint16_t blocks = 0; blocks < reservedBBMBlocks; blocks++) {
		eraseAddr = (config.block_count + blocks) * config.block_size;
		eraseSector(eraseAddr);
	}
	
	return val;
}


#if defined(__IMXRT1062__)

#define LUT0(opcode, pads, operand) (FLEXSPI_LUT_INSTRUCTION((opcode), (pads), (operand)))
#define LUT1(opcode, pads, operand) (FLEXSPI_LUT_INSTRUCTION((opcode), (pads), (operand)) << 16)
#define CMD_SDR         FLEXSPI_LUT_OPCODE_CMD_SDR
#define ADDR_SDR        FLEXSPI_LUT_OPCODE_RADDR_SDR
#define CADDR_SDR       FLEXSPI_LUT_OPCODE_CADDR_SDR
#define READ_SDR        FLEXSPI_LUT_OPCODE_READ_SDR
#define WRITE_SDR       FLEXSPI_LUT_OPCODE_WRITE_SDR
#define DUMMY_SDR       FLEXSPI_LUT_OPCODE_DUMMY_SDR
#define PINS1           FLEXSPI_LUT_NUM_PADS_1
#define PINS4           FLEXSPI_LUT_NUM_PADS_4

static const uint32_t flashBaseAddr = 0x00800000;
  //static const uint32_t flashBaseAddr = 0x01000000u;


static void flexspi2_ip_command(uint32_t index, uint32_t addr)
{
	uint32_t n;
	FLEXSPI2_IPCR0 = addr;
	FLEXSPI2_IPCR1 = FLEXSPI_IPCR1_ISEQID(index);
	FLEXSPI2_IPCMD = FLEXSPI_IPCMD_TRG;
	while (!((n = FLEXSPI2_INTR) & FLEXSPI_INTR_IPCMDDONE)); // wait
	if (n & FLEXSPI_INTR_IPCMDERR) {
		FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDERR;
		//Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\n", FLEXSPI2_IPRXFSTS);
	}
	FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDDONE;
}

static void flexspi2_ip_read(uint32_t index, uint32_t addr, void *data, uint32_t length)
{
	uint8_t *p = (uint8_t *)data;

	FLEXSPI2_INTR = FLEXSPI_INTR_IPRXWA;
	// Clear RX FIFO and set watermark to 16 bytes
	FLEXSPI2_IPRXFCR = FLEXSPI_IPRXFCR_CLRIPRXF | FLEXSPI_IPRXFCR_RXWMRK(1);
	FLEXSPI2_IPCR0 = addr;
	FLEXSPI2_IPCR1 = FLEXSPI_IPCR1_ISEQID(index) | FLEXSPI_IPCR1_IDATSZ(length);
	FLEXSPI2_IPCMD = FLEXSPI_IPCMD_TRG;
// page 1649 : Reading Data from IP RX FIFO
// page 1706 : Interrupt Register (INTR)
// page 1723 : IP RX FIFO Control Register (IPRXFCR)
// page 1732 : IP RX FIFO Status Register (IPRXFSTS)

	while (1) {
		if (length >= 16) {
			if (FLEXSPI2_INTR & FLEXSPI_INTR_IPRXWA) {
				volatile uint32_t *fifo = &FLEXSPI2_RFDR0;
				uint32_t a = *fifo++;
				uint32_t b = *fifo++;
				uint32_t c = *fifo++;
				uint32_t d = *fifo++;
				*(uint32_t *)(p+0) = a;
				*(uint32_t *)(p+4) = b;
				*(uint32_t *)(p+8) = c;
				*(uint32_t *)(p+12) = d;
				p += 16;
				length -= 16;
				FLEXSPI2_INTR = FLEXSPI_INTR_IPRXWA;
			}
		} else if (length > 0) {
			if ((FLEXSPI2_IPRXFSTS & 0xFF) >= ((length + 7) >> 3)) {
				volatile uint32_t *fifo = &FLEXSPI2_RFDR0;
				while (length >= 4) {
					*(uint32_t *)(p) = *fifo++;
					p += 4;
					length -= 4;
				}
				uint32_t a = *fifo;
				if (length >= 1) {
					*p++ = a & 0xFF;
					a = a >> 8;
				}
				if (length >= 2) {
					*p++ = a & 0xFF;
					a = a >> 8;
				}
				if (length >= 3) {
					*p++ = a & 0xFF;
					a = a >> 8;
				}
				length = 0;
			}
		} else {
			if (FLEXSPI2_INTR & FLEXSPI_INTR_IPCMDDONE) break;
		}
		// TODO: timeout...
	}
	if (FLEXSPI2_INTR & FLEXSPI_INTR_IPCMDERR) {
		FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDERR;
		//Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\r\n", FLEXSPI2_IPRXFSTS);
	}
	FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDDONE;
}

static void flexspi2_ip_write(uint32_t index, uint32_t addr, const void *data, uint32_t length)
{
	const uint8_t *src;
	uint32_t n, wrlen;

	FLEXSPI2_IPCR0 = addr;
	FLEXSPI2_IPCR1 = FLEXSPI_IPCR1_ISEQID(index) | FLEXSPI_IPCR1_IDATSZ(length);
	src = (const uint8_t *)data;
	FLEXSPI2_IPCMD = FLEXSPI_IPCMD_TRG;
	while (!((n = FLEXSPI2_INTR) & FLEXSPI_INTR_IPCMDDONE)) {
		if (n & FLEXSPI_INTR_IPTXWE) {
			wrlen = length;
			if (wrlen > 8) wrlen = 8;
			if (wrlen > 0) {
				//Serial.print("%");
				memcpy((void *)&FLEXSPI2_TFDR0, src, wrlen);
				src += wrlen;
				length -= wrlen;
				FLEXSPI2_INTR = FLEXSPI_INTR_IPTXWE;
			}
		}
	}
	if (n & FLEXSPI_INTR_IPCMDERR) {
		FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDERR;
		//Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\r\n", FLEXSPI2_IPRXFSTS);
	}
	FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDDONE;
}



bool LittleFS_QPINAND::begin() {
	
	//Serial.println("QSPI flash begin");

	configured = false;

	uint8_t buf[5] = {0, 0, 0, 0, 0};
	
	//if following is uncommented NANDs wont work?
  //FLEXSPI2_FLSHA2CR1 = FLEXSPI_FLSHCR1_CSINTERVAL(2)  //minimum interval between flash device Chip selection deassertion and flash device Chip selection assertion.
   //                    | FLEXSPI_FLSHCR1_CAS(11)						     //sets up 14 bit column address
   //                    | FLEXSPI_FLSHCR1_TCSH(3)                           //Serial Flash CS Hold time.
   //                    | FLEXSPI_FLSHCR1_TCSS(3);                          //Serial Flash CS setup time

 //Reset clock to 102.85714 Mhz
/*	  CCM_CCGR7 |= CCM_CCGR7_FLEXSPI2(CCM_CCGR_OFF);
	  CCM_CBCMR = (CCM_CBCMR & ~(CCM_CBCMR_FLEXSPI2_PODF_MASK | CCM_CBCMR_FLEXSPI2_CLK_SEL_MASK))
		  | CCM_CBCMR_FLEXSPI2_PODF(6) | CCM_CBCMR_FLEXSPI2_CLK_SEL(1); 
	  CCM_CCGR7 |= CCM_CCGR7_FLEXSPI2(CCM_CCGR_ON);
*/
  	FLEXSPI2_LUTKEY = FLEXSPI_LUTKEY_VALUE;
	FLEXSPI2_LUTCR = FLEXSPI_LUTCR_UNLOCK;
	
	// cmd index 8 = read ID bytes
	FLEXSPI2_LUT32 = LUT0(CMD_SDR, PINS1, 0x9F) | LUT1(READ_SDR, PINS1, 1);
	FLEXSPI2_LUT33 = 0;
	flexspi2_ip_read(8, 0x00800000, buf, 4);

	//Serial.printf("Flash ID: %02X %02X %02X\n", buf[1], buf[2], buf[3]);
	const struct chipinfo *info = chip_lookup(buf+1);
	if (!info) return false;
	//Serial.printf("Flash size is %.2f Mbyte\n", (float)info->chipsize / 1048576.0f);
	
	// configure FlexSPI2 for chip's size
	FLEXSPI2_FLSHA2CR0 = info->chipsize / 1024;
	
	//capacityID = buf[3];   //W25N01G has 1 die, W25N02G had 2 dies
	deviceID = (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
	//Serial.printf("Device ID: 0x%6X\n", deviceID);
	
	if(deviceID == W25N01) { 
		die = 1;
		eccSize = 64;
		PAGE_ECCSIZE = 2112;
	} else if(deviceID == W25N02) {
		die = 1;
		eccSize = 128;
		PAGE_ECCSIZE = 2176;
	} else if(deviceID == W25M02) {
		dies = 2;
		eccSize = 64;
		PAGE_ECCSIZE = 2112;
	}

	memset(&lfs, 0, sizeof(lfs));
	memset(&config, 0, sizeof(config));
	config.context = (void *)this;
	config.read = &static_read;
	config.prog = &static_prog;
	config.erase = &static_erase;
	config.sync = &static_sync;
	config.read_size = info->progsize;
	config.prog_size = info->progsize;
	config.block_size = info->erasesize;
	config.block_count = info->chipsize / info->erasesize;
	config.block_cycles = 400;
	config.cache_size = info->progsize;
	config.lookahead_size = info->progsize;
	config.name_max = LFS_NAME_MAX;
	addrbits = info->addrbits;
	progtime = info->progtime;
	erasetime = info->erasetime;
	configured = true;
	blocksize = info->erasesize;
	chipsize = info->chipsize;
	
  // cmd index 8 = read Status register
  // set in function readStatusRegister(uint8_t reg, bool dump)
  // cmd index 8 = write Status register
  // see function writeStatusRegister(uint8_t reg, uint8_t data)

  //cmd index 9 - WG reset, see function deviceReset()
  FLEXSPI2_LUT36 = LUT0(CMD_SDR, PINS1, 0xFF);  //0xFF Device Reset

  //cmd index 10 - read BBLUT
  // cmd 11 index write enable cmd
  // see function writeEnable()
  //cmd 12 Command based on PageAddress
  // see functions:
  // eraseSector(uint32_t addr) and
  // readBytes(uint32_t address, uint8_t *data, int length)
  //cmd 13 program load Data
  // see functions:
  // programDataLoad(uint16_t columnAddress, const uint8_t *data, int length)
  // and
  // randomProgramDataLoad(uint16_t columnAddress, const uint8_t *data, int length)

  
  //cmd 14 program read Data -- reserved.  0xEB FAST_READ_QUAD_IO
  FLEXSPI2_LUT56 = LUT0(CMD_SDR, PINS1, 0xEB) | LUT1(CADDR_SDR, PINS4, 0x10);  
  FLEXSPI2_LUT57 = LUT0(DUMMY_SDR, PINS4, 4) | LUT1(READ_SDR, PINS4, 1);

  //cmd 15 - program execute - 0x10
  FLEXSPI2_LUT60 = LUT0(CMD_SDR, PINS1, 0x10) | LUT1(ADDR_SDR, PINS1, 0x18);
    

  // No protection, WP-E off, WP-E prevents use of IO2.  PROT_REG(0xAO), PROT_CLEAR(0)
  writeStatusRegister(0xA0, 0);
  readStatusRegister(0xA0, false);

  // Buffered read mode (BUF = 1), ECC enabled (ECC = 1), 0xB0(0xB0), ECC_ENABLE((1 << 4)), ReadMode((1 << 3))
  //writeStatusRegister(0xB0, (1 << 3));
  writeStatusRegister(0xB0, (1 << 4) | (1 << 3));
  readStatusRegister(0xB0, false);
  
	//Serial.println("attempting to mount existing media");
	if (lfs_mount(&lfs, &config) < 0) {
		Serial.println("couldn't mount media, attemping to format");
		if (lfs_format(&lfs, &config) < 0) {
			Serial.println("format failed :(");
			return false;
		}
		Serial.println("attempting to mount freshly formatted media");
		if (lfs_mount(&lfs, &config) < 0) {
			Serial.println("mount after format failed :(");
			return false;
		}
	}
	mounted = true;
	//Serial.println("success");
	return true;

}

int LittleFS_QPINAND::read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size)
{
  const uint32_t address = block * config.block_size + offset;
  uint32_t newTargetPage;
  uint32_t targetPage = LINEAR_TO_PAGE(address);
  uint8_t val;
  
  //Page Data Read - 0x13
  FLEXSPI2_LUT48 = LUT0(CMD_SDR, PINS1, 0x13) | LUT1(ADDR_SDR, PINS1, 0x18);
  
  if(currentPageRead != targetPage){
	//need to create LUT for W25M02 Die Select command, 
	if(deviceID == W25M02) {
		if(targetPage >= pagesPerDie ) {
			targetPage -= pagesPerDie;
			val = 1;
		} else {
			val = 0;
		}
		newTargetPage = targetPage;
		
		// die select 0xc2
		FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0xC2) | LUT1(WRITE_SDR, PINS1, 1); 
		flexspi2_ip_write(11, 0x00800000, &val, 1);
	} else {
		if(targetPage > pagesPerDie ) {
			//targetPage -= sectorSize;
			newTargetPage = (1 << 16) | ((uint8_t)targetPage >> 8) | ((uint8_t) targetPage);
		} else {
			newTargetPage = targetPage;
		}
	}
	
	
    flexspi2_ip_command(12, 0x00800000 + newTargetPage);   // Page data read Lut
	wait(progtime);

    currentPageRead = targetPage;
  }

  uint16_t column = LINEAR_TO_COLUMN(address);
  flexspi2_ip_read(14, 0x00800000 + column, buf, size);
  

  
  //printtbuf(buf, 20);

  // Check ECC
  uint8_t statReg = readStatusRegister(0xC0, false);
  uint8_t eccCode = (((statReg) & ((1 << 5)|(1 << 4))) >> 4);


  switch (eccCode) {
    case 0: // Successful read, no ECC correction
      break;
    case 1: // Successful read with ECC correction
      //Serial.printf("Successful read with ECC correction (addr, code): %x, %x\n", addr, eccCode);
    case 2: // Uncorrectable ECC in a single page
      //Serial.printf("Uncorrectable ECC in a single page (addr, code): %x, %x\n", address, eccCode);
    case 3: // Uncorrectable ECC in multiple pages
      //Serial.printf("Uncorrectable ECC in a single page (addr, code): %x, %x\n", address, eccCode);
	  addBBLUT(LINEAR_TO_BLOCK(address));
	  //deviceReset();
	  break;

  }

	//Serial.print("Read: "); printtbuf(buf, 40);
	return 0;
}

int LittleFS_QPINAND::prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size)
{
	const uint32_t address = block * config.block_size + offset;
	uint32_t newTargetPage;
	uint8_t val;
	
	uint32_t pageAddress = LINEAR_TO_PAGE(address);
	uint16_t columnAddress = LINEAR_TO_COLUMN(address);
	
		
	//need to create LUT for W25M02 Die Select command, 
	if(deviceID == W25M02) {
		if(pageAddress > pagesPerDie ) {
			pageAddress -= pagesPerDie;
			val = 1;
		} else {
			val = 0;
		}
		newTargetPage = pageAddress;

		// die select 0xc2
		FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0xC2) | LUT1(WRITE_SDR, PINS1, 1); 
		flexspi2_ip_write(11, 0x00800000, &val, 1);
		wait(progtime);
	} else {
		if(pageAddress > pagesPerDie ) {
			//targetPage -= sectorSize;
			newTargetPage = (1 << 16) | ((uint8_t)pageAddress >> 8) | ((uint8_t) pageAddress);
		} else {
			newTargetPage = pageAddress;
		}
	}
		
	writeEnable();   //sets the WEL in Status Reg to 1 (bit 2)

	//Program Data Load - 0x32
	FLEXSPI2_LUT52 = LUT0(CMD_SDR, PINS1, 0x32) | LUT1(CADDR_SDR, PINS1, 0x10);
	FLEXSPI2_LUT53 = LUT0(WRITE_SDR, PINS4, 1);
	flexspi2_ip_write(13, 0x00800000 + columnAddress, buf, size);
	wait(progtime);

	//uint8_t status = readStatusRegister(0xC0, false );  //Status Register
	//if ((status &  (1 << 3)) == 1)  //Status Program Fail
	//	Serial.println( "Programed Status: FAILED" );
		

	//Serial.printf("PE pageAddress: %d\n", pageAddress);
	//cmd 15 - program execute - 0x10
	flexspi2_ip_command(15, 0x00800000 + newTargetPage);	

	return wait(progtime);
}

int LittleFS_QPINAND::erase(lfs_block_t block)
{	
	const uint32_t addr = block * config.block_size;
	
	void *buffer = malloc(config.read_size);
	if ( buffer != nullptr) {
		if ( blockIsBlank(&config, block, buffer)) {
			free(buffer);
			return 0; // Already formatted exit no wait
		}
		free(buffer);
	}
	
	eraseSector(addr);
	wait(erasetime);
	return 0;
}
 

 
uint8_t LittleFS_QPINAND::readStatusRegister(uint16_t reg, bool dump)
{
  uint8_t val;

  // cmd index 8 = read Status register #1 SPI, 0x05
  FLEXSPI2_LUT32 = LUT0(CMD_SDR, PINS1, 0x05) | LUT1(CMD_SDR, PINS1, reg);  
  FLEXSPI2_LUT33 = LUT0(READ_SDR, PINS1, 1);

  flexspi2_ip_read(8, 0x00800000, &val, 1 );

  if (dump) {
    //Serial.printf("Status of reg 0x%x: \n", reg);
    //Serial.printf("(HEX: ) 0x%02X, (Binary: )", val);
    //Serial.println(val, BIN);
    //Serial.println();
  }

  return val;

}

void LittleFS_QPINAND::writeStatusRegister(uint8_t reg, uint8_t data)
{
  uint8_t buf[1];
  buf[0] = data;
  // cmd index 8 = write Status register, 0x01
  FLEXSPI2_LUT32 = LUT0(CMD_SDR, PINS1, 0x01) | LUT1(CMD_SDR, PINS1, reg);
  FLEXSPI2_LUT33 = LUT0(WRITE_SDR, PINS1, 1);

  flexspi2_ip_write(8, 0x00800000, buf, 1);

}


bool LittleFS_QPINAND::isReady()
{
  uint8_t status = readStatusRegister(0xC0, false);
  return ((status & (1 << 0)) == 0);
}

int LittleFS_QPINAND::wait(uint32_t microseconds)
{
	elapsedMicros usec = 0;
	while (1) {
		if (isReady()) break;
		if (usec > microseconds) return LFS_ERR_IO; // timeout
		yield();
	}
	//Serial.printf("  waited %u us\n", (unsigned int)usec);
	return 0; // success
}


/**
 * The flash requires this write enable command to be sent before commands that would cause
 * a write like program and erase.
 */
bool LittleFS_QPINAND::writeEnable()
{
  uint8_t status;
  FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0x06);  //Write enable 0x06
  flexspi2_ip_command(11, 0x00800000); //Write Enable
  // Assume that we're about to do some writing, so the device is just about to become busy
  wait(progtime);
  
  status = readStatusRegister(0xC0, false);
  return status & (0x02);
}


/**
 * Erase a sector full of bytes to all 1's at the given byte offset in the flash chip.
 */
void LittleFS_QPINAND::eraseSector(uint32_t address)
{



  uint32_t pageAddr = LINEAR_TO_PAGE(address) ;
  //if(pageAddr > sectorSize) pageAddr -= sectorSize;

	uint32_t newTargetPage;
	uint8_t val;
	
	//need to create LUT for W25M02 Die Select command, 
	if(deviceID == W25M02) {
		if(pageAddr > pagesPerDie ) {
			pageAddr -= pagesPerDie;
			val = 1;
		} else {
			val = 0;
		}
		newTargetPage = pageAddr;

		// die select 0xc2
		FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0xC2) | LUT1(WRITE_SDR, PINS1, 1); 
		flexspi2_ip_write(11, 0x00800000, &val, 1);
		wait(progtime);
	} else {
		if(pageAddr > pagesPerDie ) {
			//targetPage -= sectorSize;
			newTargetPage = (1 << 16) | ((uint8_t)pageAddr >> 8) | ((uint8_t) pageAddr);
		} else {
			newTargetPage = pageAddr;
		}
	}
	
	writeEnable();   //sets the WEL in Status Reg to 1 (bit 2)
	// cmd index 12, Block Erase 0xD8
	FLEXSPI2_LUT48 = LUT0(CMD_SDR, PINS1, 0xD8) | LUT1(ADDR_SDR, PINS1, 0x18);
	flexspi2_ip_command(12, 0x00800000 + newTargetPage);

	//uint8_t status = readStatusRegister(0xC0, false );
	//if ((status &  (1 << 2)) == 1)  //Status erase fail
	//  Serial.println( "erase Status: FAILED ");

}


uint8_t LittleFS_QPINAND::readECC(uint32_t targetPage, uint8_t *buf, int size)
{

  uint16_t column = LINEAR_TO_COLUMNECC(targetPage*eccSize);
  targetPage = LINEAR_TO_PAGEECC(targetPage*eccSize);

  uint32_t newTargetPage;
  uint8_t val;
  
  //Page Data Read - 0x13
  FLEXSPI2_LUT48 = LUT0(CMD_SDR, PINS1, 0x13) | LUT1(ADDR_SDR, PINS1, 0x18);
  
	//need to create LUT for W25M02 Die Select command, 
	if(deviceID == W25M02) {
		if(targetPage >= pagesPerDie ) {
			targetPage -= pagesPerDie;
			val = 1;
		} else {
			val = 0;
		}
		newTargetPage = targetPage;
		
		// die select 0xc2
		FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0xC2) | LUT1(WRITE_SDR, PINS1, 1); 
		flexspi2_ip_write(11, 0x00800000, &val, 1);
	} else {
		if(targetPage > pagesPerDie ) {
			//targetPage -= sectorSize;
			newTargetPage = (1 << 16) | ((uint8_t)targetPage >> 8) | ((uint8_t) targetPage);
		} else {
			newTargetPage = targetPage;
		}
	}
	
	
    flexspi2_ip_command(12, 0x00800000 + newTargetPage);   // Page data read Lut
	wait(progtime);

    currentPageRead = targetPage;

  flexspi2_ip_read(14, 0x00800000 + column, buf, size);
  
  // Check ECC
  uint8_t statReg = readStatusRegister(0xC0, false);
  uint8_t eccCode = (((statReg) & ((1 << 5)|(1 << 4))) >> 4);
  
	  switch (eccCode) {
	  case 0: // Successful read, no ECC correction
		break;
	  case 1: // Successful read with ECC correction
	  case 2: // Uncorrectable ECC in a single page
	  case 3: // Uncorrectable ECC in multiple pages
		//addError(address, eccCode);
		//Serial.printf("ECC Error (addr, code): %x, %x\n", address, eccCode);
		addBBLUT(LINEAR_TO_BLOCK(targetPage*eccSize));
		//deviceReset();
		break;
	}
  
  
  return eccCode;
}

void LittleFS_QPINAND::readBBLUT(uint16_t *LBA, uint16_t *PBA, uint8_t *linkStatus)
{
    //uint16_t LBA, PBA;
    //uint16_t linkStatus[20];
    //uint16_t openEntries = 0;
	//BBLUT_TABLE_ENTRY_COUNT     20
	//BBLUT_TABLE_ENTRY_SIZE      4  // in bytes
	
    uint8_t data[20 * 4];

    FLEXSPI2_LUT40 = LUT0(CMD_SDR, PINS1, 0xA5) | LUT1(DUMMY_SDR, 8, 1);  //Read BBM_LUT 0xA5
    FLEXSPI2_LUT41 = LUT0(READ_SDR, PINS1, 1);
    flexspi2_ip_read(10, 0x00800000, data, sizeof(data));


	//See page 33 of the reference manual for W25N01G
    //Serial.println("Status of the links");
    for(int i = 0, offset = 0 ; i < 20 ; i++, offset += 4) {
      LBA[i] = data[offset+ 0] << 8 | data[offset+ 1];
      PBA[i] =  data[offset+ 2] << 8 | data[offset+ 3]; 
      
	  if (LBA[i] == 0x0000) {
		linkStatus[i] = 0;
        //openEntries++;
      } else  {
        //Serial.printf("\tEntry: %d: Logical BA - %d, Physical BA - %d\n", i, LBA[i], PBA[i]);
        linkStatus[i] = (uint8_t) (LBA[i] >> 14);
        //if(linkStatus[i] == 3) Serial.println("\t    This link is enabled and its a Valid Link!");
        //if(linkStatus[i] == 4) Serial.println("\t    This link was enabled but its not valid any more!");
        //if(linkStatus[i] == 1) Serial.println("\t    Not Applicable!");
      }
	  
    }
    //Serial.printf("OpenEntries: %d\n", openEntries);
}

uint8_t LittleFS_QPINAND::addBBLUT(uint32_t block_address)
{
  if(deviceID == W25N01) {
	//check BBLUT FULL
	uint8_t lutFull = 0;
	lutFull = readStatusRegister(0xC0, false);
	
	if(lutFull & (1 << 6)) {
		Serial.printf("Lut Full!!!!");
		exit(1);
	}
	
	//Read BBLUT
	uint16_t LBA[20], PBA[20], openEntries = 0;
	uint8_t  LUT_STATUS[20];
	uint8_t firstOpenEntry = 0;
	
	readBBLUT(LBA, PBA, LUT_STATUS);
	
	Serial.println("Status of the links");
	for(uint16_t i = 0; i < 20; i++){
		if(LUT_STATUS[i] > 0) {
			Serial.printf("\tEntry: %d: Logical BA - %d, Physical BA - %d\n", i, LBA[i], PBA[i]);
			LUT_STATUS[i] = (uint8_t) (LBA[i] >> 14);
			if(LUT_STATUS[i] == 3) Serial.println("\t    This link is enabled and its a Valid Link!");
			if(LUT_STATUS[i] == 4) Serial.println("\t    This link was enabled but its not valid any more!");
			if(LUT_STATUS[i] == 1) Serial.println("\t    Not Applicable!");
		} else {
			openEntries++;
		}
	} Serial.printf("OpenEntries: %d\n", openEntries);
	
	
	//Need to determine if the address is already in the list and what the first open entry is.
	for(uint16_t i = 0; i < 20; i++){
		if(LUT_STATUS[i] > 0) {
			if(LBA[i] == block_address) {
				Serial.printf("Address: %d, already in BBLUT!\n", block_address);
				return 0;
			}
		}
	}
	
	firstOpenEntry = 20 - openEntries;	
	Serial.printf("First Open Entry: %d\n", firstOpenEntry);
	
	//Write BBLUT with next sequential block
	uint16_t pba, lba;
	pba = block_address;
	//lba = LINEAR_TO_BLOCK((firstOpenEntry+1)*config.block_size);
	lba = LINEAR_TO_BLOCK((firstOpenEntry+1)*blocksize + chipsize);
	Serial.printf("PBA: %d, LBA: %d\n", pba, lba);
	#ifdef LATER	
	uint8_t cmd[4];
	cmd[0] = pba >> 8;
	cmd[1] = pba;
	cmd[2] = lba >> 8;
	cmd[3] = lba;
	
   //FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0xA1) | LUT0(WRITE_SDR, PINS1, 1);  
   //flexspi2_ip_write(8, 0x00800000, cmd, 4);
	#endif	
	wait(progtime);
  }
	return 0;
}

void LittleFS_QPINAND::deviceReset()
{
  //cmd index 9 - WG reset, see function deviceReset()
  FLEXSPI2_LUT36 = LUT0(CMD_SDR, PINS1, 0xFF);
  flexspi2_ip_command(9, 0x00800000); //reset

  wait(500000);

  // No protection, WP-E off, WP-E prevents use of IO2.  PROT_REG(0xAO), PROT_CLEAR(0)
  writeStatusRegister(0xA0, 0);
  readStatusRegister(0xA0, false);

  // Buffered read mode (BUF = 1), ECC enabled (ECC = 1), 0xB0(0xB0), ECC_ENABLE((1 << 4)), ReadMode((1 << 3))
  writeStatusRegister(0xB0, (1 << 4) | (1 << 3));
  readStatusRegister(0xB0, false);

}

bool LittleFS_QPINAND::lowLevelFormat(char progressChar)
{
	uint32_t eraseAddr;
	bool val;
	val = LittleFS::lowLevelFormat(progressChar);
	
	for(uint16_t blocks = 0; blocks < reservedBBMBlocks; blocks++) {
		eraseAddr = (config.block_count + blocks) * config.block_size;
		eraseSector(eraseAddr);
	}
	
	return val;
}

#endif // __IMXRT1062__