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

#pragma once
#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include "littlefs/lfs.h"
//#include <algorithm>

class LittleFSFile : public File
{
private:
	// Classes derived from File are never meant to be constructed from
	// anywhere other than openNextFile() and open() in their parent FS
	// class.  Only the abstract File class which references these
	// derived classes is meant to have a public constructor!
	LittleFSFile(lfs_t *lfsin, lfs_file_t *filein, const char *name) {
		lfs = lfsin;
		file = filein;
		dir = nullptr;
		strlcpy(fullpath, name, sizeof(fullpath));
		//Serial.printf("  LittleFSFile ctor (file), this=%x\n", (int)this);
	}
	LittleFSFile(lfs_t *lfsin, lfs_dir_t *dirin, const char *name) {
		lfs = lfsin;
		dir = dirin;
		file = nullptr;
		strlcpy(fullpath, name, sizeof(fullpath));
		//Serial.printf("  LittleFSFile ctor (dir), this=%x\n", (int)this);
	}
	friend class LittleFS;
public:
	virtual ~LittleFSFile() {
		//Serial.printf("  LittleFSFile dtor, this=%x\n", (int)this);
		close();
	}
#ifdef FILE_WHOAMI
	virtual void whoami() {
		Serial.printf("  LittleFSFile this=%x, refcount=%u\n",
			(int)this, getRefcount());
	}
#endif
	virtual size_t write(const void *buf, size_t size) {
		//Serial.println("write");
		if (!file) return 0;
		//Serial.println(" is regular file");
		return lfs_file_write(lfs, file, buf, size);
	}
	virtual int peek() {
		return -1; // TODO...
	}
	virtual int available() {
		if (!file) return 0;
		lfs_soff_t pos = lfs_file_tell(lfs, file);
		if (pos < 0) return 0;
		lfs_soff_t size = lfs_file_size(lfs, file);
		if (size < 0) return 0;
		return size - pos;
	}
	virtual void flush() {
		if (file) lfs_file_sync(lfs, file);
	}
	virtual size_t read(void *buf, size_t nbyte) {
		if (file) {
			lfs_ssize_t r = lfs_file_read(lfs, file, buf, nbyte);
			if (r < 0) r = 0;
			return r;
		}
		return 0;
	}
	virtual bool truncate(uint64_t size=0) {
		if (!file) return false;
		if (lfs_file_truncate(lfs, file, size) >= 0) return true;
		return false;
	}
	virtual bool seek(uint64_t pos, int mode = SeekSet) {
		if (!file) return false;
		int whence;
		if (mode == SeekSet) whence = LFS_SEEK_SET;
		else if (mode == SeekCur) whence = LFS_SEEK_CUR;
		else if (mode == SeekEnd) whence = LFS_SEEK_END;
		else return false;
		if (lfs_file_seek(lfs, file, pos, whence) >= 0) return true;
		return false;
	}
	virtual uint64_t position() {
		if (!file) return 0;
		lfs_soff_t pos = lfs_file_tell(lfs, file);
		if (pos < 0) pos = 0;
		return pos;
	}
	virtual uint64_t size() {
		if (!file) return 0;
		lfs_soff_t size = lfs_file_size(lfs, file);
		if (size < 0) size = 0;
		return size;
	}
	virtual void close() {
		if (file) {
			//Serial.printf("  close file, this=%x, lfs=%x", (int)this, (int)lfs);
			lfs_file_close(lfs, file); // we get stuck here, but why?
			free(file);
			file = nullptr;
		}
		if (dir) {
			//Serial.printf("  close dir, this=%x, lfs=%x", (int)this, (int)lfs);
			lfs_dir_close(lfs, dir);
			free(dir);
			dir = nullptr;
		}
		//Serial.println("  end of close");
	}
	virtual operator bool() {
		return file || dir;
	}
	virtual const char * name() {
		const char *p = strrchr(fullpath, '/');
		if (p) return p + 1;
		return fullpath;
	}
	virtual boolean isDirectory(void) {
		return dir != nullptr;
	}
	virtual File openNextFile(uint8_t mode=0) {
		if (!dir) return File();
		struct lfs_info info;
		do {
			memset(&info, 0, sizeof(info)); // is this necessary?
			if (lfs_dir_read(lfs, dir, &info) <= 0) return File();
		} while (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0);
		//Serial.printf("  next name = \"%s\"\n", info.name);
		char pathname[128];
		strlcpy(pathname, fullpath, sizeof(pathname));
		size_t len = strlen(pathname);
		if (len > 0 && pathname[len-1] != '/' && len < sizeof(pathname)-2) {
			// add trailing '/', if not already present
			pathname[len++] = '/';
			pathname[len] = 0;
		}
		strlcpy(pathname + len, info.name, sizeof(pathname) - len);
		if (info.type == LFS_TYPE_REG) {
			lfs_file_t *f = (lfs_file_t *)malloc(sizeof(lfs_file_t));
			if (!f) return File();
			if (lfs_file_open(lfs, f, pathname, LFS_O_RDONLY) >= 0) {
				return File(new LittleFSFile(lfs, f, pathname));
			}
			free(f);
		} else { // LFS_TYPE_DIR
			lfs_dir_t *d = (lfs_dir_t *)malloc(sizeof(lfs_dir_t));
			if (!d) return File();
			if (lfs_dir_open(lfs, d, pathname) >= 0) {
				return File(new LittleFSFile(lfs, d, pathname));
			}
			free(d);
		}
		return File();
	}
	virtual void rewindDirectory(void) {
		if (dir) lfs_dir_rewind(lfs, dir);
	}

