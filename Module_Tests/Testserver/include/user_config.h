// user_config.h
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-12

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

/*-------- user configurable ---------*/

// General settings:

#define REQUEST_STRING "DEVICE_INFO\n"  // Command to request the mesh-devices'
                                        // meta-data
#define REQUEST_INTERVAL 10000  // Interval, in which the meta-data-request is
                                // sent

/*------------------------------------*/

// Communication and interaction:

#define SSID "testNetwork"  // SSID of the Soft-AP
#define PASSWD "testPasswd" // Password of the Soft-AP

#define DEVICE_COM_PORT 49152 // First non-well-known nor registered port

#define BROADCAST_IP {192, 168, 4, 255};  // Broadcast-address

#endif
