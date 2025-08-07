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

// TODO(skg) We need to version client/server RPC dispatch.

#include "qvi-rmi.h"
#include "qvi-bbuff.h"
#include "qvi-bbuff-rmi.h"
#include "qvi-utils.h"

// Indicates whether the server has been signaled to shutdown.
static volatile std::sig_atomic_t g_server_shutdown_signaled(false);

/** Port environment variable string. */
static const std::string PORT_ENV_VAR = "QV_PORT";

struct qvi_rmi_msg_header {
    qvi_rmi_rpc_fid_t fid = QVI_RMI_FID_INVALID;
};

/**
 * Prints ZMQ error information. Defined as a macro so
 * that the line numbers correspond to the error site.
 */
#define zerr_msg(ers, erno)                                                   \
do {                                                                          \
    qvi_log_error("{} with errno={} ({})", ers, erno, strerror(erno));        \
} while (0)

/**
 * Prints ZMQ warnings. Defined as a macro so that
 * the line numbers correspond to the warning site.
 */
#define zwrn_msg(ers, erno)                                                   \
do {                                                                          \
    qvi_log_warn("{} with errno={} ({})", ers, erno, strerror(erno));         \
} while (0)

static void
server_signal_handler(
    int
) {
    g_server_shutdown_signaled = true;
}

static inline void
zsocket_close(
    void *sock
) {
    if (qvi_unlikely(!sock)) return;
    if (qvi_likely(sock)) {
        const int rc = zmq_close(sock);
        if (qvi_unlikely(rc != 0)) {
            const int eno = errno;
            zwrn_msg("zmq_close() failed", eno);
        }
    }
}

static inline void
zctx_destroy(
    void **ctx
) {
    if (qvi_unlikely(!ctx)) return;
    void *ictx = *ctx;
    if (qvi_likely(ictx)) {
        const int rc = zmq_ctx_destroy(ictx);
        if (qvi_unlikely(rc != 0)) {
            const int eno = errno;
            zwrn_msg("zmq_ctx_destroy() failed", eno);
        }
    }
    *ctx = nullptr;
}

static inline void *
zsocket_create(
    void *zctx,
    int sock_type
) {
    void *zsock = zmq_socket(zctx, sock_type);
    if (qvi_unlikely(!zsock)) {
        const int eno = errno;
        zerr_msg("zmq_socket() failed", eno);
        return nullptr;
    }
    return zsock;
}

static inline int
zsocket_connect(
    void *zsock,
    const char *addr
) {
    const int rc = zmq_connect(zsock, addr);
    if (qvi_unlikely(rc != 0)) {
        const int eno = errno;
        zerr_msg("zmq_connect() failed", eno);
        zsocket_close(zsock);
        return QV_ERR_RPC;
    }
    return QV_SUCCESS;
}

static inline void *
zsocket_create_and_bind(
    void *zctx,
    int sock_type,
    const char *addr
) {
    void *zsock = zmq_socket(zctx, sock_type);
    if (qvi_unlikely(!zsock)) {
        const int eno = errno;
        zerr_msg("zmq_socket() failed", eno);
        return nullptr;
    }
    const int rc = zmq_bind(zsock, addr);
    if (qvi_unlikely(rc != 0)) {
        const int eno = errno;
        zerr_msg("zmq_bind() failed", eno);
        zsocket_close(zsock);
        return nullptr;
    }
    return zsock;
}

static inline int
buffer_append_header(
    qvi_bbuff *buff,
    qvi_rmi_rpc_fid_t fid
) {
    qvi_rmi_msg_header hdr;
    hdr.fid = fid;
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
    qvi_rmi_msg_header *hdr
) {
    const size_t hdrsize = sizeof(*hdr);
    memmove(hdr, data, hdrsize);
    return hdrsize;
}

static inline int
zsock_send_bbuff(
    void *zsock,
    qvi_bbuff *bbuff,
    int *bsent
) {
    const int buff_size = bbuff->size();
    *bsent = zmq_send(zsock, bbuff->data(), buff_size, 0);
    if (qvi_unlikely(*bsent != buff_size)) {
        const int eno = errno;
        zerr_msg("zmq_send() truncated", eno);
        return QV_ERR_RPC;
    }
    // We are responsible for freeing the buffer after ZMQ has
    // taken ownership of its contents after zmq_send() returns.
    qvi_delete(&bbuff);
    return QV_SUCCESS;
}

