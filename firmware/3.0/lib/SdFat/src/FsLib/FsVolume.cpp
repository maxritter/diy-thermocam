/**
 * Copyright (c) 2011-2021 Bill Greiman
 * This file is part of the SdFat library for SD memory cards.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "FsLib.h"
FsVolume* FsVolume::m_cwv = nullptr;
//------------------------------------------------------------------------------
bool FsVolume::begin(FsBlockDevice* blockDev, bool setCwv, uint8_t part) {
  m_blockDev = blockDev;
  m_fVol = nullptr;
  m_xVol = new (m_volMem) ExFatVolume;
  if (m_xVol && m_xVol->begin(m_blockDev, false, part)) {
    goto done;
  }
  m_xVol = nullptr;
  m_fVol = new (m_volMem) FatVolume;
  if (m_fVol && m_fVol->begin(m_blockDev, false, part)) {
    goto done;
  }
  m_cwv = nullptr;
  m_fVol = nullptr;
  return false;

 done:
  if (setCwv || !m_cwv) {
    m_cwv = this;
  }
  return true;
}
//------------------------------------------------------------------------------
bool FsVolume::begin(FsBlockDevice* blockDev, bool setCwv,
 uint32_t firstSector, uint32_t numSectors) {
  m_blockDev = blockDev;
  m_fVol = nullptr;
  m_xVol = new (m_volMem) ExFatVolume;
  if (m_xVol && m_xVol->begin(m_blockDev, false, firstSector, numSectors)) {
    goto done;
  }
  m_xVol = nullptr;
  m_fVol = new (m_volMem) FatVolume;
  if (m_fVol && m_fVol->begin(m_blockDev, false, firstSector, numSectors)) {
    goto done;
  }
  m_cwv = nullptr;
  m_fVol = nullptr;
  return false;

 done:
  if (setCwv || !m_cwv) {
    m_cwv = this;
  }
  return true;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool FsVolume::ls(print_t* pr, const char* path, uint8_t flags) {
  FsBaseFile dir;
  return dir.open(this, path, O_RDONLY) && dir.ls(pr, flags);
}
//------------------------------------------------------------------------------
FsFile FsVolume::open(const char *path, oflag_t oflag) {
  FsFile tmpFile;
  tmpFile.open(this, path, oflag);
  return tmpFile;
}
#if ENABLE_ARDUINO_STRING
//------------------------------------------------------------------------------
FsFile FsVolume::open(const String &path, oflag_t oflag) {
  return open(path.c_str(), oflag );
}
#endif  // ENABLE_ARDUINO_STRING


// getVolumeLabel & setVolumeLabel from UsbMscFat by Warren Watson
bool FsVolume::getVolumeLabel(char *volume_label, size_t cb)
{
  uint8_t buf[512];
  if (!volume_label || (cb < 12)) return false; // don't want to deal with it
  *volume_label = 0; // make sure if we fail later we return empty string as well.
  uint8_t fat_type = fatType();
  uint32_t root_dir;
  // Lets go hard core here.
  if (m_fVol) {
    FatFile rootFat;
    if (!rootFat.openRoot(m_fVol)) return false;
    uint32_t root_dir_size = rootFat.dirSize(); // how big is this directory...
    rootFat.close();

    if (fat_type == FAT_TYPE_FAT32) {
      root_dir = m_fVol->dataStartSector();
    } else {
      root_dir = m_fVol->rootDirStart();
    }
    //Serial.printf("\n$$$ PFsVolume::getVolumeLabel(%u): %u %u\n", fat_type, root_dir, root_dir_size);
    uint16_t index_in_sector=0;
    m_blockDev->readSector(root_dir, buf);
    while (root_dir_size) {
      DirFat_t *dir;
      dir = reinterpret_cast<DirFat_t*>(&buf[index_in_sector]);
      if (dir->name[0] == FAT_NAME_FREE) break;  // at end of list...
      if (dir->attributes == 0x08) {
        size_t i;
        for (i = 0; i < 11; i++) {
          volume_label[i]  = dir->name[i];
          }
        while ((i > 0) && (volume_label[i - 1] == ' ')) i--; // trim off trailing blanks
        volume_label[i] = 0;
        return true;
      }
      index_in_sector += 32;  // increment to next entry...
      root_dir_size-=32;
      if (index_in_sector >= 512 && root_dir_size) {
        root_dir++;
        m_blockDev->readSector(root_dir, buf);
        index_in_sector = 0;
      }
    }
  } else if (m_xVol) {

    uint32_t chs = m_xVol->clusterHeapStartSector();
    uint32_t rdc = m_xVol->rootDirectoryCluster();
    uint32_t root_dir_size = m_xVol->rootLength();
    uint32_t spc = m_xVol->sectorsPerCluster();
    //Serial.printf("\n$$$ PFsVolume::getVolumeLabel(Ex): %u %x %x %u\n", root_dir_size, chs, rdc, spc);
    uint32_t root_dir = chs + (rdc-2)*spc;
    //Serial.printf("  $$$ Guess sector: %x\n", root_dir);

    uint16_t index_in_sector=0;
    m_blockDev->readSector(root_dir, buf);
    while (root_dir_size) {
      DirLabel_t *dir;
      dir = reinterpret_cast<DirLabel_t*>(&buf[index_in_sector]);
      //if (dir->name[0] == 0) break;  // at end of list...
      if (dir->type == EXFAT_TYPE_LABEL) {
        size_t i;
        for (i = 0; i < dir->labelLength; i++) {
          volume_label[i] = dir->unicode[2 * i];
        }
        volume_label[i] = 0;
        return true;
      } else if (dir->type == 0) break; // I believe this marks the end...

      index_in_sector += 32;  // increment to next entry...
      root_dir_size-=32;
      if (index_in_sector >= 512 && root_dir_size) {
        root_dir++;
        m_blockDev->readSector(root_dir, buf);
        index_in_sector = 0;
      }
    }
  }
  return false; // no volume label was found
}


bool FsVolume::setVolumeLabel(const char *volume_label)
{
  uint8_t buf[512];
  uint8_t fat_type = fatType();
  uint32_t root_dir;
  bool label_found = false;

  // Lets go hard core here.
  if (m_fVol) {
    FatFile rootFat;
    DirFat_t *dir = nullptr;
    if (!rootFat.openRoot(m_fVol)) return false;
    uint32_t root_dir_size = rootFat.dirSize(); // how big is this directory...
    rootFat.close();

    if (fat_type == FAT_TYPE_FAT32) {
      root_dir = m_fVol->dataStartSector();
    } else {
      root_dir = m_fVol->rootDirStart();
    }
    //Serial.printf("\n$$$ PFsVolume::setVolumeLabel(%u): %u %u\n", fat_type, root_dir, root_dir_size);
    uint16_t index_in_sector=0;
    uint32_t first_deleted_entry_sector = 0;
    uint16_t first_deleted_entry_index = 0;

    m_blockDev->readSector(root_dir, buf);
    //dump_hexbytes(buf, 512);
    while (root_dir_size) {
      dir = reinterpret_cast<DirFat_t*>(&buf[index_in_sector]);
      if (dir->name[0] == FAT_NAME_DELETED) {
        if (!first_deleted_entry_sector) {
          first_deleted_entry_sector = root_dir;
          first_deleted_entry_index = index_in_sector;
        }
      }
      else if (dir->name[0] == FAT_NAME_FREE) break;  // at end of list...
      else if (dir->attributes == 0x08) {
        label_found = true;
        break;
      }
      index_in_sector += 32;  // increment to next entry...
      root_dir_size-=32;
      if (index_in_sector >= 512 && root_dir_size) {
        root_dir++;
        m_blockDev->readSector(root_dir, buf);
        //Serial.printf(">> %x\n", root_dir);
        //dump_hexbytes(buf, 512);
        index_in_sector = 0;
      }
    }
    // Lets see if we found something...
    if (!volume_label || !*volume_label) {
      if (label_found) {
        Serial.printf("Found volume label - deleted\n");
        dir->name[0] = FAT_NAME_DELETED;  // mark item as deleted
        dir->attributes = 0;
        m_blockDev->writeSector(root_dir, buf);
        m_blockDev->syncDevice();
      }
      return true;
    }
    // Lets see where we should write...
    if (!label_found) {
      if (first_deleted_entry_sector) {
        if (first_deleted_entry_sector != root_dir) {
          root_dir = first_deleted_entry_sector;
          m_blockDev->readSector(root_dir, buf);
        }
        index_in_sector = first_deleted_entry_index;
        dir = reinterpret_cast<DirFat_t*>(&buf[index_in_sector]);
        label_found = true;
      }
      else if (dir->name[0] == FAT_NAME_FREE) label_found = true;
    }
    if (label_found) {  // or found a spot for it.
      memset((void*)dir, 0, 32);  // clear it out.
      if (FsDateTime::callback) {
        uint16_t cur_date;
        uint16_t cur_time;
        uint8_t cur_ms10;
        FsDateTime::callback(&cur_date, &cur_time, &cur_ms10);
        setLe16(dir->modifyTime, cur_time);
        setLe16(dir->modifyDate, cur_date);
      }
      for (size_t i = 0; i < 11; i++) {
        dir->name[i] = *volume_label? *volume_label++ : ' '; // fill in the 11 trailing blanks
      }
      dir->attributes = 8;  // mark as a volume label.
      m_blockDev->writeSector(root_dir, buf);
      m_blockDev->syncDevice();
        return true;
    }

  } else if (m_xVol) {
    DirLabel_t *dir = nullptr;
    uint32_t chs = m_xVol->clusterHeapStartSector();
    uint32_t rdc = m_xVol->rootDirectoryCluster();
    uint32_t root_dir_size = m_xVol->rootLength();
    uint32_t spc = m_xVol->sectorsPerCluster();
    //Serial.printf("\n$$$ PFsVolume::setVolumeLabel(Ex): %u %x %x %u\n", root_dir_size, chs, rdc, spc);
    uint32_t root_dir = chs + (rdc-2)*spc;
    //Serial.printf("  $$$ Guess sector: %x\n", root_dir);

    uint16_t index_in_sector=0;
    m_blockDev->readSector(root_dir, buf);
    //m_xVol->cacheSafeRead(root_dir, buf);
    //dump_hexbytes(buf, 512);
    while (root_dir_size) {
      dir = reinterpret_cast<DirLabel_t*>(&buf[index_in_sector]);
      //if (dir->name[0] == 0) break;  // at end of list...
      if (dir->type == EXFAT_TYPE_LABEL) {
        label_found = true;
        break;
      } else if (dir->type == 0) break;
      index_in_sector += 32;  // increment to next entry...
      root_dir_size-=32;
      if (index_in_sector >= 512 && root_dir_size) {
        root_dir++;
        m_blockDev->readSector(root_dir, buf);
        //m_xVol->cacheSafeRead(root_dir, buf);
        index_in_sector = 0;
        //Serial.println("---");
        //dump_hexbytes(buf, 512);
      }
    }
    // Lets see if we found something...
    if (!volume_label || !*volume_label) {
      if (label_found) {
        Serial.printf("Found volume label - deleted\n");
        dir->type &= 0x7f;  // mark item as deleted
        m_blockDev->writeSector(root_dir, buf);
        //m_xVol->cacheSafeWrite(root_dir, buf);
        m_xVol->cacheClear();
        m_blockDev->syncDevice();
      }
      return true;
    }
    // Lets see where we should write...
    //
    if (label_found || (dir->type == 0)) {  // or found a spot for it.
      uint8_t cb = strlen(volume_label);
      if (cb > 11) cb = 11; // truncate off.
      dir->type = EXFAT_TYPE_LABEL;
      dir->labelLength = cb;
      uint8_t *puni = dir->unicode;
      while (cb--) {
        *puni = *volume_label++;
        puni += 2;
      }
      //m_xVol->cacheSafeWrite(root_dir, buf);
      m_blockDev->writeSector(root_dir, buf);
      m_xVol->cacheClear();
      m_blockDev->syncDevice();
      return true;
    }
  }
  return false; // no volume label was found
}


