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

#include <assert.h>

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

// We currently support encoding up to 8 arguments:
// 64 bits for the underlying qvi_rpc_argv_t type divided by
// 8 bits for the QVI_RPC_TYPE_* types.
typedef uint64_t qvi_rpc_argv_t;

// Type bitmask used to help retrieve the underlying RPC type.
static const qvi_rpc_argv_t rpc_argv_type_mask = 0x00000000000000FF;

// We currently support up to 8 types. If this ever changes, please carefully
// update all structures associated with the handling of these values.
typedef uint8_t qvi_rpc_arg_type_t;
#define QVI_RPC_TYPE_NONE (0x00     )
#define QVI_RPC_TYPE_INT  (0x01 << 0)
#define QVI_RPC_TYPE_CSTR (0x01 << 1)

/**
 * Returns the maximum number of arguments that can be packed into a
 * qvi_rpc_argv_t structure.
 */
static inline size_t
qvi_rpc_args_maxn(void)
{
    return sizeof(qvi_rpc_argv_t) / sizeof(qvi_rpc_arg_type_t);
}

/**
 * Returns the number of bits used for RPC types.
 */
static inline size_t
qvi_rpc_type_nbits(void)
{
    return sizeof(qvi_rpc_arg_type_t) * 8;
}

/**
 *
 */
static inline void
qvi_rpc_argv_insert_at(
    qvi_rpc_argv_t *argv,
    const qvi_rpc_arg_type_t type,
    const uint8_t pos
) {
    const size_t offset = pos * qvi_rpc_type_nbits();
    *argv = *argv | (type << offset);
}

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

/**
 *
 */
int
qvi_rpc_client_connect(
    qvi_rpc_client_t *client,
    const char *url
);

// TODO(skg) FIXME
int
qvi_rpc_client_send(
    qvi_rpc_client_t *client
);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus // C++

/**
 * Primary qvi_rpc_argv_type template.
 */
template<typename T>
qvi_rpc_arg_type_t
qvi_rpc_argv_type(T)
{
    assert(false && "qvi_rpc_argv_type() failed: cannot deduce type.");
}

/**
 * int qvi_rpc_argv_type template specialization.
 */
template<>
qvi_rpc_arg_type_t
qvi_rpc_argv_type(int)
{
    return QVI_RPC_TYPE_INT;
}

/**
 * char const * qvi_rpc_argv_type template specialization.
 */
template<>
qvi_rpc_arg_type_t
qvi_rpc_argv_type(char const *)
{
    return QVI_RPC_TYPE_CSTR;
}

/**
 *
 */
template<typename ARG>
void
qvi_rpc_argv_pack(
    qvi_rpc_argv_t *argv,
    uint8_t pos,
    ARG arg
) {
    qvi_rpc_argv_insert_at(argv, qvi_rpc_argv_type(arg),  pos);
}

/**
 *
 */
template<typename CAR_ARGS, typename... CDR_ARGS>
void
qvi_rpc_argv_pack(
    qvi_rpc_argv_t *argv,
    uint8_t pos,
    CAR_ARGS arg,
    CDR_ARGS... args
) {
    qvi_rpc_argv_pack(argv, pos, arg);
    qvi_rpc_argv_pack(argv, pos + 1, args...);
}

#endif // C++

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
