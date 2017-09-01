// user_config.h
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-16

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

/*-------- user configurable ---------*/

// General settings:

// Annotation: Please make sure to also adapt the correlating function-assignment
// in gpio_pins_init (cf. esp_mesh.c) if the addresses of the GPIO-pins are
// modified!

#define OUTPUT_POWER_RELAY_GPIO 12  // GPIO-pin, that is connected to the red LED
                                    // as well as to the relay, which controls
                                    // the smart plug's output power; the LED is
                                    // activated when the output power is
                                    // turned on

#define BUTTON_INTERRUPT_GPIO 0 // GPIO-pin of the pushbutton, whose actuation
                                // activates the mesh-node and with which the
                                // device's operation mode can be chosen (root
                                // or sub-node)

#define OPERATION_MODE_THRESHOLD 3000 // If the pushbutton stays actuated for
                                      // longer than this time-interval, the
                                      // node's operation mode is set to root
                                      // instead of sub-node (in ms)

#define STATUS_LED_GPIO 13  // GPIO-pin of the green LED, which is used to
                            // signalize the node's connection status

#define LED_BLINK_INTERVAL_SHORT 500  // Short blink-interval for the status-LED;
                                      // used to signal that the node is trying
                                      // to connect to an existing mesh-network
                                      // (in ms)

#define LED_BLINK_INTERVAL_LONG 1000  // Long blink-interval for the status-LED;
                                      // used to signal that the node is in
                                      // ESP-TOUCH-mode and trying to initiate a
                                      // new mesh-network (in ms)

#endif
