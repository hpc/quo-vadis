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
 * @file qvi-task.cc
 */

#include "qvi-task.h"
#include "qvi-utils.h"

pid_t
qvi_task::mytid(void)
{
    return qvi_gettid();
}

qvi_rmi_client &
qvi_task::rmi(void)
{
    return m_rmi;
}

qvi_hwloc &
qvi_task::hwloc(void)
{
    return m_rmi.hwloc();
}

int
qvi_task::m_connect_to_server(void)
{
    // Discover the server's port number.
    int portno = QVI_PORT_UNSET;
    int rc = qvi_rmi_client::discover(portno);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_log_error("{}", qvi_rmi_discovery_ers());
        return QV_RES_UNAVAILABLE;
    }

    std::string url;
    rc = qvi_rmi_get_url(url, portno);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_log_error("{}", qvi_rmi_conn_env_ers());
        return QV_RES_UNAVAILABLE;
    }

    rc = m_rmi.connect(url, portno);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        const std::string msg =
            "\n\n################################################\n"
            "# A client couldn't communicate with its server.\n"
            "# Ensure {} is running and reachable."
            "\n################################################\n\n";
        qvi_log_error(msg, QVI_DAEMON_NAME);
    }
    return rc;
}

int
qvi_task::m_init_bind_stack(void)
{
    // Cache current binding.
    qvi_hwloc_bitmap current_bind;
    const int rc = m_rmi.get_cpubind(mytid(), current_bind);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    m_stack.push(current_bind);
    return QV_SUCCESS;
}

int
qvi_task::connect_to_server(void)
{
    // Connect to our server.
    int rc = m_connect_to_server();
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Initialize our bind stack.
    return m_init_bind_stack();
}

int
qvi_task::bind_push(
    const qvi_hwloc_bitmap &cpuset
) {
    // Change policy
    const int rc = m_rmi.set_cpubind(mytid(), cpuset);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Push bitmap onto stack.
    m_stack.push(cpuset);
    return rc;
}

int
qvi_task::bind_pop(void)
{
    m_stack.pop();

    return m_rmi.set_cpubind(mytid(), m_stack.top());
}

int
qvi_task::bind_top(
    qvi_hwloc_bitmap &result
) {
    return result.set(m_stack.top().cdata());
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
