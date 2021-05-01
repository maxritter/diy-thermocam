// MTP.cpp - Teensy MTP Responder library
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

#if defined(USB_MTPDISK) || defined(USB_MTPDISK_SERIAL)

#include "MTP.h"

#undef USB_DESC_LIST_DEFINE
#include "usb_desc.h"

#if defined(__IMXRT1062__)
  // following only while usb_mtp is not included in cores
  #if __has_include("usb_mtp.h")
    #include "usb_mtp.h"
  #else
    #include "usb1_mtp.h"
  #endif
#endif

#include "usb_names.h"
extern struct usb_string_descriptor_struct usb_string_serial_number; 

#define DEBUG 1
#if DEBUG>0
  #define printf(...) Serial.printf(__VA_ARGS__)
#else
  #define printf(...) 
#endif

/***************************************************************************************************/
  // Container Types
  #define MTP_CONTAINER_TYPE_UNDEFINED    0
  #define MTP_CONTAINER_TYPE_COMMAND      1
  #define MTP_CONTAINER_TYPE_DATA         2
  #define MTP_CONTAINER_TYPE_RESPONSE     3
  #define MTP_CONTAINER_TYPE_EVENT        4

  // Container Offsets
  #define MTP_CONTAINER_LENGTH_OFFSET             0
  #define MTP_CONTAINER_TYPE_OFFSET               4
  #define MTP_CONTAINER_CODE_OFFSET               6
  #define MTP_CONTAINER_TRANSACTION_ID_OFFSET     8
  #define MTP_CONTAINER_PARAMETER_OFFSET          12
  #define MTP_CONTAINER_HEADER_SIZE               12

  // MTP Operation Codes
  #define MTP_OPERATION_GET_DEVICE_INFO                       0x1001
  #define MTP_OPERATION_OPEN_SESSION                          0x1002
  #define MTP_OPERATION_CLOSE_SESSION                         0x1003
  #define MTP_OPERATION_GET_STORAGE_IDS                       0x1004
  #define MTP_OPERATION_GET_STORAGE_INFO                      0x1005
  #define MTP_OPERATION_GET_NUM_OBJECTS                       0x1006
  #define MTP_OPERATION_GET_OBJECT_HANDLES                    0x1007
  #define MTP_OPERATION_GET_OBJECT_INFO                       0x1008
  #define MTP_OPERATION_GET_OBJECT                            0x1009
  #define MTP_OPERATION_GET_THUMB                             0x100A
  #define MTP_OPERATION_DELETE_OBJECT                         0x100B
  #define MTP_OPERATION_SEND_OBJECT_INFO                      0x100C
  #define MTP_OPERATION_SEND_OBJECT                           0x100D
  #define MTP_OPERATION_INITIATE_CAPTURE                      0x100E
  #define MTP_OPERATION_FORMAT_STORE                          0x100F
  #define MTP_OPERATION_RESET_DEVICE                          0x1010
  #define MTP_OPERATION_SELF_TEST                             0x1011
  #define MTP_OPERATION_SET_OBJECT_PROTECTION                 0x1012
  #define MTP_OPERATION_POWER_DOWN                            0x1013
  #define MTP_OPERATION_GET_DEVICE_PROP_DESC                  0x1014
  #define MTP_OPERATION_GET_DEVICE_PROP_VALUE                 0x1015
  #define MTP_OPERATION_SET_DEVICE_PROP_VALUE                 0x1016
  #define MTP_OPERATION_RESET_DEVICE_PROP_VALUE               0x1017
  #define MTP_OPERATION_TERMINATE_OPEN_CAPTURE                0x1018
  #define MTP_OPERATION_MOVE_OBJECT                           0x1019
  #define MTP_OPERATION_COPY_OBJECT                           0x101A
  #define MTP_OPERATION_GET_PARTIAL_OBJECT                    0x101B
  #define MTP_OPERATION_INITIATE_OPEN_CAPTURE                 0x101C
  #define MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED            0x9801
  #define MTP_OPERATION_GET_OBJECT_PROP_DESC                  0x9802
  #define MTP_OPERATION_GET_OBJECT_PROP_VALUE                 0x9803
  #define MTP_OPERATION_SET_OBJECT_PROP_VALUE                 0x9804
  #define MTP_OPERATION_GET_OBJECT_PROP_LIST                  0x9805
  #define MTP_OPERATION_SET_OBJECT_PROP_LIST                  0x9806
  #define MTP_OPERATION_GET_INTERDEPENDENT_PROP_DESC          0x9807
  #define MTP_OPERATION_SEND_OBJECT_PROP_LIST                 0x9808
  #define MTP_OPERATION_GET_OBJECT_REFERENCES                 0x9810
  #define MTP_OPERATION_SET_OBJECT_REFERENCES                 0x9811
  #define MTP_OPERATION_SKIP                                  0x9820


  const unsigned short supported_op[]=
  {
    MTP_OPERATION_GET_DEVICE_INFO                        ,//0x1001
    MTP_OPERATION_OPEN_SESSION                           ,//0x1002
    MTP_OPERATION_CLOSE_SESSION                          ,//0x1003
    MTP_OPERATION_GET_STORAGE_IDS                        ,//0x1004
    MTP_OPERATION_GET_STORAGE_INFO                       ,//0x1005
    //MTP_OPERATION_GET_NUM_OBJECTS                        ,//0x1006
    MTP_OPERATION_GET_OBJECT_HANDLES                     ,//0x1007
    MTP_OPERATION_GET_OBJECT_INFO                        ,//0x1008
    MTP_OPERATION_GET_OBJECT                             ,//0x1009
    //MTP_OPERATION_GET_THUMB                              ,//0x100A
    MTP_OPERATION_DELETE_OBJECT                          ,//0x100B
    MTP_OPERATION_SEND_OBJECT_INFO                       ,//0x100C
    MTP_OPERATION_SEND_OBJECT                            ,//0x100D
    MTP_OPERATION_GET_DEVICE_PROP_DESC                   ,//0x1014
    MTP_OPERATION_GET_DEVICE_PROP_VALUE                  ,//0x1015
    //MTP_OPERATION_SET_DEVICE_PROP_VALUE                  ,//0x1016
    //MTP_OPERATION_RESET_DEVICE_PROP_VALUE                ,//0x1017
    MTP_OPERATION_MOVE_OBJECT                           ,//0x1019
    MTP_OPERATION_COPY_OBJECT                           ,//0x101A
    MTP_OPERATION_GET_PARTIAL_OBJECT                     ,//0x101B

    MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED             ,//0x9801
    MTP_OPERATION_GET_OBJECT_PROP_DESC                   ,//0x9802
    MTP_OPERATION_GET_OBJECT_PROP_VALUE                  ,//0x9803
    MTP_OPERATION_SET_OBJECT_PROP_VALUE                  //0x9804
    //MTP_OPERATION_GET_OBJECT_PROP_LIST                   ,//0x9805
    //MTP_OPERATION_GET_OBJECT_REFERENCES                  ,//0x9810
    //MTP_OPERATION_SET_OBJECT_REFERENCES                  ,//0x9811

    //MTP_OPERATION_GET_PARTIAL_OBJECT_64                  ,//0x95C1
    //MTP_OPERATION_SEND_PARTIAL_OBJECT                    ,//0x95C2
    //MTP_OPERATION_TRUNCATE_OBJECT                        ,//0x95C3
    //MTP_OPERATION_BEGIN_EDIT_OBJECT                      ,//0x95C4
    //MTP_OPERATION_END_EDIT_OBJECT                         //0x95C5
  };
  
  const int supported_op_size=sizeof(supported_op);
  const int supported_op_num = supported_op_size/sizeof(supported_op[0]);


  #define MTP_PROPERTY_STORAGE_ID                             0xDC01
  #define MTP_PROPERTY_OBJECT_FORMAT                          0xDC02
  #define MTP_PROPERTY_PROTECTION_STATUS                      0xDC03
  #define MTP_PROPERTY_OBJECT_SIZE                            0xDC04
  #define MTP_PROPERTY_OBJECT_FILE_NAME                       0xDC07
  #define MTP_PROPERTY_DATE_CREATED                           0xDC08
  #define MTP_PROPERTY_DATE_MODIFIED                          0xDC09
  #define MTP_PROPERTY_PARENT_OBJECT                          0xDC0B
  #define MTP_PROPERTY_PERSISTENT_UID                         0xDC41
  #define MTP_PROPERTY_NAME                                   0xDC44

  const uint16_t propertyList[] =
  {
    MTP_PROPERTY_STORAGE_ID                             ,//0xDC01
    MTP_PROPERTY_OBJECT_FORMAT                          ,//0xDC02
    MTP_PROPERTY_PROTECTION_STATUS                      ,//0xDC03
    MTP_PROPERTY_OBJECT_SIZE                            ,//0xDC04
    MTP_PROPERTY_OBJECT_FILE_NAME                       ,//0xDC07
//    MTP_PROPERTY_DATE_CREATED                           ,//0xDC08
//    MTP_PROPERTY_DATE_MODIFIED                          ,//0xDC09
    MTP_PROPERTY_PARENT_OBJECT                          ,//0xDC0B
    MTP_PROPERTY_PERSISTENT_UID                         ,//0xDC41
    MTP_PROPERTY_NAME                                    //0xDC44
  };
  
  uint32_t propertyListNum = sizeof(propertyList)/sizeof(propertyList[0]);
  

    #define MTP_EVENT_UNDEFINED                         0x4000
    #define MTP_EVENT_CANCEL_TRANSACTION                0x4001
    #define MTP_EVENT_OBJECT_ADDED                      0x4002
    #define MTP_EVENT_OBJECT_REMOVED                    0x4003
    #define MTP_EVENT_STORE_ADDED                       0x4004
    #define MTP_EVENT_STORE_REMOVED                     0x4005
    #define MTP_EVENT_DEVICE_PROP_CHANGED               0x4006
    #define MTP_EVENT_OBJECT_INFO_CHANGED               0x4007
    #define MTP_EVENT_DEVICE_INFO_CHANGED               0x4008
    #define MTP_EVENT_REQUEST_OBJECT_TRANSFER           0x4009
    #define MTP_EVENT_STORE_FULL                        0x400A
    #define MTP_EVENT_DEVICE_RESET                      0x400B
    #define MTP_EVENT_STORAGE_INFO_CHANGED              0x400C
    #define MTP_EVENT_CAPTURE_COMPLETE                  0x400D
    #define MTP_EVENT_UNREPORTED_STATUS                 0x400E
    #define MTP_EVENT_OBJECT_PROP_CHANGED               0xC801
    #define MTP_EVENT_OBJECT_PROP_DESC_CHANGED          0xC802  
    #define MTP_EVENT_OBJECT_REFERENCES_CHANGED         0xC803

