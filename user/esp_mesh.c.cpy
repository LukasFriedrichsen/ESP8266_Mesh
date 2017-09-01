// esp_mesh.c
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-09
//
// Description: This implementation of the ESP-Mesh-network is designed for the
//
//       "S20 Smart Socket" (cf. itead.cc/smart-socket-eu/html) by ITEAD,
//
// a WiFi-enabled smart socket based on the ESP8266-microcontroller. The mesh-
// node is enabled through actuating the pushbutton. The length of the actuation
// determines the device's operation-mode, either root-mode, where the device
// goes into smartconfiguration-mode and tries to connect to a router, whose
// authentication credentials it obtains via ESP-TOUCH from a nearby intermediary-
// device (e.g. a smartphone), thus initializing a new mesh-network, or sub-mode,
// where the node tries to connect to an already existing network. Afterwards,
// the device puts up or expands (depending on the operation-mode) an encrypted,
// self-healing WiFi-network (IEEE 802.11 standard, 2.4GHz band) that relays
// messages between the connected endpoints.
//
// The device cyclically executes a topology-test to determine the network-
// infrastructure, thus allowing P2P-communication between the individual nodes.
// Furthermore, it periodically broadcasts a vital sign to enable automated
// availability-monitoring. The device's meta-data can be requested via a message
// in the mesh-header-format. Similary, it is possible to remote-control the
// smart plug's output power.
// For the whole time, the device's status is displayed by the LEDs:
//
//  green (blinking, slow):     connection in progress (root-mode)
//  green (blinking, fast):     connection in progress (sub-mode)
//  green (steady):             successfully connected
//  red:                        output power turned on
//
// The configuration of the mesh-network can be modified in user_config.h.
//
// This class is based on https://github.com/espressif/ESP8266_MESH_DEMO/tree/master/mesh_demo/demo/mesh_demo.c
//
/******************************************************************************/
//  ATTENTION: Tested and compiled with ESP-NONOS-SDK version 2.0.0_16_08_10!
/******************************************************************************/

#include "mem.h"
#include "osapi.h"
#include "user_interface.h"
#include "mesh.h"
#include "device_info.h"
#include "mesh_parser.h"
#include "esp_touch.h"
#include "esp_mesh.h"
#include "user_config.h"

/*------------------------------------*/

// Definition of functions (so there won't be any complications because the
// compiler resolves the scope top-down):

// Callback-functions:
static void esp_mesh_recv_cb(void *arg, char *data, uint16_t len);
static void esp_mesh_node_join_cb(void *mac);
static void esp_mesh_rebuild_fail_cb(void *arg);
static void esp_mesh_enable_cb(int8_t result);
static void esp_mesh_disable_cb(void);

// Timer- and interrupt-handler-functions:
static void button_pressed_interrupt_handler(void *arg);
static void button_released_interrupt_handler(void *arg);
static void esp_mesh_conn_timeout_wdtfunc(void *arg);
static void esptouch_over_timerfunc(os_timer_t *timer);
static void led_blink_timerfunc(void *arg);

// GPIO control:
static void status_led_on(void);
static void status_led_off(void);
void output_power_on(void);
void output_power_off(void);

// Initialization and configuration:
static bool root_init(void);
static bool sub_init(void);
static bool mesh_init(void);
static bool esp_mesh_config(void);
static void gpio_pins_init(void);

void user_init(void);

// Radio frequency configuration
uint32 user_rf_cal_sector_set(void);
void user_rf_pre_init(void);

/*------------------------------------*/

// Declaration and initialization of variables:

const static uint8_t group_id[] = GROUP_ID;  // Local copy of GROUP_ID

static enum op_mode mode = DISABLED;

static struct station_config *station_conf = NULL;

struct espconn *esp_mesh_conn = NULL;  // Socket for connection and communication with other mesh-nodes and devices in the network

static uint32_t actuation_start_time = 0;
static uint8_t esp_mesh_enable_attempt_count = 1;

static os_timer_t *led_blink_timer = NULL, *esp_mesh_conn_timeout_wdt = NULL;

/*------------------------------------*/

// Callback-functions:

