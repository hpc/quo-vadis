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
 * @file rpc.h
 */

#ifndef QVI_RPC_H
#define QVI_RPC_H

#include "nng/nng.h"
#include "nng/protocol/reqrep0/rep.h"
#include "nng/protocol/reqrep0/req.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qvi_rpc_server_s;
typedef struct qvi_rpc_server_s qvi_rpc_server_t;

struct qvi_rpc_client_s;
typedef struct qvi_rpc_client_s qvi_rpc_client_t;

/** Message type definition. */
typedef struct qvi_rpc_wqi_s {
    enum {
        INIT,
        RECV,
        WAIT,
        SEND
    } state;
    nng_aio *aio;
    nng_socket sock;
    nng_msg *msg;
} qvi_rpc_wqi_t;

/**
 *
 */
int
qvi_rpc_server_construct(
    qvi_rpc_server_t **server
);

/**
 *
 */
void
qvi_rpc_server_destruct(
    qvi_rpc_server_t *server
);

/**
 * qdepth is the maximum number of outstanding requests we can handle.  It
 * represents outstanding work items.  Select a small number to reduce memory
 * usage.
 */
int
qvi_rpc_server_start(
    qvi_rpc_server_t *server,
    const char *url,
    int qdepth
);

/**
 *
 */
int
qvi_rpc_client_construct(
    qvi_rpc_client_t **client
);

/**
 *
 */
void
qvi_rpc_client_destruct(
    qvi_rpc_client_t *client
);

// TODO(skg) FIXME
int
qvi_rpc_client_send(
    qvi_rpc_client_t *client,
    const char *url
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
