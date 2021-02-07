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
 * @file qvi-rpc.cc
 */

#if 0
It's pretty straightforward. To terminate, you call term on context from main
thread. This call is blocking. This causes all your blocking socket threads
(blocking reads, pollers, etc) to get unblocked. You proceed to close your
sockets on their respective threads. Once all sockets are closed the call to
term will unblock. This works every time even if you are sending thousands of
messages while terminating. Clean shutdown.

The Java Jeromq's ZContext has a nasty bug in it. If you call close or destroy
on the context, it loops on its sockets and destroys them first, then it calls
term. This is bad, you get race conditions trying to close on wrong threads. It
could be that not everyone is bothered by this, but in my view clean shutdown is
important and it makes running tests a lot easier
#endif

#include "private/qvi-common.h"
#include "private/qvi-utils.h"
#include "private/qvi-rpc.h"
#include "private/qvi-log.h"

#include "zmq.h"
#include "zmq_utils.h"

#include <stdarg.h>

#define qvi_zmq_err_msg(ers, err_no)                                           \
do {                                                                           \
    int erno = (err_no);                                                       \
    qvi_log_error("{} with errno={} ({})", (ers), erno, qvi_strerr(erno));     \
} while (0)

// This should be more than plenty for our use case.
#define QVI_RPC_URL_MAX_LEN 128

struct qvi_rpc_server_s {
    void *zmq_context = nullptr;
    void *zmq_sock = nullptr;
    qvi_hwloc_t *hwloc = nullptr;
    char url[QVI_RPC_URL_MAX_LEN] = {'\0'};
};

struct qvi_rpc_client_s {
    void *zmq_context = nullptr;
    void *zmq_sock = nullptr;
};

typedef struct qvi_msg_header_s {
    qvi_rpc_funid_t funid;
    qvi_rpc_argv_t argv;
} qvi_msg_header_t;

typedef int (*qvi_rpc_fun_ptr_t)(qvi_rpc_fun_data_t *);