// Callback-function, that passes received messages from other nodes to the parser
static void ICACHE_FLASH_ATTR esp_mesh_recv_cb(void *arg, char *data, uint16_t len) {
  if (!arg || !data || len <= 0) {
    os_printf("esp_mesh_recv_cb: Invalid transfer paramters!\n");
    return;
  }

  // Pass the received data to the parser
  mesh_packet_parser(arg, data, len);
}

// Callback-function, that notifies, if a new sub-node joins the mesh-network
static void ICACHE_FLASH_ATTR esp_mesh_node_join_cb(void *mac) {
  if (!mac) {
    os_printf("esp_mesh_node_join_cb: Invalid transfer parameter!\n");
    return;
  }

  os_printf("esp_mesh_node_join_cb: New sub-node joined: " MACSTR "\n", MAC2STR((uint8_t *) mac));
}

// Callback-function, that is executed, if the mesh-network fails to be rebuild; try to re-enable the mesh-node
static void ICACHE_FLASH_ATTR esp_mesh_rebuild_fail_cb(void *arg) {
  os_printf("esp_mesh_rebuild_fail_cb: Failed to rebuild mesh! Trying to restart it!\n");
  espconn_mesh_enable(esp_mesh_enable_cb, MESH_ONLINE); // Try to re-enable the mesh-node
}

// Callback-function, that is executed on a change of the node's connection status after espconn_mesh_enable has been called;
// try to re-enable the device in case of an error until the attempt-limit has been reached or initialize the socket for inter-
// mesh-communication and start the periodical vital sign broadcasts and topology-tests
static void ICACHE_FLASH_ATTR esp_mesh_enable_cb(int8_t result) {
  if (result == MESH_OP_FAILURE) {
    os_printf("esp_mesh_enable_cb: Failed to enable the mesh-node!\n");
    if (esp_mesh_enable_attempt_count < MESH_ENABLE_ATTEMPTS_LIMIT) {
      os_printf("esp_mesh_enable_cb: Retrying!\n");

      // Increase the attempt-count
      esp_mesh_enable_attempt_count++;

      // Try to re-enable the mesh-node
      espconn_mesh_enable(esp_mesh_enable_cb, MESH_ONLINE);
    }
    else {
      os_printf("esp_mesh_enable_fail_cb: Reached attempt-limit! Disabling mesh-node and restoring initial state!\n");

      // Disable the mesh-node
      espconn_mesh_disable((espconn_mesh_callback) esp_mesh_disable_cb);
    }
  }
  else {
    os_printf("esp_mesh_enable_cb: Successfully enabled the mesh-node!\n");

    // Reset the attempt-count once the mesh is successfully enabled
    esp_mesh_enable_attempt_count = 1;

    // Disable the blink-timer and switch on the status-LED
    os_timer_disarm(led_blink_timer);
    status_led_on();

    // Initialize the socket for inter-mesh-communication
    if (!esp_mesh_conn) {
      esp_mesh_conn = (struct espconn *) os_zalloc(sizeof(struct espconn));
    }

    // Register the receive-callback
    if (!espconn_regist_recvcb(esp_mesh_conn, esp_mesh_recv_cb)) {
      // Initialize periodical topology-tests
      // Only enable this, if a sufficient power supply is guaranteed and/or if P2P-communication is required!
      //mesh_topology_init();

      // Initialize further communication- and interaction-functionalities (e.g. the possibility for other
      // devices in the mesh-network to request the node's meta-dat via an UDP-message)
      //device_info_init();

      // Start periodical vital-sign-broadcasts
      // Only enable this, if a sufficient power supply is guaranteed! For devices that require a low power consumption
      // (e.g. if they run on a battery), it is recommended to let the server request a vital sign (e.g. through the
      // device-find-functionality (cf. device_find.c)) on need.
      //vital_sign_bcast_start();
    }
    else {
      os_printf("esp_mesh_enable_cb: Error while registering receive-callback!\n");
      espconn_mesh_disable((espconn_mesh_callback) esp_mesh_disable_cb);
      return;
    }
  }
}

