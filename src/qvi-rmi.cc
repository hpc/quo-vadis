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

// TODO(skg) We need to figure out how best to handle errors in RPC code. For
// example, if a server-side error occurs, what do we do? Return locally or
// return the error via RPC to the caller?

#include "qvi-common.h"
#include "qvi-rmi.h"
#include "qvi-utils.h"
#include "qvi-bbuff-rmi.h"

#include "zmq.h"

#define ZINPROC_ADDR "inproc://qvi-rmi-workers"

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
    /** Server configuration */
    qvi_rmi_config_t *config = nullptr;
    /** ZMQ context */
    void *zctx = nullptr;
    /** Router (client-facing) socket */
    void *zrouter = nullptr;
    /** Loopback socket for managerial messages */
    void *zlo = nullptr;
    /** The worker thread */
    pthread_t worker_thread;
    /** Flag indicating if main thread blocks for workers to complete. */
    bool blocks = false;
};

struct qvi_rmi_client_s {
    /** Client configuration */
    qvi_rmi_config_t *config = nullptr;
    /** ZMQ context */
    void *zctx = nullptr;
    /** Communication socket */
    void *zsock = nullptr;
};

typedef enum qvi_rpc_funid_e {
    FID_INVALID = 0,
    FID_SERVER_SHUTDOWN,
    FID_HELLO,
    FID_GBYE,
    FID_TASK_GET_CPUBIND,
    FID_TASK_SET_CPUBIND_FROM_CPUSET,
    FID_GET_NOBJS_IN_CPUSET,
    FID_GET_DEVICE_IN_CPUSET,
    FID_SCOPE_GET_INTRINSIC_SCOPE_CPUSET
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
    qvi_bbuff_t **
);

static void
zctx_destroy(
    void **ctx
) {
    void *ictx = *ctx;
    if (!ictx) return;
    int rc = zmq_ctx_destroy(ictx);
    if (rc != 0) qvi_zwrn_msg("zmq_ctx_destroy() failed", errno);
    *ctx = nullptr;
}

static void
zsocket_close(
    void **sock
) {
    void *isock = *sock;
    if (!isock) return;
    int rc = zmq_close(isock);
    if (rc != 0) qvi_zwrn_msg("zmq_close() failed", errno);
    *sock = nullptr;
}

