/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
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

// TODO(skg) We need to figure out how best to handle errors in RPC code. For
// example, if a server-side error occurs, what do we do? Return locally or
// return the error via RPC to the caller?
//
// TODO(skg) We need to have empty RPC data types that can be returned to the
// caller in case of an non-fatal error on the server side.
//
// TODO(skg) We need to implement timeouts.
//
// TODO(skg) We need to version client/server RPC dispatch.

#include "qvi-rmi.h"
#include "qvi-bbuff.h"
#include "qvi-bbuff-rmi.h"
#include "qvi-hwpool.h"
#include "qvi-utils.h"

#include "zmq.h"

/**
 * Prints ZMQ error information. Defined as a macro so
 * that the line numbers correspond to the error site.
 */
#define qvi_zerr_msg(ers, erno)                                                \
do {                                                                           \
    qvi_log_error("{} with errno={} ({})", ers, (erno), qvi_strerr((erno)));   \
} while (0)

/**
 * Prints ZMQ warnings. Defined as a macro so that
 * the line numbers correspond to the warning site.
 */
#define qvi_zwrn_msg(ers, erno)                                                \
do {                                                                           \
    qvi_log_warn("{} with errno={} ({})", ers, (erno), qvi_strerr((erno)));    \
} while (0)

static constexpr cstr_t ZINPROC_ADDR = "inproc://qvi-rmi-workers";

typedef enum qvi_rpc_funid_e {
    FID_INVALID = 0,
    FID_SERVER_SHUTDOWN,
    FID_HELLO,
    FID_GBYE,
    FID_CPUBIND,
    FID_SET_CPUBIND,
    FID_OBJ_TYPE_DEPTH,
    FID_GET_NOBJS_IN_CPUSET,
    FID_GET_DEVICE_IN_CPUSET,
    FID_SCOPE_GET_INTRINSIC_HWPOOL
} qvi_rpc_funid_t;

typedef struct qvi_msg_header_s {
    qvi_rpc_funid_t fid = FID_INVALID;
    char picture[8] = {'\0'};
} qvi_msg_header_t;

/**
 * @note: The return value is used for operation status (e.g., was the internal
 * machinery successful?). The underlying target's return code is packed into
 * the message buffer and is meant for client-side consumption.
 */
typedef int (*qvi_rpc_fun_ptr_t)(
    qvi_rmi_server_t *,
    qvi_msg_header_t *,
    void *,
    qvi_bbuff **
);

static void
send_server_shutdown_msg(
    qvi_rmi_server_t *server
);

static void
zsocket_close(
    void **sock
) {
    if (qvi_unlikely(!sock)) return;
    void *isock = *sock;
    if (qvi_likely(isock)) {
        const int rc = zmq_close(isock);
        if (qvi_unlikely(rc != 0)) {
            qvi_zwrn_msg("zmq_close() failed", errno);
        }
    }
    *sock = nullptr;
}

static void
zctx_destroy(
    void **ctx
) {
    if (qvi_unlikely(!ctx)) return;
    void *ictx = *ctx;
    if (qvi_likely(ictx)) {
        const int rc = zmq_ctx_destroy(ictx);
        if (qvi_unlikely(rc != 0)) {
            qvi_zwrn_msg("zmq_ctx_destroy() failed", errno);
        }
    }
    *ctx = nullptr;
}

struct qvi_rmi_server_s {
    /** Server configuration. */
    qvi_rmi_config_s config;
    /** The base resource pool maintained by the server. */
    qvi_hwpool *hwpool = nullptr;
    /** ZMQ context. */
    void *zctx = nullptr;
    /** Loopback socket for managerial messages. */
    void *zlo = nullptr;
    /** The worker thread. */
    pthread_t worker_thread;
    /** Flag indicating if main thread blocks for workers to complete. */
    bool blocks = false;
    /** Constructor. */
    qvi_rmi_server_s(void)
    {
        zctx = zmq_ctx_new();
        if (qvi_unlikely(!zctx)) throw qvi_runtime_error();

        const int rc = qvi_new(&hwpool);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    }
    /** Destructor. */
    ~qvi_rmi_server_s(void)
    {
        send_server_shutdown_msg(this);
        zsocket_close(&zlo);
        zctx_destroy(&zctx);
        unlink(config.hwtopo_path.c_str());
        qvi_delete(&hwpool);
        if (!blocks) {
            pthread_join(worker_thread, nullptr);
        }
    }
};