// Callback-function, that is executed, if the mesh-network is disabled; restores the initial state of the program,
// so that the node is ready to be re-activated via the pushbutton
static void ICACHE_FLASH_ATTR esp_mesh_disable_cb(void) {
  // Disable the periodical topology-tests
  mesh_topology_disable();

  // Disable all further communication- and interaction-functionalities, including the periodical vital sign broadcasts
  // as well as the possibility to request the devices meta-data
  device_info_disable();

  // Clear possible connections and set the operation-mode to NULL_MODE
  wifi_station_disconnect();
  wifi_set_opmode(NULL_MODE);

  // Free occupied resources
  if (station_conf) {
    os_free(station_conf);
    station_conf = NULL;
  }
  if (esp_mesh_conn) {
    os_free(esp_mesh_conn);
    esp_mesh_conn = NULL;
  }
  if (led_blink_timer) {
    os_timer_disarm(led_blink_timer);
    os_free(led_blink_timer);
    led_blink_timer = NULL;
  }
  if (esp_mesh_conn_timeout_wdt) {
    os_timer_disarm(esp_mesh_conn_timeout_wdt);
    os_free(esp_mesh_conn_timeout_wdt);
    esp_mesh_conn_timeout_wdt = NULL;
  }

  // Reset relevant variables
  mode == DISABLED;
  esp_mesh_enable_attempt_count = 1;

  // Turn off the status-LED (the state of the smart plug's power outlet isn't changed, so connected peripheral equipment
  // doesn't get damaged or shut down by accident)
  status_led_off();

  // Re-enable the interrupt so that the device is ready to be re-initialized via actuation of the pushbutton
  ETS_GPIO_INTR_DISABLE();  // Disable interrupts before changing the current configuration
  ETS_GPIO_INTR_ATTACH(button_pressed_interrupt_handler, GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO)); // Attach the corresponding function to be executed to the interrupt-pin
  gpio_pin_intr_state_set(GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO), GPIO_PIN_INTR_NEGEDGE); // Configure the interrupt-function to be executed on a negative edge
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(BUTTON_INTERRUPT_GPIO));  // Clear the interrupt-mask (otherwise, the interrupt will be masked because the interrupt-handler-funciton has already been executed)
  ETS_GPIO_INTR_ENABLE(); // Re-enable the interrupts
}

/*------------------------------------*/

// Timer- and interrupt-handler-functions:

// Interrupt-handler-function, that is called on the actuation of the pushbutton; store the start-time of the actuation
// and re-configure the interrupt to be triggered again on the button's release
static void ICACHE_FLASH_ATTR button_pressed_interrupt_handler(void *arg) {
  // Store the start-time of the actuation
  actuation_start_time = system_get_time();

  // Re-configure the interrupt to be triggered again on the button's release
  ETS_GPIO_INTR_DISABLE();  // Disable interrupts before changing the current configuration
  ETS_GPIO_INTR_ATTACH(button_released_interrupt_handler, GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO)); // Attach the corresponding function to be executed to the interrupt-pin
  gpio_pin_intr_state_set(GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO), GPIO_PIN_INTR_POSEDGE); // Configure the interrupt-function to be executed on a positive edge
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(BUTTON_INTERRUPT_GPIO));  // Clear the interrupt-mask (otherwise, the interrupt will be masked because the interrupt-handler-funciton has already been executed)
  ETS_GPIO_INTR_ENABLE(); // Re-enable the interrupts
}

// Interrupt-handler-function, that is called on the release of the pushbutton; set the node's operation-mode depending
// on the button's actuation-time and initialize the mesh-network
static void ICACHE_FLASH_ATTR button_released_interrupt_handler(void *arg) {
  // Determine, how long the button has been actuated
  uint32_t actuation_time = (system_get_time()-actuation_start_time)/1000;  // Divided by 1000 because system_get_time returns the current time in µs and not in ms

  // Set the node's operation-mode depending on the actuation-time; if the actuation-time is longer than the defined
  // threshold (cf. OPERATION_MODE_THRESHOLD), the operation-mode is set to root-node, otherwise it is set to sub-node
  if (actuation_time > OPERATION_MODE_THRESHOLD) {
    mode = ROOT_NODE;
    os_printf("button_released_interrupt_handler: Setting device's operation mode to root-node!\n");
  }
  else {
    mode = SUB_NODE;
    os_printf("button_released_interrupt_handler: Setting device's operation mode to sub-node!\n");
  }

  // Disable the interrupt (it is just meant to initialize the mesh-network if it's disabled, not to change
  // the operation-mode whilst already running since that might cause problems in the networks infrastructure)
  ETS_GPIO_INTR_DISABLE();

  // Try to initialize the mesh-node
  mesh_init();
}

