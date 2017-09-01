// mesh_none.h
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-02

#ifndef __MESH_NONE_H__
#define __MESH_NONE_H__

/*------------ functions -------------*/

void mesh_parser_protocol_none(const void *mesh_header, uint8_t *data, uint16_t len);
void mesh_topology_disable(void);
void mesh_topology_init(void);

#endif
