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
 * @file rpc.cc
 */

// TODO(skg) Bind the server to some subset of hardware resources because it
// spawns threads.

#include "private/common.h"
#include "private/rpc.h"
#include "private/logger.h"

#include "nng/supplemental/util/platform.h"

#include <stdarg.h>

// This should be more than plenty for our use case.
#define URL_MAX_LEN 1024

struct qvi_rpc_server_s {
    char url[URL_MAX_LEN];
    nng_socket sock;
    int qdepth;
    qvi_rpc_t **messages;
};

struct qvi_rpc_client_s {
    char url[URL_MAX_LEN];
    nng_socket sock;
};

struct qvi_rpc_rep_s {
    int rc;
    nng_msg *msg;
};

// We currently support encoding up to 8 arguments:
// 64 bits for the underlying qvi_rpc_args_t type divided by
// 8 bits for the QVI_RPC_TYPE_* types.
typedef uint64_t qvi_rpc_args_t;

// We currently support up to 8 types. If this ever changes, please carefully
// update all structures associated with the handling of these values.
typedef uint8_t qvi_rpc_type_t;
#define QVI_RPC_TYPE_NONE (0x00     )
#define QVI_RPC_TYPE_INT  (0x01 << 0)
#define QVI_RPC_TYPE_CSTR (0x01 << 1)

typedef enum qvi_rpc_fun_e {
    TASK_GET_CPUBIND
} qvi_rpc_fun_t;

/**
 * Returns the maximum number of arguments that can be packed into a
 * qvi_rpc_args_t structure.
 */
static inline size_t
rpc_args_maxn(void)
{
    return sizeof(qvi_rpc_args_t) / sizeof(qvi_rpc_type_t);
}

/**
 * Returns the number of bits used for RPC types.
 */
static inline size_t
rpc_type_nbits(void)
{
    return sizeof(qvi_rpc_type_t) * 8;
}

/**
 * Returns the qvi_rpc_args_t from the RPC message header.
 */
static inline qvi_rpc_args_t
rpc_args_from_msg_header(
    nng_msg *msg
) {
    qvi_rpc_args_t argl;
    memcpy(&argl, nng_msg_header(msg), sizeof(argl));
    return argl;
}

/**
 *
 */
static int
rpc_pack(
    nng_msg **msg,
    const qvi_rpc_args_t argl,
    va_list args
) {
    char const *ers = nullptr;

    nng_msg *imsg = nullptr;
    int rc = nng_msg_alloc(&imsg, 0);
    if (rc != 0) {
        QVI_LOG_ERROR("nng_msg_alloc() failed");
        return QV_ERR_MSG;
    }
    const size_t nargs = rpc_args_maxn();
    const size_t tbits = rpc_type_nbits();
    // Type mask used to help retrieve the underlying RPC type.
    const qvi_rpc_args_t mask = 0xFF;
    // Flag indicating whether or not we are done processing arguments.
    bool done = false;
    // We will need to manipulate the argument list contents, so copy it.
    qvi_rpc_args_t arglc = argl;
    // Process each argument and store them into the message body in the order
    // in which they were specified.
    for (size_t argidx = 0; argidx < nargs && !done; ++argidx) {
        qvi_rpc_type_t type = (qvi_rpc_type_t)(arglc & mask);
        switch (type) {
            case QVI_RPC_TYPE_NONE: {
                done = true;
                break;
            }
            case QVI_RPC_TYPE_INT: {
                int value = va_arg(args, int);
                if (nng_msg_append(imsg, &value, sizeof(value)) != 0) {
                    rc = QV_ERR_MSG;
                    goto out;
                }
                break;
            }
            case QVI_RPC_TYPE_CSTR: {
                char *value = va_arg(args, char *);
                if (nng_msg_append(imsg, value, strlen(value) + 1) != 0) {
                    rc = QV_ERR_MSG;
                    goto out;
                }
                break;
            }
            default:
                ers = "Unrecognized RPC type";
                rc = QV_ERR_INTERNAL;
                goto out;
        }
        // Advance argument bits to process next argument.
        arglc = arglc >> tbits;
    }
    // Add argument information to the header to make unpacking easier.
    rc = nng_msg_header_append(imsg, &argl, sizeof(argl));
    if (rc != 0) {
        ers = "nng_msg_header_append() failed";
        rc = QV_ERR_MSG;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{}", ers);
        return rc;
    }
    *msg = imsg;
    return QV_SUCCESS;
}