// Timer-function to periodically check, if the connection to the router/parent-node (depending on the operation-mode) has
// been lost or if a timeout occured while trying to enable the mesh-node; restore the device's initial state in that case
static void ICACHE_FLASH_ATTR esp_mesh_conn_timeout_wdtfunc(void *arg) {
  if (espconn_mesh_get_status() == MESH_WIFI_CONN) {
    os_printf("esp_mesh_state_timerfunc: Connection got lost or a timeout while trying to enable the mesh-node occured!\n");
    espconn_mesh_disable((espconn_mesh_callback) esp_mesh_disable_cb); // Disable the mesh-node
  }
}

// Timer-function, to periodically check, if ESP-TOUCH is still running and enable the mesh-network if it has
// been successfully finished or reset the device in case it failed
static void ICACHE_FLASH_ATTR esptouch_over_timerfunc(os_timer_t *timer) {
  // Check, if ESP-TOUCH has been finished was successful
  if (!esptouch_is_running()) {
    // Disarm the timer and free the occupied resources
    if (timer) {
      os_timer_disarm(timer);
      os_free(timer);
    }

    if (esptouch_was_successful()) {
      // Arm the watchdog-timer to periodically check on possible connection-losses or timeouts whilst the node's enabling-process
      os_timer_disarm(esp_mesh_conn_timeout_wdt);
      os_timer_setfn(esp_mesh_conn_timeout_wdt, (os_timer_func_t *) esp_mesh_conn_timeout_wdtfunc, NULL);
      os_timer_arm(esp_mesh_conn_timeout_wdt, MESH_CONN_TIMEOUT_WDT_INTERVAL, true);

      // Initialize the mesh-network and register the corresponding callback-function
      // Switch MESH_LOCAL to MESH_SOFTAP if a soft-accesspoint-functionality is desired!
      espconn_mesh_enable(esp_mesh_enable_cb, MESH_LOCAL);
    }
    else {
      // Call esp_mesh_disable_cb to free all further occupied resources and restore the device's initial state
      espconn_mesh_disable((espconn_mesh_callback) esp_mesh_disable_cb);
    }
  }
}

// Timer-function, that toggles the status-LED
static void ICACHE_FLASH_ATTR led_blink_timerfunc(void *arg) {
  // Get the current state of the status-LED
  if (!(GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT(STATUS_LED_GPIO))) {
    // Set the output-value to high
    status_led_off();
  }
  else {
    // Set the output-value to low
    status_led_on();
  }
}

/*------------------------------------*/

// GPIO control:

// Switch the status-LED on and set it the corresponding pin to output-mode
static void ICACHE_FLASH_ATTR status_led_on(void) {
  gpio_output_set(0, BIT(STATUS_LED_GPIO), BIT(STATUS_LED_GPIO), 0);
}

// Switch the status-LED off and set it the corresponding pin to output-mode
static void ICACHE_FLASH_ATTR status_led_off(void) {
  gpio_output_set(BIT(STATUS_LED_GPIO), 0, BIT(STATUS_LED_GPIO), 0);
}

// Turn the smart plug's output power and the red LED on
void ICACHE_FLASH_ATTR output_power_on(void) {
  gpio_output_set(BIT(OUTPUT_POWER_RELAY_GPIO), 0, BIT(OUTPUT_POWER_RELAY_GPIO), 0);
}

// Turn the smart plug's output power and the red LED off
void ICACHE_FLASH_ATTR output_power_off(void) {
  gpio_output_set(0, BIT(OUTPUT_POWER_RELAY_GPIO), BIT(OUTPUT_POWER_RELAY_GPIO), 0);
}