struct qvi_rmi_client_s {
    /** Client configuration */
    qvi_rmi_config_s config;
    /** ZMQ context */
    void *zctx = nullptr;
    /** Communication socket */
    void *zsock = nullptr;
    /** Constructor. */
    qvi_rmi_client_s(void)
    {
        // Remember clients own the hwloc data, unlike the server.
        const int rc = qvi_hwloc_new(&config.hwloc);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
        // Create a new ZMQ context.
        zctx = zmq_ctx_new();
        if (qvi_unlikely(!zctx)) throw qvi_runtime_error();
    }
    /** Destructor. */
    ~qvi_rmi_client_s(void)
    {
        zsocket_close(&zsock);
        zctx_destroy(&zctx);
        qvi_hwloc_delete(&config.hwloc);
    }
};

int
qvi_rmi_server_new(
    qvi_rmi_server_t **server
) {
    return qvi_new(server);
}

void
qvi_rmi_server_delete(
    qvi_rmi_server_t **server
) {
    qvi_delete(server);
}

int
qvi_rmi_client_new(
    qvi_rmi_client_t **client
) {
    return qvi_new(client);
}

void
qvi_rmi_client_delete(
    qvi_rmi_client_t **client
) {
    qvi_delete(client);
}

qvi_hwloc_t *
qvi_rmi_client_hwloc(
    qvi_rmi_client_t *client
) {
    return client->config.hwloc;
}

static void *
zsocket_create_and_connect(
    void *zctx,
    int sock_type,
    const char *addr
) {
    void *zsock = zmq_socket(zctx, sock_type);
    if (qvi_unlikely(!zsock)) {
        qvi_zerr_msg("zmq_socket() failed", errno);
        return nullptr;
    }
    const int rc = zmq_connect(zsock, addr);
    if (qvi_unlikely(rc != 0)) {
        qvi_zerr_msg("zmq_connect() failed", errno);
        zsocket_close(&zsock);
        return nullptr;
    }
    return zsock;
}

static void *
zsocket_create_and_bind(
    void *zctx,
    int sock_type,
    const char *addr
) {
    void *zsock = zmq_socket(zctx, sock_type);
    if (qvi_unlikely(!zsock)) {
        qvi_zerr_msg("zmq_socket() failed", errno);
        return nullptr;
    }
    const int rc = zmq_bind(zsock, addr);
    if (qvi_unlikely(rc != 0)) {
        qvi_zerr_msg("zmq_bind() failed", errno);
        zsocket_close(&zsock);
        return nullptr;
    }
    return zsock;
}

/**
 * Buffer resource deallocation callback.
 */
static void
msg_free_byte_buffer_cb(
    void *,
    void *hint
) {
    qvi_bbuff *buff = (qvi_bbuff *)hint;
    qvi_bbuff_delete(&buff);
}

static int
buffer_append_header(
    qvi_bbuff *buff,
    qvi_rpc_funid_t fid,
    cstr_t picture
) {
    qvi_msg_header_t hdr;
    hdr.fid = fid;
    const int bcap = sizeof(hdr.picture);
    const int nw = snprintf(hdr.picture, bcap, "%s", picture);
    if (qvi_unlikely(nw >= bcap)) {
        qvi_log_error(
            "Debug picture buffer too small. Please submit a bug report."
        );
        return QV_ERR_INTERNAL;
    }
    return buff->append(&hdr, sizeof(hdr));
}

