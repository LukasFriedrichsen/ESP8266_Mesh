// mesh_device.c
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-04-27
//
// Description: This class provides functions to administrate the mesh-nodes
// registered at a certain node.
//
// This class is based on https://github.com/espressif/ESP8266_MESH_DEMO/tree/master/mesh_performance/scenario/mesh_device.c

#include "mem.h"
#include "osapi.h"
#include "mesh.h"
#include "mesh_device.h"
#include "user_config.h"

static struct mesh_device_list_type *node_list = NULL;

// The following can be used to create copies of node_list->list and
// node_list->root, which can then be returned by the get-functions, and
// therefore to create a real safe coupling/read-only towards the outside (since
// "const" can be tricked quite easily by explicit typecasting). Doing so should
// be considered if you have enough memory space. Since that isn't the case
// everytime, the standard-implementation of the get-methods "just" returns the
// actual references of node_list->root and node_list->list as "const", which is,
// of course, much more memory saving and microcontroller-friendly. But therefore
// it is possible to manipulate the "working"-data from the outside, so remember:
// WARNING: IF YOU MANIPULATE THE ACTUAL DATA OF NODE_LIST->LIST OR
// NODE_LIST->ROOT, THE MESH-FUNCTIONALITY MIGHT NO LONGER BE GIVEN!
/* static struct mesh_device_node_type *node_list_list_cpy = NULL;  // Serves as a copy of node_list->list to ensure that nobody
 *                                                                  // can manipulate node_list->list from the outside; also
 *                                                                  // prevents problems when calling the list from the outside
 *                                                                  // after it was freed internally
 *                                                                  // (e.g. if node_list->entries_count <= 1)
 * static struct mesh_device_node_type *node_list_root_cpy = NULL;  // Same reasoning as for node_list_list_cpy (just for
 *                                                                  // node_list->root instead of node_list->list)
 */

// Initialize the list containing the currently registered nodes
void ICACHE_FLASH_ATTR mesh_device_list_init(void) {
  if (!node_list) {
    node_list = (struct mesh_device_list_type *) os_zalloc(sizeof(struct mesh_device_list_type));
  }
}

// Free node_list->list and set node_list to zero
void ICACHE_FLASH_ATTR mesh_device_list_release(void) {
  if (node_list) {
    if (node_list->list) {
      os_free(node_list->list);
      node_list->list = NULL;
    }
    os_memset(node_list, 0, sizeof(struct mesh_device_list_type));
  }
}

// Print all registered nodes' MAC-adress to the serial port
void ICACHE_FLASH_ATTR mesh_device_list_disp(void) {
  if (!node_list) {
    os_printf("mesh_device_list_disp: Please initialize node_list before trying to access it!\n");
    return;
  }
  if (node_list->entries_count <= 0) {
    os_printf("mesh_device_list_disp: List is empty!\n");
    return;
  }

  uint16_t idx = 0;

  os_printf("/*---------- registered nodes ----------*/\n");
  os_printf("(Root) MAC:      " MACSTR "\n", MAC2STR(node_list->root.mac_addr.mac));
  for (idx = 0; idx < node_list->entries_count-1; idx++) {
    if (idx < 10) {
      os_printf("(Index: %d) MAC:  " MACSTR "\n", idx, MAC2STR(node_list->list[idx].mac_addr.mac));
    }
    else {
      os_printf("(Index: %d) MAC: " MACSTR "\n", idx, MAC2STR(node_list->list[idx].mac_addr.mac));
    }
  }
  os_printf("/*-------------- list end --------------*/\n");
}

