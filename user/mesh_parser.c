// mesh_parser.c
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-02
//
// Description: This class provides a parser for messages between mesh-nodes,
// determining the communication-protocol used from a list of known protocols
// and passing it on to the respective handler-class. It serves as a facade
// towards extern classes to hide the underlying complexity of the parsing-
// procedure (cf. facade-pattern in terms of pattern-based-programming).
//
// This class is based on https://github.com/espressif/ESP8266_MESH_DEMO/tree/master/mesh_performance/scenario/mesh_parser.c

#include "mem.h"
#include "osapi.h"
#include "mesh.h"
#include "mesh_none.h"
#include "mesh_device.h"
#include "mesh_parser.h"

// List of all supported communication-protocols
// Structure: {M_PROTO_Protocol, handler-funciton}
// Can be extended by further communication-protocols in the following manner:
// static struct mesh_parser_protocol_type supported_protocols[] = {
//  {M_PROTO_NONE, mesh_parser_protocol_none},
//  {M_PROTO_MQTT, mesh_parser_protocol_mqtt},
//  {M_PROTO_BIN, mesh_parser_protocol_bin},
//  ...
// };
static struct mesh_parser_protocol_type supported_protocols[] = {
  {M_PROTO_NONE, mesh_parser_protocol_none},
};

// Parser-function, that resolves a  given message, determines the communication-
// protocol in use and passes the data-part of the packet to the respective
// handler-funciton
void ICACHE_FLASH_ATTR mesh_packet_parser(void *arg, uint8_t *data, uint16_t len) {
  if (!arg || !data || len <= 0) {
    os_printf("mesh_packet_parser: Invalid transfer parameters!\n");
    return;
  }

  uint16_t idx = 0, usr_data_len = 0;
  uint8_t *usr_data = NULL;
  enum mesh_usr_proto_type protocol;
  struct mesh_header_format *header = (struct mesh_header_format *) data; // Interprete data as a packet in the mesh-header-format
  uint16_t protocol_count = sizeof(supported_protocols)/sizeof(supported_protocols[0]); // Determine the actual number of supported protocols

  // Try to resolve the communication-protocol in use
  if (espconn_mesh_get_usr_data_proto(header, &protocol)) {
    // Get the user-data as well as the respective length
    if (!espconn_mesh_get_usr_data(header, &usr_data, &usr_data_len)) {
      // Since the packet doesn't contain a data-part in case of a topology-
      // request, the header itself is set as the data to parse
      usr_data = data;
      usr_data_len = len;
    }

    // Search the list of supported protocols for the messages protocol
    for (idx = 0; idx < protocol_count; idx++) {
      if (supported_protocols[idx].protocol == protocol) {
        if (supported_protocols[idx].handler == NULL) { // Protocol is included in the list, but the corresponding handler-function is missing
          os_printf("mesh_packet_parser: No handler-function available!\n");
          break;
        }
        supported_protocols[idx].handler(header, usr_data, usr_data_len); // Pass the data-part of the packet to the respective handler-function
        break;
      }
    }
    // Check, if the for-loop has been completely cycled through (meaning, that
    // the protocol in use is not supported)
    if (idx == protocol_count) {
      os_printf("mesh_packet_parser: Protocol is not supported!\n");
    }
  }
  else {
    os_printf("mesh_packet_parser: Failed to resolve the protocol!\n");
  }
}
