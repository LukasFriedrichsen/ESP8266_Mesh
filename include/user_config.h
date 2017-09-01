// user_config.h
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-02

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

/*-------- user configurable ---------*/

// General settings:

// Annotation: Please make sure to also adapt the correlating function-assignment
// in gpio_pins_init (cf. esp_mesh.c) if the addresses of the GPIO-pins are
// modified!

#define SSID_PREFIX "MESH" // SSID-prefix of the mesh-network

#define GROUP_ID {0x53, 0x32, 0x30, 0x45, 0x53, 0x50} // Group-ID of the mesh-
                                                      // network (can be assigned
                                                      // to a logical group of
                                                      // mesh-nodes, e.g. all
                                                      // devices in the shop
                                                      // floor are members of
                                                      // group 0, all nodes in
                                                      // the administration
                                                      // belong to group 1, etc.)

#define MESH_AUTH_PASSWD "S20_SmartSocket-ESP_Mesh_Network" // Password needed by
                                                            // a node to connect
                                                            // to the rest of the
                                                            // mesh-network

#define MESH_AUTH_MODE AUTH_WPA2_PSK  // Authentication mode, each mesh-node is
                                      // secured with

#define MESH_ENABLE_ATTEMPTS_LIMIT 3  // Maximum number of attempts to enable the
                                      // mesh-node before aborting

#define MESH_CONN_TIMEOUT_WDT_INTERVAL 300000 // Time-interval, in which the mesh-
                                              // node's state is checked by a
                                              // software-watchdog-timer, which
                                              // restores the device's initial
                                              // state, if no connection to the
                                              // router/parent- node (depending
                                              // on the operation- mode) is
                                              // established (in ms)

#define MAX_HOPS 4  // Maximum number of mesh-layers a message can traverse;
                    // make sure, that the necessary heap is avaliable:
                    // heap_required = (4^MAX_HOPS-1)/3*6 [byte]
                    // => e.g. 510 byte for MAX_HOPS = 4

#define SUB_NODE_TIMEOUT_THRESHOLD 30000  // Time, after which a non-responsive
                                          // device is deleted from the list of
                                          // registered nodes (in ms)

#define OUTPUT_POWER_RELAY_GPIO 12  // GPIO-pin, that is connected to the red LED
                                    // as well as to the relay, which controls
                                    // the smart plug's output power; the blue
                                    // LED is activated when the output power is
                                    // turned on

#define BUTTON_INTERRUPT_GPIO 0 // GPIO-pin of the pushbutton, whose actuation
                                // activates the mesh-node and with which the
                                // device's operation mode can be chosen (root
                                // or sub-node)

#define STATUS_LED_GPIO 13  // GPIO-pin of the green LED, which is used to
                            // signalize the node's connection status

#define LED_BLINK_INTERVAL_SHORT 500  // Short blink-interval for the status-LED;
                                      // used to signal that the node is currently
                                      // in smartconfiguration-mode (in ms)

#define LED_BLINK_INTERVAL_LONG 2000  // Long blink-interval for the status-LED;
                                      // used to signal that the enabling of the
                                      // mesh-device is in progress (in ms)

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
                              // with other devices in the mesh-network (e.g. the
                              // node's meta-data can be requested via this port)

// Vital sign broadcast:

#define VITAL_SIGN_PORT 49153 // Second non-well-known nor registered port; the
                              // device cyclically broadcasts a vital sign on
                              // this port

#define VITAL_SIGN_TIME_INTERVAL 300000 // Time-interval, in which the vital
                                        // sign is broadcasted (in ms)

/*------------------------------------*/

// Topology-tests:

#define TOPOLOGY_TIME_INTERVAL 15000  // Time-interval, in which a topology-test
                                      // is executed (in ms)

/*------------------------------------*/

// ESP-TOUCH:

#define ESP_TOUCH_ATTEMPTS_LIMIT 3  // Maximum number of attempts to connect to
                                    // the router via ESP-TOUCH before aborting

#define ESP_TOUCH_CONFIG_TIMEOUT_THRESHOLD 30000  // Time limit to receive the
                                                  // configuration via ESP-TOUCH
                                                  
#define ESP_TOUCH_RECV_TIMEOUT_THRESHOLD 30000  // Time limit to receive the
                                                // station-configuration from the
                                                // intermediary-device

#endif
