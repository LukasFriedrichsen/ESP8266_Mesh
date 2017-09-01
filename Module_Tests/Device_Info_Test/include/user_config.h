// user_config.h
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-19

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

/*-------- user configurable ---------*/

// General settings:

#define SSID "testNetwork"  // Router-SSID
#define PASSWD "testPasswd" // Router-password

/*------------------------------------*/

// Meta-data:

#define DEVICE_PURPOSE "MESH_NODE"  // Description of the devices purpose
#define META_DATA_REQUEST_STRING "DEVICE_INFO\n"  // The device will return its
                                                  // meta-data to the sender if
                                                  // this String is received via
                                                  // an UDP-message

/*------------------------------------*/

// Communication and interaction:

#define DEVICE_COM_PORT 49152 // First non-well-known nor registered port; this
                              // port is used for communication and interaction
                              // with other devices in the mesh-network (e.g.
                              // node's meta-data can be requested via this port)

// Vital sign broadcast:

#define VITAL_SIGN_PORT 49153 // Second non-well-known nor registered port; the
                              // device cyclically broadcasts a vital sign on
                              // this port

#define VITAL_SIGN_TIME_INTERVAL 10000  // Time-interval, in which the vital
                                        // sign is broadcasted (in ms)

#endif