int
qvi_rmi_server::m_recv_msg(
    zmq_msg_t *mrx
) {
    int qvrc = QV_SUCCESS;

    int zrc = zmq_msg_init(mrx);
    if (qvi_unlikely(zrc != 0)) {
        const int eno = errno;
        zerr_msg("zmq_msg_init() failed", eno);
        return QV_ERR_RPC;
    }

    const int npoll_items = 1;
    zmq_pollitem_t poll_items[npoll_items] = {
        // Monitor m_zsock for incoming messages (recv).
        {m_zsock, 0, ZMQ_POLLIN, 0}
    };

    do {
        if (qvi_unlikely(g_server_shutdown_signaled)) {
            qvrc = QV_SUCCESS_SHUTDOWN;
            break;
        }
        // Poll for events with a timeout of 1000ms.
        zrc = zmq_poll(poll_items, npoll_items, 1000);
        if (qvi_unlikely(zrc == -1)) {
            const int eno = errno;
            // Poll interrupted by delivery of a signal before any events were
            // available. Continue to see if we had any new relevant signal
            // events.
            if (eno == EINTR) {
                continue;
            }
            // A real error occurred.
            else {
                zerr_msg("zmq_poll() failed", eno);
                qvrc = QV_ERR_RPC;
                break;
            }
        }
        // Timeout, no events.
        else if (zrc == 0) {
            continue;
        }
        // Events on the socket ready for receipt.
        if (poll_items[0].revents & ZMQ_POLLIN) {
            // Perform a blocking receive of the available message.
            zrc = zmq_msg_recv(mrx, m_zsock, 0);
            if (qvi_unlikely(zrc == -1)) {
                const int eno = errno;
                zerr_msg("zmq_msg_recv() failed", eno);
                qvrc = QV_ERR_RPC;
            }
            break;
        }
    } while (true);

    if (qvi_unlikely(qvrc != QV_SUCCESS)) zmq_msg_close(mrx);
    return qvrc;
}

template <typename... Types>
static inline int
rpc_pack(
    qvi_bbuff **buff,
    qvi_rmi_rpc_fid_t fid,
    Types &&...args
) {
    qvi_bbuff *ibuff = nullptr;
    int rc = qvi_new(&ibuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // Fill and add header.
    rc = buffer_append_header(ibuff, fid);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;

    rc = qvi_bbuff_rmi_pack(ibuff, std::forward<Types>(args)...);
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ibuff);
    }
    *buff = ibuff;
    return rc;
}

template <typename... Types>
static inline int
rpc_unpack(
    void *data,
    Types &&...args
) {
    qvi_rmi_msg_header hdr;
    const size_t trim = unpack_msg_header(data, &hdr);
    void *body = data_trim(data, trim);
    return qvi_bbuff_rmi_unpack(body, std::forward<Types>(args)...);
}

qvi_rmi_client::~qvi_rmi_client(void)
{
    // Make sure we can safely call zmq_ctx_destroy(). Otherwise it will hang.
    if (m_connected) {
        zsocket_close(m_zsock);
        zctx_destroy(&m_zctx);
    }
}

qvi_hwloc &
qvi_rmi_client::hwloc(void)
{
    return m_hwloc;
}

