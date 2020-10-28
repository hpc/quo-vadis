/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-rmi.h
 */

#ifndef QVI_RMI_H
#define QVI_RMI_H

#include "private/qvi-rpc.h"

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qvi_rmi_server_s;
typedef struct qvi_rmi_server_s qvi_rmi_server_t;

struct qvi_rmi_client_s;
typedef struct qvi_rmi_client_s qvi_rmi_client_t;

/**
 *
 */
int
qvi_rmi_server_construct(
    qvi_rmi_server_t **server
);

/**
 *
 */
void
qvi_rmi_server_destruct(
    qvi_rmi_server_t *server
);

/**
 *
 */
int
qvi_rmi_server_start(
    qvi_rmi_server_t *server,
    const char *url
);

/**
 *
 */
int
qvi_rmi_client_construct(
    qvi_rmi_client_t **client
);

/**
 *
 */
void
qvi_rmi_client_destruct(
    qvi_rmi_client_t *client
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
    qv_hwloc_bitmap_t *out_bitmap
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