////////////////////////////////////////////////////////////////////////////////
// Server-Side RPC Stub Definitions
////////////////////////////////////////////////////////////////////////////////
static int
rpc_stub_task_get_cpubind(
    qvi_rpc_fun_data_t *fun_data
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    // TODO(skg) Improve.
    hwloc_bitmap_t bitmap;
    const int bufsize = sizeof(fun_data->bitm_args[0]);
    int nwritten;

    rc = qvi_hwloc_task_get_cpubind(
        fun_data->hwloc,
        fun_data->int_args[0],
        &bitmap
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_task_get_cpubind() failed";
        rc = QV_ERR_RPC;
        goto out;
    }

    // TODO(skg) Implement helper.
    nwritten = hwloc_bitmap_snprintf(
        fun_data->bitm_args[0],
        bufsize,
        bitmap
    );
    if (nwritten >= bufsize) {
        ers = "qvi_hwloc_bitmap_snprintf() failed";
        rc = QV_ERR_INTERNAL;
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    hwloc_bitmap_free(bitmap);
    return rc;
}

/**
 * Maps a given qvi_rpc_funid_t to a given function pointer. Must be kept in
 * sync with qvi_rpc_funid_t.
 */
static const qvi_rpc_fun_ptr_t qvi_server_rpc_dispatch_table[] = {
    rpc_stub_task_get_cpubind
};

/**
 *
 */
static inline size_t
server_rpc_unpack_msg_header(
    void *msg,
    qvi_msg_header_t *hdr
) {
    const size_t hdrsize = sizeof(*hdr);
    memmove(hdr, msg, hdrsize);
    return hdrsize;
}

/**
 *
 */
static int
server_hwloc_init(
    qvi_rpc_server_t *server
) {
    char const *ers = nullptr;

    int rc = qvi_hwloc_topology_load(server->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topo_load() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

int
qvi_rpc_server_construct(
    qvi_rpc_server_t **server
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qvi_rpc_server_t *iserver = qvi_new qvi_rpc_server_t;
    if (!iserver) {
        qvi_log_error("memory allocation failed");
        return QV_ERR_OOR;
    }

    iserver->zmq_context = zmq_ctx_new();
    if (!iserver->zmq_context) {
        ers = "zmq_ctx_new() failed";
        rc = QV_ERR_MSG;
        goto out;
    }

    rc = qvi_hwloc_construct(&iserver->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_construct() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rpc_server_destruct(&iserver);
    }
    *server = iserver;
    return rc;
}

void
qvi_rpc_server_destruct(
    qvi_rpc_server_t **server
) {
    qvi_rpc_server_t *iserver = *server;
    if (!iserver) return;

    qvi_hwloc_destruct(iserver->hwloc);

    if (iserver->zmq_sock) {
        int rc = zmq_close(iserver->zmq_sock);
        if (rc != 0) {
            int erno = errno;
            qvi_log_warn("zmq_close() failed with {}", qvi_strerr(erno));
        }
    }
    if (iserver->zmq_context) {
        int rc = zmq_ctx_destroy(iserver->zmq_context);
        if (rc != 0) {
            int erno = errno;
            qvi_log_warn("zmq_ctx_destroy() failed with {}", qvi_strerr(erno));
        }
    }

    delete iserver;
    *server = nullptr;
}

/**
 *
 */
static int
server_open_commchan(
    qvi_rpc_server_t *server
) {
    server->zmq_sock = zmq_socket(server->zmq_context, ZMQ_REP);
    if (!server->zmq_sock) {
        qvi_zmq_err_msg("zmq_socket() failed", errno);
        return QV_ERR_MSG;
    }

    int rc = zmq_bind(server->zmq_sock, server->url);
    if (rc != 0) {
        qvi_zmq_err_msg("zmq_bind() failed", errno);
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static int
server_rpc_unpack(
    void *msg,
    const qvi_msg_header_t *msghdr,
    qvi_rpc_fun_data_t *fun_data
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;
    // Get pointer to start of message body.
    uint8_t *bodyp = (uint8_t *)msg;
    //
    const size_t nargs = qvi_rpc_args_maxn();
    const size_t tbits = qvi_rpc_type_nbits();
    // Flag indicating whether or not we are done processing arguments.
    bool done = false;
    // Unpack the values in the message body and populate relevant parameters.
    qvi_rpc_argv_t argv = msghdr->argv;
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
                int *arg = &fun_data->int_args[fun_data->int_i++];
                memmove(arg, bodyp, sizeof(*arg));
                bodyp += sizeof(*arg);
                break;
            }
            case QVI_RPC_TYPE_CSTR: {
                char const *cstr = (char const *)bodyp;
                const int bufsize = snprintf(NULL, 0, "%s", cstr) + 1;
                char *value = (char *)calloc(bufsize, sizeof(*value));
                if (!value) {
                    ers = "calloc() failed";
                    rc = QV_ERR_OOR;
                    goto out;
                }
                memmove(value, cstr, bufsize);
                fun_data->cstr_args[fun_data->cstr_i++] = value;
                bodyp += bufsize;
                break;
            }
            case QVI_RPC_TYPE_BITM: {
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
    if (ers) {
        qvi_log_error("{}", ers);
        return rc;
    }
    return QV_SUCCESS;
}

static inline void *
msg_trim(
    void *msg,
    size_t trim
) {
    uint8_t *new_base = (uint8_t *)msg;
    new_base += trim;
    return new_base;
}

/**
 *
 */
static inline int
server_msg_unpack(
    qvi_rpc_server_t *server,
    zmq_msg_t *zmq_msg,
    qvi_msg_header_t *msg_hdr,
    qvi_rpc_fun_data_t *unpacked
) {
    void *msg = zmq_msg_data(zmq_msg);

    const size_t trim = server_rpc_unpack_msg_header(msg, msg_hdr);
    // 'Trim' message header because server_rpc_unpack() expects it.
    msg = msg_trim(msg, trim);

    memset(unpacked, 0, sizeof(*unpacked));
    // Set hwloc instance pointer for use in server-side call.
    unpacked->hwloc = server->hwloc;

    return server_rpc_unpack(msg, msg_hdr, unpacked);
}

/**
 *
 */
static inline int
server_rpc_dispatch(
    const qvi_msg_header_t *msg_hdr,
    qvi_rpc_fun_data_t *fun_data
) {
    fun_data->rc = qvi_server_rpc_dispatch_table[msg_hdr->funid](fun_data);
    return QV_SUCCESS;
}

/**
 *
 */
static int
server_msg_recv(
    qvi_rpc_server_t *server,
    qvi_rpc_fun_data_t *fun_data
) {
    zmq_msg_t msg;
    int rc = zmq_msg_init(&msg);
    if (rc != 0) {
        qvi_log_error("zmq_msg_init() failed");
        return QV_ERR_MSG;
    }
    // Block until a message is available to be received from socket.
    rc = zmq_msg_recv(&msg, server->zmq_sock, 0);
    if (rc == -1) {
        qvi_log_error("zmq_msg_recv() failed");
        return QV_ERR_MSG;
    }

    qvi_msg_header_t msg_hdr;
    server_msg_unpack(server, &msg, &msg_hdr, fun_data);
    server_rpc_dispatch(&msg_hdr, fun_data);

    zmq_msg_close(&msg);
    return QV_SUCCESS;
}

/**
 *
 */
static int
server_msg_send(
    qvi_rpc_server_t *server,
    qvi_rpc_fun_data_t *fun_data
) {
    static const size_t sizeof_fun_data = sizeof(*fun_data);

    zmq_msg_t msg;
    int rc = zmq_msg_init_data(
        &msg,
        fun_data,
        sizeof_fun_data,
        nullptr,
        nullptr
    );
    if (rc != 0) {
        qvi_zmq_err_msg("zmq_msg_init_data() failed", errno);
        return QV_ERR_MSG;
    }
    const int nbytes_sent = zmq_msg_send(&msg, server->zmq_sock, 0);
    if (nbytes_sent != sizeof_fun_data) {
        qvi_zmq_err_msg("zmq_msg_send() truncated", errno);
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

/**
 *
 */
// See: http://api.zeromq.org/4-0:zmq-msg-recv
static int
server_go(
    qvi_rpc_server_t *server
) {
    int rc;
    qvi_rpc_fun_data_t fun_data;

    while (true) {
        rc = server_msg_recv(server, &fun_data);
        if (rc != QV_SUCCESS) return rc;
        rc = server_msg_send(server, &fun_data);
        if (rc != QV_SUCCESS) return rc;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static int
server_setup(
    qvi_rpc_server_t *server,
    const char *url
) {
    const int nwritten = snprintf(server->url, sizeof(server->url), "%s", url);
    if (nwritten >= QVI_RPC_URL_MAX_LEN) {
        qvi_log_error("snprintf() truncated");
        return QV_ERR_INTERNAL;
    }
    return QV_SUCCESS;
}

int
qvi_rpc_server_start(
    qvi_rpc_server_t *server,
    const char *url
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    rc = server_hwloc_init(server);
    if (rc != QV_SUCCESS) {
        ers = "server_hwloc_init() failed";
        goto out;
    }

    rc = server_setup(server, url);
    if (rc != QV_SUCCESS) {
        ers = "server_setup() failed";
        goto out;
    }

    rc = server_open_commchan(server);
    if (rc != QV_SUCCESS) {
        ers = "server_open_commchan() failed";
        goto out;
    }

    // TODO(skg) Add option to create in thread?
    rc = server_go(server);
    if (rc != QV_SUCCESS) {
        ers = "server_go() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

/**
 *
 */
static inline int
client_msg_append_header(
    qvi_byte_buffer_t *buff,
    qvi_msg_header_t *hdr
) {
    int rc = qvi_byte_buffer_append(buff, hdr, sizeof(*hdr));
    if (rc != QV_SUCCESS) {
        qvi_log_error("qvi_byte_buffer_append() failed");
    }
    return rc;
}

/**
 *
 */
int
client_rpc_pack_msg_prep(
    qvi_byte_buffer_t **buff,
    const qvi_rpc_funid_t funid,
    const qvi_rpc_argv_t argv
) {
    int rc = qvi_byte_buffer_construct(buff);
    if (rc != QV_SUCCESS) {
        qvi_log_error("qvi_byte_buffer_construct() failed");
        return rc;
    }

    qvi_msg_header_t msghdr = {funid, argv};
    rc = client_msg_append_header(*buff, &msghdr);
    if (rc != QV_SUCCESS) {
        qvi_byte_buffer_destruct(buff);
    }
    return rc;
}

/**
 *
 */
static int
client_rpc_pack(
    qvi_byte_buffer_t **buff,
    const qvi_rpc_funid_t funid,
    const qvi_rpc_argv_t argv,
    va_list args
) {
    char const *ers = nullptr;

    int rc = client_rpc_pack_msg_prep(buff, funid, argv);
    if (rc != QV_SUCCESS) {
        qvi_log_error("client_rpc_pack_msg_prep() failed");
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
                int value = va_arg(args, int);
                rc = qvi_byte_buffer_append(*buff, &value, sizeof(value));
                if (rc != QV_SUCCESS) {
                    ers = "QVI_RPC_TYPE_INT: qvi_byte_buffer_append() failed";
                    goto out;
                }
                break;
            }
            case QVI_RPC_TYPE_CSTR: {
                char *value = va_arg(args, char *);
                rc = qvi_byte_buffer_append(*buff, value, strlen(value) + 1);
                if (rc != QV_SUCCESS) {
                    ers = "QVI_RPC_TYPE_CSTR: qvi_byte_buffer_append() failed";
                    goto out;
                }
                break;
            }
            case QVI_RPC_TYPE_BITM: {
                // TODO(skg) Make sure this is the correct type
                hwloc_bitmap_t *value = va_arg(args, hwloc_bitmap_t *);
                QVI_UNUSED(value);
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
        qvi_log_error("{}", ers);
        qvi_byte_buffer_destruct(buff);
    }
    return rc;
}

/**
 *
 */
static void
client_msg_free_byte_buffer_cb(
    void *,
    void *hint
) {
    qvi_byte_buffer_t *buff = (qvi_byte_buffer_t *)hint;
    qvi_byte_buffer_destruct(&buff);
}

int
qvi_rpc_client_construct(
    qvi_rpc_client_t **client
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qvi_rpc_client_t *iclient = qvi_new qvi_rpc_client_t;
    if (!iclient) {
        qvi_log_error("memory allocation failed");
        return QV_ERR_OOR;
    }

    iclient->zmq_context = zmq_ctx_new();
    if (!iclient->zmq_context) {
        ers = "zmq_ctx_new() failed";
        rc = QV_ERR_MSG;
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rpc_client_destruct(&iclient);
    }
    *client = iclient;
    return rc;
}

void
qvi_rpc_client_destruct(
    qvi_rpc_client_t **client
) {
    qvi_rpc_client_t *iclient = *client;
    if (!iclient) return;

    if (iclient->zmq_sock) {
        int rc = zmq_close(iclient->zmq_sock);
        if (rc != 0) {
            int erno = errno;
            qvi_log_warn("zmq_close() failed with {}", qvi_strerr(erno));
        }
    }
    if (iclient->zmq_context) {
        int rc = zmq_ctx_destroy(iclient->zmq_context);
        if (rc != 0) {
            int erno = errno;
            qvi_log_warn("zmq_ctx_destroy() failed with {}", qvi_strerr(erno));
        }
    }

    delete iclient;
    *client = nullptr;
}

int
qvi_rpc_client_connect(
    qvi_rpc_client_t *client,
    const char *url
) {
    client->zmq_sock = zmq_socket(client->zmq_context, ZMQ_REQ);
    if (!client->zmq_sock) {
        qvi_zmq_err_msg("zmq_socket() failed", errno);
        return QV_ERR_MSG;
    }

    int rc = zmq_connect(client->zmq_sock, url);
    if (rc != 0) {
        qvi_zmq_err_msg("zmq_connect() failed", errno);
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

int
qvi_rpc_client_rep(
    qvi_rpc_client_t *client,
    qvi_rpc_fun_data_t *fun_data
) {
    int rc = QV_SUCCESS;

    zmq_msg_t msg;
    int zmq_rc = zmq_msg_init(&msg);
    if (zmq_rc != 0) {
        qvi_log_error("zmq_msg_init() failed");
        rc = QV_ERR_MSG;
        goto out;
    }
    // Block until a message is available to be received from socket.
    zmq_rc = zmq_msg_recv(&msg, client->zmq_sock, 0);
    // zmq_msg_recv() returns the message size or -1 on error.
    static const size_t esize = sizeof(*fun_data);
    if (zmq_rc != esize) {
        qvi_log_error(
            "zmq_msg_recv() failed: expected {} bytes, received {}",
            esize, zmq_rc
        );
        rc = QV_ERR_MSG;
        goto out;
    }
    memmove(fun_data, zmq_msg_data(&msg), esize);
out:
    zmq_msg_close(&msg);
    return rc;
}

int
qvi_rpc_client_req(
    qvi_rpc_client_t *client,
    const qvi_rpc_funid_t funid,
    const qvi_rpc_argv_t argv,
    ...
) {
    qvi_byte_buffer_t *buff;

    va_list vl;
    va_start(vl, argv);
    int rc = client_rpc_pack(&buff, funid, argv, vl);
    va_end(vl);
    // Do this here to make dealing with va_start()/va_end() easier.
    if (rc != QV_SUCCESS) {
        char const *ers = "client_rpc_pack() failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        return rc;
    }

    const size_t buffer_size = qvi_byte_buffer_size(buff);
    zmq_msg_t msg;
    int zmq_rc = zmq_msg_init_data(
        &msg,
        qvi_byte_buffer_data(buff),
        buffer_size,
        client_msg_free_byte_buffer_cb,
        buff
    );
    if (zmq_rc != 0) {
        qvi_zmq_err_msg("zmq_msg_init_data() failed", errno);
        return QV_ERR_MSG;
    }

    const size_t nbytes_sent = zmq_msg_send(&msg, client->zmq_sock, 0);
    if (nbytes_sent != buffer_size) {
        qvi_zmq_err_msg("zmq_msg_send() truncated", errno);
        return QV_ERR_MSG;
    }
    // Freeing up of buffer resources is done for us.
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