	using Print::write;
private:
	lfs_t *lfs;
	lfs_file_t *file;
	lfs_dir_t *dir;
	char *filename;
	char fullpath[128];
};




class LittleFS : public FS
{
public:
	LittleFS() {
		configured = false;
		mounted = false;
		config.context = nullptr;
	}
	bool quickFormat();
	bool lowLevelFormat(char progressChar=0, Print* pr=&Serial);
	uint32_t formatUnused(uint32_t blockCnt, uint32_t blockStart);
	File open(const char *filepath, uint8_t mode = FILE_READ) {
		//Serial.println("LittleFS open");
		if (!mounted) return File();
		if (mode == FILE_READ) {
			struct lfs_info info;
			if (lfs_stat(&lfs, filepath, &info) < 0) return File();
			//Serial.printf("LittleFS open got info, name=%s\n", info.name);
			if (info.type == LFS_TYPE_REG) {
				//Serial.println("  regular file");
				lfs_file_t *file = (lfs_file_t *)malloc(sizeof(lfs_file_t));
				if (!file) return File();
				if (lfs_file_open(&lfs, file, filepath, LFS_O_RDONLY) >= 0) {
					return File(new LittleFSFile(&lfs, file, filepath));
				}
				free(file);
			} else { // LFS_TYPE_DIR
				//Serial.println("  directory");
				lfs_dir_t *dir = (lfs_dir_t *)malloc(sizeof(lfs_dir_t));
				if (!dir) return File();
				if (lfs_dir_open(&lfs, dir, filepath) >= 0) {
					return File(new LittleFSFile(&lfs, dir, filepath));
				}
				free(dir);
			}
		} else {
			lfs_file_t *file = (lfs_file_t *)malloc(sizeof(lfs_file_t));
			if (!file) return File();
			if (lfs_file_open(&lfs, file, filepath, LFS_O_RDWR | LFS_O_CREAT) >= 0) {
				if (mode == FILE_WRITE) {
					// FILE_WRITE opens at end of file
					lfs_file_seek(&lfs, file, 0, LFS_SEEK_END);
				} // else FILE_WRITE_BEGIN
				return File(new LittleFSFile(&lfs, file, filepath));
			}
		}
		return File();
	}
	bool exists(const char *filepath) {
		if (!mounted) return false;
		struct lfs_info info;
		if (lfs_stat(&lfs, filepath, &info) < 0) return false;
		return true;
	}
	bool mkdir(const char *filepath) {
		if (!mounted) return false;
		if (lfs_mkdir(&lfs, filepath) < 0) return false;
		return true;
	}
	bool rename(const char *oldfilepath, const char *newfilepath) {
		if (!mounted) return false;
		if (lfs_rename(&lfs, oldfilepath, newfilepath) < 0) return false;
		return true;
	}
	bool remove(const char *filepath) {
		if (!mounted) return false;
		if (lfs_remove(&lfs, filepath) < 0) return false;
		return true;
	}
	bool rmdir(const char *filepath) {
		return remove(filepath);
	}
	uint64_t usedSize() {
		if (!mounted) return 0;
		int blocks = lfs_fs_size(&lfs);
		if (blocks < 0 || (lfs_size_t)blocks > config.block_count) return totalSize();
		return blocks * config.block_size;
	}
	uint64_t totalSize() {
		if (!mounted) return 0;
		return config.block_count * config.block_size;
	}
protected:
	bool configured;
	bool mounted;
	lfs_t lfs;
	lfs_config config;
};





