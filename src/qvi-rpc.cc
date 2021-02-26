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

// TODO(skg) Better understand va_start rules and cleanup error paths.

#include "qvi-common.h"
#include "qvi-rpc.h"
#include "qvi-utils.h"

#include "zmq.h"
#include "zmq_utils.h"

#define qvi_zmq_err_msg(ers, err_no)                                           \
do {                                                                           \
    int erno = (err_no);                                                       \
    qvi_log_error("{} with errno={} ({})", (ers), erno, qvi_strerr(erno));     \
} while (0)

struct qvi_rpc_server_s {
    void *zmq_context = nullptr;
    void *zmq_sock = nullptr;
    qvi_hwloc_t *hwloc = nullptr;
    char url[QVI_URL_MAX] = {'\0'};
};

struct qvi_rpc_client_s {
    void *zmq_context = nullptr;
    void *zmq_sock = nullptr;
};

typedef struct qvi_msg_header_s {
    qvi_rpc_funid_t fid;
    char picture[16];
} qvi_msg_header_t;

typedef void (*qvi_rpc_fun_ptr_t)(
    qvi_rpc_server_t *,
    qvi_msg_header_t *,
    void *,
    qvi_byte_buffer_t **
);

static int
buff_vsscanf(
    void *buff,
    const char *picture,
    va_list args
) {
    int rc = QV_SUCCESS;
    byte *pos = (byte *)buff;

    const int npic = strlen(picture);
    for (int i = 0; i < npic; ++i) {
        if (picture[i] == 'i') {
            memmove(va_arg(args, int *), pos, sizeof(int));
            pos += sizeof(int);
            continue;
        }
        if (picture[i] == 's') {
            const int nw = asprintf(va_arg(args, char **), "%s", pos);
            if (nw == -1) {
                rc = QV_ERR_OOR;
                break;
            }
            pos += nw + 1;
            continue;
        }
        if (picture[i] == 'b') {
            void **data = va_arg(args, void **);
            size_t *dsize = va_arg(args, size_t *);
            memmove(dsize, pos, sizeof(*dsize));
            pos += sizeof(*dsize);
            *data = calloc(*dsize, sizeof(byte));
            if (!*data) {
                rc = QV_ERR_OOR;
                break;
            }
            memmove(*data, pos, *dsize);
            pos += *dsize;
            continue;
        }
        else {
            rc = QV_ERR_INVLD_ARG;
            break;
        }
    }
    return rc;
}

static int
buff_sscanf(
    void *buff,
    const char *picture,
    ...
) {
    va_list args;
    va_start(args, picture);
    int rc = buff_vsscanf(buff, picture, args);
    va_end(args);
    return rc;
}

