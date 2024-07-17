/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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
qvi_task_s::mytid(void)
{
    return qvi_gettid();
}

int
qvi_task_s::connect_to_server(void)
{
    std::string url;
    const int rc = qvi_url(url);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_log_error("{}", qvi_conn_ers());
        return rc;
    }
    return qvi_rmi_client_connect(myrmi, url);
}

int
qvi_task_s::bind_stack_init(void)
{
    // Cache current binding.
    hwloc_cpuset_t current_bind = nullptr;
    const int rc = qvi_rmi_task_get_cpubind(
        myrmi, mytid(), &current_bind
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    mystack.push(qvi_hwloc_bitmap_s(current_bind));
    hwloc_bitmap_free(current_bind);
    return rc;
}

qvi_task_s::qvi_task_s(void)
{
    int rc = qvi_rmi_client_new(&myrmi);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    // Connect to our server.
    rc = connect_to_server();
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    // Initialize our bind stack.
    rc = bind_stack_init();
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
}

qvi_task_s::~qvi_task_s(void)
{
    while (!mystack.empty()) {
        mystack.pop();
    }
    qvi_rmi_client_free(&myrmi);
}

qvi_rmi_client_t *
qvi_task_s::rmi(void)
{
    assert(myrmi);
    return myrmi;
}

int
qvi_task_s::bind_push(
    hwloc_const_cpuset_t cpuset
) {
    // Copy input bitmap because we don't want to directly modify it.
    qvi_hwloc_bitmap_s bitmap_copy(cpuset);
    // Change policy
    const int rc = qvi_rmi_task_set_cpubind_from_cpuset(
        myrmi, mytid(), bitmap_copy.cdata()
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Push bitmap onto stack.
    mystack.push(bitmap_copy);
    return rc;
}

int
qvi_task_s::bind_pop(void)
{
    mystack.pop();

    return qvi_rmi_task_set_cpubind_from_cpuset(
        myrmi, mytid(), mystack.top().cdata()
    );
}

int
qvi_task_s::bind_top(
    hwloc_cpuset_t *dest
) {
    return qvi_hwloc_bitmap_dup(mystack.top().cdata(), dest);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
