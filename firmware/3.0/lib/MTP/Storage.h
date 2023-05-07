// Storage.h - Teensy MTP Responder library
// Copyright (C) 2017 Fredrik Hubinette <hubbe@hubbe.net>
//
// With updates from MichaelMC and Yoong Hor Meng <yoonghm@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// modified for SDFS by WMXZ
// Nov 2020 adapted to SdFat-beta / SD combo
// 19-nov-2020 adapted to FS

#ifndef Storage_H
#define Storage_H

#include "core_pins.h"

#include "FS.h"
#ifndef FILE_WRITE_BEGIN
  #define FILE_WRITE_BEGIN 2
#endif


#define MTPD_MAX_FILESYSTEMS  20
#ifndef MAX_FILENAME_LEN
  #define MAX_FILENAME_LEN 256
#endif

class mSD_Base
{
  public:
    mSD_Base() {
      fsCount = 0;
    }

    void sd_addFilesystem(FS &fs, const char *name) {
      if (fsCount < MTPD_MAX_FILESYSTEMS) {
        sd_name[fsCount] = name;
        sdx[fsCount++] = &fs;
      }
    }

    uint32_t sd_getStoreID( const char *name)
    {
      for(int ii=0; ii<fsCount;ii++) if(!strcmp(name,sd_name[ii])) return ii;
      return 0xFFFFFFFFUL;
    }

    uint32_t sd_getFSCount(void) {return fsCount;}
    const char *sd_getFSName(uint32_t store) { return sd_name[store];}

    File sd_open(uint32_t store, const char *filename, uint32_t mode) { return sdx[store]->open(filename,mode);  }
    bool sd_mkdir(uint32_t store, char *filename) {  return sdx[store]->mkdir(filename);  }
    bool sd_rename(uint32_t store, char *oldfilename, char *newfilename) { return sdx[store]->rename(oldfilename,newfilename);  }
    bool sd_remove(uint32_t store, const char *filename) {    Serial.println(filename); return sdx[store]->remove(filename);  }
    bool sd_rmdir(uint32_t store, char *filename) { return sdx[store]->rmdir(filename);  }

    uint64_t sd_totalSize(uint32_t store) { return sdx[store]->totalSize();  }
    uint64_t sd_usedSize(uint32_t store)  { return sdx[store]->usedSize();  }

    bool sd_copy(uint32_t store0, char *oldfilename, uint32_t store1, char *newfilename);
    bool sd_moveDir(uint32_t store0, char *oldfilename, uint32_t store1, char *newfilename);

  private:
    int fsCount;
    const char *sd_name[MTPD_MAX_FILESYSTEMS];
    FS *sdx[MTPD_MAX_FILESYSTEMS];
};

// This interface lets the MTP responder interface any storage.
// We'll need to give the MTP responder a pointer to one of these.
class MTPStorageInterface {
public:
  virtual void addFilesystem(FS &filesystem, const char *name)=0;
  virtual uint32_t get_FSCount(void) = 0;
  virtual const char *get_FSName(uint32_t storage) = 0;

  virtual uint64_t totalSize(uint32_t storage) = 0;
  virtual uint64_t usedSize(uint32_t storage) = 0;

  // Return true if this storage is read-only
  virtual bool readonly(uint32_t storage) = 0;

  // Does it have directories?
  virtual bool has_directories(uint32_t storage) = 0;

  virtual void StartGetObjectHandles(uint32_t storage, uint32_t parent) = 0;
  virtual uint32_t GetNextObjectHandle(uint32_t  storage) = 0;

  virtual void GetObjectInfo(uint32_t handle, char* name, uint32_t* size, uint32_t* parent, uint16_t *store) = 0;
  virtual uint32_t GetSize(uint32_t handle) = 0;

