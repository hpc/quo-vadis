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
 * @file qvi-rmi.cc
 *
 * Resource Management and Inquiry
 */

// TODO(skg) See: http://api.zeromq.org/4-0:zmq-msg-recv
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

#include "qvi-common.h"
#include "qvi-rmi.h"
#include "qvi-utils.h"
#include "qvi-bbuff.h"

#include "zmq.h"
#include "zmq_utils.h"

#define qvi_zerr_msg(ers, err_no)                                              \
do {                                                                           \
    int erno = (err_no);                                                       \
    qvi_log_error("{} with errno={} ({})", (ers), erno, qvi_strerr(erno));     \
} while (0)

#define qvi_zwrn_msg(ers, err_no)                                              \
do {                                                                           \
    int erno = (err_no);                                                       \
    qvi_log_warn("{} with errno={} ({})", (ers), erno, qvi_strerr(erno));      \
} while (0)

struct qvi_rmi_server_s {
    qvi_config_rmi_t *config = nullptr;
    void *zctx = nullptr;
    void *zsock = nullptr;
};

struct qvi_rmi_client_s {
    qvi_config_rmi_t *config = nullptr;
    void *zctx = nullptr;
    void *zsock = nullptr;
};

typedef enum qvi_rpc_funid_e {
    RPC_FID_INVALID = 0,
    RPC_FID_HELLO,
    RPC_FID_GBYE,
    RPC_FID_TASK_GET_CPUBIND,
} qvi_rpc_funid_t;

typedef struct qvi_msg_header_s {
    qvi_rpc_funid_t fid = RPC_FID_INVALID;
    char picture[8] = {'\0'};
} qvi_msg_header_t;

typedef void (*qvi_rpc_fun_ptr_t)(
    qvi_rmi_server_t *,
    qvi_msg_header_t *,
    void *,
    qvi_bbuff_t **
);

/**
 * Buffer resource deallocation callback.
 */
static void
msg_free_byte_buffer_cb(
    void *,
    void *hint
) {
    qvi_bbuff_t *buff = (qvi_bbuff_t *)hint;
    qvi_bbuff_free(&buff);
}

static int
buffer_append_header(
    qvi_bbuff_t *buff,
    qvi_rpc_funid_t fid,
    const char *picture
) {
    qvi_msg_header_t hdr;
    hdr.fid = fid;
    const int bcap = sizeof(hdr.picture);
    const int nw = snprintf(hdr.picture, bcap, "%s", picture);
    assert(nw < bcap);
    return qvi_bbuff_append(buff, &hdr, sizeof(hdr));
}

static inline void *
data_trim(
    void *msg,
    size_t trim
) {
    byte *new_base = (byte *)msg;
    new_base += trim;
    return new_base;
}

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
    qvi_bbuff_t **buff,
    qvi_rpc_funid_t fid,
    const char *picture,
    va_list args
) {
    int rc = QV_SUCCESS;
    *buff = nullptr;

    qvi_bbuff_t *ibuff;
    rc = qvi_bbuff_new(&ibuff);
    if (rc != QV_SUCCESS) {
        qvi_log_error("qvi_bbuff_new() failed");
        return rc;
    }
    // Fill and add header.
    rc = buffer_append_header(ibuff, fid, picture);
    if (rc != QV_SUCCESS) {
        qvi_log_error("buffer_append_header() failed");
        return rc;
    }
    rc = qvi_bbuff_vasprintf(ibuff, picture, args);
    if (rc == QV_SUCCESS) *buff = ibuff;
    return rc;
}

static int
rpc_pack(
    qvi_bbuff_t **buff,
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
    return qvi_data_vsscanf(body, picture, args);
}

static int
rpc_req(
    qvi_rmi_client_t *client,
    qvi_rpc_funid_t fid,
    const char *picture
    ...
) {
    int qvrc = QV_SUCCESS;
    qvi_bbuff_t *buff;

    va_list vl;
    va_start(vl, picture);
    int rc = rpc_vpack(&buff, fid, picture, vl);
    va_end(vl);
    // Do this here to make dealing with va_start()/va_end() easier.
    if (rc != QV_SUCCESS) {
        cstr ers = "rpc_vpack() failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_bbuff_free(&buff);
        return rc;
    }

    const size_t buffer_size = qvi_bbuff_size(buff);
    zmq_msg_t msg;
    int zrc = zmq_msg_init_data(
        &msg,
        qvi_bbuff_data(buff),
        buffer_size,
        msg_free_byte_buffer_cb,
        buff
    );
    if (zrc != 0) {
        qvi_zerr_msg("zmq_msg_init_data() failed", errno);
        qvrc = QV_ERR_MSG;
        goto out;
    }

    size_t nbytes_sent;
    nbytes_sent = zmq_msg_send(&msg, client->zsock, 0);
    if (nbytes_sent != buffer_size) {
        qvi_zerr_msg("zmq_msg_send() truncated", errno);
        qvrc = QV_ERR_MSG;
        goto out;
    }
out:
    if (qvrc != QV_SUCCESS) {
        zmq_msg_close(&msg);
    }
    // Else freeing of buffer and message resources is done for us.
    return qvrc;
}

