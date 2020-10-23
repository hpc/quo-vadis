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

// TODO(skg)
// * Bind the server to some subset of hardware resources because it
//   spawns threads.
// * Add message magic to front/back of message body and verify entire message
//   was sent successfully.

#include "private/common.h"
#include "private/rpc.h"
#include "private/logger.h"

#include "nng/supplemental/util/platform.h"

#include <stdarg.h>

// This should be more than plenty for our use case.
#define URL_MAX_LEN 1024

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

struct qvi_rpc_server_s {
    char url[URL_MAX_LEN];
    nng_socket sock;
    uint16_t qdepth;
    qvi_rpc_wqi_t **wqis;
};

struct qvi_rpc_client_s {
    char url[URL_MAX_LEN];
    nng_socket sock;
};

typedef struct qvi_msg_header_s {
    qvi_rpc_funid_t funid;
    qvi_rpc_argv_t argv;
} qvi_msg_header_t;

/**
 *
 */
static void
register_atexit(void)
{
    // We want exactly one atexit handler installed per process.
    static bool handler_installed = false;
    if (!handler_installed) {
        const int rc = atexit(nng_fini);
        if (rc) {
            QVI_LOG_WARN("atexit(nng_fini) failed");
        }
        handler_installed = true;
    }
}

/**
 *
 */