int
qvi_rmi_client::connect(
    const std::string &url,
    const int &portno
) {
    // Create a new ZMQ context.
    m_zctx = zmq_ctx_new();
    if (qvi_unlikely(!m_zctx)) return QV_RES_UNAVAILABLE;
    // Create the ZMQ socket used for communication with the server.
    m_zsock = zsocket_create(m_zctx, ZMQ_REQ);
    if (qvi_unlikely(!m_zsock)) return QV_RES_UNAVAILABLE;
    // Note: ZMQ_CONNECT_TIMEOUT doesn't seem to have an appreciable effect.
    int zrc = zsocket_connect(m_zsock, url.c_str());
    if (qvi_unlikely(zrc != 0)) {
        const int eno = errno;
        zerr_msg("zsocket_connect() failed", eno);
        return QV_RES_UNAVAILABLE;
    }
    // To avoid hangs in faulty connections, set a timeout
    // before initiating the first client/server exchange.
    const int timeout_in_ms = 5000;
    zrc = zmq_setsockopt(
        m_zsock, ZMQ_RCVTIMEO, &timeout_in_ms, sizeof(timeout_in_ms)
    );
    if (qvi_unlikely(zrc != 0)) {
        const int eno = errno;
        zerr_msg("zmq_setsockopt(ZMQ_RCVTIMEO) failed", eno);
        return QV_RES_UNAVAILABLE;
    }
    // Now initiate the client/server exchange.
    std::string hwtopo_path;
    int rc = m_hello(hwtopo_path);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        return QV_RES_UNAVAILABLE;
    }
    else {
        m_connected = true;
    }
    // Now that we have all the info we need,
    // finish populating the RMI config.
    m_config.portno = portno;
    m_config.url = url;
    m_config.hwtopo_path = hwtopo_path;
    // Now we can initialize and load our topology.
    rc = m_hwloc.topology_init(m_config.hwtopo_path);
    if (qvi_unlikely(rc != QV_SUCCESS)) return QV_RES_UNAVAILABLE;

    rc = m_hwloc.topology_load();
    if (qvi_unlikely(rc != QV_SUCCESS)) return QV_RES_UNAVAILABLE;

    return QV_SUCCESS;
}

int
qvi_rmi_client::m_recv_msg(
    zmq_msg_t *mrx
) const {
    int qvrc = QV_SUCCESS;

    int rc = zmq_msg_init(mrx);
    if (qvi_unlikely(rc != 0)) {
        const int eno = errno;
        zerr_msg("zmq_msg_init() failed", eno);
        return QV_ERR_RPC;
    }
    // Block until a message is available to be received from socket.
    rc = zmq_msg_recv(mrx, m_zsock, 0);
    if (qvi_unlikely(rc == -1)) {
        const int eno = errno;
        zerr_msg("zmq_msg_recv() failed", eno);
        qvrc = QV_ERR_RPC;
    }

    if (qvi_unlikely(qvrc != QV_SUCCESS)) zmq_msg_close(mrx);
    return qvrc;
}

template <typename... Types>
int
qvi_rmi_client::rpc_rep(
    Types &&...args
) const {
    zmq_msg_t msg;
    int rc = m_recv_msg(&msg);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    rc = rpc_unpack(zmq_msg_data(&msg), std::forward<Types>(args)...);
out:
    zmq_msg_close(&msg);
    return rc;
}

template <typename... Types>
int
qvi_rmi_client::rpc_req(
    qvi_rmi_rpc_fid_t fid,
    Types &&...args
) const {
    qvi_bbuff *bbuff = nullptr;
    const int rc = rpc_pack(&bbuff, fid, std::forward<Types>(args)...);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&bbuff);
        return rc;
    }
    int bsent = 0;
    return zsock_send_bbuff(m_zsock, bbuff, &bsent);
}

////////////////////////////////////////////////////////////////////////////////
// Client-Side RPC Definitions
////////////////////////////////////////////////////////////////////////////////
int
qvi_rmi_client::m_hello(
    std::string &hwtopo_path
) {
    int qvrc = rpc_req(QVI_RMI_FID_HELLO, qvi_gettid());
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(&rpcrc, hwtopo_path);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) {
        return qvrc;
    }
    return rpcrc;
}

int
qvi_rmi_client::get_cpubind(
    pid_t who,
    hwloc_cpuset_t *cpuset
) const {
    int qvrc = rpc_req(QVI_RMI_FID_GET_CPUBIND, who);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(&rpcrc, cpuset);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) {
        qvi_hwloc::bitmap_delete(cpuset);
        return qvrc;
    }
    return rpcrc;
}

int
qvi_rmi_client::set_cpubind(
    pid_t who,
    hwloc_const_cpuset_t cpuset
) {
    int qvrc = rpc_req(QVI_RMI_FID_SET_CPUBIND, who, cpuset);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(&rpcrc);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    return rpcrc;
}

