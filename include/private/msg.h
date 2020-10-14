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
 * @file msg.h
 */

#ifndef QVI_MSG_H
#define QVI_MSG_H

#include "nng/nng.h"
#include "nng/protocol/reqrep0/rep.h"
#include "nng/protocol/reqrep0/req.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qvi_msg_server_s;
typedef struct qvi_msg_server_s qvi_msg_server_t;

struct qvi_msg_client_s;
typedef struct qvi_msg_client_s qvi_msg_client_t;

/** Message type definition. */
typedef struct qvi_msg_s {
    enum {
        INIT,
        RECV,
        WAIT,
        SEND
    } state;
    nng_aio *aio;
    nng_socket sock;
    nng_msg *payload;
} qvi_msg_t;

/**
 *
 */
int
qvi_msg_server_construct(
    qvi_msg_server_t **server
);

/**
 *
 */
void
qvi_msg_server_destruct(
    qvi_msg_server_t *server
);

/**
 *
 */
int
qvi_msg_server_start(
    qvi_msg_server_t *server,
    const char *url,
    int qdepth
);

/**
 *
 */
int
qvi_msg_client_construct(
    qvi_msg_client_t **client
);

/**
 *
 */
void
qvi_msg_client_destruct(
    qvi_msg_client_t *client
);

// TODO(skg) FIXME
int
qvi_msg_client_send(
    qvi_msg_client_t *client,
    const char *url,
    const char *msecstr
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