/*------------------------------------*/

// Initialization and configuration:

// Start ESP-TOUCH and enable the mesh-node in root-mode
static bool ICACHE_FLASH_ATTR root_init(void) {
  if (!station_conf) {
    os_printf("sub_init: Please initialize station_conf by calling mesh_init first!\n");
    return false;
  }

  // Start the timer to toggle the status-LED (long blinking-interval in root-mode)
  if (led_blink_timer) {
    os_timer_disarm(led_blink_timer);
    os_timer_setfn(led_blink_timer, (os_timer_func_t *) led_blink_timerfunc, NULL);
    os_timer_arm(led_blink_timer, LED_BLINK_INTERVAL_LONG, true);
  }

  // Enable the mesh-network after ESP-TOUCH finished
  // The mesh-network (STATIONAP_MODE) must not been enabled while ESP-TOUCH is running (STATION_MODE)!
  // So wait for it to finish and then execute espconn_mesh_enable afterwards.
  os_timer_t *esptouch_wait_timer = (os_timer_t *) os_zalloc(sizeof(os_timer_t));
  if (!esptouch_wait_timer) {
    os_printf("root_init: Failed to initialize esptouch_wait_timer! Aborting!\n");
    return false;
  }
  os_timer_disarm(esptouch_wait_timer);
  os_timer_setfn(esptouch_wait_timer, (os_timer_func_t *) esptouch_over_timerfunc, esptouch_wait_timer); // Assign the timer-function
  os_timer_arm(esptouch_wait_timer, 500, true);  // Arm the timer; check on ESP-TOUCH once every 500ms

  // Start ESP-TOUCH
  esptouch_init();

  return true;
}

// Try to receive the WiFi-configuration from the other nodes and enable the mesh-device in sub-mode
static bool ICACHE_FLASH_ATTR sub_init(void) {
  if (!station_conf) {
    os_printf("sub_init: Please initialzie station_conf by calling mesh_init first!\n");
    return false;
  }

  // Set the WiFi-operation-mode to STATIONAP_MODE
  wifi_set_opmode(STATIONAP_MODE);

  // Try to receive the router-information from the other nodes, set the obtained station-configuration as the own configuration and
  // enable the mesh-device
  if (espconn_mesh_get_router(station_conf)) {  // Try to get the router-information
    if (station_conf->ssid[0] == 0xff || station_conf->ssid[0] == 0x00) { // Check, if the received station-configuration is valid
      const char *SSID = "EMRA_Prototype_Network";  // Router-SSID
      const char *PASSWD = "EnergyMeterReadoutAutomation"; // Router-password
      os_memcpy(station_conf->ssid, SSID, os_strlen(SSID));
      os_memcpy(station_conf->password, PASSWD, os_strlen(PASSWD));
    }
    if (espconn_mesh_set_router(station_conf)) { // Set the obtained station-configuration as the mesh-router-configuration
      // Start the timer to toggle the status-LED (short blinking-interval in root-mode)
      if (led_blink_timer) {
        os_timer_disarm(led_blink_timer);
        os_timer_setfn(led_blink_timer, (os_timer_func_t *) led_blink_timerfunc, NULL);
        os_timer_arm(led_blink_timer, LED_BLINK_INTERVAL_SHORT, true);
      }

      // Arm the watchdog-timer to periodically check on possible connection-losses or timeouts whilst the node's enabling-process
      os_timer_disarm(esp_mesh_conn_timeout_wdt);
      os_timer_setfn(esp_mesh_conn_timeout_wdt, (os_timer_func_t *) esp_mesh_conn_timeout_wdtfunc, NULL);
      os_timer_arm(esp_mesh_conn_timeout_wdt, MESH_CONN_TIMEOUT_WDT_INTERVAL, true);

      // Enable the mesh-node
      espconn_mesh_enable(esp_mesh_enable_cb, MESH_ONLINE);

      return true;
    }
    else {
      os_printf("sub_init: Error while setting mesh-router!\n");
    }
  }
  else {
    os_printf("sub_init: Failed to get mesh-router-information!\n");
  }
  return false;
}