class LittleFS_RAM : public LittleFS
{
public:
	LittleFS_RAM() { }
	bool begin(uint32_t size) {
#if defined(__IMXRT1062__)
		return begin(extmem_malloc(size), size);
#else
		return begin(malloc(size), size);
#endif
	}
	bool begin(void *ptr, uint32_t size) {
		//Serial.println("configure "); delay(5);
		configured = false;
		if (!ptr) return false;
		// memset(ptr, 0xFF, size); // always start with blank slate
		size = size & 0xFFFFFF00;
		memset(&lfs, 0, sizeof(lfs));
		memset(&config, 0, sizeof(config));
		config.context = ptr;
		config.read = &static_read;
		config.prog = &static_prog;
		config.erase = &static_erase;
		config.sync = &static_sync;
		if ( size > 1024*1024 ) {
			config.read_size = 256; // Must set cache_size. If read_buffer or prog_buffer are provided manually, these must be cache_size.
			config.prog_size = 256;
			config.block_size = 2048;
			config.block_count = size / config.block_size;
			config.block_cycles = 900;
			config.cache_size = 256;
			config.lookahead_size = 512; // (config.block_count / 8 + 7) & 0xFFFFFFF8;
		}
		else {
			config.read_size = 64;
			config.prog_size = 64;
			config.block_size = 256;
			config.block_count = size / 256;
			config.block_cycles = 50;
			config.cache_size = 64;
			config.lookahead_size = 64;
		}
		config.name_max = LFS_NAME_MAX;
		config.file_max = 0;
		config.attr_max = 0;
		configured = true;
		if (lfs_mount(&lfs, &config) < 0) {
			memset(ptr, 0xFF, size); // always start with blank slate
			if (lfs_format(&lfs, &config) < 0) return false;
			//Serial.println("formatted");
			if (lfs_mount(&lfs, &config) < 0) return false;
		}
		//Serial.println("mounted atfer format");
		mounted = true;
		return true;
	}
	FLASHMEM
	uint32_t formatUnused(uint32_t blockCnt, uint32_t blockStart) {
		return 0;
	}
private:
	static int static_read(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, void *buffer, lfs_size_t size) {
		//Serial.printf("    ram rd: block=%d, offset=%d, size=%d\n", block, offset, size);
		uint32_t index = block * c->block_size + offset;
		memcpy(buffer, (uint8_t *)(c->context) + index, size);
		return 0;
	}
	static int static_prog(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, const void *buffer, lfs_size_t size) {
		//Serial.printf("    ram wr: block=%d, offset=%d, size=%d\n", block, offset, size);
		uint32_t index = block * c->block_size + offset;
		memcpy((uint8_t *)(c->context) + index, buffer, size);
		return 0;
	}
	static int static_erase(const struct lfs_config *c, lfs_block_t block) {
		uint32_t index = block * c->block_size;
		memset((uint8_t *)(c->context) + index, 0xFF, c->block_size);
		return 0;
	}
	static int static_sync(const struct lfs_config *c) {
		if ( c->context >= (void *)0x20200000 ) {
			//Serial.printf("    arm_dcache_flush_delete: ptr=0x%x, size=%d\n", c->context, c->block_count * c->block_size);
			arm_dcache_flush_delete(c->context, c->block_count * c->block_size );
		}
		return 0;
	}
};


class LittleFS_SPIFlash : public LittleFS
{
public:
	LittleFS_SPIFlash() {
		port = nullptr;
	}
	bool begin(uint8_t cspin, SPIClass &spiport=SPI);
private:
	int read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size);
	int prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size);
	int erase(lfs_block_t block);
	int wait(uint32_t microseconds);
	static int static_read(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, void *buffer, lfs_size_t size) {
		//Serial.printf("  flash rd: block=%d, offset=%d, size=%d\n", block, offset, size);
		return ((LittleFS_SPIFlash *)(c->context))->read(block, offset, buffer, size);
	}
	static int static_prog(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, const void *buffer, lfs_size_t size) {
		//Serial.printf("  flash wr: block=%d, offset=%d, size=%d\n", block, offset, size);
		return ((LittleFS_SPIFlash *)(c->context))->prog(block, offset, buffer, size);
	}
	static int static_erase(const struct lfs_config *c, lfs_block_t block) {
		//Serial.printf("  flash er: block=%d\n", block);
		return ((LittleFS_SPIFlash *)(c->context))->erase(block);
	}
	static int static_sync(const struct lfs_config *c) {
		return 0;
	}
	SPIClass *port;
	uint8_t pin;
	uint8_t addrbits;
	uint32_t progtime;
	uint32_t erasetime;
};