// Search the currently registered nodes for the given MAC-adress; returns true
// if the list contains it and false if not
bool ICACHE_FLASH_ATTR mesh_device_list_search(struct mesh_device_mac_type *node) {
  if (!node) {
    os_printf("mesh_device_list_search: Invalid transfer parameter!\n");
    return false;
  }
  if (!node_list) {
    os_printf("mesh_device_list_search: Please initialize node_list before trying to access it!\n");
    return false;
  }
  if (node_list->entries_count <= 0) {
    os_printf("mesh_device_list_search: List is empty!\n");
    return false;
  }

  uint16_t idx = 0;

  if (os_memcmp(&node_list->root.mac_addr, node, sizeof(struct mesh_device_mac_type)) == 0) { // Check the root-device
    return true;
  }
  for (idx = 0; idx < node_list->entries_count-1; idx++) {
    if (os_memcmp(&node_list->list[idx].mac_addr, node, sizeof(struct mesh_device_mac_type)) == 0){  // Check every currently registered node
      return true;
    }
  }
  return false;
}

// Return the currently registered nodes as well as their number
bool ICACHE_FLASH_ATTR mesh_device_list_get(const struct mesh_device_node_type **nodes, uint16_t *count) {
  if (!nodes || !count) {
    os_printf("mesh_device_list_get: Invalid transfer parameters!\n");
    return false;
  }
  if (!node_list) {
    os_printf("mesh_device_list_get: Please initialize node_list before trying to access it!\n");
    return false;
  }

  if (node_list->entries_count <= 1 || !node_list->list) {  // Should both apply at the same time
    os_printf("mesh_device_list_get: List is empty!\n");
    *nodes = NULL;
    *count = 0;
  }
  else {
    // As already mentioned above, this would be a possible implementation of a
    // clean coupling/read-only without the usage of "const".
    /* if (node_list_list_cpy) {
     *  os_free(node_list_list_cpy);  // Free if already allocated
     * }
     * // Re-allocate node_list_list_cpy and copy node_list->list to it
     * node_list_list_cpy = (struct mesh_device_node_type *) os_malloc((node_list->entries_count-1)*sizeof(struct mesh_device_node_type));
     * if (!node_list_list_cpy) {
     *  os_printf("mesh_device_get_mac_list: Re-allocating node_list_list_cpy failed!\n");
     * }
     * os_memcpy(node_list_list_cpy, node_list->list, (node_list->entries_count-1)*sizeof(struct mesh_device_node_type));
     * *nodes = node_list_list_cpy;  // Set the reference of nodes to node_list_list_cpy (so node_list->list stays hidden and can't be manipulated from the outside)
     */
    *nodes = node_list->list;
    *count = node_list->entries_count-1;
  }
  return true;
}

// Set the current root to the given MAC-adress
bool ICACHE_FLASH_ATTR mesh_device_root_set(struct mesh_device_mac_type *root) {
  if (!root) {
    os_printf("mesh_device_root_set: Invalid transfer parameter!\n");
    return false;
  }

  // Check whether node_list has been initialized yet and initialize it if not
  if (!node_list) {
    mesh_device_list_init();
  }

  if (node_list->entries_count <= 0) { // Check if there currently is a root
    os_printf("mesh_device_root_set: Setting new root: " MACSTR "\n", MAC2STR(root->mac));
    mesh_device_list_release(); // Clean reset in case some kind of distortion of the data occured; not directly necessary here, just a safety measure
    os_memcpy(&node_list->root.mac_addr, root, sizeof(struct mesh_device_mac_type));
    node_list->entries_count = 1;
  }
  else if (os_memcmp(&node_list->root.mac_addr, root, sizeof(struct mesh_device_mac_type)) != 0){  // Current root is NOT the same as the given MAC-adress
    os_printf("mesh_device_root_set: Switching root from: " MACSTR " to: " MACSTR "\n", MAC2STR(node_list->root.mac_addr.mac), MAC2STR(root->mac));
    mesh_device_list_release(); // Release the current list of registered nodes (since they belonged to the old root-device)
    os_memcpy(&node_list->root.mac_addr, root, sizeof(struct mesh_device_mac_type));
    node_list->entries_count = 1; // Since the old list of registered nodes has been released, the only existing entry is that of the root-device.
  }
  if (os_memcmp(&node_list->root.mac_addr, root, sizeof(struct mesh_device_mac_type)) == 0) {
    return true;
  }
  else {
    os_printf("mesh_device_root_set: Setting new root failed!\n");
    return false;
  }
}