const uint16_t supported_events[] =
  {
//    MTP_EVENT_UNDEFINED                         ,//0x4000
//    MTP_EVENT_CANCEL_TRANSACTION                ,//0x4001
//    MTP_EVENT_OBJECT_ADDED                      ,//0x4002
//    MTP_EVENT_OBJECT_REMOVED                    ,//0x4003
    MTP_EVENT_STORE_ADDED                       ,//0x4004
    MTP_EVENT_STORE_REMOVED                     ,//0x4005
//    MTP_EVENT_DEVICE_PROP_CHANGED               ,//0x4006
//    MTP_EVENT_OBJECT_INFO_CHANGED               ,//0x4007
//    MTP_EVENT_DEVICE_INFO_CHANGED               ,//0x4008
//    MTP_EVENT_REQUEST_OBJECT_TRANSFER           ,//0x4009
//    MTP_EVENT_STORE_FULL                        ,//0x400A
    MTP_EVENT_DEVICE_RESET                      ,//0x400B
    MTP_EVENT_STORAGE_INFO_CHANGED              ,//0x400C
//    MTP_EVENT_CAPTURE_COMPLETE                  ,//0x400D
//    MTP_EVENT_UNREPORTED_STATUS                 ,//0x400E
//    MTP_EVENT_OBJECT_PROP_CHANGED               ,//0xC801
//    MTP_EVENT_OBJECT_PROP_DESC_CHANGED          ,//0xC802  
//    MTP_EVENT_OBJECT_REFERENCES_CHANGED          //0xC803
  };
  
  const int supported_event_num = sizeof(supported_events)/sizeof(supported_events[0]);

 uint32_t sessionID_;

// MTP Responder.
/*
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
  };
*/

  void MTPD::write8 (uint8_t  x) { write((char*)&x, sizeof(x)); }
  void MTPD::write16(uint16_t x) { write((char*)&x, sizeof(x)); }
  void MTPD::write32(uint32_t x) { write((char*)&x, sizeof(x)); }
  void MTPD::write64(uint64_t x) { write((char*)&x, sizeof(x)); }

