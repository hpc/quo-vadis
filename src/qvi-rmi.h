/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-rmi.h
 *
 * Resource Management and Inquiry
 */

#ifndef QVI_RMI_H
#define QVI_RMI_H

#include "qvi-hwloc.h"
#include "qvi-bbuff.h"

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qvi_rmi_config_s {
    char *url;
    char *hwtopo_path;
} qvi_rmi_config_t;

// Forward declarations.
struct qvi_rmi_server_s;
typedef struct qvi_rmi_server_s qvi_rmi_server_t;

struct qvi_rmi_client_s;
typedef struct qvi_rmi_client_s qvi_rmi_client_t;

/**
 *
 */
int
qvi_rmi_config_new(
    qvi_rmi_config_t **config
);

/**
 *
 */
void
qvi_rmi_config_free(
    qvi_rmi_config_t **config
);

/**
 *
 */
int
qvi_rmi_config_pack(
    qvi_rmi_config_t *config,
    qvi_bbuff_t **packed
);

/**
 *
 */
int
qvi_rmi_config_unpack(
    void *buff,
    qvi_rmi_config_t *config
);

/**
 *
 */
int
qvi_rmi_server_new(
    qvi_rmi_server_t **server
);

/**
 *
 */
void
qvi_rmi_server_free(
    qvi_rmi_server_t **server
);

/**
 *
 */
int
qvi_rmi_server_config(
    qvi_rmi_server_t *server,
    qvi_rmi_config_t *config
);

/**
 *
 */
int
qvi_rmi_server_start(
    qvi_rmi_server_t *server
);

/**
 *
 */
int
qvi_rmi_client_new(
    qvi_rmi_client_t **client
);

/**
 *
 */
void
qvi_rmi_client_free(
    qvi_rmi_client_t **client
);

/**
 *
 */
int
qvi_rmi_client_connect(
    qvi_rmi_client_t *client,
    const char *url
);

int
qvi_rmi_task_get_cpubind(
    qvi_rmi_client_t *client,
    pid_t who,
    hwloc_bitmap_t *out_bitmap
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
