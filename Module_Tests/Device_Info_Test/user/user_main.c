// user_main.c
// Copyright 2017 Lukas Friedrichsen
// License: Modified BSD-License
//
// 2017-02-28

#include "mem.h"
#include "osapi.h"
#include "espconn.h"
#include "user_interface.h"
#include "device_info.h"
#include "user_config.h"

// Initialize local copies of SSID and PASSWD
// NO CONNECTION TO THE ACCESS-POINT WILL BE POSSIBLE OTHERWISE!
const char ssid[32] = SSID;
const char passwd[64] = PASSWD;

struct espconn* udp_socket = NULL;

// Wifi-event-callback; prints the current connection-status and initializes
// the UDP-socket on a successfully established connection
void ICACHE_FLASH_ATTR wifi_event_cb(System_Event_t *evt) {
  os_printf("WiFi-event: %x\n", evt->event);
  switch (evt->event) {
    case EVENT_STAMODE_GOT_IP:
      os_printf("Got IP-address!\n");
      device_info_init();
      vital_sign_bcast_start();
      break;
    case EVENT_STAMODE_CONNECTED:
      os_printf("Connected!\n");
      break;
    default:
      break;
  }
}

// Initialize WiFi-station-mode and go into sleep-mode until a connection is
// established; periodically (once per second) retries if no connection is
// established yet
void ICACHE_FLASH_ATTR wifi_init() {
  // Clear possible connections before trying to set up a new connection
  wifi_station_disconnect();

  // Set up station-configuration
  struct station_config station_conf;
  os_memcpy(&station_conf.ssid, ssid, 32);
  os_memcpy(&station_conf.password, passwd, 64);
  station_conf.bssid_set = 0; // No need to check for the MAC-adress of the AP here

  // Set station-mode, load station-configuration and configure reconnect-policy
  // Restart the system, if one of the above fails!
  if (!wifi_set_opmode(STATION_MODE) || !wifi_station_set_config_current(&station_conf) || !wifi_station_set_auto_connect(true) || !wifi_station_set_reconnect_policy(true)) {
    os_printf("Error while initializing station-mode! Rebooting...\n");
    system_restart();
  }

  // Set wifi-event-callback
  wifi_set_event_handler_cb(wifi_event_cb);

  // Sleep until a wifi-event occures
  wifi_set_sleep_type(MODEM_SLEEP_T);
}

// Initializiation
void ICACHE_FLASH_ATTR user_init() {
  os_printf("Initializing...\n");

  // Initializing WiFi
  wifi_init();
}

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