int
qvi_rmi_client::get_intrinsic_hwpool(
    const std::vector<pid_t> &who,
    qv_scope_intrinsic_t iscope,
    qvi_hwpool **hwpool
) {
    *hwpool = nullptr;

    int qvrc = rpc_req(QVI_RMI_FID_GET_INTRINSIC_HWPOOL, who, iscope);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Create the new hardware pool.
    qvi_hwpool *ihwpool = nullptr;
    qvrc = qvi_new(&ihwpool);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(&rpcrc, ihwpool);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) {
        qvi_delete(&ihwpool);
        return qvrc;
    }
    *hwpool = ihwpool;
    return rpcrc;
}

int
qvi_rmi_client::get_obj_depth(
    qv_hw_obj_type_t type,
    int *depth
) {
    int qvrc = rpc_req(QVI_RMI_FID_OBJ_TYPE_DEPTH, type);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(&rpcrc, depth);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    return rpcrc;
}

int
qvi_rmi_client::get_nobjs_in_cpuset(
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
) {
    int qvrc = rpc_req(QVI_RMI_FID_GET_NOBJS_IN_CPUSET, target_obj, cpuset);
    if (qvrc != QV_SUCCESS) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(&rpcrc, nobjs);
    if (qvrc != QV_SUCCESS) return qvrc;
    return rpcrc;
}

int
qvi_rmi_client::get_device_in_cpuset(
    qv_hw_obj_type_t dev_obj,
    int dev_i,
    hwloc_const_cpuset_t cpuset,
    qv_device_id_type_t dev_id_type,
    char **dev_id
) {
    int qvrc = rpc_req(
        QVI_RMI_FID_GET_DEVICE_IN_CPUSET,
        dev_obj, dev_i, cpuset, dev_id_type
    );
    if (qvrc != QV_SUCCESS) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(&rpcrc, dev_id);
    if (qvrc != QV_SUCCESS) return qvrc;
    return rpcrc;
}

int
qvi_rmi_client::get_cpuset_for_nobjs(
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t obj_type,
    int nobjs,
    qvi_hwloc_bitmap &result
) {
    // TODO(skg) At some point we will acquire the resources
    // for improved splitting and resource distribution.
    return m_hwloc.get_cpuset_for_nobjs(
        cpuset, obj_type, nobjs, result
    );
}

int
qvi_rmi_client::send_shutdown_message(void)
{
    return rpc_req(QVI_RMI_FID_SERVER_SHUTDOWN);
}

qvi_rmi_server::qvi_rmi_server(void)
{
    // Register signal handlers.
    struct sigaction action = {};
    action.sa_handler = server_signal_handler;

    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGHUP, &action, nullptr);

    m_rpc_dispatch_table = {
        {QVI_RMI_FID_INVALID, s_rpc_invalid},
        {QVI_RMI_FID_SERVER_SHUTDOWN, s_rpc_shutdown},
        {QVI_RMI_FID_HELLO, s_rpc_hello},
        {QVI_RMI_FID_GET_CPUBIND, s_rpc_get_cpubind},
        {QVI_RMI_FID_SET_CPUBIND, s_rpc_set_cpubind},
        {QVI_RMI_FID_OBJ_TYPE_DEPTH, s_rpc_obj_type_depth},
        {QVI_RMI_FID_GET_NOBJS_IN_CPUSET, s_rpc_get_nobjs_in_cpuset},
        {QVI_RMI_FID_GET_DEVICE_IN_CPUSET, s_rpc_get_device_in_cpuset},
        {QVI_RMI_FID_GET_INTRINSIC_HWPOOL, s_rpc_get_intrinsic_hwpool}
    };

    int qvrc = m_hwloc.topology_init();
    if (qvi_unlikely(qvrc != QV_SUCCESS)) {
        static cstr_t ers = "hwloc.topology_init() failed";
        qvi_log_error("{} (rc={}, {})", ers, qvrc, qv_strerr(qvrc));
        throw qvi_runtime_error();
    }

    qvrc = m_hwloc.topology_load();
    if (qvi_unlikely(qvrc != QV_SUCCESS)) {
        static cstr_t ers = "hwloc.topology_load() failed";
        qvi_log_error("{} (rc={}, {})", ers, qvrc, qv_strerr(qvrc));
        throw qvi_runtime_error();
    }

    m_zctx = zmq_ctx_new();
    if (qvi_unlikely(!m_zctx)) throw qvi_runtime_error();
}

