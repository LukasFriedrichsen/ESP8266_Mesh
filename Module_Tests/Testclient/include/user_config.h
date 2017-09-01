// user_config.h
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-12

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

/*-------- user configurable ---------*/

// Communication and interaction:

#define SSID "MESH_1_17D2EB"  // SSID of the AP
#define BSSID {0xa2, 0x20, 0xa6, 0x17, 0xd2, 0xeb}
#define PASSWD "S20_SmartSocket-ESP_Mesh_Network" // Password of the AP

#define REMOTE_PORT 49154 // Third non-well-known nor registered port

#define BROADCAST_IP {192, 168, 42, 255};  // Broadcast-address
#define BROADCAST_INTERVAL 1000 // Interval, in which a message is broadcasted

#endif