class LittleFS_SPIFram : public LittleFS
{
public:
	LittleFS_SPIFram() {
		port = nullptr;
	}
	bool begin(uint8_t cspin, SPIClass &spiport=SPI);
private:
	int read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size);
	int prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size);
	int erase(lfs_block_t block);
	int wait(uint32_t microseconds);
	static int static_read(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, void *buffer, lfs_size_t size) {
		//Serial.printf("  flash rd: block=%d, offset=%d, size=%d\n", block, offset, size);
		return ((LittleFS_SPIFram *)(c->context))->read(block, offset, buffer, size);
	}
	static int static_prog(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, const void *buffer, lfs_size_t size) {
		//Serial.printf("  flash wr: block=%d, offset=%d, size=%d\n", block, offset, size);
		return ((LittleFS_SPIFram *)(c->context))->prog(block, offset, buffer, size);
	}
	static int static_erase(const struct lfs_config *c, lfs_block_t block) {
		//Serial.printf("  flash er: block=%d\n", block);
		return ((LittleFS_SPIFram *)(c->context))->erase(block);
	}
	static int static_sync(const struct lfs_config *c) {
		return 0;
	}
	
	
	
	SPIClass *port;
	uint8_t pin;
	uint8_t addrbits;
	uint32_t progtime;
	uint32_t erasetime;
};


#if defined(__IMXRT1062__)
class LittleFS_QSPIFlash : public LittleFS
{
public:
	LittleFS_QSPIFlash() { }
	bool begin();
private:
	int read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size);
	int prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size);
	int erase(lfs_block_t block);
	int wait(uint32_t microseconds);
	static int static_read(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, void *buffer, lfs_size_t size) {
		//Serial.printf("   qspi rd: block=%d, offset=%d, size=%d\n", block, offset, size);
		return ((LittleFS_QSPIFlash *)(c->context))->read(block, offset, buffer, size);
	}
	static int static_prog(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, const void *buffer, lfs_size_t size) {
		//Serial.printf("   qspi wr: block=%d, offset=%d, size=%d\n", block, offset, size);
		return ((LittleFS_QSPIFlash *)(c->context))->prog(block, offset, buffer, size);
	}
	static int static_erase(const struct lfs_config *c, lfs_block_t block) {
		//Serial.printf("   qspi er: block=%d\n", block);
		return ((LittleFS_QSPIFlash *)(c->context))->erase(block);
	}
	static int static_sync(const struct lfs_config *c) {
		return 0;
	}
	uint8_t addrbits;
	uint32_t progtime;
	uint32_t erasetime;
};
#else
class LittleFS_QSPIFlash : public LittleFS
{
public:
	LittleFS_QSPIFlash() { }
	bool begin() { return false; }
};
#endif



#if defined(__IMXRT1062__)
class LittleFS_Program : public LittleFS
{
public:
	LittleFS_Program() { }
	bool begin(uint32_t size);
private:
	static int static_read(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, void *buffer, lfs_size_t size);
	static int static_prog(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, const void *buffer, lfs_size_t size);
	static int static_erase(const struct lfs_config *c, lfs_block_t block);
	static int static_sync(const struct lfs_config *c) { return 0; }
	static uint32_t baseaddr;
};
#else
// TODO: implement for Teensy 3.x...
class LittleFS_Program : public LittleFS
{
public:
	LittleFS_Program() { }
	bool begin(uint32_t size) { return false; }
};
#endif

class LittleFS_SPINAND : public LittleFS
{
public:
	LittleFS_SPINAND() {
		port = nullptr;
	}
	bool begin(uint8_t cspin, SPIClass &spiport=SPI);
	uint8_t readECC(uint32_t address, uint8_t *data, int length);
	void readBBLUT(uint16_t *LBA, uint16_t *PBA, uint8_t *linkStatus);
	bool lowLevelFormat(char progressChar, Print* pr=&Serial);
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