static inline void *
data_trim(
    void *msg,
    size_t trim
) {
    byte_t *new_base = (byte_t *)msg;
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

static inline int
zmsg_init_from_bbuff(
    qvi_bbuff *bbuff,
    zmq_msg_t *zmsg
) {
    const size_t buffer_size = bbuff->size();
    const int zrc = zmq_msg_init_data(
        zmsg, bbuff->data(), buffer_size,
        msg_free_byte_buffer_cb, bbuff
    );
    if (qvi_unlikely(zrc != 0)) {
        qvi_zerr_msg("zmq_msg_init_data() failed", errno);
        return QV_ERR_RPC;
    }
    return QV_SUCCESS;
}

static int
zmsg_send(
    void *zsock,
    zmq_msg_t *msg,
    int *bsent
) {
    int qvrc = QV_SUCCESS;
    *bsent = zmq_msg_send(msg, zsock, 0);
    if (qvi_unlikely(*bsent == -1)) {
        qvi_zerr_msg("zmq_msg_send() failed", errno);
        qvrc = QV_ERR_RPC;
    }
    if (qvi_unlikely(qvrc != QV_SUCCESS)) zmq_msg_close(msg);
    return qvrc;
}

static int
zmsg_recv(
    void *zsock,
    zmq_msg_t *mrx
) {
    int qvrc = QV_SUCCESS;
    int rc = zmq_msg_init(mrx);
    if (qvi_unlikely(rc != 0)) {
        qvi_zerr_msg("zmq_msg_init() failed", errno);
        qvrc = QV_ERR_RPC;
        goto out;
    }
    // Block until a message is available to be received from socket.
    rc = zmq_msg_recv(mrx, zsock, 0);
    if (qvi_unlikely(rc == -1)) {
        qvi_zerr_msg("zmq_msg_recv() failed", errno);
        qvrc = QV_ERR_RPC;
    }
out:
    if (qvi_unlikely(qvrc != QV_SUCCESS)) zmq_msg_close(mrx);
    return qvrc;
}

template <typename... Types>
static int
rpc_pack(
    qvi_bbuff **buff,
    qvi_rpc_funid_t fid,
    Types &&...args
) {
    std::string picture;

    qvi_bbuff *ibuff = nullptr;
    int rc = qvi_bbuff_new(&ibuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // Get the picture based on the types passed.
    qvi_bbuff_rmi_get_picture(picture, std::forward<Types>(args)...);
    // Fill and add header.
    rc = buffer_append_header(ibuff, fid, picture.c_str());
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;

    rc = qvi_bbuff_rmi_pack(ibuff, std::forward<Types>(args)...);
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_bbuff_delete(&ibuff);
    }
    *buff = ibuff;
    return rc;
}

template <typename... Types>
static int
rpc_unpack(
    void *data,
    Types &&...args
) {
    qvi_msg_header_t hdr;
    const size_t trim = unpack_msg_header(data, &hdr);
    // Get the picture based on the types passed.
    std::string picture;
    qvi_bbuff_rmi_get_picture(picture, std::forward<Types>(args)...);
    // Verify it matches the arguments provided.
    if (qvi_unlikely(strcmp(picture.c_str(), hdr.picture) != 0)) {
        qvi_log_error(
            "RPC pack/unpack type mismatch: "
            "expected \"{}\", but detected \"{}\" "
            "in RPC call to {}",
            hdr.picture,
            picture.c_str(),
            hdr.fid
        );
        return QV_ERR_RPC;
    }
    void *body = data_trim(data, trim);
    return qvi_bbuff_rmi_unpack(body, std::forward<Types>(args)...);
}

template <typename... Types>
static inline int
rpc_req(
    void *zsock,
    qvi_rpc_funid_t fid,
    Types &&...args
) {
    int buffer_size = 0;

    qvi_bbuff *buff = nullptr;
    int rc = rpc_pack(&buff, fid, std::forward<Types>(args)...);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_bbuff_delete(&buff);
        return rc;
    }
    zmq_msg_t msg;
    rc = zmsg_init_from_bbuff(buff, &msg);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // Cache buffer size here because our call to qvi_bbuff_size() after
    // zmsg_send() may be invalid because msg_free_byte_buffer_cb() may have
    // already been called.
    buffer_size = (int)buff->size();

    int nbytes_sent;
    rc = zmsg_send(zsock, &msg, &nbytes_sent);
    if (qvi_unlikely(nbytes_sent != buffer_size)) {
        qvi_zerr_msg("zmq_msg_send() truncated", errno);
        rc = QV_ERR_RPC;
    }
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) zmq_msg_close(&msg);
    // Else freeing of buffer and message resources is done for us.
    return rc;
}

