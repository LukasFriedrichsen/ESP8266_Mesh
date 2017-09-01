// mesh_parser.c
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-02
//
// Description: This class provides the possibility to execute a - at will
// periodical - topology-test (can be imagined as a "broadcast-ping" to determine
// the root-device as well as all currently connected nodes). Thus, node-to-node
// communication becomes possible for non-root-nodes as well and debugging is
// simplified. Furthermore, this class implements the corresponding handler-
// function to process the response to the topology-test from the root-node and
// which is to be registered at the parser.
//
// This class is based on https://github.com/espressif/ESP8266_MESH_DEMO/tree/master/mesh_performance/scenario/mesh_none.c

#include "mem.h"
#include "osapi.h"
#include "mesh.h"
#include "mesh_device.h"
#include "esp_mesh.h"
#include "mesh_none.h"
#include "user_config.h"

static os_timer_t *topology_timer = NULL;

// Handler-function to process the connected devices' responses to the topology-
// test; all registered nodes whose timestamp exceeds the defined timeout-
// threshold and who didn't respond to the topology-test are deleted from the
// device-list, all registered and responding nodes' timestamp is updated and
// all not yet registered nodes are newly added
void ICACHE_FLASH_ATTR mesh_parser_protocol_none(const void *mesh_header, uint8_t *data, uint16_t len) {
  if (!mesh_header || !data || len <= 0) {
    os_printf("mesh_parser_protocol_none: Invalid transfer parameters!\n");
    return;
  }

  uint16_t op_idx = 1, dev_count = 0, dev_idx = 0;
  uint8_t *dev_mac = NULL;
  struct mesh_header_option_format *option = NULL;
  static const struct mesh_device_node_type *node_list_list_cpy;
  struct mesh_header_format *header = (struct mesh_header_format *) data; // Interprete data as a packet in the mesh-header-format

  // Check, if the message received happens to be a response to the topology-
  // request
  if (espconn_mesh_get_option(header, M_O_TOPO_RESP, op_idx, &option)) {
    // Set the root-device to the received message's source-address (since only
    // the current root answers a topology-request)
    if (mesh_device_root_set((struct mesh_device_mac_type *) header->src_addr)) {
      mesh_device_update_timestamp((struct mesh_device_mac_type *) header->src_addr, 1);
    }
    else {
      os_printf("mesh_parser_protocol_none: Failed to set the root-device!\n");
    }

    // Extract the MAC-addresses of the current mesh-nodes from the messages
    // options-field (the corresponding key is M_O_TOPO_RESP) add not yet
    // registered nodes to the device-list and update the device's timestamps
    while (espconn_mesh_get_option(header, M_O_TOPO_RESP, op_idx++, &option)) {
      dev_count = option->olen/sizeof(struct mesh_device_mac_type);
      dev_mac = option->ovalue;
      if (mesh_device_add((struct mesh_device_mac_type *) dev_mac, dev_count)) {  // Redundant (already registered) nodes are caught in mesh_device_add(...), so no reason to worry about this here
        mesh_device_update_timestamp((struct mesh_device_mac_type *) dev_mac, dev_count);
      }
      else {
        os_printf("mesh_parser_protocol_none: Failed to add new sub-nodes!\n");
      }
    }

    // Get the all currently registered nodes
    mesh_device_list_get(&node_list_list_cpy, &dev_idx);

    // Delete all nodes whose timestamp exceeds the defined timeout-threshold
    // from the list
    if (node_list_list_cpy) {
      while (dev_idx-- > 0) {
        if ((system_get_time()-node_list_list_cpy[dev_idx].timestamp)/1000 > SUB_NODE_TIMEOUT_THRESHOLD) {  // Has to be divided by 1000 because timestamp and the systemtime are given in microseconds and not in milliseconds
          mesh_device_del((struct mesh_device_mac_type *) &node_list_list_cpy[dev_idx].mac_addr, 1);
        }
      }
    }

    // Display all currently registered nodes
    mesh_device_list_disp();
  }
}

