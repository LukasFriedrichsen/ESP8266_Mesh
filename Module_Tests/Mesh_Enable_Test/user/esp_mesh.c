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
#include "esp_touch.h"
#include "esp_mesh.h"
#include "user_config.h"

/*------------------------------------*/

// Definition of functions (so there won't be any complications because the
// compiler resolves the scope top-down):

// Callback-functions:
static void esp_mesh_node_join_cb(void *mac);
static void esp_mesh_rebuild_fail_cb(void *arg);
static void esp_mesh_enable_cb(int8_t result);
static void esp_mesh_disable_cb(void);

// Timer-functions:
static void esp_mesh_conn_timeout_wdtfunc(void *arg);
static void esptouch_over_timerfunc(os_timer_t *timer);

// Initialization and configuration:
static bool dev_init(void);
static bool mesh_init(void);
static bool esp_mesh_config(void);

void user_init(void);

// Radio frequency configuration
uint32 user_rf_cal_sector_set(void);
void user_rf_pre_init(void);

/*------------------------------------*/

// Declaration and initialization of variables:

const static uint8_t group_id[] = GROUP_ID;  // Local copy of GROUP_ID

static struct station_config *station_conf = NULL;

struct espconn *esp_mesh_conn = NULL;  // Socket for connection and communication with other mesh-nodes and devices in the network

static uint8_t esp_mesh_enable_attempt_count = 1;

static os_timer_t *esp_mesh_conn_timeout_wdt = NULL;

/*------------------------------------*/

// Callback-functions:

// Callback-function, that notifies, if a new sub-node joins the mesh-network
static void ICACHE_FLASH_ATTR esp_mesh_node_join_cb(void *mac) {
  if (!mac) {
    os_printf("esp_mesh_node_join_cb: Invalid transfer parameter!\n");
    return;
  }

  os_printf("esp_mesh_node_join_cb: New sub-node joined: " MACSTR "\n", MAC2STR((uint8_t *) mac));
  os_printf("Currently connected: %d\n", wifi_softap_get_station_num());
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
      if (espconn_mesh_is_root()) {
        espconn_mesh_enable(esp_mesh_enable_cb, MESH_LOCAL);
      }
      else {
        espconn_mesh_enable(esp_mesh_enable_cb, MESH_ONLINE);
      }
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

    os_printf("Currently connected: %d\n", wifi_softap_get_station_num());

    if (espconn_mesh_is_root()) {
      os_printf("esp_mesh_enable_cb: Root!\n");
    }
  }
}

// Callback-function, that is executed, if the mesh-network is disabled; restores the initial state of the program and restart it
static void ICACHE_FLASH_ATTR esp_mesh_disable_cb(void) {
  // Clear possible connections and set the operation-mode to NULL_MODE
  wifi_station_disconnect();

  // Free occupied resources
  if (station_conf) {
    os_free(station_conf);
    station_conf = NULL;
  }
  if (esp_mesh_conn) {
    os_free(esp_mesh_conn);
    esp_mesh_conn = NULL;
  }
  if (esp_mesh_conn_timeout_wdt) {
    os_timer_disarm(esp_mesh_conn_timeout_wdt);
    os_free(esp_mesh_conn_timeout_wdt);
    esp_mesh_conn_timeout_wdt = NULL;
  }

  // Reset relevant variables
  esp_mesh_enable_attempt_count = 1;

  // Try to restart the program
  user_init();
}

/*------------------------------------*/

// Timer-functions:

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
      // Call esp_mesh_disable_cb to free all occupied resources and restore the device's initial state
      espconn_mesh_disable((espconn_mesh_callback) esp_mesh_disable_cb);
    }
  }
}

/*------------------------------------*/

// Initialization and configuration:

// Try to initialize the mesh-device
static bool ICACHE_FLASH_ATTR dev_init(void) {
  if (!station_conf) {
    os_printf("sub_init: Please initialize station_conf by calling mesh_init first!\n");
    return false;
  }

  os_memset(station_conf, 0, sizeof(struct station_config));
  espconn_mesh_get_router(station_conf);
  if (station_conf->ssid[0] == 0xff || station_conf->ssid[0] == 0x00) {
    os_memcpy(station_conf->ssid, SSID, os_strlen(SSID));
    os_memcpy(station_conf->password, PASSWD, os_strlen(PASSWD));
  }
  os_printf("dev_init: SSID: %s\nPASSWORD: %s", station_conf->ssid, station_conf->password);

  if (!espconn_mesh_set_router(station_conf)) {
    os_printf("dev_init: Failed to set mesh-router-configuration!");
    return false;
  }
  espconn_mesh_enable(esp_mesh_enable_cb, MESH_ONLINE);

  return true;
}


// Try to enable the mesh-node
static bool ICACHE_FLASH_ATTR mesh_init(void) {
  // Initialize the station-configuration and reset the currently set mesh-router-configuration (may cause unwanted behaviour otherwise)
  if (!station_conf) {
    station_conf = (struct station_config *) os_zalloc(sizeof(struct station_config));
    if (!station_conf || !espconn_mesh_set_router(station_conf)) {
      os_printf("mesh_init: Failed to initialize station_conf or to reset the current mesh-router-configuration!\n");
      return false;
    }
  }

  // Initialize the watchdog-timer to continually check, if the connection to the router/parent-node (depending on the operation-mode)
  // has been lost or if a timeout occured while trying to enable the mesh-node
  if (!esp_mesh_conn_timeout_wdt) {
    esp_mesh_conn_timeout_wdt = (os_timer_t *) os_zalloc(sizeof(os_timer_t));
  }
  if (esp_mesh_conn_timeout_wdt) {
    // Try to initialize the mesh-device
    if (dev_init()) {
      return true;
    }
    else {
      os_printf("mesh_init: Error while initializing the mesh-node!\n");
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

// Entry point in the program; start the initialization-process
void user_init(void) {
  os_printf("user_init: Starting the initialization-process!\n");

  // Configure the mesh-device before trying to enable the node
  if (!esp_mesh_config()) {
    os_printf("user_init: Error while configuring the mesh-device! Aborting!\n");
    return;
  }

  // Initialize the mesh-node
  mesh_init();
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
