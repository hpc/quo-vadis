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
 * @file qvi-rmi.h
 *
 * Resource Management and Inquiry
 */

#ifndef QVI_RMI_H
#define QVI_RMI_H

#include "qvi-common.h"
#include "qvi-hwpool.h"
#include "zmq.h"

struct qvi_rmi_msg_header;
struct qvi_rmi_server;

enum qvi_rmi_rpc_fid_t {
    QVI_RMI_FID_INVALID = 0,
    QVI_RMI_FID_SERVER_SHUTDOWN,
    QVI_RMI_FID_HELLO,
    QVI_RMI_FID_GET_CPUBIND,
    QVI_RMI_FID_SET_CPUBIND,
    QVI_RMI_FID_OBJ_TYPE_DEPTH,
    QVI_RMI_FID_GET_NOBJS_IN_CPUSET,
    QVI_RMI_FID_GET_DEVICE_IN_CPUSET,
    QVI_RMI_FID_GET_INTRINSIC_HWPOOL
};

/**
 * @note: The return value is used for operation status (e.g., was the internal
 * machinery successful?). The underlying target's return code is packed into
 * the message buffer and is meant for client-side consumption.
 */
using qvi_rmi_rpc_fun_ptr_t = std::function<
    int(qvi_rmi_server *, qvi_rmi_msg_header *, void *, qvi_bbuff **)
>;

/**
 * Maintains RMI configuration information.
 */
struct qvi_rmi_config {
    /** Connection URL. */
    std::string url;
    /** Connection port number. */
    int portno = QVI_PORT_UNSET;
    /** Path to hardware topology file. */
    std::string hwtopo_path;
};

/**
 * RMI server.
 */
struct qvi_rmi_server {
private:
    /** Maps function IDs to function pointers. */
    std::map<qvi_rmi_rpc_fid_t, qvi_rmi_rpc_fun_ptr_t> m_rpc_dispatch_table;
    /** Server configuration. */
    qvi_rmi_config m_config;
    /** Maintains hardware locality information. */
    qvi_hwloc m_hwloc;
    /** The base resource pool maintained by the server. */
    qvi_hwpool m_hwpool;
    /** ZMQ context. */
    void *m_zctx = nullptr;
    /** Communication socket. */
    void *m_zsock = nullptr;
    /** Populates base hardware pool. */
    int
    m_populate_base_hwpool(void);
    /** Reveives messages. */
    int
    m_recv_msg(
        zmq_msg_t *mrx
    );
    /** Performs RPC dispatch. */
    int
    m_rpc_dispatch(
        void *zsock,
        zmq_msg_t *command_msg,
        int *bsent
    );
    /** */
    int
    m_get_iscope_bitmap_user(
        qvi_hwloc_bitmap &bitmap
    );
    /** */
    int
    m_get_iscope_bitmap_job(
        const std::vector<pid_t> &who,
        qvi_hwloc_bitmap &bitmap
    );
    /** */
    int
    m_get_iscope_bitmap_proc(
        const std::vector<pid_t> &who,
        qvi_hwloc_bitmap &bitmap
    );
    /** Executes the main server loop. */
    int
    m_enter_main_server_loop(void);
    /** */
    static int
    s_rpc_invalid(
        qvi_rmi_server *,
        qvi_rmi_msg_header *,
        void *,
        qvi_bbuff **
    );
    /** */
    static int
    s_rpc_shutdown(
        qvi_rmi_server *,
        qvi_rmi_msg_header *hdr,
        void *,
        qvi_bbuff **output
    );
    /** */
    static int
    s_rpc_hello(
        qvi_rmi_server *server,
        qvi_rmi_msg_header *hdr,
        void *input,
        qvi_bbuff **output
    );
    /** */
    static int
    s_rpc_get_cpubind(
        qvi_rmi_server *server,
        qvi_rmi_msg_header *hdr,
        void *input,
        qvi_bbuff **output
    );
    /** */
    static int
    s_rpc_set_cpubind(
        qvi_rmi_server *server,
        qvi_rmi_msg_header *hdr,
        void *input,
        qvi_bbuff **output
    );
    /** */
    static int
    s_rpc_obj_type_depth(
        qvi_rmi_server *server,
        qvi_rmi_msg_header *hdr,
        void *input,
        qvi_bbuff **output
    );
    /** */
    static int
    s_rpc_get_nobjs_in_cpuset(
        qvi_rmi_server *server,
        qvi_rmi_msg_header *hdr,
        void *input,
        qvi_bbuff **output
    );
    /** */
    static int
    s_rpc_get_device_in_cpuset(
        qvi_rmi_server *server,
        qvi_rmi_msg_header *hdr,
        void *input,
        qvi_bbuff **output
    );
    /** */
    static int
    s_rpc_get_intrinsic_hwpool(
        qvi_rmi_server *server,
        qvi_rmi_msg_header *hdr,
        void *input,
        qvi_bbuff **output
    );
public:
    /** Constructor. */
    qvi_rmi_server(void);
    /** Destructor. */
    ~qvi_rmi_server(void);
    /** Configures the server's RMI from the provided config. */
    int
    configure(
        const qvi_rmi_config &config
    );
    /** Exports hardware topology. */
    int
    topology_export(
        const std::string &base_path,
        std::string &path
    );
    /** Starts the server. */
    int
    start(void);
};