/**
 *
 */
static int
rpc_unpack(
    nng_msg *msg
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;
    // Get argument list from message header.
    qvi_rpc_args_t argl = rpc_args_from_msg_header(msg);
    // Get pointer to start of message body.
    uint8_t *bodyp = (uint8_t *)nng_msg_body(msg);

    const size_t nargs = rpc_args_maxn();
    const size_t tbits = rpc_type_nbits();
    // Type mask used to help retrieve the underlying RPC type.
    const qvi_rpc_args_t mask = 0xFF;
    // Flag indicating whether or not we are done processing arguments.
    bool done = false;
    // Process each argument and store them into the message body in the order
    // in which they were specified.
    for (size_t argidx = 0; argidx < nargs && !done; ++argidx) {
        qvi_rpc_type_t type = (qvi_rpc_type_t)(argl & mask);
        switch (type) {
            case QVI_RPC_TYPE_NONE: {
                done = true;
                break;
            }
            case QVI_RPC_TYPE_INT: {
                int value;
                memcpy(&value, bodyp, sizeof(value));
                QVI_LOG_INFO("INT = {}", value);
                bodyp += sizeof(value);
                break;
            }
            case QVI_RPC_TYPE_CSTR: {
                char const *cstr = (char const *)bodyp;
                int bufsize = snprintf(NULL, 0, "%s", cstr) + 1;
                char *value = (char *)calloc(bufsize, sizeof(*value));
                if (!value) {
                    ers = "calloc() failed";
                    rc = QV_ERR_OOR;
                    goto out;
                }
                memcpy(value, cstr, bufsize);
                QVI_LOG_INFO("CSTR = {}", value);
                bodyp += bufsize;
                break;
            }
            default:
                ers = "Unrecognized RPC type";
                rc = QV_ERR_INTERNAL;
                goto out;
        }
        // Advance argument bits to process next argument.
        argl = argl >> tbits;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{}", ers);
        return rc;
    }
    return QV_SUCCESS;
}

int
qvi_rpc_call(
    qvi_rpc_fun_t funid,
    qvi_rpc_args_t argl,
    ...
) {
    va_list vl;
    va_start(vl, argl);
    nng_msg *msg = nullptr;
    rpc_pack(&msg, argl, vl);
    va_end(vl);
    rpc_unpack(msg);
    nng_msg_free(msg);
    return 0;
}

static void
server_cb(
    void *data
) {
    qvi_rpc_t *msg = (qvi_rpc_t *)data;
    nng_msg *payload;
    int rv;
    uint32_t when;

    switch (msg->state) {
    case qvi_rpc_t::INIT:
        msg->state = qvi_rpc_t::RECV;
        nng_recv_aio(msg->sock, msg->aio);
        break;
    case qvi_rpc_t::RECV:
        // NOTE: this call typically fails during teardown.
        if (nng_aio_result(msg->aio) != 0) {
            return;
        }
        payload = nng_aio_get_msg(msg->aio);
        if ((rv = nng_msg_trim_u32(payload, &when)) != 0) {
            // Bad message, just ignore it.
            nng_msg_free(payload);
            nng_recv_aio(msg->sock, msg->aio);
            return;
        }
        msg->payload = payload;
        msg->state = qvi_rpc_t::WAIT;
        nng_sleep_aio(when, msg->aio);
        break;
    case qvi_rpc_t::WAIT:
        // We could add more data to the message here.
        nng_aio_set_msg(msg->aio, msg->payload);
        msg->payload = nullptr;
        msg->state = qvi_rpc_t::SEND;
        nng_send_aio(msg->sock, msg->aio);
        break;
    case qvi_rpc_t::SEND:
        if ((rv = nng_aio_result(msg->aio)) != 0) {
            nng_msg_free(msg->payload);
            // TODO(skg) deal with error
        }
        msg->state = qvi_rpc_t::RECV;
        nng_recv_aio(msg->sock, msg->aio);
        break;
    default:
        QVI_LOG_ERROR("Invalid state entered: {}", msg->state);
        break;
    }
}

