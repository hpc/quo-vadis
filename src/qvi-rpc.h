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
 * @file qvi-rpc.h
 */

#ifndef QVI_RPC_H
#define QVI_RPC_H

#include "qvi-hwloc.h"

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qvi_rpc_server_s;
typedef struct qvi_rpc_server_s qvi_rpc_server_t;

struct qvi_rpc_client_s;
typedef struct qvi_rpc_client_s qvi_rpc_client_t;

struct qvi_rpc_fun_data_s;
typedef struct qvi_rpc_fun_data_s qvi_rpc_fun_data_t;

typedef enum qvi_rpc_funid_e {
    RPC_FID_INVALID = 0,
    RPC_FID_TASK_GET_CPUBIND,
} qvi_rpc_funid_t;

/**
 *
 */
int
qvi_rpc_server_new(
    qvi_rpc_server_t **server
);

/**
 *
 */
void
qvi_rpc_server_free(
    qvi_rpc_server_t **server
);

/**
 *
 */
int
qvi_rpc_server_start(
    qvi_rpc_server_t *server,
    const char *url
);

/**
 *
 */
int
qvi_rpc_client_new(
    qvi_rpc_client_t **client
);

/**
 *
 */
void
qvi_rpc_client_free(
    qvi_rpc_client_t **client
);

/**
 *
 */
int
qvi_rpc_client_connect(
    qvi_rpc_client_t *client,
    const char *url
);

int
qvi_rpc_req(
    qvi_rpc_client_t *client,
    qvi_rpc_funid_t fid,
    const char *picture
    ...
);

int
qvi_rpc_rep(
    qvi_rpc_client_t *client,
    const char *picture,
    ...
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
