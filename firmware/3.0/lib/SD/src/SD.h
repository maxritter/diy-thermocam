/* SD library compatibility wrapper for use of SdFat on Teensy
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

#ifndef __SD_H__
#define __SD_H__

#include <Arduino.h>
#include <SdFat.h>
#if !defined(SD_FAT_TEENSY_MODIFIED)
#error "Teensy's SD library uses a custom modified copy of SdFat.  Standard SdFat was mistakenly used.  Arduino should print multiple libraries found for SdFat.h.  To resolve this error, you will need to move or delete the copy Arduino is using, or otherwise take steps to cause Teensy's special copy of SdFat to be used."
#endif
// Use FILE_READ & FILE_WRITE as defined by FS.h
#if defined(FILE_READ) && !defined(FS_H)
#undef FILE_READ
#endif
#if defined(FILE_WRITE) && !defined(FS_H)
#undef FILE_WRITE
#endif
#include <FS.h>

#if defined(__MK64FX512__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
#define BUILTIN_SDCARD 254
#endif

#if defined(__arm__)
  // Support everything on 32 bit boards with enough memory
  #define SDFAT_FILE FsFile
  #define SDFAT_BASE SdFs
  #define MAX_FILENAME_LEN 256
#elif defined(__AVR__)
  // Limit to 32GB cards on 8 bit Teensy with only limited memory
  #define SDFAT_FILE File32
  #define SDFAT_BASE SdFat32
  #define MAX_FILENAME_LEN 64
#endif

class SDFile : public FileImpl
{
private:
	// Classes derived from File are never meant to be constructed
	// anywhere other than open() in the parent FS class and
	// openNextFile() while traversing a directory.
	// Only the abstract File class which references these derived
	// classes is meant to have a public constructor!
	SDFile(const SDFAT_FILE &file) : sdfatfile(file), filename(nullptr) { }
	friend class SDClass;
public:
	virtual ~SDFile(void) {
		close();
	}
	virtual size_t write(const void *buf, size_t size) {
		return sdfatfile.write(buf, size);
	}
	virtual int peek() {
		return sdfatfile.peek();
	}
	virtual int available() {
		return sdfatfile.available();
	}
	virtual void flush() {
		sdfatfile.flush();
	}
	virtual size_t read(void *buf, size_t nbyte) {
		return sdfatfile.read(buf, nbyte);
	}
	virtual bool truncate(uint64_t size=0) {
		return sdfatfile.truncate(size);
	}
	virtual bool seek(uint64_t pos, int mode = SeekSet) {
		if (mode == SeekSet) return sdfatfile.seekSet(pos);
		if (mode == SeekCur) return sdfatfile.seekCur(pos);
		if (mode == SeekEnd) return sdfatfile.seekEnd(pos);
		return false;
	}
	virtual uint64_t position() {
		return sdfatfile.curPosition();
	}
	virtual uint64_t size() {
		return sdfatfile.size();
	}
	virtual void close() {
		if (filename) {
			free(filename);
			filename = nullptr;
		}
		if (sdfatfile.isOpen()) {
			sdfatfile.close();
		}
	}
	virtual bool isOpen() {
		return sdfatfile.isOpen();
	}
	virtual const char * name() {
		if (!filename) {
			filename = (char *)malloc(MAX_FILENAME_LEN);
			if (filename) {
				sdfatfile.getName(filename, MAX_FILENAME_LEN);
			} else {
				static char zeroterm = 0;
				filename = &zeroterm;
			}
		}
		return filename;
	}
	virtual boolean isDirectory(void) {
		return sdfatfile.isDirectory();
	}
	virtual File openNextFile(uint8_t mode=0) {
		SDFAT_FILE file = sdfatfile.openNextFile();
		if (file) return File(new SDFile(file));
		return File();
	}
	virtual void rewindDirectory(void) {
		sdfatfile.rewindDirectory();
	}
	virtual bool getCreateTime(DateTimeFields &tm) {
		uint16_t fat_date, fat_time;
		if (!sdfatfile.getCreateDateTime(&fat_date, &fat_time)) return false;
		if ((fat_date == 0) && (fat_time == 0)) return false;
		tm.sec = FS_SECOND(fat_time);
		tm.min = FS_MINUTE(fat_time);
		tm.hour = FS_HOUR(fat_time);
		tm.mday = FS_DAY(fat_date);
		tm.mon = FS_MONTH(fat_date) - 1;
		tm.year = FS_YEAR(fat_date) - 1900;
		return true;
	}
	virtual bool getModifyTime(DateTimeFields &tm) {
		uint16_t fat_date, fat_time;
		if (!sdfatfile.getModifyDateTime(&fat_date, &fat_time)) return false;
		if ((fat_date == 0) && (fat_time == 0)) return false;
		tm.sec = FS_SECOND(fat_time);
		tm.min = FS_MINUTE(fat_time);
		tm.hour = FS_HOUR(fat_time);
		tm.mday = FS_DAY(fat_date);
		tm.mon = FS_MONTH(fat_date) - 1;
		tm.year = FS_YEAR(fat_date) - 1900;
		return true;
	}
	virtual bool setCreateTime(const DateTimeFields &tm) {
		if (tm.year < 80 || tm.year > 207) return false;
		return sdfatfile.timestamp(T_CREATE, tm.year + 1900, tm.mon + 1,
			tm.mday, tm.hour, tm.min, tm.sec);
	}
	virtual bool setModifyTime(const DateTimeFields &tm) {
		if (tm.year < 80 || tm.year > 207) return false;
		return sdfatfile.timestamp(T_WRITE, tm.year + 1900, tm.mon + 1,
			tm.mday, tm.hour, tm.min, tm.sec);
	}
private:
	SDFAT_FILE sdfatfile;
	char *filename;
};



class SDClass : public FS
{
public:
	SDClass() { }
	bool begin(uint8_t csPin = 10);
	File open(const char *filepath, uint8_t mode = FILE_READ) {
		oflag_t flags = O_READ;
		if (mode == FILE_WRITE) flags = O_RDWR | O_CREAT | O_AT_END;
		else if (mode == FILE_WRITE_BEGIN) flags = O_RDWR | O_CREAT;
		SDFAT_FILE file = sdfs.open(filepath, flags);
		if (file) return File(new SDFile(file));
		return File();
	}
	bool exists(const char *filepath) {
		return sdfs.exists(filepath);
	}
	bool mkdir(const char *filepath) {
		return sdfs.mkdir(filepath);
	}
	bool rename(const char *oldfilepath, const char *newfilepath) {
		return sdfs.rename(oldfilepath, newfilepath);
	}
	bool remove(const char *filepath) {
		return sdfs.remove(filepath);
	}
	bool rmdir(const char *filepath) {
		return sdfs.rmdir(filepath);
	}
	uint64_t usedSize() {
		if (!cardPreviouslyPresent) return (uint64_t)0;
		return (uint64_t)(sdfs.clusterCount() - sdfs.freeClusterCount())
		  * (uint64_t)sdfs.bytesPerCluster();
	}
	uint64_t totalSize() {
		if (!cardPreviouslyPresent) return (uint64_t)0;
		return (uint64_t)sdfs.clusterCount() * (uint64_t)sdfs.bytesPerCluster();
	}
	bool format(int type=0, char progressChar=0, Print& pr=Serial);
	bool mediaPresent();

	// call to allow you to specify if your SD reader has an IO pin that
	// can be used to detect if an chip is inserted.  On BUILTIN_SDCARD
	// we will default to use it if you use the begin method, howver
	// if you bypass this and call directly to SDFat begin, then you
	// can use this to let us know. 
	bool setMediaDetectPin(uint8_t pin);

public: // allow access, so users can mix SD & SdFat APIs
	SDFAT_BASE sdfs;
	operator SDFAT_BASE & () { return sdfs; }
	static void dateTime(uint16_t *date, uint16_t *time);
private:
	bool cardPreviouslyPresent = false;
	uint8_t csPin_ = 0xff;
	uint8_t cdPin_ = 0xff;
};

extern SDClass SD;

// do not expose these defines in Arduino sketches or other libraries
#undef SDFAT_FILE
#undef SDFAT_BASE
#undef MAX_FILENAME_LEN


#define SD_CARD_TYPE_SD1 0
#define SD_CARD_TYPE_SD2 1
#define SD_CARD_TYPE_SDHC 3
class Sd2Card
{
public:
	bool init(uint32_t speed, uint8_t csPin) {
		return SD.begin(csPin);
	}
	uint8_t type() {
		return SD.sdfs.card()->type();
	}
};
class SdVolume
{
public:
	bool init(Sd2Card &card) {
		return SD.sdfs.vol() != nullptr;
	}
	uint8_t fatType() {
		return SD.sdfs.vol()->fatType();
	}
	uint32_t blocksPerCluster() {
		return SD.sdfs.vol()->sectorsPerCluster();
	}
	uint32_t clusterCount() {
		return SD.sdfs.vol()->clusterCount();
	}
#if defined(__arm__)
	operator FsVolume * () __attribute__ ((deprecated("Use SD.begin() to access SD cards"))) {
#elif defined(__AVR__)
	operator FatVolume * () __attribute__ ((deprecated("Use SD.begin() to access SD cards"))) {
#endif
		return SD.sdfs.vol();
	}
};

#endif