static int
server_allocate_outstanding_msg_queue(
    qvi_rpc_server_t *server
) {
    if (!server) return QV_ERR_INVLD_ARG;

    server->messages = (qvi_rpc_t **)calloc(
        server->qdepth,
        sizeof(*(server->messages))
    );
    if (!server->messages) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }
    for (int i = 0; i < server->qdepth; ++i) {
        qvi_rpc_t *imsg = nullptr;
        if (!(imsg = (qvi_rpc_t *)nng_alloc(sizeof(*imsg)))) {
            QVI_LOG_ERROR("nng_alloc() failed");
            return QV_ERR_OOR;
        }
        int rc = nng_aio_alloc(&imsg->aio, server_cb, imsg);
        if (rc != 0) {
            QVI_LOG_ERROR(
                "nng_aio_alloc() failed with rc={} ({})",
                rc,
                nng_strerror(rc)
            );
            return QV_ERR_MSG;
        }
        imsg->state = qvi_rpc_t::INIT;
        imsg->sock  = server->sock;
        server->messages[i] = imsg;
    }
    return QV_SUCCESS;
}

int
qvi_rpc_server_construct(
    qvi_rpc_server_t **server
) {
    if (!server) return QV_ERR_INVLD_ARG;

    qvi_rpc_server_t *iserver = (qvi_rpc_server_t *)calloc(1, sizeof(*iserver));
    if (!iserver) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }
    *server = iserver;
    return QV_SUCCESS;
}

void
qvi_rpc_server_destruct(
    qvi_rpc_server_t *server
) {
    if (!server) return;
    // Close the socket before we free any other resources.
    int rc = nng_close(server->sock);
    if (rc != 0) {
        char const *ers = "nng_close() failed";
        QVI_LOG_WARN("{} with rc={} ({})", ers, rc, nng_strerror(rc));
    }

    if (server->messages) {
        for (int i = 0; i < server->qdepth; ++i) {
            nng_aio_free(server->messages[i]->aio);
            nng_free(server->messages[i], sizeof(qvi_rpc_t));
        }
        free(server->messages);
    }
    free(server);
}

static void
server_register_atexit(void)
{
    // We want exactly one atexit handler installed per process.
    static bool handler_installed = false;
    if (!handler_installed) {
        int rc = atexit(nng_fini);
        if (rc) {
            QVI_LOG_WARN("atexit(nng_fini) failed");
        }
        handler_installed = true;
    }
}

static int
server_setup(
    qvi_rpc_server_t *server,
    const char *url,
    int qdepth
) {
    // Queue depth sanity
    if (qdepth < 0) {
        QVI_LOG_ERROR("Negative queue depths not supported");
        return QV_ERR_INVLD_ARG;
    }
    server->qdepth = qdepth;

    const int nwritten = snprintf(server->url, sizeof(server->url), "%s", url);
    if (nwritten >= URL_MAX_LEN) {
        QVI_LOG_ERROR("snprintf() truncated");
        return QV_ERR_INTERNAL;
    }
    // Register cleanup function at exit.
    server_register_atexit();
    return QV_SUCCESS;
}