/**
 * RMI client.
 */
struct qvi_rmi_client {
private:
    /** Client configuration. */
    qvi_rmi_config m_config;
    /** Maintains hardware locality information. */
    qvi_hwloc m_hwloc;
    /** ZMQ context. */
    void *m_zctx = nullptr;
    /** Communication socket. */
    void *m_zsock = nullptr;
    /** Flag indicating whether client is connected to server. */
    bool m_connected = false;
    /** Reveives messages. */
    int
    m_recv_msg(
        zmq_msg_t *mrx
    ) const;
    /** Performs RPC reply. */
    template <typename... Types>
    int
    rpc_rep(
        Types &&...args
    ) const;
    /** Performs RPC request. */
    template <typename... Types>
    int
    rpc_req(
        qvi_rmi_rpc_fid_t fid,
        Types &&...args
    ) const;
    /** Performs connection handshake. */
    int
    m_hello(
        std::string &hwtopo_path
    );
public:
    /** Constructor. */
    qvi_rmi_client(void) = default;
    /** Destructor. */
    ~qvi_rmi_client(void);
    /** Returns a pointer to the client's hwloc instance. */
    qvi_hwloc &
    hwloc(void);
    /** Discovers server connection information. */
    static int
    discover(
        int &portno
    );
    /** Connects a client to to the server specified by the provided info. */
    int
    connect(
        const std::string &url,
        const int portno
    );
    /** Returns the current cpuset of the provided PID. */
    int
    get_cpubind(
        pid_t task_id,
        qvi_hwloc_bitmap &cpuset
    ) const;
    /** Sets the cpuset of the provided PID. */
    int
    set_cpubind(
        pid_t task_id,
        const qvi_hwloc_bitmap &cpuset
    );
    /** Returns a new hardware pool based on the intrinsic scope specifier. */
    int
    get_intrinsic_hwpool(
        const std::vector<pid_t> &who,
        qv_scope_intrinsic_t iscope,
        qvi_hwpool &hwpool
    );
    /** Returns the depth of the provided object type. */
    int
    get_obj_depth(
        qv_hw_obj_type_t type,
        int &depth
    );
    /** Returns the number of objects in the provided cpuset. */
    int
    get_nobjs_in_cpuset(
        qv_hw_obj_type_t target_obj,
        const qvi_hwloc_bitmap &cpuset,
        int &nobjs
    );
    /** Returns a device ID string for the requested device. */
    int
    get_device_in_cpuset(
        qv_hw_obj_type_t dev_obj,
        int dev_i,
        const qvi_hwloc_bitmap &cpuset,
        qv_device_id_type_t dev_id_type,
        std::string &dev_id
    );
    /** Returns a cpuset representing n objects of the requested type. */
    int
    get_cpuset_for_nobjs(
        hwloc_const_cpuset_t cpuset,
        qv_hw_obj_type_t obj_type,
        int nobjs,
        qvi_hwloc_bitmap &result
    );
    /** Sends a shutdown message to the server. */
    int
    send_shutdown_message(void);
};

/**
 * Returns a connection URL. When called with a portno of QVI_COMM_PORT_UNSET,
 * then a valid portno is determined via an environment variable and returned.
 * If portno is set, then a URL is generated based on the provided port number.
 */
int
qvi_rmi_get_url(
    std::string &url,
    int &portno
);

/**
 *
 */
std::string
qvi_rmi_conn_env_ers(void);

/**
 *
 */
std::string
qvi_rmi_discovery_ers(void);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