#define Store2Storage(x) (x+1)
#define Storage2Store(x) (x-1)

  void MTPD::writestring(const char* str) {
    if (*str) 
    { write8(strlen(str) + 1);
      while (*str) {  write16(*str);  ++str;  } write16(0);
    } else 
    { write8(0);
    }
  }

  void MTPD::WriteDescriptor() {
    write16(100);  // MTP version
    write32(6);    // MTP extension
//    write32(0xFFFFFFFFUL);    // MTP extension
    write16(100);  // MTP version
    writestring("microsoft.com: 1.0;");
    write16(0);    // functional mode

    // Supported operations (array of uint16)
    write32(supported_op_num);
    for(int ii=0; ii<supported_op_num;ii++) write16(supported_op[ii]);
    
    // Events (array of uint16)
    write32(supported_event_num);      
    for(int ii=0; ii<supported_event_num;ii++) write16(supported_events[ii]);

    write32(1);       // Device properties (array of uint16)
    write16(0xd402);  // Device friendly name

    write32(0);       // Capture formats (array of uint16)

    write32(2);       // Playback formats (array of uint16)
    write16(0x3000);  // Undefined format
    write16(0x3001);  // Folders (associations)

    writestring(MTP_MANUF);     // Manufacturer
    writestring(MTP_MODEL);     // Model
    //writestring(MTP_VERS);      // version
    //writestring(MTP_SERNR);     // serial
    
    char buf[20];    
    dtostrf( (float)(TEENSYDUINO / 100.0f), 3, 2, buf);
    strlcat(buf, " / MTP " MTP_VERS, sizeof(buf) );
    writestring( buf );    
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Warray-bounds"
    for (size_t i=0; i<10; i++) buf[i] = usb_string_serial_number.wString[i];
    #pragma GCC diagnostic pop
    writestring(buf);    
  }

  void MTPD::WriteStorageIDs() {
    uint32_t num=storage_->get_FSCount();
    write32(num); // number of storages (disks)
    for(uint32_t ii=0;ii<num;ii++)  write32(Store2Storage(ii)); // storage id
  }

  void MTPD::GetStorageInfo(uint32_t storage) {
    uint32_t store = Storage2Store(storage);
    write16(storage_->readonly(store) ? 0x0001 : 0x0004);   // storage type (removable RAM)
    write16(storage_->has_directories(store) ? 0x0002: 0x0001);   // filesystem type (generic hierarchical)
    write16(0x0000);   // access capability (read-write)

    uint64_t ntotal = storage_->totalSize(store) ; 
    uint64_t nused = storage_->usedSize(store) ; 

    write64(ntotal);  // max capacity
    write64((ntotal-nused));  // free space (100M)
    //
    write32(0xFFFFFFFFUL);  // free space (objects)
    const char *name = storage_->get_FSName(store);
    writestring(name);  // storage descriptor
    writestring("");  // volume identifier

    //printf("%d %d ",storage,store); Serial.println(name); Serial.flush();
  }

  uint32_t MTPD::GetNumObjects(uint32_t storage, uint32_t parent) 
  { uint32_t store = Storage2Store(storage);
    storage_->StartGetObjectHandles(store, parent);
    int num = 0;
    while (storage_->GetNextObjectHandle(store)) num++;
    return num;
  }

  void MTPD::GetObjectHandles(uint32_t storage, uint32_t parent) 
  { uint32_t store = Storage2Store(storage);
    if (write_get_length_) {
      write_length_ = GetNumObjects(storage, parent);
      write_length_++;
      write_length_ *= 4;
    }
    else{
      write32(GetNumObjects(storage, parent));
      int handle;
      storage_->StartGetObjectHandles(store, parent);
      while ((handle = storage_->GetNextObjectHandle(store))) write32(handle);
    }
  }
  
  void MTPD::GetObjectInfo(uint32_t handle) 
  {
    char filename[MAX_FILENAME_LEN];
    uint32_t size, parent;
    uint16_t store;
    storage_->GetObjectInfo(handle, filename, &size, &parent, &store);

    uint32_t storage = Store2Storage(store);
    write32(storage); // storage
    write16(size == 0xFFFFFFFFUL ? 0x3001 : 0x0000); // format
    write16(0);  // protection
    write32(size); // size
    write16(0); // thumb format
    write32(0); // thumb size
    write32(0); // thumb width
    write32(0); // thumb height
    write32(0); // pix width
    write32(0); // pix height
    write32(0); // bit depth
    write32(parent); // parent
    write16(size == 0xFFFFFFFFUL ? 1 : 0); // association type
    write32(0); // association description
    write32(0);  // sequence number
    writestring(filename);
    writestring("");  // date created
    writestring("");  // date modified
    writestring("");  // keywords
  }

  uint32_t MTPD::ReadMTPHeader() 
  {
    MTPHeader header;
    read((char *)&header, sizeof(MTPHeader));
    // check that the type is data
    if(header.type==2)
      return header.len - 12;
    else
      return 0;
  }

  uint8_t MTPD::read8() { uint8_t ret; read((char*)&ret, sizeof(ret));  return ret;  }
  uint16_t MTPD::read16() { uint16_t ret; read((char*)&ret, sizeof(ret)); return ret; }
  uint32_t MTPD::read32() { uint32_t ret; read((char*)&ret, sizeof(ret)); return ret; }

  void MTPD::readstring(char* buffer) {
    int len = read8();
    if (!buffer) {
      read(NULL, len * 2);
    } else {
      for (int i = 0; i < len; i++) {
        int16_t c2;
        *(buffer++) = c2 = read16();
      }
    }
  }

  void MTPD::GetDevicePropValue(uint32_t prop) {
    switch (prop) {
      case 0xd402: // friendly name
        // This is the name we'll actually see in the windows explorer.
        // Should probably be configurable.
        writestring(MTP_NAME);
        break;
    }
  }

  void MTPD::GetDevicePropDesc(uint32_t prop) {
    switch (prop) {
      case 0xd402: // friendly name
        write16(prop);
        write16(0xFFFF); // string type
        write8(0);       // read-only
        GetDevicePropValue(prop);
        GetDevicePropValue(prop);
        write8(0);       // no form
    }
  }

    void MTPD::getObjectPropsSupported(uint32_t p1)
    {
      write32(propertyListNum);
      for(uint32_t ii=0; ii<propertyListNum;ii++) write16(propertyList[ii]);
    }

    void MTPD::getObjectPropDesc(uint32_t p1, uint32_t p2)
    {
      switch(p1)
      {
        case MTP_PROPERTY_STORAGE_ID:         //0xDC01:
          write16(0xDC01);
          write16(0x006);
          write8(0); //get
          write32(0);
          write32(0);
          write8(0);
          break;
        case MTP_PROPERTY_OBJECT_FORMAT:        //0xDC02:
          write16(0xDC02);
          write16(0x004);
          write8(0); //get
          write16(0);
          write32(0);
          write8(0);
          break;
        case MTP_PROPERTY_PROTECTION_STATUS:    //0xDC03:
          write16(0xDC03);
          write16(0x004);
          write8(0); //get
          write16(0);
          write32(0);
          write8(0);
          break;
        case MTP_PROPERTY_OBJECT_SIZE:        //0xDC04:
          write16(0xDC04);
          write16(0x008);
          write8(0); //get
          write64(0);
          write32(0);
          write8(0);
          break;
        case MTP_PROPERTY_OBJECT_FILE_NAME:   //0xDC07:
          write16(0xDC07);
          write16(0xFFFF);
          write8(1); //get/set
          write8(0);
          write32(0);
          write8(0);
          break;
        case MTP_PROPERTY_DATE_CREATED:       //0xDC08:
          write16(0xDC08);
          write16(0xFFFF);
          write8(0); //get
          write8(0);
          write32(0);
          write8(0);
          break;
        case MTP_PROPERTY_DATE_MODIFIED:      //0xDC09:
          write16(0xDC09);
          write16(0xFFFF);
          write8(0); //get
          write8(0);
          write32(0);
          write8(0);
          break;
        case MTP_PROPERTY_PARENT_OBJECT:    //0xDC0B:
          write16(0xDC0B);
          write16(6);
          write8(0); //get
          write32(0);
          write32(0);
          write8(0);
          break;
        case MTP_PROPERTY_PERSISTENT_UID:   //0xDC41:
          write16(0xDC41);
          write16(0x0A);
          write8(0); //get
          write64(0);
          write64(0);
          write32(0);
          write8(0);
          break;
        case MTP_PROPERTY_NAME:             //0xDC44:
          write16(0xDC44);
          write16(0xFFFF);
          write8(0); //get
          write8(0);
          write32(0);
          write8(0);
          break;
        default:
          break;
      }
    }

    void MTPD::getObjectPropValue(uint32_t p1, uint32_t p2)
    { char name[MAX_FILENAME_LEN];
      uint32_t dir;
      uint32_t size;
      uint32_t parent;
      uint16_t store;
      storage_->GetObjectInfo(p1,name,&size,&parent, &store);
      dir = size == 0xFFFFFFFFUL;
      uint32_t storage = Store2Storage(store);
      switch(p2)
      {
        case MTP_PROPERTY_STORAGE_ID:         //0xDC01:
          write32(storage);
          break;
        case MTP_PROPERTY_OBJECT_FORMAT:      //0xDC02:
          write16(dir?0x3001:0x3000);
          break;
        case MTP_PROPERTY_PROTECTION_STATUS:  //0xDC03:
          write16(0);
          break;
        case MTP_PROPERTY_OBJECT_SIZE:        //0xDC04:
          write32(size);
          write32(0);
          break;
        case MTP_PROPERTY_OBJECT_FILE_NAME:   //0xDC07:
          writestring(name);
          break;
        case MTP_PROPERTY_DATE_CREATED:       //0xDC08:
          writestring("");
          break;
        case MTP_PROPERTY_DATE_MODIFIED:      //0xDC09:
          writestring("");
          break;
        case MTP_PROPERTY_PARENT_OBJECT:      //0xDC0B:
          write32((store==parent)? 0: parent);
          break;
        case MTP_PROPERTY_PERSISTENT_UID:     //0xDC41:
          write32(p1);
          write32(parent);
          write32(storage);
          write32(0);
          break;
        case MTP_PROPERTY_NAME:               //0xDC44:
          writestring(name);
          break;
        default:
          break;
      }
    }
    
    uint32_t MTPD::deleteObject(uint32_t handle)
    {
        if (!storage_->DeleteObject(handle))
        {
          return 0x2012; // partial deletion
        }
        return 0x2001;
    }

    uint32_t MTPD::moveObject(uint32_t handle, uint32_t newStorage, uint32_t newHandle)
    { uint32_t store1=Storage2Store(newStorage);
      if(storage_->move(handle,store1,newHandle)) return 0x2001; else return  0x2005;
    }
    
    uint32_t MTPD::copyObject(uint32_t handle, uint32_t newStorage, uint32_t newHandle)
    { uint32_t store1=Storage2Store(newStorage);
      return storage_->copy(handle,store1,newHandle);
    }
    
    void MTPD::openSession(uint32_t id)
    {
      sessionID_ = id;
      storage_->ResetIndex();
    }


#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)

//  usb_packet_t *data_buffer_ = NULL;
  void MTPD::get_buffer() {
    while (!data_buffer_) {
      data_buffer_ = usb_malloc();
      if (!data_buffer_) mtp_yield();
    }
  }

  void MTPD::receive_buffer() {
    while (!data_buffer_) {
      data_buffer_ = usb_rx(MTP_RX_ENDPOINT);
      if (!data_buffer_) mtp_yield();
    }
  }

  void MTPD::write(const char *data, int len) {
    if (write_get_length_) {
      write_length_ += len;
    } else {
      int pos = 0;
      while (pos < len) {
        get_buffer();
        int avail = sizeof(data_buffer_->buf) - data_buffer_->len;
        int to_copy = min(len - pos, avail);
        memcpy(data_buffer_->buf + data_buffer_->len,
               data + pos,
               to_copy);
        data_buffer_->len += to_copy;
        pos += to_copy;
        if (data_buffer_->len == sizeof(data_buffer_->buf)) {
          usb_tx(MTP_TX_ENDPOINT, data_buffer_);
          data_buffer_ = NULL;
        }
      }
    }
  }
  void MTPD::GetObject(uint32_t object_id) 
  {
    uint32_t size = storage_->GetSize(object_id);
    if (write_get_length_) {
      write_length_ += size;
    } else {
      uint32_t pos = 0;
      while (pos < size) {
        get_buffer();
        uint32_t avail = sizeof(data_buffer_->buf) - data_buffer_->len;
        uint32_t to_copy = min(size - pos, avail);
        // Read directly from storage into usb buffer.
        storage_->read(object_id, pos,
                    (char*)(data_buffer_->buf + data_buffer_->len), to_copy);
        pos += to_copy;
        data_buffer_->len += to_copy;
        if (data_buffer_->len == sizeof(data_buffer_->buf)) {
          usb_tx(MTP_TX_ENDPOINT, data_buffer_);
          data_buffer_ = NULL;
        }
      }
    }
  }

  #define CONTAINER ((struct MTPContainer*)(receive_buffer->buf))

  #define TRANSMIT(FUN) do {                              \
      write_length_ = 0;                                  \
      write_get_length_ = true;                           \
      FUN;                                                \
      write_get_length_ = false;                          \
      MTPHeader header;                                   \
      header.len = write_length_ + 12;                    \
      header.type = 2;                                    \
      header.op = CONTAINER->op;                          \
      header.transaction_id = CONTAINER->transaction_id;  \
      write((char *)&header, sizeof(header));             \
      FUN;                                                \
      get_buffer();                                       \
      usb_tx(MTP_TX_ENDPOINT, data_buffer_);              \
      data_buffer_ = NULL;                                \
    } while(0)

    #define printContainer() \
    { printf("%x %d %d %d: ", CONTAINER->op, CONTAINER->len, CONTAINER->type, CONTAINER->transaction_id); \
      if(CONTAINER->len>12) printf(" %x", CONTAINER->params[0]); \
      if(CONTAINER->len>16) printf(" %x", CONTAINER->params[1]); \
      if(CONTAINER->len>20) printf(" %x", CONTAINER->params[2]); \
      printf("\n"); \
    }

  void MTPD::read(char* data, uint32_t size) 
  {
    while (size) {
      receive_buffer();
      uint32_t to_copy = data_buffer_->len - data_buffer_->index;
      to_copy = min(to_copy, size);
      if (data) {
        memcpy(data, data_buffer_->buf + data_buffer_->index, to_copy);
        data += to_copy;
      }
      size -= to_copy;
      data_buffer_->index += to_copy;
      if (data_buffer_->index == data_buffer_->len) {
        usb_free(data_buffer_);
        data_buffer_ = NULL;
      }
    }
  }

  uint32_t MTPD::SendObjectInfo(uint32_t storage, uint32_t parent) {
    uint32_t len = ReadMTPHeader();
    char filename[MAX_FILENAME_LEN];

    uint32_t store = Storage2Store(storage);

    read32(); len-=4; // storage
    bool dir = read16() == 0x3001; len-=2; // format
    read16();  len-=2; // protection
    read32(); len-=4; // size
    read16(); len-=2; // thumb format
    read32(); len-=4; // thumb size
    read32(); len-=4; // thumb width
    read32(); len-=4; // thumb height
    read32(); len-=4; // pix width
    read32(); len-=4; // pix height
    read32(); len-=4; // bit depth
    read32(); len-=4; // parent
    read16(); len-=2; // association type
    read32(); len-=4; // association description
    read32(); len-=4; // sequence number

    readstring(filename); len -= (2*(strlen(filename)+1)+1); 
    // ignore rest of ObjectInfo
    while(len>=4) { read32(); len-=4;}
    while(len) {read8(); len--;}
    
    return storage_->Create(store, parent, dir, filename);
  }

  bool MTPD::SendObject() {
    uint32_t len = ReadMTPHeader();
    while (len) 
    { 
      receive_buffer();
      uint32_t to_copy = data_buffer_->len - data_buffer_->index;
      to_copy = min(to_copy, len);
      if(!storage_->write((char*)(data_buffer_->buf + data_buffer_->index), to_copy)) return false;
      data_buffer_->index += to_copy;
      len -= to_copy;
      if (data_buffer_->index == data_buffer_->len) 
      {
        usb_free(data_buffer_);
        data_buffer_ = NULL;
      }
    }
    storage_->close();
    return true;
  }
  
    uint32_t MTPD::setObjectPropValue(uint32_t p1, uint32_t p2)
    {
      receive_buffer();
      if(p2==0xDC07)
      {
        char filename[MAX_FILENAME_LEN];
        ReadMTPHeader();
        readstring(filename);

        storage_->rename(p1,filename);

        return 0x2001;
      }
      else
        return 0x2005;
    }

  void MTPD::loop(void) 
  {
    usb_packet_t *receive_buffer;
    if ((receive_buffer = usb_rx(MTP_RX_ENDPOINT))) {
      printContainer();
      
        int op = CONTAINER->op;
        int p1 = CONTAINER->params[0];
        int p2 = CONTAINER->params[1];
        int p3 = CONTAINER->params[2];
        int id = CONTAINER->transaction_id;
        int len= CONTAINER->len;
        int typ= CONTAINER->type;
        TID=id;

      uint32_t return_code = 0;
      if (receive_buffer->len >= 12) {
        return_code = 0x2001;  // Ok
        receive_buffer->len = 12;
        
        if (typ == 1) { // command
          switch (op) {
            case 0x1001: // GetDescription
              TRANSMIT(WriteDescriptor());
              break;
            case 0x1002:  // OpenSession
              openSession(p1);
              break;
            case 0x1003:  // CloseSession
              break;
            case 0x1004:  // GetStorageIDs
              TRANSMIT(WriteStorageIDs());
              break;
            case 0x1005:  // GetStorageInfo
              TRANSMIT(GetStorageInfo(p1));
              break;
            case 0x1006:  // GetNumObjects
              if (p2) {
                return_code = 0x2014; // spec by format unsupported
              } else {
                p1 = GetNumObjects(p1, p3);
              }
              break;
            case 0x1007:  // GetObjectHandles
              if (p2) {
                return_code = 0x2014; // spec by format unsupported
              } else {
                TRANSMIT(GetObjectHandles(p1, p3));
              }
              break;
            case 0x1008:  // GetObjectInfo
              TRANSMIT(GetObjectInfo(p1));
              break;
            case 0x1009:  // GetObject
              TRANSMIT(GetObject(p1));
              break;
            case 0x100B:  // DeleteObject
              if (p2) {
                return_code = 0x2014; // spec by format unsupported
              } else {
                if (!storage_->DeleteObject(p1)) {
                  return_code = 0x2012; // partial deletion
                }
              }
              break;
              p3 =  SendObjectInfo(p1, // storage
                                   p2); // parent
              CONTAINER->params[1]=p2;
              CONTAINER->params[2]=p3;
              len = receive_buffer->len = 12 + 3 * 4;
              break;
            case 0x100D:  // SendObject
              SendObject();
              break;
            case 0x1014:  // GetDevicePropDesc
              TRANSMIT(GetDevicePropDesc(p1));
              break;
            case 0x1015:  // GetDevicePropvalue
              TRANSMIT(GetDevicePropValue(p1));
              break;
              
          case 0x1010:  // Reset
              return_code = 0x2005;
              break;

          case 0x1019:  // MoveObject
              return_code = moveObject(p1,p2,p3);
              len  = receive_buffer->len = 12;
              break;

          case 0x101A:  // CopyObject
              return_code = copyObject(p1,p2,p3);
              if(! return_code) { len  = receive_buffer->len = 12; return_code = 0x2005; }
              else {p1 = return_code; return_code=0x2001;}
              break;

          case 0x9801:  // getObjectPropsSupported
              TRANSMIT(getObjectPropsSupported(p1));
              break;

          case 0x9802:  // getObjectPropDesc
            TRANSMIT(getObjectPropDesc(p1,p2));
              break;

          case 0x9803:  // getObjectPropertyValue
            TRANSMIT(getObjectPropValue(p1,p2));
              break;

          case 0x9804:  // setObjectPropertyValue
              return_code = setObjectPropValue(p1,p2);
              break;
              
            default:
              return_code = 0x2005;  // operation not supported
              break;
          }
        } else {
          return_code = 0x2000;  // undefined
        }
      }
      if (return_code) {
        CONTAINER->type=3;
        CONTAINER->len=len;
        CONTAINER->op=return_code;
        CONTAINER->transaction_id=id;
        CONTAINER->params[0]=p1;
        #if DEBUG>1
          printContainer();
        #endif

        usb_tx(MTP_TX_ENDPOINT, receive_buffer);
        receive_buffer = 0;
      } else {
          usb_free(receive_buffer);
      }
    }
    // Maybe put event handling inside mtp_yield()?
    if ((receive_buffer = usb_rx(MTP_EVENT_ENDPOINT))) {
      printf("Event: "); printContainer();
      usb_free(receive_buffer);
    }
  }

#elif defined(__IMXRT1062__)  

    int MTPD::push_packet(uint8_t *data_buffer,uint32_t len)
    {
      while(usb_mtp_send(data_buffer,len,60)<=0) ;
      return 1;
    }

    int MTPD::pull_packet(uint8_t *data_buffer)
    {
      while(!usb_mtp_available());
      return usb_mtp_recv(data_buffer,60);
    }

    int MTPD::fetch_packet(uint8_t *data_buffer)
    {
      return usb_mtp_recv(data_buffer,60);
    }

    void MTPD::write(const char *data, int len) 
    { if (write_get_length_) 
      {
        write_length_ += len;
      } 
      else 
      { 
        static uint8_t *dst=0;
        if(!write_length_) dst=tx_data_buffer;   
        write_length_ += len;
        
        const char * src=data;
        //
        int pos = 0; // into data
        while(pos<len)
        { int avail = tx_data_buffer+MTP_TX_SIZE - dst;
          int to_copy = min(len - pos, avail);
          memcpy(dst,src,to_copy);
          pos += to_copy;
          src += to_copy;
          dst += to_copy;
          if(dst == tx_data_buffer+MTP_TX_SIZE)
          { push_packet(tx_data_buffer,MTP_TX_SIZE);
            dst=tx_data_buffer;
          }
        }
      }
    }

    void MTPD::GetObject(uint32_t object_id) 
    {
      uint32_t size = storage_->GetSize(object_id);

      if (write_get_length_) {
        write_length_ += size;
      } else 
      { 
        uint32_t pos = 0; // into data
        uint32_t len = sizeof(MTPHeader);

        disk_pos=DISK_BUFFER_SIZE;
        while(pos<size)
        {
          if(disk_pos==DISK_BUFFER_SIZE)
          {
            uint32_t nread=min(size-pos,(uint32_t)DISK_BUFFER_SIZE);
            storage_->read(object_id,pos,(char *)disk_buffer,nread);
            disk_pos=0;
          }

          uint32_t to_copy = min(size-pos,MTP_TX_SIZE-len);
          to_copy = min (to_copy, DISK_BUFFER_SIZE-disk_pos);

          memcpy(tx_data_buffer+len,disk_buffer+disk_pos,to_copy);
          disk_pos += to_copy;
          pos += to_copy;
          len += to_copy;

          if(len==MTP_TX_SIZE)
          { push_packet(tx_data_buffer,MTP_TX_SIZE);
            len=0;
          }
        }
        if(len>0)
        { push_packet(tx_data_buffer,MTP_TX_SIZE);
          len=0;
        }
      }
    }
    uint32_t MTPD::GetPartialObject(uint32_t object_id, uint32_t offset, uint32_t NumBytes) 
    {
      uint32_t size = storage_->GetSize(object_id);

      size -= offset;
      if(NumBytes == 0xffffffff) NumBytes=size;
      if (NumBytes<size) size=NumBytes;

      if (write_get_length_) {
        write_length_ += size;
      } else 
      { 
        uint32_t pos = offset; // into data
        uint32_t len = sizeof(MTPHeader);

        disk_pos=DISK_BUFFER_SIZE;
        while(pos<size)
        {
          if(disk_pos==DISK_BUFFER_SIZE)
          {
            uint32_t nread=min(size-pos,(uint32_t)DISK_BUFFER_SIZE);
            storage_->read(object_id,pos,(char *)disk_buffer,nread);
            disk_pos=0;
          }

          uint32_t to_copy = min(size-pos,MTP_TX_SIZE-len);
          to_copy = min (to_copy, DISK_BUFFER_SIZE-disk_pos);

          memcpy(tx_data_buffer+len,disk_buffer+disk_pos,to_copy);
          disk_pos += to_copy;
          pos += to_copy;
          len += to_copy;

          if(len==MTP_TX_SIZE)
          { push_packet(tx_data_buffer,MTP_TX_SIZE);
            len=0;
          }
        }
        if(len>0)
        { push_packet(tx_data_buffer,MTP_TX_SIZE);
          len=0;
        }
      }
      return size;
    }

    #define CONTAINER ((struct MTPContainer*)(rx_data_buffer))

    #define TRANSMIT(FUN) do {                            \
      write_length_ = 0;                                  \
      write_get_length_ = true;                           \
      FUN;                                                \
      \
      MTPHeader header;                                   \
      header.len = write_length_ + sizeof(header);        \
      header.type = 2;                                    \
      header.op = CONTAINER->op;                          \
      header.transaction_id = CONTAINER->transaction_id;  \
      write_length_ = 0;                                  \
      write_get_length_ = false;                          \
      write((char *)&header, sizeof(header));             \
      FUN;                                                \
      \
      uint32_t rest;                                      \
      rest = (header.len % MTP_TX_SIZE);                  \
      if(rest>0)                                          \
      {                                                   \
        push_packet(tx_data_buffer,rest);                 \
      }                                                   \
    } while(0)

    #define TRANSMIT1(FUN) do {                           \
      write_length_ = 0;                                  \
      write_get_length_ = true;                           \
      uint32_t dlen = FUN;                                \
      \
      MTPContainer header;                                   \
      header.len = write_length_ + sizeof(MTPHeader)+4;      \
      header.type = 2;                                    \
      header.op = CONTAINER->op;                          \
      header.transaction_id = CONTAINER->transaction_id;  \
      header.params[0]=dlen;                              \
      write_length_ = 0;                                  \
      write_get_length_ = false;                          \
      write((char *)&header, sizeof(header));             \
      FUN;                                                \
      \
      uint32_t rest;                                      \
      rest = (header.len % MTP_TX_SIZE);                  \
      if(rest>0)                                          \
      {                                                   \
        push_packet(tx_data_buffer,rest);                        \
      }                                                   \
    } while(0)


    #define printContainer() \
    { printf("%x %d %d %d: ", CONTAINER->op, CONTAINER->len, CONTAINER->type, CONTAINER->transaction_id); \
      if(CONTAINER->len>12) printf(" %x", CONTAINER->params[0]); \
      if(CONTAINER->len>16) printf(" %x", CONTAINER->params[1]); \
      if(CONTAINER->len>20) printf(" %x", CONTAINER->params[2]); \
    printf("\r\n"); \
    }


    void MTPD::read(char* data, uint32_t size) 
    {
      static int index=0;
      if(!size) 
      {
        index=0;
        return;
      }

      while (size) {
        uint32_t to_copy = MTP_RX_SIZE - index;
        to_copy = min(to_copy, size);
        if (data) {
          memcpy(data, rx_data_buffer + index, to_copy);
          data += to_copy;
        }
        size -= to_copy;
        index += to_copy;
        if (index == MTP_RX_SIZE) {
          pull_packet(rx_data_buffer);
          index=0;
        }
      }
    }

    uint32_t MTPD::SendObjectInfo(uint32_t storage, uint32_t parent) {
      pull_packet(rx_data_buffer);
      read(0,0); // resync read
//      printContainer(); 
      uint32_t store = Storage2Store(storage);

      int len=ReadMTPHeader();
      char filename[MAX_FILENAME_LEN];

      read32(); len -=4; // storage
      bool dir = (read16() == 0x3001); len -=2; // format
      read16(); len -=2; // protection
      read32(); len -=4; // size
      read16(); len -=2; // thumb format
      read32(); len -=4; // thumb size
      read32(); len -=4; // thumb width
      read32(); len -=4; // thumb height
      read32(); len -=4; // pix width
      read32(); len -=4; // pix height
      read32(); len -=4; // bit depth
      read32(); len -=4; // parent
      read16(); len -=2; // association type
      read32(); len -=4; // association description
      read32(); len -=4; // sequence number
      readstring(filename); len -= (2*(strlen(filename)+1)+1); 
      // ignore rest of ObjectInfo
      while(len>=4) { read32(); len-=4;}
      while(len) {read8(); len--;}

      return storage_->Create(store, parent, dir, filename);
    }

    bool MTPD::SendObject() 
    { 
      pull_packet(rx_data_buffer);
      read(0,0);
//      printContainer(); 

      uint32_t len = ReadMTPHeader();
      uint32_t index = sizeof(MTPHeader);
      disk_pos=0;
      
      while((int)len>0)
      { uint32_t bytes = MTP_RX_SIZE - index;                     // how many data in usb-packet
        bytes = min(bytes,len);                                   // loimit at end
        uint32_t to_copy=min(bytes, DISK_BUFFER_SIZE-disk_pos);   // how many data to copy to disk buffer
        memcpy(disk_buffer+disk_pos, rx_data_buffer + index,to_copy);
        disk_pos += to_copy;
        bytes -= to_copy;
        len -= to_copy;
        //printf("a %d %d %d %d %d\n", len,disk_pos,bytes,index,to_copy);
        //
        if(disk_pos==DISK_BUFFER_SIZE)
        {
          if(storage_->write((const char *)disk_buffer, DISK_BUFFER_SIZE)<DISK_BUFFER_SIZE) return false;
          disk_pos =0;

          if(bytes) // we have still data in transfer buffer, copy to initial disk_buffer
          {
            memcpy(disk_buffer,rx_data_buffer+index+to_copy,bytes);
            disk_pos += bytes;
            len -= bytes;
          }
          //printf("b %d %d %d %d %d\n", len,disk_pos,bytes,index,to_copy);
        }
        if(len>0)  // we have still data to be transfered
        { pull_packet(rx_data_buffer);
          index=0;
        }
      }
      //printf("len %d\n",disk_pos);
      if(disk_pos)
      {
        if(storage_->write((const char *)disk_buffer, disk_pos)<disk_pos) return false;
      }
      storage_->close();
      return true;
    }

    uint32_t MTPD::setObjectPropValue(uint32_t handle, uint32_t p2)
    { pull_packet(rx_data_buffer);
      read(0,0);
      //printContainer(); 
         
      if(p2==0xDC07)
      { 
        char filename[MAX_FILENAME_LEN]; 
        ReadMTPHeader();
        readstring(filename);
        if(storage_->rename(handle,filename)) return 0x2001; else return 0x2005;
      }
      else
        return 0x2005;
    }

    void MTPD::loop(void)
    { if(!usb_mtp_available()) return;
      if(fetch_packet(rx_data_buffer))
      { printContainer(); // to switch on set debug to 1 at beginning of file

        int op = CONTAINER->op;
        int p1 = CONTAINER->params[0];
        int p2 = CONTAINER->params[1];
        int p3 = CONTAINER->params[2];
        int id = CONTAINER->transaction_id;
        int len= CONTAINER->len;
        int typ= CONTAINER->type;
        TID=id;

        int return_code =0x2001; //OK use as default value

        if(typ==2) return_code=0x2005; // we should only get cmds

        switch (op)
        {
          case 0x1001:
            TRANSMIT(WriteDescriptor());
            break;

          case 0x1002:  //open session
            openSession(p1);
            break;

          case 0x1003:  // CloseSession
            break;

          case 0x1004:  // GetStorageIDs
              TRANSMIT(WriteStorageIDs());
            break;

          case 0x1005:  // GetStorageInfo
            TRANSMIT(GetStorageInfo(p1));
            break;

          case 0x1006:  // GetNumObjects
            if (p2) 
            {
                return_code = 0x2014; // spec by format unsupported
            } else 
            {
                p1 = GetNumObjects(p1, p3);
            }
            break;

          case 0x1007:  // GetObjectHandles
            if (p2) 
            { return_code = 0x2014; // spec by format unsupported
            } else 
            { 
              TRANSMIT(GetObjectHandles(p1, p3));
            }
            break;

          case 0x1008:  // GetObjectInfo
            TRANSMIT(GetObjectInfo(p1));
            break;

          case 0x1009:  // GetObject
            TRANSMIT(GetObject(p1));
            break;

          case 0x100B:  // DeleteObject
              if (p2) {
                return_code = 0x2014; // spec by format unsupported
              } else {
                if (!storage_->DeleteObject(p1)) {
                  return_code = 0x2012; // partial deletion
                }
              }
              break;

          case 0x100C:  // SendObjectInfo
              p3 = SendObjectInfo(p1, // storage
                                  p2); // parent

              CONTAINER->params[1]=p2;
              CONTAINER->params[2]=p3;
              len = 12 + 3 * 4;
              break;

          case 0x100D:  // SendObject
              if(!SendObject()) return_code = 0x2005;
              len = 12;
              break;

          case 0x1014:  // GetDevicePropDesc
              TRANSMIT(GetDevicePropDesc(p1));
              break;

          case 0x1015:  // GetDevicePropvalue
              TRANSMIT(GetDevicePropValue(p1));
              break;

          case 0x1010:  // Reset
              return_code = 0x2005;
              break;

          case 0x1019:  // MoveObject
              return_code = moveObject(p1,p2,p3);
              len = 12;
              break;

          case 0x101A:  // CopyObject
              return_code = copyObject(p1,p2,p3);
              if(!return_code) 
              { return_code=0x2005; len = 12; }
              else
              { p1 = return_code; return_code=0x2001; len = 16;  }
              break;

          case 0x101B:  // GetPartialObject
              TRANSMIT1(GetPartialObject(p1,p2,p3));
              break;

          case 0x9801:  // getObjectPropsSupported
              TRANSMIT(getObjectPropsSupported(p1));
              break;

          case 0x9802:  // getObjectPropDesc
              TRANSMIT(getObjectPropDesc(p1,p2));
              break;

          case 0x9803:  // getObjectPropertyValue
              TRANSMIT(getObjectPropValue(p1,p2));
              break;

          case 0x9804:  // setObjectPropertyValue
              return_code = setObjectPropValue(p1,p2);
              break;

          default:
              return_code = 0x2005;  // operation not supported
              break;
        }
        if(return_code)
        {
            CONTAINER->type=3;
            CONTAINER->len=len;
            CONTAINER->op=return_code;
            CONTAINER->transaction_id=id;
            CONTAINER->params[0]=p1;
            #if DEBUG >1
              printContainer(); // to switch on set debug to 2 at beginning of file
            #endif

            memcpy(tx_data_buffer,rx_data_buffer,len);
            push_packet(tx_data_buffer,len); // for acknowledge use rx_data_buffer
        }
      }
    }


