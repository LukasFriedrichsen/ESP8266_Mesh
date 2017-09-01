// mesh_device.h
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-03-23

#ifndef __MESH_DEVICE_H__
#define __MESH_DEVICE_H__

#include "c_types.h"

/*------------- structs --------------*/

struct mesh_device_mac_type {
    uint8_t mac[6];
} __packed;

struct mesh_device_node_type {
    struct mesh_device_mac_type mac_addr;
    uint32_t timestamp;
} __packed;

struct mesh_device_list_type {
    uint16_t entries_count; // Entry 1 = root, entries 2..n = registered nodes
    struct mesh_device_node_type root;
    struct mesh_device_node_type *list;
};

/*------------ functions -------------*/

void mesh_device_list_init(void);
void mesh_device_list_release(void);
void mesh_device_list_disp(void);
bool mesh_device_list_search(struct mesh_device_mac_type *node);
bool mesh_device_update_timestamp(struct mesh_device_mac_type *nodes, uint16_t count);
bool mesh_device_list_get(const struct mesh_device_node_type **nodes, uint16_t *count);
bool mesh_device_root_set(struct mesh_device_mac_type *root);
bool mesh_device_root_get(const struct mesh_device_node_type **root);
bool mesh_device_add(struct mesh_device_mac_type *nodes, uint16_t count);
bool mesh_device_del(struct mesh_device_mac_type *nodes, uint16_t count);

#endif