static int
buff_vasprintf(
    qvi_byte_buffer_t *buff,
    const char *picture,
    va_list args
) {
    int rc = QV_SUCCESS;

    const int npic = strlen(picture);
    for (int i = 0; i < npic; ++i) {
        if (picture[i] == 'i') {
            int data = va_arg(args, int);
            rc = qvi_byte_buffer_append(buff, &data, sizeof(data));
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 's') {
            char *data = va_arg(args, char *);
            rc = qvi_byte_buffer_append(buff, data, strlen(data) + 1);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'b') {
            void *data = va_arg(args, void *);
            size_t dsize = va_arg(args, size_t);
            // We store size then data so unpack has an easier time, but keep
            // the user interface order as data then size because.
            rc = qvi_byte_buffer_append(buff, &dsize, sizeof(dsize));
            if (rc != QV_SUCCESS) break;
            rc = qvi_byte_buffer_append(buff, data, dsize);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        else {
            rc = QV_ERR_INVLD_ARG;
            break;
        }
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

/**
 *
 */
static int
buffer_append_header(
    qvi_byte_buffer_t *buff,
    qvi_rpc_funid_t fid,
    const char *picture
) {
    qvi_msg_header_t hdr;
    const int bcap = sizeof(hdr.picture);
    const int nw = snprintf(hdr.picture, bcap, "%s", picture);
    if (nw >= bcap) return QV_ERR_INTERNAL;
    hdr.fid = fid;
    return qvi_byte_buffer_append(buff, &hdr, sizeof(hdr));
}

/**
 *
 */
static inline void *
data_trim(
    void *msg,
    size_t trim
) {
    byte *new_base = (byte *)msg;
    new_base += trim;
    return new_base;
}

/**
 *
 */
static inline size_t
unpack_msg_header(
    void *data,
    qvi_msg_header_t *hdr
) {
    const size_t hdrsize = sizeof(*hdr);
    memmove(hdr, data, hdrsize);
    return hdrsize;
}

/**
 * i = int
 * s = char *
 * b = void *, size_t (two arguments)
 */
static int
rpc_vpack(
    qvi_byte_buffer_t **buff,
    qvi_rpc_funid_t fid,
    const char *picture,
    va_list args
) {
    int rc = QV_SUCCESS;
    *buff = nullptr;

    qvi_byte_buffer_t *ibuff;
    rc = qvi_byte_buffer_construct(&ibuff);
    if (rc != QV_SUCCESS) {
        qvi_log_error("qvi_byte_buffer_construct() failed");
        return rc;
    }
    // Fill and add header.
    rc = buffer_append_header(ibuff, fid, picture);
    if (rc != QV_SUCCESS) {
        qvi_log_error("buffer_append_header() failed");
        return rc;
    }
    rc = buff_vasprintf(ibuff, picture, args);
    if (rc == QV_SUCCESS) *buff = ibuff;
    return rc;
}

static int
rpc_pack(
    qvi_byte_buffer_t **buff,
    qvi_rpc_funid_t fid,
    const char *picture,
    ...
) {
    va_list vl;
    va_start(vl, picture);
    int rc = rpc_vpack(buff, fid, picture, vl);
    va_end(vl);
    return rc;
}

static int
rpc_vunpack(
    void *data,
    const char *picture,
    va_list args
) {
    qvi_msg_header_t hdr;
    size_t trim = unpack_msg_header(data, &hdr);
    void *body = data_trim(data, trim);
    return buff_vsscanf(body, picture, args);
}

////////////////////////////////////////////////////////////////////////////////
// Server-Side RPC Stub Definitions
////////////////////////////////////////////////////////////////////////////////
static void
rpc_stub_task_get_cpubind(
    qvi_rpc_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_byte_buffer_t **output
) {
    int who;
    buff_sscanf(input, hdr->picture, &who);

    int rpcrc;
    // TODO(skg) Improve.
    hwloc_bitmap_t bitmap;
    rpcrc = qvi_hwloc_task_get_cpubind(
        server->hwloc,
        who,
        &bitmap
    );
    char *bitmaps;
    hwloc_bitmap_asprintf(&bitmaps, bitmap);

    cstr picture = "is";
    rpc_pack(output, hdr->fid, picture, rpcrc, bitmaps);

    hwloc_bitmap_free(bitmap);
    free(bitmaps);
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
static int
server_hwloc_init(
    qvi_rpc_server_t *server
) {
    cstr ers = nullptr;

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
    cstr ers = nullptr;

    qvi_rpc_server_t *iserver = qvi_new qvi_rpc_server_t;
    if (!iserver) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
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

    qvi_hwloc_destruct(&iserver->hwloc);

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
static inline int
server_rpc_dispatch(
    qvi_rpc_server_t *server,
    void *data,
    zmq_msg_t *res
) {
    qvi_msg_header_t hdr;
    const size_t trim = unpack_msg_header(data, &hdr);
    void *body = data_trim(data, trim);

    qvi_byte_buffer_t *buff;
    qvi_server_rpc_dispatch_table[hdr.fid](server, &hdr, body, &buff);

    const size_t buffer_size = qvi_byte_buffer_size(buff);
    int zmq_rc = zmq_msg_init_data(
        res,
        qvi_byte_buffer_data(buff),
        buffer_size,
        client_msg_free_byte_buffer_cb,
        buff
    );
    if (zmq_rc != 0) {
        qvi_zmq_err_msg("zmq_msg_init_data() failed", errno);
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static int
server_msg_recv(
    qvi_rpc_server_t *server,
    zmq_msg_t *mrx,
    zmq_msg_t *mtx
) {
    int rc = 0, qvrc = QV_SUCCESS;

    rc = zmq_msg_init(mrx);
    if (rc != 0) {
        qvi_zmq_err_msg("zmq_msg_init() failed", errno);
        qvrc = QV_ERR_MSG;
        goto out;
    }
    // Block until a message is available to be received from socket.
    rc = zmq_msg_recv(mrx, server->zmq_sock, 0);
    if (rc == -1) {
        qvi_zmq_err_msg("zmq_msg_recv() failed", errno);
        qvrc = QV_ERR_MSG;
        goto out;
    }
    // Do RPC function dispatch.
    qvrc = server_rpc_dispatch(server, zmq_msg_data(mrx), mtx);
    if (qvrc != QV_SUCCESS) {
        qvi_log_error("server_rpc_dispatch() failed");
        goto out;
    }
out:
    zmq_msg_close(mrx);
    return qvrc;
}

/**
 *
 */
static int
server_msg_send(
    qvi_rpc_server_t *server,
    zmq_msg_t *msg
) {
    int rc = zmq_msg_send(msg, server->zmq_sock, 0);
    if (rc == -1) {
        qvi_zmq_err_msg("zmq_msg_send() failed", errno);
        return QV_ERR_MSG;
    }
    zmq_msg_close(msg);
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

    while (true) {
        zmq_msg_t mrx, mtx;
        rc = server_msg_recv(server, &mrx, &mtx);
        if (rc != QV_SUCCESS) return rc;
        rc = server_msg_send(server, &mtx);
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
    if (nwritten >= QVI_URL_MAX) {
        qvi_log_error("snprintf() truncated");
        return QV_ERR_INTERNAL;
    }
    return QV_SUCCESS;
}

// TODO(skg) Add option to load synthetic topologies.
int
qvi_rpc_server_start(
    qvi_rpc_server_t *server,
    const char *url
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;

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

int
qvi_rpc_client_construct(
    qvi_rpc_client_t **client
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    qvi_rpc_client_t *iclient = qvi_new qvi_rpc_client_t;
    if (!iclient) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
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
    const char *picture,
    ...
) {
    int rc = QV_SUCCESS;

    zmq_msg_t msg;
    int zrc = zmq_msg_init(&msg);
    if (zrc != 0) {
        qvi_log_error("zmq_msg_init() failed");
        rc = QV_ERR_MSG;
        goto out;
    }
    // Block until a message is available to be received from socket.
    zrc = zmq_msg_recv(&msg, client->zmq_sock, 0);
    if (zrc == -1) {
        rc = QV_ERR_MSG;
        goto out;
    }

    va_list vl;
    va_start(vl, picture);
    rc = rpc_vunpack(zmq_msg_data(&msg), picture, vl);
    va_end(vl);
out:
    zmq_msg_close(&msg);
    return rc;
}

int
qvi_rpc_client_req(
    qvi_rpc_client_t *client,
    qvi_rpc_funid_t fid,
    const char *picture
    ...
) {
    qvi_byte_buffer_t *buff;

    va_list args;
    va_start(args, picture);
    int rc = rpc_vpack(&buff, fid, picture, args);
    va_end(args);
    // Do this here to make dealing with va_start()/va_end() easier.
    if (rc != QV_SUCCESS) {
        cstr ers = "rpc_vpack() failed";
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
