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

struct qvi_rmi_msg_header {
    qvi_rmi_rpc_fid_t fid = QVI_RMI_FID_INVALID;
    char picture[16] = {'\0'};
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

static inline void
zsocket_close(
    void **sock
) {
    if (qvi_unlikely(!sock)) return;
    void *isock = *sock;
    if (qvi_likely(isock)) {
        const int rc = zmq_close(isock);
        if (qvi_unlikely(rc != 0)) {
            zwrn_msg("zmq_close() failed", errno);
        }
    }
    *sock = nullptr;
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
            zwrn_msg("zmq_ctx_destroy() failed", errno);
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
        zerr_msg("zmq_socket() failed", errno);
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
        zerr_msg("zmq_connect() failed", errno);
        zsocket_close(&zsock);
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
        zerr_msg("zmq_socket() failed", errno);
        return nullptr;
    }
    const int rc = zmq_bind(zsock, addr);
    if (qvi_unlikely(rc != 0)) {
        zerr_msg("zmq_bind() failed", errno);
        zsocket_close(&zsock);
        return nullptr;
    }
    return zsock;
}

static inline int
buffer_append_header(
    qvi_bbuff *buff,
    qvi_rmi_rpc_fid_t fid,
    cstr_t picture
) {
    qvi_rmi_msg_header hdr;
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
        zerr_msg("zmq_send() truncated", errno);
        return QV_ERR_RPC;
    }
    // We are resposible for freeing the buffer after ZMQ has
    // taken ownership of its contents after zmq_send() returns.
    qvi_bbuff_delete(&bbuff);
    return QV_SUCCESS;
}

static inline int
zsock_recv_msg(
    void *zsock,
    zmq_msg_t *mrx
) {
    int qvrc = QV_SUCCESS;
    int rc = zmq_msg_init(mrx);
    if (qvi_unlikely(rc != 0)) {
        zerr_msg("zmq_msg_init() failed", errno);
        qvrc = QV_ERR_RPC;
        goto out;
    }
    // Block until a message is available to be received from socket.
    rc = zmq_msg_recv(mrx, zsock, 0);
    if (qvi_unlikely(rc == -1)) {
        zerr_msg("zmq_msg_recv() failed", errno);
        qvrc = QV_ERR_RPC;
    }
out:
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
static inline int
rpc_unpack(
    void *data,
    Types &&...args
) {
    qvi_rmi_msg_header hdr;
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
    qvi_rmi_rpc_fid_t fid,
    Types &&...args
) {
    qvi_bbuff *bbuff = nullptr;
    const int rc = rpc_pack(&bbuff, fid, std::forward<Types>(args)...);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_bbuff_delete(&bbuff);
        return rc;
    }
    int bsent = 0;
    return zsock_send_bbuff(zsock, bbuff, &bsent);
}

template <typename... Types>
static inline int
rpc_rep(
    void *zsock,
    Types &&...args
) {
    zmq_msg_t msg;
    int rc = zsock_recv_msg(zsock, &msg);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    rc = rpc_unpack(zmq_msg_data(&msg), std::forward<Types>(args)...);
out:
    zmq_msg_close(&msg);
    return rc;
}

qvi_rmi_client::qvi_rmi_client(void)
{
    // Remember clients own the hwloc data, unlike the server.
    const int rc = qvi_hwloc_new(&m_config.hwloc);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    // Create a new ZMQ context.
    m_zctx = zmq_ctx_new();
    if (qvi_unlikely(!m_zctx)) throw qvi_runtime_error();
    // Create the ZMQ socket used for communication with the server.
    m_zsock = zsocket_create(m_zctx, ZMQ_REQ);
    if (qvi_unlikely(!m_zsock)) throw qvi_runtime_error();
}

qvi_rmi_client::~qvi_rmi_client(void)
{
    zsocket_close(&m_zsock);
    zctx_destroy(&m_zctx);
    qvi_hwloc_delete(&m_config.hwloc);
}

qvi_hwloc_t *
qvi_rmi_client::hwloc(void) const
{
    return m_config.hwloc;
}

int
qvi_rmi_client::connect(
    const std::string &url
) {
    int rc = zsocket_connect(m_zsock, url.c_str());
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    std::string hwtopo_path;
    rc = m_hello(hwtopo_path);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Now that we have all the info we need,
    // finish populating the RMI config.
    m_config.url = url;
    m_config.hwtopo_path = hwtopo_path;
    // Now we can initialize and load our topology.
    rc = qvi_hwloc_topology_init(
        m_config.hwloc, m_config.hwtopo_path.c_str()
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return qvi_hwloc_topology_load(m_config.hwloc);
}

////////////////////////////////////////////////////////////////////////////////
// Client-Side RPC Definitions
////////////////////////////////////////////////////////////////////////////////
int
qvi_rmi_client::m_hello(
    std::string &hwtopo_path
) {
    int qvrc = rpc_req(m_zsock, QVI_RMI_FID_HELLO, qvi_gettid());
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(m_zsock, &rpcrc, hwtopo_path);
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
    int qvrc = rpc_req(m_zsock, QVI_RMI_FID_GET_CPUBIND, who);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(m_zsock, &rpcrc, cpuset);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) {
        qvi_hwloc_bitmap_delete(cpuset);
        return qvrc;
    }
    return rpcrc;
}

int
qvi_rmi_client::set_cpubind(
    pid_t who,
    hwloc_const_cpuset_t cpuset
) {
    int qvrc = rpc_req(m_zsock, QVI_RMI_FID_SET_CPUBIND, who, cpuset);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(m_zsock, &rpcrc);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    return rpcrc;
}

int
qvi_rmi_client::get_intrinsic_hwpool(
    pid_t who,
    qv_scope_intrinsic_t iscope,
    qvi_hwpool **hwpool
) {
    *hwpool = nullptr;

    int qvrc = rpc_req(m_zsock, QVI_RMI_FID_GET_INTRINSIC_HWPOOL, who, iscope);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(m_zsock, &rpcrc, hwpool);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    return rpcrc;
}

int
qvi_rmi_client::get_obj_depth(
    qv_hw_obj_type_t type,
    int *depth
) {
    int qvrc = rpc_req(m_zsock, QVI_RMI_FID_OBJ_TYPE_DEPTH, type);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(m_zsock, &rpcrc, depth);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    return rpcrc;
}

int
qvi_rmi_client::get_nobjs_in_cpuset(
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
) {
    int qvrc = rpc_req(m_zsock, QVI_RMI_FID_GET_NOBJS_IN_CPUSET, target_obj, cpuset);
    if (qvrc != QV_SUCCESS) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(m_zsock, &rpcrc, nobjs);
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
        m_zsock, QVI_RMI_FID_GET_DEVICE_IN_CPUSET,
        dev_obj, dev_i, cpuset, dev_id_type
    );
    if (qvrc != QV_SUCCESS) return qvrc;
    // Should be set by rpc_rep, so assume an error.
    int rpcrc = QV_ERR_RPC;
    qvrc = rpc_rep(m_zsock, &rpcrc, dev_id);
    if (qvrc != QV_SUCCESS) return qvrc;
    return rpcrc;
}

int
qvi_rmi_client::get_cpuset_for_nobjs(
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t obj_type,
    int nobjs,
    hwloc_cpuset_t *result
) {
    // TODO(skg) At some point we will acquire the resources
    // for improved splitting and resource distribution.
    return qvi_hwloc_get_cpuset_for_nobjs(
        m_config.hwloc, cpuset,
        obj_type, nobjs, result
    );
}

int
qvi_rmi_client::send_shutdown_message(void)
{
    return rpc_req(m_zsock, QVI_RMI_FID_SERVER_SHUTDOWN);
}

qvi_rmi_server::qvi_rmi_server(void)
{
    m_rpc_dispatch_table = {
        {QVI_RMI_FID_INVALID, s_rpc_invalid},
        {QVI_RMI_FID_SERVER_SHUTDOWN, s_rpc_shutdown},
        {QVI_RMI_FID_HELLO, s_rpc_hello},
        {QVI_RMI_FID_GOODBYE, s_rpc_goodbye},
        {QVI_RMI_FID_GET_CPUBIND, s_rpc_get_cpubind},
        {QVI_RMI_FID_SET_CPUBIND, s_rpc_set_cpubind},
        {QVI_RMI_FID_OBJ_TYPE_DEPTH, s_rpc_obj_type_depth},
        {QVI_RMI_FID_GET_NOBJS_IN_CPUSET, s_rpc_get_nobjs_in_cpuset},
        {QVI_RMI_FID_GET_DEVICE_IN_CPUSET, s_rpc_get_device_in_cpuset},
        {QVI_RMI_FID_GET_INTRINSIC_HWPOOL, s_rpc_get_intrinsic_hwpool}
    };

    m_zctx = zmq_ctx_new();
    if (qvi_unlikely(!m_zctx)) throw qvi_runtime_error();

    const int rc = qvi_new(&m_hwpool);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
}