static void *
zsocket_create_and_connect(
    void *zctx,
    int sock_type,
    const char *addr
) {
    void *zsock = zmq_socket(zctx, sock_type);
    if (!zsock) qvi_zerr_msg("zmq_socket() failed", errno);

    int rc = zmq_connect(zsock, addr);
    if (rc != 0) {
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
    if (!zsock) qvi_zerr_msg("zmq_socket() failed", errno);

    int rc = zmq_bind(zsock, addr);
    if (rc != 0) {
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

static inline int
zmsg_init_from_bbuff(
    qvi_bbuff_t *bbuff,
    zmq_msg_t *zmsg
) {
    const size_t buffer_size = qvi_bbuff_size(bbuff);
    int zrc = zmq_msg_init_data(
        zmsg,
        qvi_bbuff_data(bbuff),
        buffer_size,
        msg_free_byte_buffer_cb,
        bbuff
    );
    if (zrc != 0) {
        qvi_zerr_msg("zmq_msg_init_data() failed", errno);
        return QV_ERR_MSG;
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
    if (*bsent == -1) {
        qvi_zerr_msg("zmq_msg_send() failed", errno);
        qvrc = QV_ERR_MSG;
    }
    if (qvrc != QV_SUCCESS) zmq_msg_close(msg);
    return qvrc;
}

static int
zmsg_recv(
    void *zsock,
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
    rc = zmq_msg_recv(mrx, zsock, 0);
    if (rc == -1) {
        qvi_zerr_msg("zmq_msg_recv() failed", errno);
        qvrc = QV_ERR_MSG;
        goto out;
    }
out:
    if (qvrc != QV_SUCCESS) zmq_msg_close(mrx);
    return qvrc;
}

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
    rc = qvi_bbuff_rmi_vsprintf(ibuff, picture, args);
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
    return qvi_data_rmi_vsscanf(body, picture, args);
}

static int
rpc_vreq(
    void *zsock,
    qvi_rpc_funid_t fid,
    const char *picture,
    va_list vl
) {
    int qvrc = QV_SUCCESS;
    int buffer_size = 0;

    qvi_bbuff_t *buff;
    int rc = rpc_vpack(&buff, fid, picture, vl);
    if (rc != QV_SUCCESS) {
        qvi_bbuff_free(&buff);
        return rc;
    }

    zmq_msg_t msg;
    rc = zmsg_init_from_bbuff(buff, &msg);
    if (rc != QV_SUCCESS) goto out;
    // Cache buffer size here because our call to qvi_bbuff_size() after
    // zmsg_send() may be invalid because msg_free_byte_buffer_cb() may have
    // already been called.
    buffer_size = (int)qvi_bbuff_size(buff);

    int nbytes_sent;
    qvrc = zmsg_send(zsock, &msg, &nbytes_sent);
    if (nbytes_sent != buffer_size) {
        qvi_zerr_msg("zmq_msg_send() truncated", errno);
        qvrc = QV_ERR_MSG;
        goto out;
    }
out:
    if (qvrc != QV_SUCCESS) zmq_msg_close(&msg);
    // Else freeing of buffer and message resources is done for us.
    return qvrc;
}

static int
rpc_req(
    void *zsock,
    qvi_rpc_funid_t fid,
    const char *picture
    ...
) {
    va_list vl;
    va_start(vl, picture);
    int rc = rpc_vreq(zsock, fid, picture, vl);
    va_end(vl);
    return rc;
}

static int
rpc_vrep(
    void *zsock,
    const char *picture,
    va_list vl
) {
    zmq_msg_t msg;
    int rc = zmsg_recv(zsock, &msg);
    if (rc != QV_SUCCESS) goto out;
    rc = rpc_vunpack(zmq_msg_data(&msg), picture, vl);
out:
    zmq_msg_close(&msg);
    return rc;
}

static int
rpc_rep(
    void *zsock,
    const char *picture,
    ...
) {
    va_list vl;
    va_start(vl, picture);
    int rc = rpc_vrep(zsock, picture, vl);
    va_end(vl);
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
    qvi_bbuff_t **
) {
    qvi_log_error("Something bad happened in RPC dispatch.");
    assert(false);
    return QV_ERR_INVLD_ARG;
}

static int
rpc_ssi_shutdown(
    qvi_rmi_server_t *,
    qvi_msg_header_t *hdr,
    void *,
    qvi_bbuff_t **output
) {
    (void)rpc_pack(output, hdr->fid, "z");
    return QV_SUCCESS_SHUTDOWN;
}

static int
rpc_ssi_hello(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff_t **output
) {
    // TODO(skg) This will go into some registry somewhere.
    int whoisit;
    int rc = qvi_data_rmi_sscanf(input, hdr->picture, &whoisit);
    if (rc != QV_SUCCESS) return rc;
    // Pack relevant configuration information.
    rc = rpc_pack(
        output,
        hdr->fid,
        QVI_RMI_CONFIG_PICTURE,
        server->config->url,
        server->config->hwtopo_path
    );
    return rc;
}

static int
rpc_ssi_gbye(
    qvi_rmi_server_t *,
    qvi_msg_header_t *,
    void *,
    qvi_bbuff_t **
) {
    return QV_ERR_INVLD_ARG;
}

static int
rpc_ssi_task_get_cpubind(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff_t **output
) {
    int who = 0;
    int qvrc = qvi_data_rmi_sscanf(input, hdr->picture, &who);
    if (qvrc != QV_SUCCESS) return qvrc;

    hwloc_cpuset_t bitmap = nullptr;
    int rpcrc = qvi_hwloc_task_get_cpubind(
        server->config->hwloc,
        who,
        &bitmap
    );

    qvrc = rpc_pack(output, hdr->fid, "ic", rpcrc, bitmap);
    hwloc_bitmap_free(bitmap);

    return qvrc;
}

static int
rpc_ssi_task_set_cpubind_from_cpuset(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff_t **output
) {
    int who = 0;
    hwloc_cpuset_t cpuset = nullptr;
    int qvrc = qvi_data_rmi_sscanf(input, hdr->picture, &who, &cpuset);
    if (qvrc != QV_SUCCESS) return qvrc;

    int rpcrc = qvi_hwloc_task_set_cpubind_from_cpuset(
        server->config->hwloc,
        who,
        cpuset
    );
    hwloc_bitmap_free(cpuset);

    return rpc_pack(output, hdr->fid, "i", rpcrc);
}

static int
rpc_ssi_get_nobjs_in_cpuset(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff_t **output
) {
    int target_obj_ai = 0;
    hwloc_cpuset_t cpuset = nullptr;
    int qvrc = qvi_data_rmi_sscanf(
        input,
        hdr->picture,
        &target_obj_ai,
        &cpuset
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    unsigned nobjs = 0;
    int rpcrc = qvi_hwloc_get_nobjs_in_cpuset(
        server->config->hwloc,
        (qv_hw_obj_type_t)target_obj_ai,
        cpuset,
        &nobjs
    );

    qvrc = rpc_pack(output, hdr->fid, "iu", rpcrc, nobjs);

    hwloc_bitmap_free(cpuset);
    return qvrc;
}

static int
rpc_ssi_get_device_in_cpuset(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff_t **output
) {
    int dev_obj_ai = 0, dev_i = 0, dev_id_type_ai = 0;
    hwloc_cpuset_t cpuset = nullptr;
    int qvrc = qvi_data_rmi_sscanf(
        input,
        hdr->picture,
        &dev_obj_ai,
        &dev_i,
        &cpuset,
        &dev_id_type_ai
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    char *dev_id = nullptr;
    int rpcrc = qvi_hwloc_get_device_in_cpuset(
        server->config->hwloc,
        (qv_hw_obj_type_t)dev_obj_ai,
        dev_i,
        cpuset,
        (qv_device_id_type_t)dev_id_type_ai,
        &dev_id
    );

    qvrc = rpc_pack(output, hdr->fid, "is", rpcrc, dev_id);

    hwloc_bitmap_free(cpuset);
    free(dev_id);
    return qvrc;
}

// TODO(skg) Lots of error path cleanup is required.
static int
rpc_ssi_scope_get_intrinsic_scope_cpuset(
    qvi_rmi_server_t *server,
    qvi_msg_header_t *hdr,
    void *input,
    qvi_bbuff_t **output
) {
    hwloc_topology_t topo = qvi_hwloc_topo_get(server->config->hwloc);
    // Get requestor PID, intrinsic scope as integers from client request.
    int pid, iscope;
    int rc = qvi_data_rmi_sscanf(input, hdr->picture, &pid, &iscope);
    if (rc != QV_SUCCESS) return rc;

    int rpcrc = QV_SUCCESS;
    hwloc_cpuset_t cpuset = nullptr;
    // TODO(skg) Implement the rest.
    switch ((qv_scope_intrinsic_t)iscope) {
        case QV_SCOPE_SYSTEM:
        case QV_SCOPE_USER:
        case QV_SCOPE_JOB: {
            // TODO(skg) Deal with errors.
            rc = qvi_hwloc_bitmap_alloc(&cpuset);
            if (rc != QV_SUCCESS) return rc;
            rc = qvi_hwloc_bitmap_copy(
                hwloc_topology_get_topology_cpuset(topo),
                cpuset
            );
            if (rc != QV_SUCCESS) return rc;
            break;
        }
        case QV_SCOPE_PROCESS:
            rc = qvi_hwloc_task_get_cpubind(
                server->config->hwloc,
                pid,
                &cpuset
            );
            if (rc != QV_SUCCESS) return rc;
            break;
        default:
            rpcrc = QV_ERR_INVLD_ARG;
            break;
    }

    rc = rpc_pack(output, hdr->fid, "ic", rpcrc, cpuset);
    if (rc != QV_SUCCESS) return rc;

    hwloc_bitmap_free(cpuset);

    return rc;
}

/**
 * Maps a given qvi_rpc_funid_t to a given function pointer. Must be kept in
 * sync with qvi_rpc_funid_t.
 */
static const qvi_rpc_fun_ptr_t rpc_dispatch_table[] = {
    rpc_ssi_invalid,
    rpc_ssi_shutdown,
    rpc_ssi_hello,
    rpc_ssi_gbye,
    rpc_ssi_task_get_cpubind,
    rpc_ssi_task_set_cpubind_from_cpuset,
    rpc_ssi_get_nobjs_in_cpuset,
    rpc_ssi_get_device_in_cpuset,
    rpc_ssi_scope_get_intrinsic_scope_cpuset
};

static int
server_open_clichans(
    qvi_rmi_server_t *server
) {
    server->zrouter = zsocket_create_and_bind(
        server->zctx,
        ZMQ_ROUTER,
        server->config->url
    );
    if (!server->zrouter) return QV_ERR_MSG;

    server->zlo = zsocket_create_and_connect(
        server->zctx,
        ZMQ_REQ,
        server->config->url
    );
    if (!server->zlo) return QV_ERR_MSG;

    return QV_SUCCESS;
}

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

    qvi_bbuff_t *res;
    rc = rpc_dispatch_table[hdr.fid](server, &hdr, body, &res);
    if (rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN) {
        cstr ers = "RPC dispatch failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        goto out;
    }
    // Shutdown?
    if (rc == QV_SUCCESS_SHUTDOWN) shutdown = true;

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
    qvi_rmi_server_t *server = (qvi_rmi_server_t *)data;
    void *zworksock = zsocket_create_and_connect(
        server->zctx,
        ZMQ_REP,
        ZINPROC_ADDR
    );
    if (!zworksock) return nullptr;

    int rc, bsent, bsentt = 0;
    bool active = true;
    do {
        zmq_msg_t mrx, mtx;
        rc = zmsg_recv(zworksock, &mrx);
        if (rc != QV_SUCCESS) break;
        rc = server_rpc_dispatch(server, &mrx, &mtx);
        if (rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN) break;
        if (rc == QV_SUCCESS_SHUTDOWN) active = false;
        rc = zmsg_send(zworksock, &mtx, &bsent);
        if (rc != QV_SUCCESS) break;
        bsentt += bsent;
    } while(active);
    // Nice to understand messaging characteristics.
    qvi_log_debug("Server Sent {} bytes", bsentt);
    zsocket_close(&zworksock);
    if (rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN) {
        qvi_log_error("RX/TX loop exited with rc={} ({})", rc, qv_strerr(rc));
    }
    return nullptr;
}

int
qvi_rmi_config_new(
    qvi_rmi_config_t **config
) {
    int rc = QV_SUCCESS;
    qvi_rmi_config_t *ic = (qvi_rmi_config_t *)calloc(1, sizeof(*ic));
    if (!ic) rc = QV_ERR_OOR;
    if (rc != QV_SUCCESS) qvi_rmi_config_free(&ic);
    *config = ic;
    return rc;
}

void
qvi_rmi_config_free(
    qvi_rmi_config_t **config
) {
    if (!config) return;
    qvi_rmi_config_t *ic = *config;
    if (!ic) return;
    if (ic->url) free(ic->url);
    if (ic->hwtopo_path) free(ic->hwtopo_path);
    free(ic);
    *config = nullptr;
}

int
qvi_rmi_config_cp(
    qvi_rmi_config_t *from,
    qvi_rmi_config_t *to
) {
    to->hwloc = from->hwloc;
    int nw = asprintf(&to->url, "%s", from->url);
    if (nw == -1) return QV_ERR_OOR;
    nw = asprintf(&to->hwtopo_path, "%s", from->hwtopo_path);
    if (nw == -1) return QV_ERR_OOR;
    return QV_SUCCESS;
}

int
qvi_rmi_config_pack(
    qvi_rmi_config_t *config,
    qvi_bbuff_t *buff
) {
    int rc = qvi_bbuff_rmi_sprintf(
        buff,
        QVI_RMI_CONFIG_PICTURE,
        config->url,
        config->hwtopo_path
    );
    return rc;
}

int
qvi_rmi_config_unpack(
    void *buff,
    qvi_rmi_config_t **config
) {
    int rc = qvi_rmi_config_new(config);
    if (rc != QV_SUCCESS) return rc;
    rc = qvi_data_rmi_sscanf(
        buff,
        QVI_RMI_CONFIG_PICTURE,
        &(*config)->url,
        &(*config)->hwtopo_path
    );
    return rc;
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

    rc = qvi_rmi_config_new(&iserver->config);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_config_new() failed";
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

static void
send_server_shutdown_msg(
    qvi_rmi_server_t *server
) {
    (void)rpc_req(server->zlo, FID_SERVER_SHUTDOWN, "z");
    (void)rpc_rep(server->zlo, "z");
}

void
qvi_rmi_server_free(
    qvi_rmi_server_t **server
) {
    if (!server) return;
    qvi_rmi_server_t *iserver = *server;
    if (!iserver) return;
    send_server_shutdown_msg(iserver);
    zsocket_close(&iserver->zlo);
    zsocket_close(&iserver->zrouter);
    zctx_destroy(&iserver->zctx);
    unlink(iserver->config->hwtopo_path);
    qvi_rmi_config_free(&iserver->config);
    if (!iserver->blocks) {
        pthread_join(iserver->worker_thread, nullptr);
    }
    delete iserver;
    *server = nullptr;
}

int
qvi_rmi_server_config(
    qvi_rmi_server_t *server,
    qvi_rmi_config_t *config
) {
    return qvi_rmi_config_cp(config, server->config);
}

static void *
server_start_workers(
    void *data
) {
    qvi_rmi_server_t *server = (qvi_rmi_server_t *)data;

    void *zdealer = zsocket_create_and_bind(
        server->zctx,
        ZMQ_DEALER,
        ZINPROC_ADDR
    );
    if (!zdealer) return nullptr;

    pthread_t worker;
    int rc = pthread_create(&worker, nullptr, server_go, server);
    if (rc != 0) {
        cstr ers = "pthread_create() failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qvi_strerr(rc));
        return nullptr;
    }
    // The zmq_proxy() function always returns -1 and errno set to ETERM.
    zmq_proxy(server->zrouter, zdealer, nullptr);
    pthread_join(worker, nullptr);
    zsocket_close(&zdealer);

    return nullptr;
}

int
qvi_rmi_server_start(
    qvi_rmi_server_t *server,
    bool block
) {
    // Main thread opens channels used to communicate with clients.
    int rc = server_open_clichans(server);
    if (rc != QV_SUCCESS) return rc;
    // Start workers in new thread.
    rc = pthread_create(
        &server->worker_thread,
        nullptr,
        server_start_workers,
        server
    );
    if (rc != 0) {
        cstr ers = "pthread_create() failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qvi_strerr(rc));
        rc = QV_ERR_SYS;
    }
    if (block && rc == QV_SUCCESS) {
        server->blocks = true;
        pthread_join(server->worker_thread, nullptr);
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

    rc = qvi_rmi_config_new(&icli->config);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_config_new() failed";
        goto out;
    }
    // Remember clients own the hwloc data, unlike the server.
    rc = qvi_hwloc_new(&icli->config->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_new() failed";
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
    if (!client) return;
    qvi_rmi_client_t *iclient = *client;
    if (!iclient) return;
    zsocket_close(&iclient->zsock);
    zctx_destroy(&iclient->zctx);
    qvi_hwloc_free(&iclient->config->hwloc);
    qvi_rmi_config_free(&iclient->config);
    delete iclient;
    *client = nullptr;
}

static int
hello_handshake(
    qvi_rmi_client_t *client
) {
    int rc = rpc_req(client->zsock, FID_HELLO, "i", (int)getpid());
    if (rc != QV_SUCCESS) return rc;

    qvi_rmi_config_t *config = client->config;
    return rpc_rep(
        client->zsock,
        QVI_RMI_CONFIG_PICTURE,
        &config->url,
        &config->hwtopo_path
    );
}

int
qvi_rmi_client_connect(
    qvi_rmi_client_t *client,
    const char *url
) {
    client->zsock = zsocket_create_and_connect(
        client->zctx,
        ZMQ_REQ,
        url
    );
    if (!client->zsock) return QV_ERR_MSG;

    int rc = hello_handshake(client);
    if (rc != QV_SUCCESS) return rc;

    rc = qvi_hwloc_topology_init(
        client->config->hwloc,
        client->config->hwtopo_path
    );
    if (rc != QV_SUCCESS) return rc;

    return qvi_hwloc_topology_load(client->config->hwloc);
}

qvi_hwloc_t *
qvi_rmi_client_hwloc_get(
    qvi_rmi_client_t *client
) {
    return client->config->hwloc;
}

////////////////////////////////////////////////////////////////////////////////
// Client-Side (Public) RPC Stub Definitions
////////////////////////////////////////////////////////////////////////////////
int
qvi_rmi_task_get_cpubind(
    qvi_rmi_client_t *client,
    pid_t who,
    hwloc_cpuset_t *cpuset
) {
    int qvrc = rpc_req(client->zsock, FID_TASK_GET_CPUBIND, "i", (int)who);
    if (qvrc != QV_SUCCESS) return qvrc;

    int rpcrc;
    qvrc = rpc_rep(client->zsock, "ic", &rpcrc, cpuset);
    if (qvrc != QV_SUCCESS) return qvrc;

    return rpcrc;
}

int
qvi_rmi_task_set_cpubind_from_cpuset(
    qvi_rmi_client_t *client,
    pid_t who,
    hwloc_const_cpuset_t cpuset
) {
    int qvrc = rpc_req(
        client->zsock,
        FID_TASK_SET_CPUBIND_FROM_CPUSET,
        "ic",
        (int)who,
        cpuset
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    int rpcrc;
    qvrc = rpc_rep(client->zsock, "i", &rpcrc);
    if (qvrc != QV_SUCCESS) return qvrc;

    return rpcrc;
}

int
qvi_rmi_scope_get_intrinsic_scope_cpuset(
    qvi_rmi_client_t *client,
    pid_t requestor_pid,
    qv_scope_intrinsic_t iscope,
    hwloc_cpuset_t cpuset
) {
    int qvrc = rpc_req(
        client->zsock,
        FID_SCOPE_GET_INTRINSIC_SCOPE_CPUSET,
        "ii",
        (int)requestor_pid,
        (int)iscope
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    int rpcrc;
    hwloc_cpuset_t rcpuset = nullptr;
    qvrc = rpc_rep(client->zsock, "ic", &rpcrc, &rcpuset);
    if (qvrc != QV_SUCCESS) return qvrc;

    qvrc = qvi_hwloc_bitmap_copy(rcpuset, cpuset);
    hwloc_bitmap_free(rcpuset);

    if (qvrc != QV_SUCCESS) return qvrc;
    return rpcrc;
}

int
qvi_rmi_get_nobjs_in_cpuset(
    qvi_rmi_client_t *client,
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    unsigned *nobjs
) {
    int qvrc = rpc_req(
        client->zsock,
        FID_GET_NOBJS_IN_CPUSET,
        "ic",
        (int)target_obj,
        cpuset
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    int rpcrc;
    qvrc = rpc_rep(client->zsock, "iu", &rpcrc, nobjs);
    if (qvrc != QV_SUCCESS) return qvrc;

    return rpcrc;
}

int
qvi_rmi_get_device_in_cpuset(
    qvi_rmi_client_t *client,
    qv_hw_obj_type_t dev_obj,
    int dev_i,
    hwloc_cpuset_t cpuset,
    qv_device_id_type_t dev_id_type,
    char **dev_id
) {
    int qvrc = rpc_req(
        client->zsock,
        FID_GET_DEVICE_IN_CPUSET,
        "iici",
        (int)dev_obj,
        dev_i,
        cpuset,
        (int)dev_id_type
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    int rpcrc;
    qvrc = rpc_rep(client->zsock, "is", &rpcrc, dev_id);
    if (qvrc != QV_SUCCESS) return qvrc;

    return rpcrc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */
