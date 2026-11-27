/*
 * CONNECTION_V2 - DIY-Thermocam USB Protocol v2.0 full-duplex server
 * Compatible with DIY-Thermocam Studio (Tauri) on V1 (Teensy 3.1/3.2) and V2 (Teensy 3.6).
 */

#ifndef CONNECTION_V2_H
#define CONNECTION_V2_H

#include <stdint.h>

void serialConnectV2(bool headless);
bool checkSerialV2Magic();

#endif /* CONNECTION_V2_H */
