// user_config.h
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-02

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

/*-------- user configurable ---------*/

// General settings:

#define SSID "EMRA_Prototype_Network"  // Router-SSID
#define PASSWD "EnergyMeterReadoutAutomation" // Router-password

#define SSID_PREFIX "MESH_" // SSID-prefix of the mesh-network
#define GROUP_ID {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} // Group-ID of the mesh-
                                                      // network (can be assigned
                                                      // to a logical group of
                                                      // mesh-nodes, e.g. all
                                                      // devices in the shop
                                                      // floor are members of
                                                      // group 0, all nodes in
                                                      // the administration
                                                      // belong to group 1, etc.)

#define MESH_AUTH_PASSWD "EnergyMeterReadoutAutomation" // Password needed by
                                                            // a node to connect
                                                            // to the rest of the
                                                            // mesh-network
#define MESH_AUTH_MODE AUTH_WPA_WPA2_PSK  // Authentication mode, each mesh-node is
                                      // secured with

#define MESH_ENABLE_ATTEMPTS_LIMIT 3  // Maximum number of attempts to enable the
                                      // mesh-node before aborting

#define MESH_CONN_TIMEOUT_WDT_INTERVAL 60000  // Time-interval, in which the mesh-
                                              // node's state is checked by a
                                              // software-watchdog-timer, who
                                              // restores the device's initial
                                              // state, if no connection to the
                                              // router/parent- node (depending
                                              // on the operation- mode) is
                                              // established (in ms)

#define MAX_HOPS 4  // Maximum number of mesh-layers a message can traverse;
                    // make sure, that the necessary heap is avaliable:
                    // heap_required = (4^MAX_HOPS-1)/3*6 [byte]
                    // => e.g. 510 byte for MAX_HOPS = 4

/*------------------------------------*/

// ESP-TOUCH:

#define ESP_TOUCH_ATTEMPTS_LIMIT 3  // Maximum number of attempts to connect to
                                    // the router via ESP-TOUCH before aborting
#define ESP_TOUCH_CONFIG_TIMEOUT_THRESHOLD 30000  // Time limit to receive the
                                                  // configuration via ESP-TOUCH
#define ESP_TOUCH_RECV_TIMEOUT_THRESHOLD 30000  // Time limit to receive the
                                                // station-configuration from the
                                                // intermediary-device

#define ESP_TOUCH_CONNECTION_TIMEOUT_THRESHOLD 60000  // Time limit to connect to
                                                      // the router after
                                                      // obtaining SSID & PSWD
                                                                                                      // via ESP-TOUCH

#endif