qvi_rmi_server::~qvi_rmi_server(void)
{
    zsocket_close(&m_zsock);
    zctx_destroy(&m_zctx);
    unlink(m_config.hwtopo_path.c_str());
    qvi_delete(&m_hwpool);
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
    // TODO(skg) This will go into some registry somewhere.
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
qvi_rmi_server::s_rpc_goodbye(
    qvi_rmi_server *,
    qvi_rmi_msg_header *,
    void *,
    qvi_bbuff **
) {
    // TODO(skg) This is where we will do bookkeeping for closed connections.
    return QV_ERR_INVLD_ARG;
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
    const int rpcrc = qvi_hwloc_task_get_cpubind(
        server->m_config.hwloc, who, &bitmap
    );
    qvrc = rpc_pack(output, hdr->fid, rpcrc, bitmap);
    qvi_hwloc_bitmap_delete(&bitmap);

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
    hwloc_cpuset_t cpuset = nullptr;
    const int qvrc = qvi_bbuff_rmi_unpack(input, &who, &cpuset);
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;

    const int rpcrc = qvi_hwloc_task_set_cpubind_from_cpuset(
        server->m_config.hwloc, who, cpuset
    );
    qvi_hwloc_bitmap_delete(&cpuset);
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
    const int qvrc = qvi_bbuff_rmi_unpack(
        input, &obj
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    int depth = 0;
    const int rpcrc = qvi_hwloc_obj_type_depth(
        server->m_config.hwloc, obj, &depth
    );

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
    hwloc_cpuset_t cpuset = nullptr;
    int qvrc = qvi_bbuff_rmi_unpack(
        input, &target_obj, &cpuset
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    int nobjs = 0;
    const int rpcrc = qvi_hwloc_get_nobjs_in_cpuset(
        server->m_config.hwloc, target_obj, cpuset, &nobjs
    );

    qvrc = rpc_pack(output, hdr->fid, rpcrc, nobjs);

    qvi_hwloc_bitmap_delete(&cpuset);
    return qvrc;
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
    int rpcrc = qvi_hwloc_get_device_id_in_cpuset(
        server->m_config.hwloc, dev_obj, dev_i, cpuset, devid_type, &dev_id
    );

    qvrc = rpc_pack(output, hdr->fid, rpcrc, dev_id);

    qvi_hwloc_bitmap_delete(&cpuset);
    free(dev_id);
    return qvrc;
}

int
qvi_rmi_server::m_get_intrinsic_scope_user(
    pid_t,
    qvi_hwpool **hwpool
) {
    // TODO(skg) Is the cpuset the best way to do this?
    return qvi_hwpool::create(
        m_config.hwloc, qvi_hwloc_topo_get_cpuset(m_config.hwloc), hwpool
    );
}

int
qvi_rmi_server::m_get_intrinsic_scope_proc(
    pid_t who,
    qvi_hwpool **hwpool
) {
    hwloc_cpuset_t cpuset = nullptr;
    int rc = qvi_hwloc_task_get_cpubind(
        m_config.hwloc, who, &cpuset
    );
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool::create(
        m_config.hwloc, cpuset, hwpool
    );
out:
    qvi_hwloc_bitmap_delete(&cpuset);
    if (rc != QV_SUCCESS) {
        qvi_delete(hwpool);
    }
    return rc;
}

// TODO(skg) Lots of error path cleanup is required.
int
qvi_rmi_server::s_rpc_get_intrinsic_hwpool(
    qvi_rmi_server *server,
    qvi_rmi_msg_header *hdr,
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
            rpcrc = server->m_get_intrinsic_scope_user(requestor, &hwpool);
            break;
        case QV_SCOPE_PROCESS:
            rpcrc = server->m_get_intrinsic_scope_proc(requestor, &hwpool);
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
    volatile int bsentt = 0;
    volatile bool active = true;
    zmq_msg_t mrx;
    do {
        rc = zsock_recv_msg(m_zsock, &mrx);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;
        rc = m_rpc_dispatch(m_zsock, &mrx, &bsent);
        if (qvi_unlikely(rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN)) break;
        if (qvi_unlikely(rc == QV_SUCCESS_SHUTDOWN)) active = false;
        bsentt += bsent;
    } while(qvi_likely(active));
#if QVI_DEBUG_MODE == 1
    // Nice to understand messaging characteristics.
    qvi_log_debug("Server Sent {} bytes", bsentt);
#else
    qvi_unused(bsentt);
#endif
    if (qvi_unlikely(rc != QV_SUCCESS && rc != QV_SUCCESS_SHUTDOWN)) {
        qvi_log_error("RX/TX loop exited with rc={} ({})", rc, qv_strerr(rc));
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
qvi_rmi_server::m_populate_base_hwpool(void)
{
    qvi_hwloc_t *const hwloc = m_config.hwloc;
    hwloc_const_cpuset_t cpuset = qvi_hwloc_topo_get_cpuset(hwloc);
    // The base resource pool will contain all available processors and devices.
    return m_hwpool->initialize(hwloc, cpuset);
}

int
qvi_rmi_server::start(void)
{
    // First populate the base hardware resource pool.
    int qvrc = m_populate_base_hwpool();
    if (qvi_unlikely(qvrc != QV_SUCCESS)) return qvrc;
    // Setup our connection.
    m_zsock = zsocket_create_and_bind(
        m_zctx, ZMQ_REP, m_config.url.c_str()
    );
    if (qvi_unlikely(!m_zsock)) return QV_ERR_SYS;
    // Start the main service loop.
    return m_enter_main_server_loop();
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