// Try to receive the WiFi-configuration depending on the device's operation-mode and enable the mesh-node
static bool ICACHE_FLASH_ATTR mesh_init(void) {
  // Initialize the station-configuration and reset the currently set mesh-router-configuration (may cause unwanted behaviour otherwise)
  if (!station_conf) {
    station_conf = (struct station_config *) os_zalloc(sizeof(struct station_config));
    if (!station_conf || !espconn_mesh_set_router(station_conf)) {
      os_printf("mesh_init: Failed to initialize station_conf or to reset the current mesh-router-configuration!\n");
      return false;
    }
  }

  // Initialize the timer to toggle the status-LED while the connection and enabling of the mesh-device are in progress
  if (!led_blink_timer) {
    led_blink_timer = (os_timer_t *) os_zalloc(sizeof(os_timer_t));
    if (!led_blink_timer) { // Won't cause the program to abort since this only affects the status-LED
      os_printf("mesh_init: Failed to initialize led_blink_timer! Continuing without!\n");
    }
  }

  // Initialize the watchdog-timer to continually check, if the connection to the router/parent-node (depending on the operation-mode)
  // has been lost or if a timeout occured while trying to enable the mesh-node
  if (!esp_mesh_conn_timeout_wdt) {
    esp_mesh_conn_timeout_wdt = (os_timer_t *) os_zalloc(sizeof(os_timer_t));
  }
  if (esp_mesh_conn_timeout_wdt) {
    // Try to connect to a router via ESP-TOUCH in case the device's operation-mode is ROOT_NODE, thus initializing a new mesh-network
    if (mode == ROOT_NODE) {
      if (root_init()) {
        return true;
      }
      else {
        os_printf("mesh_init: Failed to initialize the mesh-node in root-mode!\n");
      }
    }
    // If the device's operation-mode is SUB_NODE on the other hand (thus trying to connect to an already existing mesh-network), try to
    // receive the router-information from the other mesh-nodes
    else if (mode == SUB_NODE) {
      if (sub_init()) {
        return true;
      }
      else {
        os_printf("mesh_init: Failed to initialize the mesh-node in sub-mode!\n");
      }
    }
    else {
      os_printf("mesh_init: Wrong operation-mode!\n");
    }
  }
  else {
    os_printf("mesh_init: Failed to initialize esp_mesh_conn_timeout_wdt!\n");
  }
  // Call esp_mesh_disable_cb to free all occupied resources and restore the device's initial state
  espconn_mesh_disable((espconn_mesh_callback) esp_mesh_disable_cb);
  return false;
}

// Configure the node's setting concerning the mesh-network
static bool ICACHE_FLASH_ATTR esp_mesh_config(void) {
  // Print the mesh's version
  espconn_mesh_print_ver();

  // Set the node's authentication credentials (authentication mode (WPA, WPA2, etc.) and password for each mesh-node)
  if (!espconn_mesh_encrypt_init(MESH_AUTH_MODE, MESH_AUTH_PASSWD, os_strlen(MESH_AUTH_PASSWD))) {
    os_printf("esp_mesh_config: Failed to set the node's authentication credentials!\n");
    return false;
  }

  // Set the maximum number of hops possible (meaning how many mesh-layers a message can traverse)
  if (system_get_free_heap_size() > (4^MAX_HOPS-1)/3*6) { // Check, if the remaining free heap is sufficient
    if (!espconn_mesh_set_max_hops(MAX_HOPS)) {
      os_printf("esp_mesh_config: Failed to set MAX_HOPS! Set maximum number of traversable mesh-layers to %d instead!\n", espconn_mesh_get_max_hops());
      return false;
    }
  }
  else {
    os_printf("esp_mesh_config: Not enough free heap! Please reduce MAX_HOPS!\n");
    return false;
  }

  // Set the SSID-prefix (represents the mesh-network together with the mesh-group-ID)
  if (!espconn_mesh_set_ssid_prefix(SSID_PREFIX, os_strlen(SSID_PREFIX))) {
    os_printf("esp_mesh_config: Failed to set SSID-prefix!\n");
    return false;
  }

  // Set the mesh-group-ID (represents the mesh-network together with the SSID-prefix); see user_config for a more detailed explanation
  if (!espconn_mesh_group_id_init((uint8_t *) group_id, sizeof(group_id))) {
    os_printf("esp_mesh_config: Failed to set the mesh-group-ID!\n");
    return false;
  }

  // Register further callback-functions
  espconn_mesh_regist_usr_cb(esp_mesh_node_join_cb);  // Called, when a new sub-node joins the network
  espconn_mesh_regist_rebuild_fail_cb(esp_mesh_rebuild_fail_cb); // Called, if the rebuild of mesh fails

  return true;
}

