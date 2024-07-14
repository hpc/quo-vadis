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

using qvi_task_bind_stack_t = std::stack<qvi_hwloc_bitmap_s>;

struct qvi_task_s {
    /** Client-side connection to the RMI. */
    qvi_rmi_client_t *rmi = nullptr;
    /** The task's bind stack. */
    qvi_task_bind_stack_t stack;
    /** Returns the caller's thread ID. */
    static pid_t
    me(void)
    {
        return qvi_gettid();
    }
    /** Connects to the RMI server. */
    int
    connect_to_server(void)
    {
        std::string url;
        const int rc = qvi_url(url);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            qvi_log_error("{}", qvi_conn_ers());
            return rc;
        }
        return qvi_rmi_client_connect(rmi, url);
    }
    /** Initializes the bind stack. */
    int
    bind_stack_init(void)
    {
        // Cache current binding.
        hwloc_cpuset_t current_bind = nullptr;
        const int rc = qvi_rmi_task_get_cpubind(
            rmi, me(), &current_bind
        );
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

        stack.push(qvi_hwloc_bitmap_s(current_bind));
        hwloc_bitmap_free(current_bind);
        return rc;
    }
    /** Default constructor. */
    qvi_task_s(void)
    {
        int rc = qvi_rmi_client_new(&rmi);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
        // Connect to our server.
        rc = connect_to_server();
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
        // Initialize our bind stack.
        rc = bind_stack_init();
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    }
    /** Copy constructor. */
    qvi_task_s(const qvi_task_s &src) = delete;
    /** Destructor. */
    ~qvi_task_s(void)
    {
        while (!stack.empty()) {
            stack.pop();
        }
        qvi_rmi_client_free(&rmi);
    }
    /** Changes the task's affinity. */
    int
    bind_push(
        hwloc_const_cpuset_t cpuset
    ) {
        // Copy input bitmap because we don't want to directly modify it.
        qvi_hwloc_bitmap_s bitmap_copy(cpuset);
        // Change policy
        const int rc = qvi_rmi_task_set_cpubind_from_cpuset(
            rmi, me(), bitmap_copy.cdata()
        );
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        // Push bitmap onto stack.
        stack.push(bitmap_copy);
        return rc;
    }
    /** */
    int
    bind_pop(void)
    {
        stack.pop();

        return qvi_rmi_task_set_cpubind_from_cpuset(
            rmi, me(), stack.top().cdata()
        );
    }
    /** Returns the task's current cpuset. */
    int
    bind_top(
        hwloc_cpuset_t *dest
    ) {
        return qvi_hwloc_bitmap_dup(stack.top().cdata(), dest);
    }
};

pid_t
qvi_task_id(void)
{
    return qvi_task_s::me();
}

int
qvi_task_new(
    qvi_task_t **task
) {
    return qvi_new(task);
}

void
qvi_task_free(
    qvi_task_t **task
) {
    qvi_delete(task);
}

qvi_rmi_client_t *
qvi_task_rmi(
    qvi_task_t *task
) {
    return task->rmi;
}

int
qvi_task_bind_push(
    qvi_task_t *task,
    hwloc_const_cpuset_t cpuset
) {
    return task->bind_push(cpuset);
}

int
qvi_task_bind_pop(
    qvi_task_t *task
) {
    return task->bind_pop();
}

int
qvi_task_bind_top(
    qvi_task_t *task,
    hwloc_cpuset_t *dest
) {
    return task->bind_top(dest);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