// This function initiates a test of the mesh-network's topology. The concrete
// process of this topology-test is differs based on the device's role in the
// network. Whilst a root-node can directly call up it's sub-nodes, a non-root-
// device has to broadcast a topology-request to all connected devices,
// which is then answered by the root-node (broadcast because the sub-node
// doesn't know it's current root-device).
static bool ICACHE_FLASH_ATTR mesh_topology_test(void) {
  if (!esp_mesh_conn) {
    os_printf("mesh_topology_test: Please initialzie esp_mesh_conn before trying to execute a topology-test!\n");
    return false;
  }
  uint8_t mesh_status = espconn_mesh_get_status();
  if (!(mesh_status == MESH_LOCAL_AVAIL || mesh_status == MESH_ONLINE_AVAIL || mesh_status == MESH_SOFTAP_AVAIL || mesh_status == MESH_LEAF_AVAIL)) {  // Only execute a topology-test, if the mesh-device is enabled and not in (re-)connection-process
    return false;
  }

  uint8_t op_mode = 0;
  struct mesh_device_mac_type src, dst;
  struct mesh_header_format *header = NULL;
  struct mesh_header_option_format *option = NULL;
  uint8_t ot_len = sizeof(struct mesh_header_option_header_type) + sizeof(struct mesh_header_option_format) + sizeof(struct mesh_device_mac_type);

  // If the device is the mesh-network's root-node, it can directly call up it's
  // sub-nodes, so a topology-request via a broadcast isn't necessary.
  if (espconn_mesh_is_root()) {
    uint16_t sub_dev_count = 0, dev_idx = 0;
    struct mesh_device_mac_type *sub_dev_mac = NULL;
    static const struct mesh_device_node_type *node_list_list_cpy;

    // Obtain the root-device's sub-node's MAC-addresses
    if (espconn_mesh_get_node_info(MESH_NODE_ALL, (uint8_t **) &sub_dev_mac, &sub_dev_count)) {
      if (sub_dev_count >= 1) {
        // The first entry is the router's (= "the root-node's root") MAC-address
        if (mesh_device_root_set(sub_dev_mac)) {
          mesh_device_update_timestamp(sub_dev_mac, 1);
        }
        else {
          os_printf("mesh_topology_test: Failed to set the root-device!\n");
        }
        if (sub_dev_count > 1) {
          // Add the sub-nodes to the list of registered devices and update their
          // timestamp
          if (mesh_device_add(sub_dev_mac+1, sub_dev_count-1)) {
            mesh_device_update_timestamp(sub_dev_mac+1, sub_dev_count-1);
          }
          else {
            os_printf("mesh_topology_test: Failed to add new sub-nodes!\n");
          }
        }
      }

      // Get the all currently registered nodes
      mesh_device_list_get(&node_list_list_cpy, &dev_idx);

      // Delete all nodes whose timestamp exceeds the defined timeout-threshold
      // from the list
      if (node_list_list_cpy) {
        while (dev_idx-- > 0) {
          if ((system_get_time()-node_list_list_cpy[dev_idx].timestamp)/1000 > SUB_NODE_TIMEOUT_THRESHOLD) {  // Has to be divided by 1000 because timestamp and the systemtime are given in microseconds and not in milliseconds
            mesh_device_del((struct mesh_device_mac_type *) &node_list_list_cpy[dev_idx].mac_addr, 1);
          }
        }
      }

      // Display all currently registered nodes
      mesh_device_list_disp();

      // Release the memory occupied by the MAC-addresses
      espconn_mesh_get_node_info(MESH_NODE_ALL, NULL, NULL);

      return true;
    }
    else {
      os_printf("mesh_topology_test: Failed to obtain the root-device's sub-nodes!\n");
    }
  }
  else {
    if (!esp_mesh_conn) {
      os_printf("mesh_topology_test: Please initialize esp_mesh_conn before executing a topology-test!\n");
      return false;
    }

    // Check for the operation-mode of the device and get the respective MAC-address
    op_mode = wifi_get_opmode();
    if (op_mode == SOFTAP_MODE || op_mode == STATION_MODE || op_mode == STATIONAP_MODE) { // Prevent errors resulting from runtime-conditions concerning the WiFi-operation-mode (e.g. if the device is switched into sleep-mode)
      if (op_mode == SOFTAP_MODE) {
        wifi_get_macaddr(SOFTAP_IF, src.mac);
      }
      else {
        wifi_get_macaddr(STATION_IF, src.mac);
      }
    }
    else {
      os_printf("mesh_topology_test: Wrong WiFi-operation-mode!\n");
      return false;
    }

    // Since the root-node isn't known yet, one has to broadcast the topology-
    // request to all connected devices
    os_memset(&dst, 0, sizeof(struct mesh_device_mac_type));  // Set broadcast-address as destination (the MAC-addresses are used for the communication between mesh-nodes instead of an IP-address)

    // Initialize the topology-request
    header = (struct mesh_header_format *) espconn_mesh_create_packet(dst.mac,      // Destination address
                                                                      src.mac,      // Source address
                                                                      false,        // P2P flag
                                                                      true,         // Flow request flag (if set to true, the request for a permit to send data to avoid network congestion (cf. Isarithmetic Congestion Control) will be piggybacked onto the message)
                                                                      M_PROTO_NONE, // Communication-protocol
                                                                      0,            // Data length
                                                                      true,         // Option flag
                                                                      ot_len,       // Total option length
                                                                      false,        // Fragmentation flag (allow fragmentation)
                                                                      0,            // Fragmentation type (options for the fragment)
                                                                      false,        // More fragmentation flag (indicates, if this fragment is the last of a package or if more fragments are following)
                                                                      0,            // Fragmentation index/offset (postion of the fragment's data in relation to the first byte of the package)
                                                                      0);           // Fragmentation id (identity of the frame; espacially important in a mesh-network since the different fragments might take different paths to reach the target)

    if (header) {
      // Create the topology-request-option
      option = (struct mesh_header_option_format *) espconn_mesh_create_option(M_O_TOPO_REQ, dst.mac, sizeof(struct mesh_device_mac_type));
      if (option) {
        // Add the topology-request-option to the package
        if (espconn_mesh_add_option(header, option)) {
          // Try to broadcast the package to all other mesh-nodes
          if (!espconn_mesh_sent(esp_mesh_conn, (uint8_t *) header, header->len)) {
            // Free occupied resouces
            os_free(header);
            os_free(option);
            return true;
          }
          else {
            os_printf("mesh_topology_test: Error while sending the topology-request-package!\n");
          }
        }
        else {
          os_printf("mesh_topology_test: Failed to add the topology-request-option to the package!\n");
        }
      }
      else {
        os_printf("mesh_topology_test: Creation of the topology-request-option failed!\n");
      }
    }
    else {
      os_printf("mesh_topology_test: Creating the topology-request-package failed!\n");
    }
    // Free occupied resouces
    if (header) {
      os_free(header);
    }
    if (option) {
      os_free(option);
    }
    return false;
  }
}

// Disable the periodical topology-tests and free the occupied resouces
void ICACHE_FLASH_ATTR mesh_topology_disable(void) {
  os_printf("mesh_com_disable: Disabling periodical topology-tests!\n");

  if (topology_timer) {
    os_timer_disarm(topology_timer);  // Disarm the timer for the periodical topology-tests
    os_free(topology_timer);  // Free the occupied resources
    topology_timer = NULL;
  }

  mesh_device_list_release(); // Release the device-list to free the occupied resources
}

// Initialize a periodical topology-test
void ICACHE_FLASH_ATTR mesh_topology_init(void) {
  if (!esp_mesh_conn) {
    os_printf("mesh_topology_init: Please initialzie esp_mesh_conn first!\n");
    return;
  }

  os_printf("mesh_com_init: Initializing periodical topology-tests!\n");

  // Initialize the timer
  if (!topology_timer) {
    topology_timer = (os_timer_t *) os_zalloc(sizeof(os_timer_t));
  }
  if (!topology_timer) {
    os_printf("mesh_topology_init: Failed to initialize the timer for the periodical topology-tests!\n");
    return;
  }

  // Initialize the device-list
  mesh_device_list_init();

  // Initialize the timer and assign the function to test the mesh's topology
  os_timer_disarm(topology_timer);
  os_timer_setfn(topology_timer, (os_timer_func_t *) mesh_topology_test, NULL);
  os_timer_arm(topology_timer, TOPOLOGY_TIME_INTERVAL, true);
}