// Return the current root-device
bool ICACHE_FLASH_ATTR mesh_device_root_get(const struct mesh_device_node_type **root) {
  if (!root) {
    os_printf("mesh_device_list_get: Invalid transfer parameters!\n");
    return false;
  }
  if (!node_list) {
    os_printf("mesh_device_root_get: Please initialize node_list before trying to access it!\n");
    return false;
  }
  if (node_list->entries_count <= 0) { // No entries yet (that also means no root)
    os_printf("mesh_device_root_get: No current root!\n");
    return false;
  }

  // Again, a clean coupling/read-only implementation alternative without
  // the necessity of "const".
  /* // Check whether node_list_root_cpy has been initialized yet and initialize it
   * // if not
   * if (!node_list_root_cpy) {
   *  node_list_root_cpy = (struct mesh_device_node_type *) os_zalloc(sizeof(struct mesh_device_node_type));
   * }
   * os_memcpy(node_list_root_cpy, node_list->root, sizeof(struct mesh_device_node_type)); // Copy the current root to node_list_root_cpy
   * *root = node_list_root_cpy;  // Set the reference of root to node_list_root_cpy (so node_list->root stays hidden and can't be manipulated from the outside)
   */
  *root = &node_list->root;
  return true;
}

// Update the timestamp of the given nodes to the current system-time; return
// true if all the devices timestamp could be updated and false if not (nodes
// not yet registered are not considered)
bool ICACHE_FLASH_ATTR mesh_device_update_timestamp(struct mesh_device_mac_type *nodes, uint16_t count) {
  if (!nodes || count < 0) {
    os_printf("mesh_device_update_timestamp: Invalid transfer parameter!\n");
    return false;
  }
  if (!node_list) {
    os_printf("mesh_device_update_timestamp: Please initialize node_list before trying to access it!\n");
    return false;
  }
  if (node_list->entries_count <= 0) {
    os_printf("mesh_device_update_timestamp: List is empty!\n");
    return false;
  }

  uint16_t update_nodes_idx = 0, idx = 0;

  while(update_nodes_idx < count) {
    // Skip nodes that are not included in the list and possible nullpointers
    if (&nodes[update_nodes_idx] && mesh_device_list_search(&nodes[update_nodes_idx])) {
      if (os_memcmp(&node_list->root.mac_addr, &nodes[update_nodes_idx], sizeof(struct mesh_device_mac_type)) == 0) { // Check the root-device
        node_list->root.timestamp = system_get_time();
      }
      for (idx = 0; idx < node_list->entries_count-1; idx++) {
        if (os_memcmp(&node_list->list[idx].mac_addr, &nodes[update_nodes_idx], sizeof(struct mesh_device_mac_type)) == 0){  // Check every currently registered node
          node_list->list[idx].timestamp = system_get_time();
          break;
        }
      }
    }
    update_nodes_idx++;
  }
  return true;
}

// Re-allocate node_list->list and add a number of nodes
bool ICACHE_FLASH_ATTR mesh_device_add(struct mesh_device_mac_type *nodes, uint16_t count) {
  if (!nodes || count < 0) {
    os_printf("mesh_device_add: Invalid transfer parameters!\n");
    return false;
  }
  if (!node_list) {
    os_printf("mesh_device_add: Please initialize node_list before trying to access it!\n");
    return false;
  }
  if (node_list->entries_count < 1) {
    os_printf("mesh_device_add: No current root! Can't add nodes!\n");
    return false;
  }

  if (count > 0) {  // Only do stuff if count is bigger than zero
    uint16_t new_nodes_count_idx = 0, new_nodes_count = 0, idx = 0;
    struct mesh_device_node_type *buf = NULL;

    // Determine the number of new (not yet registered) nodes
    while (new_nodes_count_idx < count) {
      if (&nodes[new_nodes_count_idx] && !mesh_device_list_search(&nodes[new_nodes_count_idx])) {
        new_nodes_count++;
      }
      new_nodes_count_idx++;
    }

    // Only do stuff, if there are new nodes to add in the transfer parameters
    if (new_nodes_count > 0) {
      // Re-allocate node_list->list to match the new number of registered nodes
      buf = (struct mesh_device_node_type *) os_zalloc((node_list->entries_count-1+new_nodes_count)*sizeof(struct mesh_device_node_type));
      if (!buf) {
        os_printf("mesh_device_add: Re-allocating node_list->list failed!\n");
        return false;
      }
      if (node_list->list) {
        // Move the content of node_list->list to buf
        if (node_list->entries_count > 1) {
          os_memcpy(buf, node_list->list, (node_list->entries_count-1)*sizeof(struct mesh_device_node_type));
        }
        os_free(node_list->list);  // Free the (now redundant) old list
      }
      node_list->list = buf; // Set the reference of node_list->list to buf

      // Add the new nodes to node_list->list
      while (idx < count) {
        // Skip nodes that are already included in the list and possible
        // nullpointers
        if (&nodes[idx] && !mesh_device_list_search(&nodes[idx])) {
          os_memcpy(&node_list->list[node_list->entries_count-1].mac_addr, &nodes[idx], sizeof(struct mesh_device_mac_type));
          node_list->entries_count++;
        }
        idx++;
      }
    }
  }
  return true;
}