static int
rpc_rep(
    qvi_rmi_client_t *client,
    const char *picture,
    ...
) {
    int rc = QV_SUCCESS;

    zmq_msg_t msg;
    int zrc = zmq_msg_init(&msg);
    if (zrc != 0) {
        qvi_zerr_msg("zmq_msg_init() failed", errno);
        rc = QV_ERR_MSG;
        goto out;
    }
    // Block until a message is available to be received from socket.
    zrc = zmq_msg_recv(&msg, client->zsock, 0);
    if (zrc == -1) {
        qvi_zerr_msg("zmq_msg_recv() failed", errno);
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

////////////////////////////////////////////////////////////////////////////////
// Server-Side RPC Stub Definitions
////////////////////////////////////////////////////////////////////////////////
static void
rpc_stub_invalid(
    qvi_rmi_server_t *,
    qvi_msg_header_t *,
    void *,
    qvi_bbuff_t **
) {
    qvi_log_error("Something bad happened in RPC dispatch.");
    assert(false);
}

static void
rpc_stub_hello(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff_t **output
) {
    // TODO(skg) This will go into some registry somewhere.
    int whoisit;
    qvi_data_sscanf(input, hdr->picture, &whoisit);

    rpc_pack(
        output,
        hdr->fid,
        "ss",
        server->config->url,
        server->config->hwtopo_path
    );
}

static void
rpc_stub_gbye(
    qvi_rmi_server_t *,
    qvi_msg_header_t *,
    void *,
    qvi_bbuff_t **
) {
}

static void
rpc_stub_task_get_cpubind(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff_t **output
) {
    int who;
    qvi_data_sscanf(input, hdr->picture, &who);

    int rpcrc;
    // TODO(skg) Improve.
    hwloc_bitmap_t bitmap;
    rpcrc = qvi_hwloc_task_get_cpubind(
        server->config->hwloc,
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
    rpc_stub_invalid,
    rpc_stub_hello,
    rpc_stub_gbye,
    rpc_stub_task_get_cpubind
};

static int
server_open_commchan(
    qvi_rmi_server_t *server
) {
    server->zsock = zmq_socket(server->zctx, ZMQ_REP);
    if (!server->zsock) {
        qvi_zerr_msg("zsocket() failed", errno);
        return QV_ERR_MSG;
    }

    int rc = zmq_bind(server->zsock, server->config->url);
    if (rc != 0) {
        qvi_zerr_msg("zmq_bind() failed", errno);
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

static int
server_rpc_dispatch(
    qvi_rmi_server_t *server,
    zmq_msg_t *msg_in,
    zmq_msg_t *msg_out
) {
    int qvrc = QV_SUCCESS;

    void *data = zmq_msg_data(msg_in);
    qvi_msg_header_t hdr;
    const size_t trim = unpack_msg_header(data, &hdr);
    void *body = data_trim(data, trim);

    qvi_bbuff_t *resbuff;
    qvi_server_rpc_dispatch_table[hdr.fid](server, &hdr, body, &resbuff);

    const size_t buffer_size = qvi_bbuff_size(resbuff);
    int zrc = zmq_msg_init_data(
        msg_out,
        qvi_bbuff_data(resbuff),
        buffer_size,
        msg_free_byte_buffer_cb,
        resbuff
    );
    if (zrc != 0) {
        qvi_zerr_msg("zmq_msg_init_data() failed", errno);
        qvrc = QV_ERR_MSG;
    }
    zmq_msg_close(msg_in);
    if (qvrc != QV_SUCCESS) zmq_msg_close(msg_out);
    return qvrc;
}

static int
server_msg_recv(
    qvi_rmi_server_t *server,
    zmq_msg_t *mrx
) {
    int rc = 0, qvrc = QV_SUCCESS;

    rc = zmq_msg_init(mrx);
    if (rc != 0) {
        qvi_zerr_msg("zmq_msg_init() failed", errno);
        qvrc = QV_ERR_MSG;
        goto out;
    }
    // Block until a message is available to be received from socket.
    rc = zmq_msg_recv(mrx, server->zsock, 0);
    if (rc == -1) {
        qvi_zerr_msg("zmq_msg_recv() failed", errno);
        qvrc = QV_ERR_MSG;
        goto out;
    }
out:
    if (qvrc != QV_SUCCESS) zmq_msg_close(mrx);
    return qvrc;
}

static inline int
server_msg_send(
    qvi_rmi_server_t *server,
    zmq_msg_t *msg
) {
    int qvrc = QV_SUCCESS;

    int rc = zmq_msg_send(msg, server->zsock, 0);
    if (rc == -1) {
        qvi_zerr_msg("zmq_msg_send() failed", errno);
        qvrc = QV_ERR_MSG;
    }
    if (qvrc != QV_SUCCESS) zmq_msg_close(msg);
    return qvrc;
}

static int
server_go(
    qvi_rmi_server_t *server
) {
    int rc;

    while (true) {
        zmq_msg_t mrx, mtx;
        rc = server_msg_recv(server, &mrx);
        if (rc != QV_SUCCESS) return rc;
        rc = server_rpc_dispatch(server, &mrx, &mtx);
        if (rc != QV_SUCCESS) return rc;
        rc = server_msg_send(server, &mtx);
        if (rc != QV_SUCCESS) return rc;
    }
    return QV_SUCCESS;
}

int
qvi_rmi_server_new(
    qvi_rmi_server_t **server
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    qvi_rmi_server_t *iserver = qvi_new qvi_rmi_server_t;
    if (!iserver) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_config_rmi_new(&iserver->config);
    if (rc != QV_SUCCESS) {
        ers = "qvi_config_rmi_new() failed";
        goto out;
    }

    iserver->zctx = zmq_ctx_new();
    if (!iserver->zctx) {
        ers = "zmq_ctx_new() failed";
        rc = QV_ERR_MSG;
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rmi_server_free(&iserver);
    }
    *server = iserver;
    return rc;
}

void
qvi_rmi_server_free(
    qvi_rmi_server_t **server
) {
    qvi_rmi_server_t *iserver = *server;
    if (!iserver) return;
    if (iserver->zsock) {
        int rc = zmq_close(iserver->zsock);
        if (rc != 0) {
            qvi_zwrn_msg("zmq_close() failed", errno);
        }
    }
    if (iserver->zctx) {
        int rc = zmq_ctx_destroy(iserver->zctx);
        if (rc != 0) {
            qvi_zwrn_msg("zmq_ctx_destroy() failed", errno);
        }
    }
    qvi_config_rmi_free(&iserver->config);
    delete iserver;
    *server = nullptr;
}

int
qvi_rmi_server_config(
    qvi_rmi_server_t *server,
    qvi_config_rmi_t *config
) {
    return qvi_config_rmi_cp(config, server->config);
}

int
qvi_rmi_server_start(
    qvi_rmi_server_t *server
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;

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
qvi_rmi_client_new(
    qvi_rmi_client_t **client
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    qvi_rmi_client_t *icli = qvi_new qvi_rmi_client_t;
    if (!icli) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_config_rmi_new(&icli->config);
    if (rc != QV_SUCCESS) {
        ers = "qvi_config_rmi_new() failed";
        goto out;
    }

    icli->zctx = zmq_ctx_new();
    if (!icli->zctx) {
        ers = "zmq_ctx_new() failed";
        rc = QV_ERR_MSG;
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rmi_client_free(&icli);
    }
    *client = icli;
    return rc;
}

void
qvi_rmi_client_free(
    qvi_rmi_client_t **client
) {
    qvi_rmi_client_t *iclient = *client;
    if (!iclient) return;
    if (iclient->zsock) {
        int rc = zmq_close(iclient->zsock);
        if (rc != 0) {
            qvi_zwrn_msg("zmq_close() failed", errno);
        }
    }
    if (iclient->zctx) {
        int rc = zmq_ctx_destroy(iclient->zctx);
        if (rc != 0) {
            qvi_zwrn_msg("zmq_ctx_destroy() failed", errno);
        }
    }
    delete iclient;
    *client = nullptr;
}

static int
hello_handshake(
    qvi_rmi_client_t *client
) {
    cstr ers = nullptr;

    int rc = rpc_req(
        client,
        RPC_FID_HELLO,
        "i",
        (int)getpid()
    );
    if (rc != QV_SUCCESS) {
        ers = "rpc_req() failed";
        goto out;
    }

    rc = rpc_rep(
        client,
        "ss",
        &client->config->url,
        &client->config->hwtopo_path
    );

    qvi_log_debug("URL {}, HTOPO {}", client->config->url, client->config->hwtopo_path);
out:
    return rc;
}

int
qvi_rmi_client_connect(
    qvi_rmi_client_t *client,
    const char *url
) {
    client->zsock = zmq_socket(client->zctx, ZMQ_REQ);
    if (!client->zsock) {
        qvi_zerr_msg("zsocket() failed", errno);
        return QV_ERR_MSG;
    }
    int rc = zmq_connect(client->zsock, url);
    if (rc != 0) {
        qvi_zerr_msg("zmq_connect() failed", errno);
        return QV_ERR_MSG;
    }
    hello_handshake(client);
    return QV_SUCCESS;
}

int
qvi_rmi_task_get_cpubind(
    qvi_rmi_client_t *client,
    pid_t who,
    hwloc_bitmap_t *out_bitmap
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;
    hwloc_bitmap_t bitmap = hwloc_bitmap_alloc();

    rc = rpc_req(
        client,
        RPC_FID_TASK_GET_CPUBIND,
        "i",
        (int)who
    );
    if (rc != QV_SUCCESS) {
        ers = "rpc_req() failed";
        goto out;
    }

    int rpcrc;
    char *bitmaps;
    rc = rpc_rep(
        client,
        "is",
        &rpcrc,
        &bitmaps
    );
    if (rc != QV_SUCCESS) {
        ers = "rpc_rep() failed";
        goto out;
    }
    hwloc_bitmap_sscanf(bitmap, bitmaps);
    *out_bitmap = bitmap;
    free(bitmaps);
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */
