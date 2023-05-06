// MTP.h - Teensy MTP Responder library
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

#ifndef MTP_H
#define MTP_H

#if !defined(USB_MTPDISK) && !defined(USB_MTPDISK_SERIAL)
  #error "You need to select USB Type: 'MTP Disk (Experimental)'"
#endif

#include "core_pins.h"
#include "usb_dev.h"
extern "C" 	int usb_mtp_sendEvent(const void *buffer, uint32_t len, uint32_t timeout);

#include "Storage.h"
// modify strings if needed (see MTP.cpp how they are used)
#define MTP_MANUF "PJRC"
#define MTP_MODEL "Teensy"
#define MTP_VERS  "1.0"
#define MTP_SERNR "1234"
#define MTP_NAME  "Teensy"

#define USE_EVENTS 1

// MTP Responder.
class MTPD {
public:

  explicit MTPD(MTPStorageInterface* storage): storage_(storage) {}

private:
  MTPStorageInterface* storage_;

  struct MTPHeader {
    uint32_t len;  // 0
    uint16_t type; // 4
    uint16_t op;   // 6
    uint32_t transaction_id; // 8
  };

  struct MTPContainer {
    uint32_t len;  // 0
    uint16_t type; // 4
    uint16_t op;   // 6
    uint32_t transaction_id; // 8
    uint32_t params[5];    // 12
  } __attribute__((__may_alias__)) ;

#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
  usb_packet_t *data_buffer_ = NULL;
  void get_buffer() ;
  void receive_buffer() ;
//  inline MTPContainer *contains (usb_packet_t *receive_buffer) { return (MTPContainer*)(receive_buffer->buf);  }
// possible events for T3.xx ?

#elif defined(__IMXRT1062__)
  #define MTP_RX_SIZE MTP_RX_SIZE_480 
  #define MTP_TX_SIZE MTP_TX_SIZE_480 
  
  uint8_t rx_data_buffer[MTP_RX_SIZE] __attribute__ ((aligned(32)));
  uint8_t tx_data_buffer[MTP_TX_SIZE] __attribute__ ((aligned(32)));

  #define DISK_BUFFER_SIZE 8*1024
  uint8_t disk_buffer[DISK_BUFFER_SIZE] __attribute__ ((aligned(32)));
  uint32_t disk_pos=0;

  int push_packet(uint8_t *data_buffer, uint32_t len);
  int fetch_packet(uint8_t *data_buffer);
  int pull_packet(uint8_t *data_buffer);

#endif

  bool write_get_length_ = false;
  uint32_t write_length_ = 0;
  void write(const char *data, int len) ;

  void write8 (uint8_t  x) ;
  void write16(uint16_t x) ;
  void write32(uint32_t x) ;
  void write64(uint64_t x) ;

  void writestring(const char* str) ;

  void WriteDescriptor() ;
  void WriteStorageIDs() ;

  void GetStorageInfo(uint32_t storage) ;

  uint32_t GetNumObjects(uint32_t storage, uint32_t parent) ;

  void GetObjectHandles(uint32_t storage, uint32_t parent) ;
  
  void GetObjectInfo(uint32_t handle) ;
  void GetObject(uint32_t object_id) ;
  uint32_t GetPartialObject(uint32_t object_id, uint32_t offset, uint32_t NumBytes) ;

  void read(char* data, uint32_t size) ;

  uint32_t ReadMTPHeader() ;

  uint8_t read8() ;
  uint16_t read16() ;
  uint32_t read32() ;
  void readstring(char* buffer) ;

//  void read_until_short_packet() ;

  uint32_t SendObjectInfo(uint32_t storage, uint32_t parent) ;
  bool SendObject() ;

  void GetDevicePropValue(uint32_t prop) ;
  void GetDevicePropDesc(uint32_t prop) ;
  void getObjectPropsSupported(uint32_t p1) ;

  void getObjectPropDesc(uint32_t p1, uint32_t p2) ;
  void getObjectPropValue(uint32_t p1, uint32_t p2) ;

  uint32_t setObjectPropValue(uint32_t p1, uint32_t p2) ;

  uint32_t deleteObject(uint32_t p1) ;
  uint32_t copyObject(uint32_t p1,uint32_t p2, uint32_t p3) ;
  uint32_t moveObject(uint32_t p1,uint32_t p2, uint32_t p3) ;
  void openSession(uint32_t id) ;

  uint32_t TID;  
#if USE_EVENTS==1
  int send_Event(uint16_t eventCode);
  int send_Event(uint16_t eventCode, uint32_t p1);
  int send_Event(uint16_t eventCode, uint32_t p1, uint32_t p2);
  int send_Event(uint16_t eventCode, uint32_t p1, uint32_t p2, uint32_t p3);
#endif

public:
  void loop(void) ;
  void test(void) ;

#if USE_EVENTS==1
  int send_addObjectEvent(uint32_t p1);
  int send_removeObjectEvent(uint32_t p1);
  int send_StorageInfoChangedEvent(uint32_t p1);
  int send_StorageRemovedEvent(uint32_t p1);
  int send_DeviceResetEvent(void);
#endif
};

#endif