qvi_rmi_server::~qvi_rmi_server(void)
{
    zsocket_close(m_zsock);
    zctx_destroy(&m_zctx);
    unlink(m_config.hwtopo_path.c_str());
}

int
qvi_rmi_server::m_get_iscope_bitmap_user(
    qvi_hwloc_bitmap &bitmap
) {
    return bitmap.set(m_hwloc.topology_get_cpuset());
}

int
qvi_rmi_server::m_get_iscope_bitmap_job(
    const std::vector<pid_t> &who,
    qvi_hwloc_bitmap &bitmap
) {
    int rc = QV_SUCCESS;
    const size_t nmembers = who.size();

    qvi_hwloc_bitmaps bitmaps(nmembers);

    for (size_t i = 0; i < nmembers; ++i) {
        hwloc_cpuset_t cpuset = nullptr;
        rc = m_hwloc.task_get_cpubind(who[i], &cpuset);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;

        bitmaps[i].set(cpuset);
        qvi_hwloc::bitmap_delete(&cpuset);
    }
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    bitmap = qvi_hwloc_bitmap::op_or(bitmaps);

    return QV_SUCCESS;
}

int
qvi_rmi_server::m_get_iscope_bitmap_proc(
    const std::vector<pid_t> &who,
    qvi_hwloc_bitmap &bitmap
) {
    // This is an internal bug.
    if (qvi_unlikely(who.size() != 1)) qvi_abort();

    hwloc_cpuset_t cpuset = nullptr;
    int rc = m_hwloc.task_get_cpubind(who[0], &cpuset);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    bitmap.set(cpuset);
    qvi_hwloc::bitmap_delete(&cpuset);

    return QV_SUCCESS;
}

int
qvi_rmi_server::s_rpc_get_intrinsic_hwpool(
    qvi_rmi_server *server,
    qvi_rmi_msg_header *hdr,
    void *input,
    qvi_bbuff **output
) {
    int rpcrc = QV_SUCCESS;
    // Bitmap that encodes available host resources.
    qvi_hwloc_bitmap sbitmap;
    qvi_hwpool hwpool;

    do {
        std::vector<pid_t> who;
        qv_scope_intrinsic_t iscope;
        rpcrc = qvi_bbuff_rmi_unpack(input, who, &iscope);
        // Drop the message. Send an empty hardware pool with the error code.
        if (qvi_unlikely(rpcrc != QV_SUCCESS)) break;

        switch (iscope) {
            case QV_SCOPE_SYSTEM:
                rpcrc = QV_ERR_NOT_SUPPORTED;
                break;
            case QV_SCOPE_USER:
                rpcrc = server->m_get_iscope_bitmap_user(sbitmap);
                break;
            case QV_SCOPE_JOB:
                rpcrc = server->m_get_iscope_bitmap_job(who, sbitmap);
                break;
            case QV_SCOPE_PROCESS:
                rpcrc = server->m_get_iscope_bitmap_proc(who, sbitmap);
                break;
            default:
                rpcrc = QV_ERR_INVLD_ARG;
                break;
        }
        if (qvi_unlikely(rpcrc != QV_SUCCESS)) break;
        rpcrc = hwpool.initialize(server->m_hwloc, sbitmap);
    } while (false);

    return rpc_pack(output, hdr->fid, rpcrc, hwpool);
}

////////////////////////////////////////////////////////////////////////////////
// Server-Side RPC Definitions
////////////////////////////////////////////////////////////////////////////////
int
qvi_rmi_server::s_rpc_invalid(
    qvi_rmi_server *,
    qvi_rmi_msg_header *,
    void *,
    qvi_bbuff **
) {
    qvi_log_error("Something bad happened in RPC dispatch.");
    qvi_abort();
    return QV_ERR_INVLD_ARG;
}

int
qvi_rmi_server::s_rpc_shutdown(
    qvi_rmi_server *,
    qvi_rmi_msg_header *hdr,
    void *,
    qvi_bbuff **output
) {
    (void)rpc_pack(output, hdr->fid, QVI_BBUFF_RMI_ZERO_MSG);
    return QV_SUCCESS_SHUTDOWN;
}

