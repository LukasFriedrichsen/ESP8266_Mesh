// mesh_parser.h
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-02

#ifndef __MESH_PARSER_H__
#define __MESH_PARSER_H__

#include "c_types.h"

/*-------- structs and types ---------*/

typedef void (*mesh_parser_protocol_handler)(const void *mesh_header, uint8_t *data, uint16_t len); // Handler-function prototype

struct mesh_parser_protocol_type {
  uint8_t protocol;
  mesh_parser_protocol_handler handler;
};

/*------------ functions -------------*/

void mesh_packet_parser(void *arg, uint8_t *data, uint16_t len);

#endif
