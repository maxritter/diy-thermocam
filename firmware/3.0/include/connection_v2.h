/*
 * CONNECTION_V2 - DIY-Thermocam USB Protocol v2.0 Server
 */

#ifndef CONNECTION_V2_H
#define CONNECTION_V2_H

#include <Arduino.h>

void serialConnectV2(bool headless);
bool checkSerialV2OrV1(bool headless);

#endif /* CONNECTION_V2_H */