static int
server_open_commchan(
    qvi_rpc_server_t *server
) {
    int rc = nng_rep0_open_raw(&server->sock);
    if (rc != 0) {
        char const *ers = "nng_rep0_open_raw() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

static int
server_listen(
    qvi_rpc_server_t *server
) {
    char const *ers = nullptr;

    int rc = nng_listen(server->sock, server->url, nullptr, 0);
    if (rc != 0) {
        ers = "nng_listen() failed";
        goto out;
    }
    for (int i = 0; i < server->qdepth; ++i) {
        // This starts the state machine.
        server_cb(server->messages[i]);
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

int
qvi_rpc_server_start(
    qvi_rpc_server_t *server,
    const char *url,
    int qdepth
) {
    if (!server || !url) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    int rc = server_setup(server, url, qdepth);
    if (rc != QV_SUCCESS) {
        ers = "server_setup() failed";
        goto out;
    }

    rc = server_open_commchan(server);
    if (rc != QV_SUCCESS) {
        ers = "server_open_commchan() failed";
        goto out;
    }

    rc = server_allocate_outstanding_msg_queue(server);
    if (rc != QV_SUCCESS) {
        ers = "server_allocate_outstanding_msg_queue() failed";
        goto out;
    }

    rc = server_listen(server);
    if (rc != QV_SUCCESS) {
        ers = "server_listen() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        return rc;
    }
    return QV_SUCCESS;
}

static void
client_register_atexit(void)
{
    // We want exactly one atexit handler installed per process.
    static bool handler_installed = false;
    if (!handler_installed) {
        int rc = atexit(nng_fini);
        if (rc) {
            QVI_LOG_WARN("atexit(nng_fini) failed");
        }
        handler_installed = true;
    }
}

int
qvi_rpc_client_construct(
    qvi_rpc_client_t **client
) {
    if (!client) return QV_ERR_INVLD_ARG;

    qvi_rpc_client_t *iclient = (qvi_rpc_client_t *)calloc(1, sizeof(*iclient));
    if (!iclient) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }
    client_register_atexit();
    *client = iclient;
    return QV_SUCCESS;
}

void
qvi_rpc_client_destruct(
    qvi_rpc_client_t *client
) {
    if (!client) return;

    free(client);
}

int
qvi_rpc_client_send(
    qvi_rpc_client_t *client,
    const char *url,
    const char *msecstr
) {
    int rv;
    nng_msg *msg;
    nng_time start;
    nng_time end;
    uint32_t msec;

    qvi_rpc_args_t args = QVI_RPC_TYPE_INT;
    args = args | (QVI_RPC_TYPE_CSTR << 8);
    args = args | (QVI_RPC_TYPE_INT << 16);

    qvi_rpc_call(TASK_GET_CPUBIND, args, 1, "a loooooooooooooooong string", -42);

    msec = atoi(msecstr) * 100;

    if ((rv = nng_req0_open(&client->sock)) != 0) {
        QVI_LOG_INFO("nng_req0_open failed");
    }

    if ((rv = nng_dial(client->sock, url, nullptr, 0)) != 0) {
        QVI_LOG_INFO("nng_dial failed");
    }

    start = nng_clock();

    if ((rv = nng_msg_alloc(&msg, 0)) != 0) {
        QVI_LOG_INFO("nng_msg_alloc failed");
    }
    if ((rv = nng_msg_append_u32(msg, msec)) != 0) {
        QVI_LOG_INFO("nng_msg_append_u32 failed");
    }

    if ((rv = nng_sendmsg(client->sock, msg, 0)) != 0) {
        QVI_LOG_INFO("nng_sendmsg failed");
    }

    if ((rv = nng_recvmsg(client->sock, &msg, 0)) != 0) {
        QVI_LOG_INFO("nng_recvmsg failed");
    }
    end = nng_clock();
    nng_msg_free(msg);
    nng_close(client->sock);

    printf("Request took %u milliseconds.\n", (uint32_t)(end - start));
    return 0;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