int
qvi_rmi_server::s_rpc_hello(
    qvi_rmi_server *server,
    qvi_rmi_msg_header *hdr,
    void *input,
    qvi_bbuff **output
) {
    pid_t whoisit;
    const int rc = qvi_bbuff_rmi_unpack(input, &whoisit);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack relevant configuration information.
    const int rpcrc = QV_SUCCESS;
    return rpc_pack(
        output, hdr->fid, rpcrc,
        server->m_config.hwtopo_path
    );
}

int
qvi_rmi_server::s_rpc_get_cpubind(
    qvi_rmi_server *server,
    qvi_rmi_msg_header *hdr,
    void *input,
    qvi_bbuff **output
) {
    pid_t who;
    int qvrc = qvi_bbuff_rmi_unpack(input, &who);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;

    hwloc_cpuset_t bitmap = nullptr;
    const int rpcrc = server->m_hwloc.task_get_cpubind(who, &bitmap);
    qvrc = rpc_pack(output, hdr->fid, rpcrc, bitmap);
    qvi_hwloc::bitmap_delete(&bitmap);

    return qvrc;
}

int
qvi_rmi_server::s_rpc_set_cpubind(
    qvi_rmi_server *server,
    qvi_rmi_msg_header *hdr,
    void *input,
    qvi_bbuff **output
) {
    pid_t who;
    qvi_hwloc_bitmap cpuset;
    const int qvrc = qvi_bbuff_rmi_unpack(input, &who, cpuset);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;

    const int rpcrc = server->m_hwloc.task_set_cpubind_from_cpuset(
        who, cpuset.cdata()
    );
    return rpc_pack(output, hdr->fid, rpcrc);
}

int
qvi_rmi_server::s_rpc_obj_type_depth(
    qvi_rmi_server *server,
    qvi_rmi_msg_header *hdr,
    void *input,
    qvi_bbuff **output
) {
    qv_hw_obj_type_t obj;
    const int qvrc = qvi_bbuff_rmi_unpack(input, &obj);
    if (qvrc != QV_SUCCESS) return qvrc;

    int depth = 0;
    const int rpcrc = server->m_hwloc.obj_type_depth(obj, &depth);

    return rpc_pack(output, hdr->fid, rpcrc, depth);
}

int
qvi_rmi_server::s_rpc_get_nobjs_in_cpuset(
    qvi_rmi_server *server,
    qvi_rmi_msg_header *hdr,
    void *input,
    qvi_bbuff **output
) {
    qv_hw_obj_type_t target_obj;
    qvi_hwloc_bitmap cpuset;
    int qvrc = qvi_bbuff_rmi_unpack(
        input, &target_obj, cpuset
    );
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;

    int nobjs = 0;
    const int rpcrc = server->m_hwloc.get_nobjs_in_cpuset(
        target_obj, cpuset.cdata(), &nobjs
    );

    return rpc_pack(output, hdr->fid, rpcrc, nobjs);
}

int
qvi_rmi_server::s_rpc_get_device_in_cpuset(
    qvi_rmi_server *server,
    qvi_rmi_msg_header *hdr,
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
    int rpcrc = server->m_hwloc.get_device_id_in_cpuset(
        dev_obj, dev_i, cpuset, devid_type, &dev_id
    );

    qvrc = rpc_pack(output, hdr->fid, rpcrc, dev_id);

    qvi_hwloc::bitmap_delete(&cpuset);
    free(dev_id);
    return qvrc;
}