// Deletes a number of nodes from the list of currently registered nodes and
// re-allocate node_list->list
bool ICACHE_FLASH_ATTR mesh_device_del(struct mesh_device_mac_type *nodes, uint16_t count) {
  if (!nodes || count <= 0) { // Nothing to do if nodes == NULL or count <= 0; return true (since node_list->list doesn't contain the given node either way)
    os_printf("mesh_device_del: Warning: nodes == NULL or count <= 0!\n");
    return true;
  }
  if (!node_list) {
    os_printf("mesh_device_del: Please initialize node_list before trying to access it!\n");
    return false;
  }
  if (node_list->entries_count <= 0) {
    os_printf("mesh_device_del: List is empty! No node to delete!\n");
    return true;
  }

  uint16_t redundant_nodes_idx = 0, idx = 0;
  uint16_t entries_count_cpy = node_list->entries_count;
  struct mesh_device_node_type *buf = NULL;

  while (redundant_nodes_idx < count) {
    // Skip nodes that are not included in the list and possible nullpointers
    if (&nodes[redundant_nodes_idx] && mesh_device_list_search(&nodes[redundant_nodes_idx])) {
      // Check if the current to-delete-node is the root device
      if (os_memcmp(&node_list->root.mac_addr, &nodes[redundant_nodes_idx], sizeof(struct mesh_device_mac_type) == 0)) {
        mesh_device_list_release(); // node_list->list has to be reset as well if the root-device is deleted
        return true;
      }
      // Check every currently registered node
      for (idx = 0; idx < node_list->entries_count-1; idx++) {
        if (os_memcmp(&node_list->list[idx].mac_addr, &nodes[redundant_nodes_idx], sizeof(struct mesh_device_mac_type)) == 0) {
          if (idx < node_list->entries_count-2) {  // Is the to-delete-node the last node in the list?
            // "Delete" the to-delete-node through moving every further node that
            // comes afterwards in the list one place to the front
            os_memcpy(&node_list->list[idx], &node_list->list[idx+1], (node_list->entries_count-1-(idx+1))*sizeof(struct mesh_device_node_type));
          }
          node_list->entries_count--;
          break;
        }
      }
      redundant_nodes_idx++;
    }
  }

  // Re-allocate node_list->list to match the new number of registered nodes if
  // at least one node was deleted
  if (node_list->entries_count != entries_count_cpy) {
    if (node_list->entries_count > 1) {
      buf = (struct mesh_device_node_type *) os_zalloc((node_list->entries_count-1)*sizeof(struct mesh_device_node_type));
      if (!buf) {
        os_printf("mesh_device_del: Re-allocating node_list->list failed!\n");
        return false;
      }
      os_memcpy(buf, node_list->list, (node_list->entries_count-1)*sizeof(struct mesh_device_node_type));
    }
    os_free(node_list->list);  // Free the (now redundant) old list
    node_list->list = buf; // Set the reference of node_list->list to buf
  }
  return true;
}