static inline int
msg_append_header(
    nng_msg *msg,
    qvi_msg_header_t *hdr
) {
    const int rc = nng_msg_append(msg, hdr, sizeof(*hdr));
    if (rc != 0) {
        char const *ers = "nng_msg_append() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static inline int
rpc_pack_msg_prep(
    nng_msg **msg,
    const qvi_rpc_funid_t funid,
    const qvi_rpc_argv_t argv
) {
    int rc = nng_msg_alloc(msg, 0);
    if (rc != 0) {
        QVI_LOG_ERROR("nng_msg_alloc() failed");
        return QV_ERR_MSG;
    }

    qvi_msg_header_t msghdr = {funid, argv};
    rc = msg_append_header(*msg, &msghdr);
    if (rc != QV_SUCCESS) {
        nng_msg_free(*msg);
        *msg = nullptr;
        return rc;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static int
rpc_pack(
    nng_msg **msg,
    const qvi_rpc_funid_t funid,
    const qvi_rpc_argv_t argv,
    va_list args
) {
    char const *ers = nullptr;

    nng_msg *imsg = nullptr;
    int rc = rpc_pack_msg_prep(&imsg, funid, argv);
    if (rc != QV_SUCCESS) {
        QVI_LOG_ERROR("rpc_pack_msg_prep() failed");
        return rc;
    }
    //
    const size_t nargs = qvi_rpc_args_maxn();
    const size_t tbits = qvi_rpc_type_nbits();
    // Flag indicating whether or not we are done processing arguments.
    bool done = false;
    // We will need to manipulate the argument list contents, so copy it.
    qvi_rpc_argv_t argvc = argv;
    // Process each argument and store them into the message body in the order
    // in which they were specified.
    for (size_t argidx = 0; argidx < nargs && !done; ++argidx) {
        const qvi_rpc_arg_type_t type = (qvi_rpc_arg_type_t)(
            argvc & rpc_argv_type_mask
        );
        switch (type) {
            // The values are packed contiguously, so we have reached the end.
            case QVI_RPC_TYPE_NONE: {
                done = true;
                break;
            }
            case QVI_RPC_TYPE_INT: {
                const int value = va_arg(args, int);
                if (nng_msg_append(imsg, &value, sizeof(value)) != 0) {
                    ers = "QVI_RPC_TYPE_INT: nng_msg_append() failed";
                    rc = QV_ERR_MSG;
                    goto out;
                }
                break;
            }
            case QVI_RPC_TYPE_CSTR: {
                char *value = va_arg(args, char *);
                if (nng_msg_append(imsg, value, strlen(value) + 1) != 0) {
                    ers = "QVI_RPC_TYPE_CSTR: nng_msg_append() failed";
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
        argvc = argvc >> tbits;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{}", ers);
        nng_msg_free(imsg);
        *msg = nullptr;
        return rc;
    }
    *msg = imsg;
    return QV_SUCCESS;
}

/**
 *
 */
static inline size_t
rpc_unpack_msg_header(
    nng_msg *msg,
    qvi_msg_header_t *hdr
) {
    const size_t hdrsize = sizeof(*hdr);
    memmove(hdr, nng_msg_body(msg), hdrsize);
    return hdrsize;
}

/**
 *
 */
static int
rpc_unpack(
    nng_msg *msg,
    const qvi_msg_header_t msghdr,
    ...
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;
    //
    va_list args;
    va_start(args, msghdr);
    // Get pointer to start of message body.
    uint8_t *bodyp = (uint8_t *)nng_msg_body(msg);
    //
    const size_t nargs = qvi_rpc_args_maxn();
    const size_t tbits = qvi_rpc_type_nbits();
    // Flag indicating whether or not we are done processing arguments.
    bool done = false;
    // Unpack the values in the message body and 'return' them to the caller by
    // reference. Notice during the unpacking process, namely during va_arg(),
    // we add an extra level of indirection to the decoded type. For example,
    // QVI_RPC_TYPE_INT will expect an int * as an argument, not an int.
    qvi_rpc_argv_t argv = msghdr.argv;
    for (size_t argidx = 0; argidx < nargs && !done; ++argidx) {
        const qvi_rpc_arg_type_t type = (qvi_rpc_arg_type_t)(
            argv & rpc_argv_type_mask
        );
        switch (type) {
            case QVI_RPC_TYPE_NONE: {
                done = true;
                break;
            }
            case QVI_RPC_TYPE_INT: {
                int *out = va_arg(args, int *);
                memmove(out, bodyp, sizeof(*out));
                QVI_LOG_INFO("INT = {}", *out);
                bodyp += sizeof(*out);
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
                memmove(value, cstr, bufsize);
                bodyp += bufsize;

                char **out = va_arg(args, char **);
                *out = value;
                QVI_LOG_INFO("CSTR = {}", *out);
                break;
            }
            default:
                ers = "Unrecognized RPC type";
                rc = QV_ERR_INTERNAL;
                goto out;
        }
        // Advance argument bits to process next argument.
        argv = argv >> tbits;
    }
out:
    // Always paired with va_start().
    va_end(args);
    if (ers) {
        QVI_LOG_ERROR("{}", ers);
        return rc;
    }
    return QV_SUCCESS;
}

int
test_fun(int a, char *b, int c) {
    QVI_LOG_INFO("RPC SAYS {} {} {}", a, b, c);
    return 0;
}

/**
 *
 */
static int
rpc_dispatch(
    nng_msg *msg
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qvi_msg_header_t msghdr;
    const size_t trim = rpc_unpack_msg_header(msg, &msghdr);
    rc = nng_msg_trim(msg, trim);
    if (rc != 0) {
        ers = "nng_msg_trim() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }

    switch (msghdr.funid) {
        case TASK_GET_CPUBIND: {
            int a, c;
            char *b;
            rpc_unpack(msg, msghdr, &a, &b, &c);
            test_fun(a, b, c);
            free(b);
            break;
        }
        default:
            QVI_LOG_ERROR("FID={}", msghdr.funid);
            ers = "Unrecognized function ID in RPC dispatch";
            rc = QV_ERR_INTERNAL;
            goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{}", ers);
    }
    return rc;
}

/**
 *
 */
static int
client_connect(
    qvi_rpc_client_t *client,
    const char *url
) {
    char const *ers = nullptr;

    int rc = nng_req0_open(&client->sock);
    if (rc != 0) {
        ers = "nng_req0_open() failed";
        goto out;
    }

    rc = nng_dial(client->sock, url, nullptr, 0);
    if (rc != 0) {
        ers = "nng_dial() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        nng_close(client->sock);
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

int
qvi_rpc_client_req(
    qvi_rpc_client_t *client,
    const qvi_rpc_funid_t funid,
    const qvi_rpc_argv_t argv,
    ...
) {
    va_list vl;
    va_start(vl, argv);

    nng_msg *msg = nullptr;
    int rc = rpc_pack(&msg, funid, argv, vl);

    va_end(vl);
    // Do this here to make dealing with va_start()/va_end() easier.
    if (rc != QV_SUCCESS) {
        char const *ers = "rpc_pack() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        return rc;
    }

    rc = nng_sendmsg(client->sock, msg, 0);
    if (rc != 0) {
        QVI_LOG_INFO("nng_sendmsg() failed");
    }

    rc = nng_recvmsg(client->sock, &msg, 0);
    if (rc != 0) {
        QVI_LOG_INFO("nng_recvmsg() failed");
    }

    nng_msg_free(msg);

    return QV_SUCCESS;
}

/**
 *
 */
// TODO(skg) Optimize
static void
server_cb(
    void *data
) {
    qvi_rpc_wqi_t *wqi = (qvi_rpc_wqi_t *)data;
    nng_msg *msg;
    int rv;

    switch (wqi->state) {
    case qvi_rpc_wqi_t::INIT:
        wqi->state = qvi_rpc_wqi_t::RECV;
        nng_recv_aio(wqi->sock, wqi->aio);
        break;
    case qvi_rpc_wqi_t::RECV:
        // NOTE: this call typically fails during teardown.
        if (nng_aio_result(wqi->aio) != 0) {
            return;
        }
        msg = nng_aio_get_msg(wqi->aio);
        // TODO(skg) trim in dispatch to eat all of the body?
        // TODO(skg) Check for rpc_dispatch errors.
        rpc_dispatch(msg);
        wqi->msg = msg;
        wqi->state = qvi_rpc_wqi_t::WAIT;
        // XXX(skg) Used to kick the state machine into the next step.
        // Is there a better way?
        nng_sleep_aio(0, wqi->aio);
        break;
    case qvi_rpc_wqi_t::WAIT:
        // We could add more data to the message here.
        nng_aio_set_msg(wqi->aio, wqi->msg);
        wqi->msg = nullptr;
        wqi->state = qvi_rpc_wqi_t::SEND;
        nng_send_aio(wqi->sock, wqi->aio);
        break;
    case qvi_rpc_wqi_t::SEND:
        if ((rv = nng_aio_result(wqi->aio)) != 0) {
            nng_msg_free(wqi->msg);
            // TODO(skg) deal with error
        }
        wqi->state = qvi_rpc_wqi_t::RECV;
        nng_recv_aio(wqi->sock, wqi->aio);
        break;
    default:
        QVI_LOG_ERROR("Invalid state entered: {}", wqi->state);
        break;
    }
}

/**
 *
 */
static void
server_deallocate_outstanding_msg_queue(
    qvi_rpc_server_t *server
) {
    if (!server->wqis) return;

    for (uint16_t i = 0; i < server->qdepth; ++i) {
        if (server->wqis[i]->aio) {
            nng_aio_free(server->wqis[i]->aio);
        }
        if (server->wqis[i]) {
            nng_free(server->wqis[i], sizeof(qvi_rpc_wqi_t));
        }
    }
    free(server->wqis);
    server->wqis = nullptr;
}

/**
 *
 */
static int
server_allocate_outstanding_msg_queue(
    qvi_rpc_server_t *server
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    server->wqis = (qvi_rpc_wqi_t **)calloc(
        server->qdepth,
        sizeof(*server->wqis)
    );
    if (!server->wqis) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }

    for (uint16_t i = 0; i < server->qdepth; ++i) {
        qvi_rpc_wqi_t *wqi = nullptr;
        if (!(wqi = (qvi_rpc_wqi_t *)nng_alloc(sizeof(*wqi)))) {
            ers = "nng_alloc() failed";
            rc = QV_ERR_OOR;
            goto out;
        }
        rc = nng_aio_alloc(&wqi->aio, server_cb, wqi);
        if (rc != 0) {
            ers = "nng_aio_alloc() failed";
            rc = QV_ERR_OOR;
            goto out;
        }
        wqi->state = qvi_rpc_wqi_t::INIT;
        wqi->sock  = server->sock;
        server->wqis[i] = wqi;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{}", ers);
        // Relinquish any allocated resources.
        server_deallocate_outstanding_msg_queue(server);
        return rc;
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
    //
    server_deallocate_outstanding_msg_queue(server);
    //
    free(server);
}


static int
server_setup(
    qvi_rpc_server_t *server,
    const char *url,
    uint16_t qdepth
) {
    server->qdepth = qdepth;

    const int nwritten = snprintf(server->url, sizeof(server->url), "%s", url);
    if (nwritten >= URL_MAX_LEN) {
        QVI_LOG_ERROR("snprintf() truncated");
        return QV_ERR_INTERNAL;
    }
    // Register cleanup function at exit.
    register_atexit();
    return QV_SUCCESS;
}

/**
 *
 */
static int
server_open_commchan(
    qvi_rpc_server_t *server
) {
    const int rc = nng_rep0_open_raw(&server->sock);
    if (rc != 0) {
        char const *ers = "nng_rep0_open_raw() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

/**
 *
 */
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
    for (uint16_t i = 0; i < server->qdepth; ++i) {
        // This starts the state machine.
        server_cb(server->wqis[i]);
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
    uint16_t qdepth
) {
    if (!server || !url) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    rc = server_setup(server, url, qdepth);
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
        // Start failed, so relinquish allocated resources.
        qvi_rpc_server_destruct(server);
        return rc;
    }
    return QV_SUCCESS;
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
    register_atexit();
    *client = iclient;
    return QV_SUCCESS;
}

void
qvi_rpc_client_destruct(
    qvi_rpc_client_t *client
) {
    if (!client) return;

    nng_close(client->sock);
    free(client);
}

int
qvi_rpc_client_connect(
    qvi_rpc_client_t *client,
    const char *url
) {
    return client_connect(client, url);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
