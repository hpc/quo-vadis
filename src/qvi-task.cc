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
#include "qvi-rmi.h"
#include "qvi-utils.h"

pid_t
qvi_task::mytid(void)
{
    return qvi_gettid();
}

int
qvi_task::connect_to_server(void)
{
    std::string url;
    const int rc = qvi_url(url);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_log_error("{}", qvi_conn_ers());
        return rc;
    }
    return m_rmi->connect(url);
}

int
qvi_task::init_bind_stack(void)
{
    // Cache current binding.
    hwloc_cpuset_t current_bind = nullptr;
    const int rc = m_rmi->get_cpubind(mytid(), &current_bind);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    m_stack.push(qvi_hwloc_bitmap(current_bind));
    qvi_hwloc_bitmap_delete(&current_bind);
    return rc;
}

qvi_task::qvi_task(void)
{
    int rc = qvi_new(&m_rmi);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    // Connect to our server.
    rc = connect_to_server();
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    // Initialize our bind stack.
    rc = init_bind_stack();
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
}

qvi_task::~qvi_task(void)
{
    while (!m_stack.empty()) {
        m_stack.pop();
    }
    qvi_delete(&m_rmi);
}

qvi_rmi_client &
qvi_task::rmi(void)
{
    if (qvi_unlikely(!m_rmi)) {
        throw qvi_runtime_error();
    }
    return *m_rmi;
}

qvi_hwloc_t *
qvi_task::hwloc(void)
{
    assert(m_rmi);
    return m_rmi->hwloc();
}

int
qvi_task::bind_push(
    hwloc_const_cpuset_t cpuset
) {
    // Copy input bitmap because we don't want to directly modify it.
    qvi_hwloc_bitmap bitmap_copy(cpuset);
    // Change policy
    const int rc = m_rmi->set_cpubind(mytid(), bitmap_copy.cdata());
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Push bitmap onto stack.
    m_stack.push(bitmap_copy);
    return rc;
}

int
qvi_task::bind_pop(void)
{
    m_stack.pop();

    return m_rmi->set_cpubind(mytid(), m_stack.top().cdata());
}

int
qvi_task::bind_top(
    hwloc_cpuset_t *result
) {
    return qvi_hwloc_bitmap_dup(m_stack.top().cdata(), result);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