  virtual uint32_t Create(uint32_t storage, uint32_t parent, bool folder, const char* filename) = 0;
  virtual void read(uint32_t handle, uint32_t pos, char* buffer, uint32_t bytes) = 0;
  virtual size_t write(const char* data, uint32_t size) = 0;
  virtual void close() = 0;
  virtual bool DeleteObject(uint32_t object) = 0;
  virtual void CloseIndex() = 0;

  virtual void ResetIndex() = 0;
  virtual bool rename(uint32_t handle, const char* name) = 0 ;
  virtual bool move(uint32_t handle, uint32_t newStorage, uint32_t newParent) = 0 ;
  virtual uint32_t copy(uint32_t handle, uint32_t newStorage, uint32_t newParent) = 0 ;

  virtual bool CopyFiles(uint32_t storage, uint32_t handle, uint32_t newHandle) = 0;
};

  struct Record 
  { uint32_t parent;
    uint32_t child;  // size stored here for files
    uint32_t sibling;
    uint8_t isdir;
    uint8_t scanned;
    uint16_t store;  // index int physical storage (0 ... num_storages-1)
    char name[MAX_FILENAME_LEN];
  };

  void mtp_yield(void);


// Storage implementation for SD. SD needs to be already initialized.
class MTPStorage_SD : public MTPStorageInterface, mSD_Base
{ 
public:
  void addFilesystem(FS &fs, const char *name) { sd_addFilesystem(fs, name);}
  void dumpIndexList(void);
  uint32_t getStoreID(const char *name) {return sd_getStoreID(name);}

private:
  File index_;
  File file_;
  File child_;

  int num_storage = 0;
  const char **sd_str = 0;
  
  uint32_t mode_ = 0;
  uint32_t open_file_ = 0xFFFFFFFEUL;

  bool readonly(uint32_t storage);
  bool has_directories(uint32_t storage) ;
  
  uint64_t totalSize(uint32_t storage) ;
  uint64_t usedSize(uint32_t storage) ;

  void CloseIndex() ;
  void OpenIndex() ;
  void GenerateIndex(uint32_t storage) ;
  void ScanDir(uint32_t storage, uint32_t i) ;
  void ScanAll(uint32_t storage) ;

  void removeFile(uint32_t store, char *filename);

  uint32_t index_entries_ = 0;
  bool index_generated = false;

  bool all_scanned_ = false;
  uint32_t next_;
  bool follow_sibling_;

  void WriteIndexRecord(uint32_t i, const Record& r) ;
  uint32_t AppendIndexRecord(const Record& r) ;
  Record ReadIndexRecord(uint32_t i) ;
  uint16_t ConstructFilename(int i, char* out, int len) ;
  void OpenFileByIndex(uint32_t i, uint32_t mode = FILE_READ) ;
  void printRecord(int h, Record *p);

  uint32_t get_FSCount(void) {return sd_getFSCount();}
  const char *get_FSName(uint32_t storage) { return sd_getFSName(storage);}

  void StartGetObjectHandles(uint32_t storage, uint32_t parent) override ;
  uint32_t GetNextObjectHandle(uint32_t  storage) override ;
  void GetObjectInfo(uint32_t handle, char* name, uint32_t* size, uint32_t* parent, uint16_t *store) override ;
  uint32_t GetSize(uint32_t handle) override;
  void read(uint32_t handle, uint32_t pos, char* out, uint32_t bytes) override ;
  bool DeleteObject(uint32_t object) override ;

  uint32_t Create(uint32_t storage, uint32_t parent,  bool folder, const char* filename) override ;

  size_t write(const char* data, uint32_t bytes) override ;
  void close() override ;

  bool rename(uint32_t handle, const char* name) override ;
  bool move(uint32_t handle, uint32_t newStorage, uint32_t newParent) override ;
  uint32_t copy(uint32_t handle, uint32_t newStorage, uint32_t newParent) override ;

  bool CopyFiles(uint32_t storage, uint32_t handle, uint32_t newHandle) override ;
  void ResetIndex() override ;
};

#endif