#endif
#if USE_EVENTS==1

 #if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)

  #include "usb_mtp.h"
  extern "C"
  {
    usb_packet_t *tx_event_packet=NULL;

    int usb_init_events(void)
    {
      tx_event_packet = usb_malloc();
      if(tx_event_packet) return 1; else return 0; 
    }


    int usb_mtp_sendEvent(const void *buffer, uint32_t len, uint32_t timeout)
    {
      if (!usb_configuration) return -1;
      memcpy(tx_event_packet->buf, buffer, len);
      tx_event_packet->len = len;
      usb_tx(MTP_EVENT_ENDPOINT, tx_event_packet);
      return len;
    }
  }

  #elif defined(__IMXRT1062__)
  // keep this here until cores is upgraded 

  #include "usb_mtp.h"
  extern "C"
  {
    static transfer_t tx_event_transfer[1] __attribute__ ((used, aligned(32)));
    static uint8_t tx_event_buffer[MTP_EVENT_SIZE] __attribute__ ((used, aligned(32)));

    static transfer_t rx_event_transfer[1] __attribute__ ((used, aligned(32)));
    static uint8_t rx_event_buffer[MTP_EVENT_SIZE] __attribute__ ((used, aligned(32)));

    static uint32_t mtp_txEventcount=0;
    static uint32_t mtp_rxEventcount=0;

    uint32_t get_mtp_txEventcount() {return mtp_txEventcount; }
    uint32_t get_mtp_rxEventcount() {return mtp_rxEventcount; }
    
    static void txEvent_event(transfer_t *t) { mtp_txEventcount++;}
    static void rxEvent_event(transfer_t *t) { mtp_rxEventcount++;}

    int usb_init_events(void)
    {
        usb_config_tx(MTP_EVENT_ENDPOINT, MTP_EVENT_SIZE, 0, txEvent_event);
        //	
        usb_config_rx(MTP_EVENT_ENDPOINT, MTP_EVENT_SIZE, 0, rxEvent_event);
        usb_prepare_transfer(rx_event_transfer + 0, rx_event_buffer, MTP_EVENT_SIZE, 0);
        usb_receive(MTP_EVENT_ENDPOINT, rx_event_transfer + 0);
        return 1;
    }

    static int usb_mtp_wait(transfer_t *xfer, uint32_t timeout)
    {
      uint32_t wait_begin_at = systick_millis_count;
      while (1) {
        if (!usb_configuration) return -1; // usb not enumerated by host
        uint32_t status = usb_transfer_status(xfer);
        if (!(status & 0x80)) break; // transfer descriptor ready
        if (systick_millis_count - wait_begin_at > timeout) return 0;
        yield();
      }
      return 1;
    }

    int usb_mtp_recvEvent(void *buffer, uint32_t len, uint32_t timeout)
    {
      int ret= usb_mtp_wait(rx_event_transfer, timeout); if(ret<=0) return ret;

      memcpy(buffer, rx_event_buffer, len);
      memset(rx_event_transfer, 0, sizeof(rx_event_transfer));

      NVIC_DISABLE_IRQ(IRQ_USB1);
      usb_prepare_transfer(rx_event_transfer + 0, rx_event_buffer, MTP_EVENT_SIZE, 0);
      usb_receive(MTP_EVENT_ENDPOINT, rx_event_transfer + 0);
      NVIC_ENABLE_IRQ(IRQ_USB1);
      return MTP_EVENT_SIZE;
    }

    int usb_mtp_sendEvent(const void *buffer, uint32_t len, uint32_t timeout)
    {
      transfer_t *xfer = tx_event_transfer;
      int ret= usb_mtp_wait(xfer, timeout); if(ret<=0) return ret;

      uint8_t *eventdata = tx_event_buffer;
      memcpy(eventdata, buffer, len);
      usb_prepare_transfer(xfer, eventdata, len, 0);
      usb_transmit(MTP_EVENT_ENDPOINT, xfer);
      return len;
    }
  }

  #endif
  const uint32_t EVENT_TIMEOUT=60;

  int MTPD::send_Event(uint16_t eventCode)
  {
    MTPContainer event;
    event.len = 12;
    event.op =eventCode ;
    event.type = MTP_CONTAINER_TYPE_EVENT; 
    event.transaction_id=TID;
    event.params[0]=0;
    event.params[1]=0;
    event.params[2]=0;
    return usb_mtp_sendEvent((const void *) &event, event.len, EVENT_TIMEOUT);
  }
  int MTPD::send_Event(uint16_t eventCode, uint32_t p1)
  {
    MTPContainer event;
    event.len = 16;
    event.op =eventCode ;
    event.type = MTP_CONTAINER_TYPE_EVENT; 
    event.transaction_id=TID;
    event.params[0]=p1;
    event.params[1]=0;
    event.params[2]=0;
    return usb_mtp_sendEvent((const void *) &event, event.len, EVENT_TIMEOUT);
  }
  int MTPD::send_Event(uint16_t eventCode, uint32_t p1, uint32_t p2)
  {
    MTPContainer event;
    event.len = 20;
    event.op =eventCode ;
    event.type = MTP_CONTAINER_TYPE_EVENT; 
    event.transaction_id=TID;
    event.params[0]=p1;
    event.params[1]=p2;
    event.params[2]=0;
    return usb_mtp_sendEvent((const void *) &event, event.len, EVENT_TIMEOUT);
  }
  int MTPD::send_Event(uint16_t eventCode, uint32_t p1, uint32_t p2, uint32_t p3)
  {
    MTPContainer event;
    event.len = 24;
    event.op =eventCode ;
    event.type = MTP_CONTAINER_TYPE_EVENT; 
    event.transaction_id=TID;
    event.params[0]=p1;
    event.params[1]=p2;
    event.params[2]=p3;
    return usb_mtp_sendEvent((const void *) &event, event.len, EVENT_TIMEOUT);
  }

  int MTPD::send_DeviceResetEvent(void) 
  { return send_Event(MTP_EVENT_DEVICE_RESET); } 
  // following WIP
  int MTPD::send_StorageInfoChangedEvent(uint32_t p1) 
  { return send_Event(MTP_EVENT_STORAGE_INFO_CHANGED, Store2Storage(p1));}

  // following not tested
  int MTPD::send_addObjectEvent(uint32_t p1) 
  { return send_Event(MTP_EVENT_OBJECT_ADDED, p1); }
  int MTPD::send_removeObjectEvent(uint32_t p1) 
  { return send_Event(MTP_EVENT_OBJECT_REMOVED, p1); }

#endif
#endif