// Initialize the GPIO-pins to function as intended
static void ICACHE_FLASH_ATTR gpio_pins_init(void) {
  os_printf("gpio_pins_init: Initializing GPIO-pins!\n");

  // Initialize the GPIO-subsystem
  gpio_init();

  // Set the defined pins' operation-mode to GPIO
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);

  // Enable the pull-up resistor of the status-LED's GPIO-pin (since the LED is connected in reverse logic); that's
  // also why status_led_on "disables" the pin in order to turn on the light (resp. the other way around for status_led_off)
  PIN_PULLUP_EN(PERIPHS_IO_MUX_MTCK_U);

  // Set the status-LED's GPIO-pin to output-mode and deactivate it
  status_led_off();

  // Set the output-power-relay's GPIO-pin to output-mode and energize it by default, so that the outlet, which the
  // smart plug is connected to, isn't blocked and can still be used as long as the relay isn't turned off per command
  // (cf. mesh_bin.c)
  output_power_on();

  // Set the pushbutton's GPIO-pin to input-mode
  gpio_output_set(0, 0, 0, BIT(BUTTON_INTERRUPT_GPIO));

  // Initialize the pushbutton-pin to function as an interrupt
  ETS_GPIO_INTR_DISABLE();  // Disable interrupts before changing the current configuration
  ETS_GPIO_INTR_ATTACH(button_pressed_interrupt_handler, GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO)); // Attach the corresponding function to be executed to the interrupt-pin
  gpio_pin_intr_state_set(GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO), GPIO_PIN_INTR_NEGEDGE); // Configure the interrupt-function to be executed on a negative edge
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(BUTTON_INTERRUPT_GPIO));  // Clear the interrupt-mask (otherwise, the interrupt might be masked because of random initialization-values in the corresponding interrupt-register)
  ETS_GPIO_INTR_ENABLE(); // Re-enable the interrupts
}

// Entry point in the program; start the initialization-process
void user_init(void) {
  os_printf("user_init: Starting the initialization-process!\n");

  // Clear possible connections and set the operation-mode to NULL_MODE
  wifi_station_disconnect();
  wifi_set_opmode(NULL_MODE);

  // Initialize the GPIO-pins
  gpio_pins_init();

  // Configure the mesh-device before trying to enable the node
  if (!esp_mesh_config()) {
    os_printf("user_init: Error while configuring the mesh-device! Aborting!\n");
    return;
  }
}

/*------------------------------------*/

// Radio frequency configuration:

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABBBCDDD
 *                A : rf cal
 *                B : at parameters
 *                C : rf init data
 *                D : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void) {
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
      case FLASH_SIZE_4M_MAP_256_256:
        rf_cal_sec = 128 - 8;
        break;

      case FLASH_SIZE_8M_MAP_512_512:
        rf_cal_sec = 256 - 5;
        break;

      case FLASH_SIZE_16M_MAP_512_512:
      case FLASH_SIZE_16M_MAP_1024_1024:
        rf_cal_sec = 512 - 5;
        break;

      case FLASH_SIZE_32M_MAP_512_512:
      case FLASH_SIZE_32M_MAP_1024_1024:
        rf_cal_sec = 1024 - 5;
        break;

      default:
        rf_cal_sec = 0;
        break;
    }

    return rf_cal_sec;
}

void ICACHE_FLASH_ATTR user_rf_pre_init(void) {
  // Nothing to do...
}