template <typename... Types>
static inline int
rpc_rep(
    void *zsock,
    Types &&...args
) {
    zmq_msg_t msg;
    int rc = zmsg_recv(zsock, &msg);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    rc = rpc_unpack(zmq_msg_data(&msg), std::forward<Types>(args)...);
out:
    zmq_msg_close(&msg);
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Server-Side RPC Stub Definitions
////////////////////////////////////////////////////////////////////////////////
static int
rpc_ssi_invalid(
    qvi_rmi_server_t *,
    qvi_msg_header_t *,
    void *,
    qvi_bbuff **
) {
    qvi_log_error("Something bad happened in RPC dispatch.");
    qvi_abort();
    return QV_ERR_INVLD_ARG;
}

static int
rpc_ssi_shutdown(
    qvi_rmi_server_t *,
    qvi_msg_header_t *hdr,
    void *,
    qvi_bbuff **output
) {
    (void)rpc_pack(output, hdr->fid, QVI_BBUFF_RMI_ZERO_MSG);
    return QV_SUCCESS_SHUTDOWN;
}

static int
rpc_ssi_hello(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff **output
) {
    // TODO(skg) This will go into some registry somewhere.
    pid_t whoisit;
    const int rc = qvi_bbuff_rmi_unpack(input, &whoisit);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack relevant configuration information.
    return rpc_pack(
        output, hdr->fid,
        server->config.url,
        server->config.hwtopo_path
    );
}

static int
rpc_ssi_gbye(
    qvi_rmi_server_t *,
    qvi_msg_header_t *,
    void *,
    qvi_bbuff **
) {
    return QV_ERR_INVLD_ARG;
}

static int
rpc_ssi_cpubind(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff **output
) {
    pid_t who;
    int qvrc = qvi_bbuff_rmi_unpack(input, &who);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;

    hwloc_cpuset_t bitmap = nullptr;
    const int rpcrc = qvi_hwloc_task_get_cpubind(
        server->config.hwloc, who, &bitmap
    );
    qvrc = rpc_pack(output, hdr->fid, rpcrc, bitmap);
    qvi_hwloc_bitmap_delete(&bitmap);

    return qvrc;
}

static int
rpc_ssi_set_cpubind(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff **output
) {
    pid_t who;
    hwloc_cpuset_t cpuset = nullptr;
    const int qvrc = qvi_bbuff_rmi_unpack(input, &who, &cpuset);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;

    const int rpcrc = qvi_hwloc_task_set_cpubind_from_cpuset(
        server->config.hwloc, who, cpuset
    );
    qvi_hwloc_bitmap_delete(&cpuset);
    return rpc_pack(output, hdr->fid, rpcrc);
}

static int
rpc_ssi_obj_type_depth(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff **output
) {
    qv_hw_obj_type_t obj;
    const int qvrc = qvi_bbuff_rmi_unpack(
        input, &obj
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    int depth = 0;
    const int rpcrc = qvi_hwloc_obj_type_depth(
        server->config.hwloc, obj, &depth
    );

    return rpc_pack(output, hdr->fid, rpcrc, depth);
}

static int
rpc_ssi_get_nobjs_in_cpuset(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff **output
) {
    qv_hw_obj_type_t target_obj;
    hwloc_cpuset_t cpuset = nullptr;
    int qvrc = qvi_bbuff_rmi_unpack(
        input, &target_obj, &cpuset
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    int nobjs = 0;
    const int rpcrc = qvi_hwloc_get_nobjs_in_cpuset(
        server->config.hwloc, target_obj, cpuset, &nobjs
    );

    qvrc = rpc_pack(output, hdr->fid, rpcrc, nobjs);

    qvi_hwloc_bitmap_delete(&cpuset);
    return qvrc;
}

static int
rpc_ssi_get_device_in_cpuset(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff **output
) {
    qv_hw_obj_type_t dev_obj;
    int dev_i;
    hwloc_cpuset_t cpuset = nullptr;
    qv_device_id_type_t devid_type;
    int qvrc = qvi_bbuff_rmi_unpack(
        input, &dev_obj, &dev_i, &cpuset, &devid_type
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    char *dev_id = nullptr;
    int rpcrc = qvi_hwloc_get_device_id_in_cpuset(
        server->config.hwloc, dev_obj, dev_i, cpuset, devid_type, &dev_id
    );

    qvrc = rpc_pack(output, hdr->fid, rpcrc, dev_id);

    qvi_hwloc_bitmap_delete(&cpuset);
    free(dev_id);
    return qvrc;
}

static int
get_intrinsic_scope_user(
    qvi_rmi_server_t *server,
    pid_t,
    qvi_hwpool **hwpool
) {
    // TODO(skg) Is the cpuset the best way to do this?
    return qvi_hwpool::create(
        server->config.hwloc,
        qvi_hwloc_topo_get_cpuset(server->config.hwloc),
        hwpool
    );
}

static int
get_intrinsic_scope_proc(
    qvi_rmi_server_t *server,
    pid_t who,
    qvi_hwpool **hwpool
) {
    hwloc_cpuset_t cpuset = nullptr;
    int rc = qvi_hwloc_task_get_cpubind(
        server->config.hwloc, who, &cpuset
    );
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool::create(
        server->config.hwloc, cpuset, hwpool
    );
out:
    qvi_hwloc_bitmap_delete(&cpuset);
    if (rc != QV_SUCCESS) {
        qvi_delete(hwpool);
    }
    return rc;
}

// TODO(skg) Lots of error path cleanup is required.
static int
rpc_ssi_scope_get_intrinsic_hwpool(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff **output
) {
    // Get requestor task id (type and pid) and intrinsic scope as integers
    // from client request.
    pid_t requestor;
    qv_scope_intrinsic_t iscope;
    int rc = qvi_bbuff_rmi_unpack(input, &requestor, &iscope);
    if (rc != QV_SUCCESS) return rc;

    int rpcrc = QV_SUCCESS;
    // TODO(skg) Implement the rest.
    qvi_hwpool *hwpool = nullptr;
    switch (iscope) {
        case QV_SCOPE_SYSTEM:
        case QV_SCOPE_USER:
        case QV_SCOPE_JOB:
            rpcrc = get_intrinsic_scope_user(server, requestor, &hwpool);
            break;
        case QV_SCOPE_PROCESS:
            rpcrc = get_intrinsic_scope_proc(server, requestor, &hwpool);
            break;
        default:
            rpcrc = QV_ERR_INVLD_ARG;
            break;
    }
    // TODO(skg) Protect against errors above.
    rc = rpc_pack(output, hdr->fid, rpcrc, hwpool);

    qvi_delete(&hwpool);
    if (rc != QV_SUCCESS) {
        return rc;
    }
    return rpcrc;
}

/**
 * Maps a given qvi_rpc_funid_t to a given function pointer.
 */
static const std::map<qvi_rpc_funid_t, qvi_rpc_fun_ptr_t> rpc_dispatch_table = {
    {FID_INVALID, rpc_ssi_invalid},
    {FID_SERVER_SHUTDOWN, rpc_ssi_shutdown},
    {FID_HELLO, rpc_ssi_hello},
    {FID_GBYE, rpc_ssi_gbye},
    {FID_CPUBIND, rpc_ssi_cpubind},
    {FID_SET_CPUBIND, rpc_ssi_set_cpubind},
    {FID_OBJ_TYPE_DEPTH, rpc_ssi_obj_type_depth},
    {FID_GET_NOBJS_IN_CPUSET, rpc_ssi_get_nobjs_in_cpuset},
    {FID_GET_DEVICE_IN_CPUSET, rpc_ssi_get_device_in_cpuset},
    {FID_SCOPE_GET_INTRINSIC_HWPOOL, rpc_ssi_scope_get_intrinsic_hwpool}
};

static int
server_rpc_dispatch(
    qvi_rmi_server_t *server,
    zmq_msg_t *msg_in,
    zmq_msg_t *msg_out
) {
    int rc = QV_SUCCESS;
    bool shutdown = false;

    void *data = zmq_msg_data(msg_in);
    qvi_msg_header_t hdr;
    const size_t trim = unpack_msg_header(data, &hdr);
    void *body = data_trim(data, trim);

    const auto fidfunp = rpc_dispatch_table.find(hdr.fid);
    if (qvi_unlikely(fidfunp == rpc_dispatch_table.end())) {
        qvi_log_error("Unknown function ID ({}) in RPC. Aborting.", hdr.fid);
        rc = QV_ERR_RPC;
        goto out;
    }

    qvi_bbuff *res;
    rc = fidfunp->second(server, &hdr, body, &res);
    if (qvi_unlikely(rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN)) {
        cstr_t ers = "RPC dispatch failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        goto out;
    }
    // Shutdown?
    if (qvi_unlikely(rc == QV_SUCCESS_SHUTDOWN)) {
        shutdown = true;
    }
    rc = zmsg_init_from_bbuff(res, msg_out);
out:
    zmq_msg_close(msg_in);
    if (rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN) {
        zmq_msg_close(msg_out);
    }
    return (shutdown ? QV_SUCCESS_SHUTDOWN : rc);
}

static void *
server_go(
    void *data
) {
    qvi_rmi_server_t *const server = (qvi_rmi_server_t *)data;

    void *zworksock = zsocket_create_and_connect(
        server->zctx, ZMQ_REP, ZINPROC_ADDR
    );
    if (qvi_unlikely(!zworksock)) return nullptr;

    int rc, bsent;
    volatile int bsentt = 0;
    volatile std::atomic<bool> active{true};
    do {
        zmq_msg_t mrx, mtx;
        rc = zmsg_recv(zworksock, &mrx);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;
        rc = server_rpc_dispatch(server, &mrx, &mtx);
        if (qvi_unlikely(rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN)) break;
        if (qvi_unlikely(rc == QV_SUCCESS_SHUTDOWN)) active = false;
        rc = zmsg_send(zworksock, &mtx, &bsent);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;
        bsentt += bsent;
    } while(qvi_likely(active));
#if QVI_DEBUG_MODE == 1
    // Nice to understand messaging characteristics.
    qvi_log_debug("Server Sent {} bytes", bsentt);
#else
    QVI_UNUSED(bsentt);
#endif
    zsocket_close(&zworksock);
    if (qvi_unlikely(rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN)) {
        qvi_log_error("RX/TX loop exited with rc={} ({})", rc, qv_strerr(rc));
    }
    return nullptr;
}

static void
send_server_shutdown_msg(
    qvi_rmi_server_t *server
) {
    (void)rpc_req(server->zlo, FID_SERVER_SHUTDOWN, QVI_BBUFF_RMI_ZERO_MSG);
    (void)rpc_rep(server->zlo, QVI_BBUFF_RMI_ZERO_MSG);
}

int
qvi_rmi_server_config(
    qvi_rmi_server_t *server,
    qvi_rmi_config_s *config
) {
    server->config = *config;
    return QV_SUCCESS;
}

/**
 * Populates base hardware pool.
 */
static int
server_populate_base_hwpool(
    qvi_rmi_server_t *server
) {
    qvi_hwloc_t *const hwloc = server->config.hwloc;
    hwloc_const_cpuset_t cpuset = qvi_hwloc_topo_get_cpuset(hwloc);
    // The base resource pool will contain all available processors and devices.
    return server->hwpool->initialize(hwloc, cpuset);
}

static void *
server_start_threads(
    void *data
) {
    qvi_rmi_server_t *server = (qvi_rmi_server_t *)data;

    void *clients = zsocket_create_and_bind(
        server->zctx, ZMQ_ROUTER, server->config.url.c_str()
    );
    if (qvi_unlikely(!clients)) {
        cstr_t ers = "zsocket_create_and_bind() failed";
        qvi_log_error("{}", ers);
        return nullptr;
    }

    void *workers = zsocket_create_and_bind(
        server->zctx, ZMQ_DEALER, ZINPROC_ADDR
    );
    if (qvi_unlikely(!workers)) {
        cstr_t ers = "zsocket_create_and_bind() failed";
        qvi_log_error("{}", ers);
        return nullptr;
    }

    pthread_t worker;
    int rc = pthread_create(&worker, nullptr, server_go, server);
    if (qvi_unlikely(rc != 0)) {
        cstr_t ers = "pthread_create() failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qvi_strerr(rc));
    }
    // The zmq_proxy() function always returns -1 and errno set to ETERM.
    zmq_proxy(clients, workers, nullptr);
    pthread_join(worker, nullptr);
    zsocket_close(&workers);

    zsocket_close(&clients);
    return nullptr;
}

int
qvi_rmi_server_start(
    qvi_rmi_server_t *server,
    bool block
) {
    // First populate the base hardware resource pool.
    int qvrc = server_populate_base_hwpool(server);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;

    server->zlo = zsocket_create_and_connect(
        server->zctx, ZMQ_REQ, server->config.url.c_str()
    );
    if (qvi_unlikely(!server->zlo)) return QV_ERR_RPC;

    const int rc = pthread_create(
        &server->worker_thread, nullptr,
        server_start_threads, server
    );
    if (qvi_unlikely(rc != 0)) {
        cstr_t ers = "pthread_create() failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qvi_strerr(rc));
        qvrc = QV_ERR_SYS;
    }

    if (block && qvrc == QV_SUCCESS) {
        server->blocks = true;
        pthread_join(server->worker_thread, nullptr);
    }
    return qvrc;
}

static int
hello_handshake(
    qvi_rmi_client_t *client
) {
    const int rc = rpc_req(
        client->zsock, FID_HELLO, getpid()
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return rpc_rep(
        client->zsock,
        client->config.url,
        client->config.hwtopo_path
    );
}

int
qvi_rmi_client_connect(
    qvi_rmi_client_t *client,
    const std::string &url
) {
    client->zsock = zsocket_create_and_connect(
        client->zctx, ZMQ_REQ, url.c_str()
    );
    if (qvi_unlikely(!client->zsock)) return QV_ERR_RPC;

    int rc = hello_handshake(client);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = qvi_hwloc_topology_init(
        client->config.hwloc,
        client->config.hwtopo_path.c_str()
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return qvi_hwloc_topology_load(client->config.hwloc);
}

////////////////////////////////////////////////////////////////////////////////
// Client-Side (Public) RPC Stub Definitions
////////////////////////////////////////////////////////////////////////////////
int
qvi_rmi_cpubind(
    qvi_rmi_client_t *client,
    pid_t who,
    hwloc_cpuset_t *cpuset
) {
    int qvrc = rpc_req(client->zsock, FID_CPUBIND, who);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(client->zsock, &rpcrc, cpuset);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) {
        qvi_hwloc_bitmap_delete(cpuset);
        return qvrc;
    }
    return rpcrc;
}

int
qvi_rmi_set_cpubind(
    qvi_rmi_client_t *client,
    pid_t who,
    hwloc_const_cpuset_t cpuset
) {
    int qvrc = rpc_req(client->zsock, FID_SET_CPUBIND, who, cpuset);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(client->zsock, &rpcrc);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    return rpcrc;
}

int
qvi_rmi_get_intrinsic_hwpool(
    qvi_rmi_client_t *client,
    pid_t who,
    qv_scope_intrinsic_t iscope,
    qvi_hwpool **hwpool
) {
    *hwpool = nullptr;

    int qvrc = rpc_req(
        client->zsock,
        FID_SCOPE_GET_INTRINSIC_HWPOOL,
        who,
        iscope
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(
        client->zsock, &rpcrc, hwpool
    );
    if (qvrc != QV_SUCCESS) return qvrc;
    return rpcrc;
}

int
qvi_rmi_obj_type_depth(
    qvi_rmi_client_t *client,
    qv_hw_obj_type_t type,
    int *depth
) {
    int qvrc = rpc_req(
        client->zsock,
        FID_OBJ_TYPE_DEPTH,
        type
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(client->zsock, &rpcrc, depth);
    if (qvrc != QV_SUCCESS) return qvrc;

    return rpcrc;
}

int
qvi_rmi_get_nobjs_in_cpuset(
    qvi_rmi_client_t *client,
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
) {
    int qvrc = rpc_req(
        client->zsock,
        FID_GET_NOBJS_IN_CPUSET,
        target_obj,
        cpuset
    );
    if (qvrc != QV_SUCCESS) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(client->zsock, &rpcrc, nobjs);
    if (qvrc != QV_SUCCESS) return qvrc;

    return rpcrc;
}

int
qvi_rmi_get_device_in_cpuset(
    qvi_rmi_client_t *client,
    qv_hw_obj_type_t dev_obj,
    int dev_i,
    hwloc_const_cpuset_t cpuset,
    qv_device_id_type_t dev_id_type,
    char **dev_id
) {
    int qvrc = rpc_req(
        client->zsock,
        FID_GET_DEVICE_IN_CPUSET,
        dev_obj,
        dev_i,
        cpuset,
        dev_id_type
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(client->zsock, &rpcrc, dev_id);
    if (qvrc != QV_SUCCESS) return qvrc;

    return rpcrc;
}

int
qvi_rmi_get_cpuset_for_nobjs(
    qvi_rmi_client_t *client,
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t obj_type,
    int nobjs,
    hwloc_cpuset_t *result
) {
    // TODO(skg) At some point we will acquire the resources
    // for improved splitting and resource distribution.
    return qvi_hwloc_get_cpuset_for_nobjs(
        client->config.hwloc, cpuset,
        obj_type, nobjs, result
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
