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
#include <string.h>
#define DBG_FILE "FatPartition.cpp"
#include "../common/DebugMacros.h"
#include "FatLib.h"

//------------------------------------------------------------------------------
bool FatPartition::allocateCluster(uint32_t current, uint32_t* next) {
  uint32_t find;
  bool setStart;
  if (m_allocSearchStart < current) {
    // Try to keep file contiguous. Start just after current cluster.
    find = current;
    setStart = false;
  } else {
    find = m_allocSearchStart;
    setStart = true;
  }
  while (1) {
    find++;
    if (find > m_lastCluster) {
      if (setStart) {
        // Can't find space, checked all clusters.
        DBG_FAIL_MACRO;
        goto fail;
      }
      find = m_allocSearchStart;
      setStart = true;
      continue;
    }
    if (find == current) {
      // Can't find space, already searched clusters after current.
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint32_t f;
    int8_t fg = fatGet(find, &f);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (fg && f == 0) {
      break;
    }
  }
  if (setStart) {
    m_allocSearchStart = find;
  }
  // Mark end of chain.
  if (!fatPutEOC(find)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (current) {
    // Link clusters.
    if (!fatPut(current, find)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  updateFreeClusterCount(-1);
  *next = find;
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
// find a contiguous group of clusters
bool FatPartition::allocContiguous(uint32_t count, uint32_t* firstCluster) {
  // flag to save place to start next search
  bool setStart = true;
  // start of group
  uint32_t bgnCluster;
  // end of group
  uint32_t endCluster;
  // Start at cluster after last allocated cluster.
  endCluster = bgnCluster = m_allocSearchStart + 1;

  // search the FAT for free clusters
  while (1) {
    if (endCluster > m_lastCluster) {
      // Can't find space.
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint32_t f;
    int8_t fg = fatGet(endCluster, &f);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (f || fg == 0) {
      // don't update search start if unallocated clusters before endCluster.
      if (bgnCluster != endCluster) {
        setStart = false;
      }
      // cluster in use try next cluster as bgnCluster
      bgnCluster = endCluster + 1;
    } else if ((endCluster - bgnCluster + 1) == count) {
      // done - found space
      break;
    }
    endCluster++;
  }
  // Remember possible next free cluster.
  if (setStart) {
    m_allocSearchStart = endCluster;
  }
  // mark end of chain
  if (!fatPutEOC(endCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // link clusters
  while (endCluster > bgnCluster) {
    if (!fatPut(endCluster - 1, endCluster)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    endCluster--;
  }
  // Maintain count of free clusters.
  updateFreeClusterCount(-count);

  // return first cluster number to caller
  *firstCluster = bgnCluster;
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
// Fetch a FAT entry - return -1 error, 0 EOC, else 1.
int8_t FatPartition::fatGet(uint32_t cluster, uint32_t* value) {
  uint32_t sector;
  uint32_t next;
  uint8_t* pc;

  // error if reserved cluster of beyond FAT
  if (cluster < 2 || cluster > m_lastCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  if (fatType() == 32) {
    sector = m_fatStartSector + (cluster >> (m_bytesPerSectorShift - 2));
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_READ);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint16_t offset = (cluster << 2) & m_sectorMask;
    next = getLe32(pc + offset);
  } else if (fatType() == 16) {
    cluster &= 0XFFFF;
    sector = m_fatStartSector + (cluster >> (m_bytesPerSectorShift - 1) );
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_READ);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint16_t offset = (cluster << 1) & m_sectorMask;
    next = getLe16(pc + offset);
  } else if (FAT12_SUPPORT && fatType() == 12) {
    uint16_t index = cluster;
    index += index >> 1;
    sector = m_fatStartSector + (index >> m_bytesPerSectorShift);
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_READ);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    index &= m_sectorMask;
    uint16_t tmp = pc[index];
    index++;
    if (index == m_bytesPerSector) {
      pc = fatCachePrepare(sector + 1, FsCache::CACHE_FOR_READ);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      index = 0;
    }
    tmp |= pc[index] << 8;
    next = cluster & 1 ? tmp >> 4 : tmp & 0XFFF;
  } else {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (isEOC(next)) {
    return 0;
  }
  *value = next;
  return 1;

 fail:
  return -1;
}
//------------------------------------------------------------------------------
// Store a FAT entry
bool FatPartition::fatPut(uint32_t cluster, uint32_t value) {
  uint32_t sector;
  uint8_t* pc;

  // error if reserved cluster of beyond FAT
  if (cluster < 2 || cluster > m_lastCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  if (fatType() == 32) {
    sector = m_fatStartSector + (cluster >> (m_bytesPerSectorShift - 2));
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint16_t offset = (cluster << 2) & m_sectorMask;
    setLe32(pc + offset, value);
    return true;
  }

  if (fatType() == 16) {
    cluster &= 0XFFFF;
    sector = m_fatStartSector + (cluster >> (m_bytesPerSectorShift - 1) );
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    uint16_t offset = (cluster << 1) & m_sectorMask;
    setLe16(pc + offset, value);
    return true;
  }

  if (FAT12_SUPPORT && fatType() == 12) {
    uint16_t index = cluster;
    index += index >> 1;
    sector = m_fatStartSector + (index >> m_bytesPerSectorShift);
    pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    index &= m_sectorMask;
    uint8_t tmp = value;
    if (cluster & 1) {
      tmp = (pc[index] & 0XF) | tmp << 4;
    }
    pc[index] = tmp;

    index++;
    if (index == m_bytesPerSector) {
      sector++;
      index = 0;
      pc = fatCachePrepare(sector, FsCache::CACHE_FOR_WRITE);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    tmp = value >> 4;
    if (!(cluster & 1)) {
      tmp = ((pc[index] & 0XF0)) | tmp >> 4;
    }
    pc[index] = tmp;
    return true;
  } else {
    DBG_FAIL_MACRO;
    goto fail;
  }

 fail:
  return false;
}
//------------------------------------------------------------------------------
// free a cluster chain
bool FatPartition::freeChain(uint32_t cluster) {
  uint32_t next;
  int8_t fg;
  do {
    fg = fatGet(cluster, &next);
    if (fg < 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // free cluster
    if (!fatPut(cluster, 0)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // Add one to count of free clusters.
    updateFreeClusterCount(1);
    if (cluster < m_allocSearchStart) {
      m_allocSearchStart = cluster - 1;
    }
    cluster = next;
  } while (fg);

  return true;

 fail:
  return false;
}

// Structure to use for doing free cluster count using callbacks
struct FreeClusterCountStruct {
  uint32_t clusters_to_do;
  uint32_t free_count;
};

//------------------------------------------------------------------------------
void FatPartition::freeClusterCount_cb_fat16(uint32_t sector, uint8_t *buf, void *context) {
   struct FreeClusterCountStruct *state = (struct FreeClusterCountStruct *)context;
  uint16_t *p = (uint16_t *)buf;
  unsigned int n = state->clusters_to_do;
  if (n > 256) n = 256;
  uint16_t *e = p + n;
  while (p < e) {
    if (*p++ == 0) state->free_count++;
  }
  state->clusters_to_do -= n;
}

//------------------------------------------------------------------------------
void FatPartition::freeClusterCount_cb_fat32(uint32_t sector, uint8_t *buf, void *context) {
  struct FreeClusterCountStruct *state = (struct FreeClusterCountStruct *)context;
  uint32_t *p = (uint32_t *)buf;
  unsigned int n = state->clusters_to_do;
  if (n > 128) n = 128;
  uint32_t *e = p + n;
  while (p < e) {
    if (*p++ == 0) state->free_count++;
  }
  state->clusters_to_do -= n;
}

//------------------------------------------------------------------------------
int32_t FatPartition::freeClusterCount() {
#if MAINTAIN_FREE_CLUSTER_COUNT
  if (m_freeClusterCount >= 0) {
    return m_freeClusterCount;
  }
#endif  // MAINTAIN_FREE_CLUSTER_COUNT
 if (FAT12_SUPPORT && fatType() == 12) {
    uint32_t free = 0;
    uint32_t todo = m_lastCluster + 1;
    for (unsigned i = 2; i < todo; i++) {
      uint32_t c;
      int8_t fg = fatGet(i, &c);
      if (fg < 0) {
        DBG_FAIL_MACRO;
        return -1;
      }
      if (fg && c == 0) {
        free++;
      }
    }
    return free;
  }

  struct FreeClusterCountStruct state;

  state.free_count = 0;
  state.clusters_to_do = m_lastCluster + 1;

  uint32_t num_sectors;

  //num_sectors = SD.sdfs.m_fVol->sectorsPerFat(); // edit FsVolume.h for public
  //Serial.printf("  num_sectors = %u\n", num_sectors);

  num_sectors = m_sectorsPerFat;
  //Serial.printf("  num_sectors = %u\n", num_sectors);
#if USE_SEPARATE_FAT_CACHE
  uint8_t *buf = m_fatCache.clear();  // will clear out anything and return buffer 
#else  
  uint8_t *buf = m_cache.clear();  // will clear out anything and return buffer 
#endif  // USE_SEPARATE_FAT_CACHE
  if (buf == nullptr) return -1;
  if (fatType() == FAT_TYPE_FAT32) {
    if (!m_blockDev->readSectorsCallback(m_fatStartSector, buf, num_sectors, freeClusterCount_cb_fat32, &state)) return -1;
  } else {
    if (!m_blockDev->readSectorsCallback(m_fatStartSector, buf, num_sectors, freeClusterCount_cb_fat16, &state)) return -1;
  }

  setFreeClusterCount(state.free_count);
  return state.free_count;
}


//------------------------------------------------------------------------------
bool FatPartition::init(FsBlockDevice* dev, uint8_t part) {
//  Serial.printf(" FatPartition::init(%x %u)\n", (uint32_t)dev, part);
  uint32_t clusterCount;
  uint32_t totalSectors;
  uint32_t volumeStartSector = 0;
  m_blockDev = dev;
  pbs_t* pbs;
  BpbFat32_t* bpb;
  MbrSector_t* mbr;
  uint8_t tmp;
  m_fatType = 0;
  m_allocSearchStart = 1;
  m_cache.init(dev);
#if USE_SEPARATE_FAT_CACHE
  m_fatCache.init(dev);
#endif  // USE_SEPARATE_FAT_CACHE
  // if part == 0 assume super floppy with FAT boot sector in sector zero
  // if part > 0 assume mbr volume with partition table
  if (part) {
    if (part > 4) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    mbr = reinterpret_cast<MbrSector_t*>
          (dataCachePrepare(0, FsCache::CACHE_FOR_READ));
    MbrPart_t* mp = mbr->part + part - 1;

    if (!mbr || mp->type == 0 || (mp->boot != 0 && mp->boot != 0X80)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    volumeStartSector = getLe32(mp->relativeSectors);
  }
  pbs = reinterpret_cast<pbs_t*>
        (dataCachePrepare(volumeStartSector, FsCache::CACHE_FOR_READ));
  bpb = reinterpret_cast<BpbFat32_t*>(pbs->bpb);
  if (!pbs || getLe16(bpb->bytesPerSector) != m_bytesPerSector) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // handle fat counts 1 or 2...
  m_fatCount = bpb->fatCount;
  if ((m_fatCount != 1) && (m_fatCount != 2)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_sectorsPerCluster = bpb->sectorsPerCluster;
  m_clusterSectorMask = m_sectorsPerCluster - 1;
  // determine shift that is same as multiply by m_sectorsPerCluster
  m_sectorsPerClusterShift = 0;
  for (tmp = 1; m_sectorsPerCluster != tmp; tmp <<= 1) {
    if (tmp == 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    m_sectorsPerClusterShift++;
  }
  m_sectorsPerFat = getLe16(bpb->sectorsPerFat16);
  if (m_sectorsPerFat == 0) {
    m_sectorsPerFat = getLe32(bpb->sectorsPerFat32);
  }
  m_fatStartSector = volumeStartSector + getLe16(bpb->reservedSectorCount);

  // count for FAT16 zero for FAT32
  m_rootDirEntryCount = getLe16(bpb->rootDirEntryCount);

  // directory start for FAT16 dataStart for FAT32
  m_rootDirStart = m_fatStartSector + bpb->fatCount * m_sectorsPerFat;
  // data start for FAT16 and FAT32
  m_dataStartSector = m_rootDirStart +
    ((FS_DIR_SIZE*m_rootDirEntryCount + m_bytesPerSector - 1)/m_bytesPerSector);

  // total sectors for FAT16 or FAT32
  totalSectors = getLe16(bpb->totalSectors16);
  if (totalSectors == 0) {
    totalSectors = getLe32(bpb->totalSectors32);
  }
  // total data sectors
  clusterCount = totalSectors - (m_dataStartSector - volumeStartSector);

  // divide by cluster size to get cluster count
  clusterCount >>= m_sectorsPerClusterShift;
  m_lastCluster = clusterCount + 1;

  // Indicate unknown number of free clusters.
  setFreeClusterCount(-1);
  // FAT type is determined by cluster count
  if (clusterCount < 4085) {
    m_fatType = 12;
    if (!FAT12_SUPPORT) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else if (clusterCount < 65525) {
    m_fatType = 16;
  } else {
    m_rootDirStart = getLe32(bpb->fat32RootCluster);
    m_fatType = 32;
  }
  m_cache.setMirrorOffset(m_sectorsPerFat);
#if USE_SEPARATE_FAT_CACHE
  m_fatCache.setMirrorOffset(m_sectorsPerFat);
#endif  // USE_SEPARATE_FAT_CACHE
  return true;

 fail:
  return false;
}

//------------------------------------------------------------------------------
bool FatPartition::init(FsBlockDevice* dev, uint32_t firstSector, uint32_t numSectors) {
//  Serial.printf(" FatPartition::init(%x %u %u)\n", (uint32_t)dev, firstSector, numSectors);
  uint32_t clusterCount;
  uint32_t totalSectors;
  uint32_t volumeStartSector = firstSector;
  m_blockDev = dev;
  pbs_t* pbs;
  BpbFat32_t* bpb;
  uint8_t tmp;

  m_fatType = 0;
  m_allocSearchStart = 1;
  m_cache.init(dev);
#if USE_SEPARATE_FAT_CACHE
  m_fatCache.init(dev);
#endif  // USE_SEPARATE_FAT_CACHE
  pbs = reinterpret_cast<pbs_t*>
        (dataCachePrepare(volumeStartSector, FsCache::CACHE_FOR_READ));
  bpb = reinterpret_cast<BpbFat32_t*>(pbs->bpb);
  if (!pbs || getLe16(bpb->bytesPerSector) != m_bytesPerSector) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_fatCount = bpb->fatCount;
  // handle fat counts 1 or 2...
  if ((m_fatCount != 1) && (m_fatCount != 2)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  m_sectorsPerCluster = bpb->sectorsPerCluster;
  m_clusterSectorMask = m_sectorsPerCluster - 1;
  // determine shift that is same as multiply by m_sectorsPerCluster
  m_sectorsPerClusterShift = 0;
  for (tmp = 1; m_sectorsPerCluster != tmp; tmp <<= 1) {
    if (tmp == 0) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    m_sectorsPerClusterShift++;
  }
  m_sectorsPerFat = getLe16(bpb->sectorsPerFat16);
  if (m_sectorsPerFat == 0) {
    m_sectorsPerFat = getLe32(bpb->sectorsPerFat32);
  }
  m_fatStartSector = volumeStartSector + getLe16(bpb->reservedSectorCount);

  // count for FAT16 zero for FAT32
  m_rootDirEntryCount = getLe16(bpb->rootDirEntryCount);

  // directory start for FAT16 dataStart for FAT32
  m_rootDirStart = m_fatStartSector + bpb->fatCount * m_sectorsPerFat;
  // data start for FAT16 and FAT32
  m_dataStartSector = m_rootDirStart +
    ((FS_DIR_SIZE*m_rootDirEntryCount + m_bytesPerSector - 1)/m_bytesPerSector);

  // total sectors for FAT16 or FAT32
  totalSectors = getLe16(bpb->totalSectors16);
  if (totalSectors == 0) {
    totalSectors = getLe32(bpb->totalSectors32);
  }
  if (totalSectors > numSectors) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // total data sectors
  clusterCount = totalSectors - (m_dataStartSector - volumeStartSector);

  // divide by cluster size to get cluster count
  clusterCount >>= m_sectorsPerClusterShift;
  m_lastCluster = clusterCount + 1;

  // Indicate unknown number of free clusters.
  setFreeClusterCount(-1);
  // FAT type is determined by cluster count
  if (clusterCount < 4085) {
    m_fatType = 12;
    if (!FAT12_SUPPORT) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else if (clusterCount < 65525) {
    m_fatType = 16;
  } else {
    m_rootDirStart = getLe32(bpb->fat32RootCluster);
    m_fatType = 32;
  }
  m_cache.setMirrorOffset(m_sectorsPerFat);
#if USE_SEPARATE_FAT_CACHE
  m_fatCache.setMirrorOffset(m_sectorsPerFat);
#endif  // USE_SEPARATE_FAT_CACHE
  return true;

 fail:
  return false;
}
