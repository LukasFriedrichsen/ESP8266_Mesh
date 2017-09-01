// esp_mesh.h
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-05

#ifndef __ESP_MESH_H__
#define __ESP_MESH_H__

/*-------- structs and types ---------*/

enum op_mode {
  ROOT_NODE = 0,
  SUB_NODE,
  DISABLED = -1
};

/*-------- program parameters --------*/

#define SIG_RUN 0
#define SIG_PRINT 1

#define TASK_PRIO_0 0
#define TASK_PRIO_1 1
#define TASK_QUEUE_LENGTH 2

#endif