int
qvi_rmi_server::m_rpc_dispatch(
    void *zsock,
    zmq_msg_t *command_msg,
    int *bsent
) {
    int rc = QV_SUCCESS;
    bool shutdown = false;

    void *data = zmq_msg_data(command_msg);

    qvi_rmi_msg_header hdr;
    const size_t trim = unpack_msg_header(data, &hdr);
    void *body = data_trim(data, trim);

    const auto fidfunp = m_rpc_dispatch_table.find(hdr.fid);
    if (qvi_unlikely(fidfunp == m_rpc_dispatch_table.end())) {
        qvi_log_error("Unknown function ID ({}) in RPC. Aborting.", hdr.fid);
        rc = QV_ERR_RPC;
        goto out;
    }

    qvi_bbuff *result;
    rc = fidfunp->second(this, &hdr, body, &result);
    if (qvi_unlikely(rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN)) {
        cstr_t ers = "RPC dispatch failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        goto out;
    }
    // Shutdown?
    if (qvi_unlikely(rc == QV_SUCCESS_SHUTDOWN)) {
        shutdown = true;
    }
    rc = zsock_send_bbuff(zsock, result, bsent);
out:
    zmq_msg_close(command_msg);
    return (shutdown ? QV_SUCCESS_SHUTDOWN : rc);
}

int
qvi_rmi_server::m_enter_main_server_loop(void)
{
    int rc, bsent;
    int bsentt = 0;
    zmq_msg_t mrx;
    do {
        rc = m_recv_msg(&mrx);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;
        rc = m_rpc_dispatch(m_zsock, &mrx, &bsent);
        if (qvi_likely(rc == QV_SUCCESS)) bsentt += bsent;
        else break;
    } while(true);

    // Nice to understand messaging characteristics.
    qvi_log_info("Server Sent {} bytes", bsentt);

    if (qvi_unlikely(rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN)) {
        qvi_log_error("RX/TX loop exited with rc={} ({})", rc, qv_strerr(rc));
        return rc;
    }
    return QV_SUCCESS;
}

int
qvi_rmi_server::configure(
    const qvi_rmi_config &config
) {
    m_config = config;
    return QV_SUCCESS;
}

int
qvi_rmi_server::topology_export(
    const std::string &base_path,
    std::string &path
) {
    return m_hwloc.topology_export(base_path, path);
}

int
qvi_rmi_server::m_populate_base_hwpool(void)
{
    qvi_hwloc_bitmap base_bitmap(m_hwloc.topology_get_cpuset());
    // The base resource pool will contain all available processors and devices.
    return m_hwpool.initialize(m_hwloc, base_bitmap);
}

int
qvi_rmi_server::start(void)
{
    // Populate the base hardware resource pool.
    const int qvrc = m_populate_base_hwpool();
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Setup our connection.
    m_zsock = zsocket_create_and_bind(
        m_zctx, ZMQ_REP, m_config.url.c_str()
    );
    if (qvi_unlikely(!m_zsock)) return QV_ERR_SYS;
    // Set linger period to 0 so clients won't hang when a server shutdown
    // request is handled. A value of 0 means the following: pending messages
    // shall be discarded immediately when the socket is closed.
    const int linger = 0;
    const int zrc = zmq_setsockopt(
        m_zsock, ZMQ_LINGER, &linger, sizeof(linger)
    );
    if (qvi_unlikely(zrc != 0)) {
        const int eno = errno;
        zerr_msg("zmq_setsockopt(ZMQ_LINGER) failed", eno);
        return QV_ERR_SYS;
    }
    // Start the main service loop.
    return m_enter_main_server_loop();
}

static int
get_rmi_port_from_env(
    int &portno
) {
    portno = QVI_RMI_PORT_UNSET;

    const cstr_t ports = getenv(PORT_ENV_VAR.c_str());
    if (!ports) return QV_ERR_ENV;
    const int rc = qvi_stoi(std::string(ports), portno);
    if (qvi_unlikely(rc != QV_SUCCESS)) return QV_ERR_ENV;
    return QV_SUCCESS;
}

int
qvi_rmi_get_url(
    std::string &url,
    int &portno
) {
    static const std::string base = "tcp://127.0.0.1";

    if (portno == QVI_RMI_PORT_UNSET) {
        const int rc = get_rmi_port_from_env(portno);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    }

    url = base + ":" + std::to_string(portno);
    return QV_SUCCESS;
}

std::string
qvi_rmi_conn_env_ers(void)
{
    static const std::string msg =
        "\n\n#############################################\n"
        "# Cannot determine connection information.\n"
        "# Make sure that the following environment\n"
        "# environment variable is set to an unused\n"
        "# port number: " + PORT_ENV_VAR + ""
        "\n#############################################\n\n";
    return msg;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
