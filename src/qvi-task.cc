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

using qvi_task_bind_stack_t = std::stack<hwloc_cpuset_t>;

struct qvi_task_s {
    /** Client-side connection to the RMI. */
    qvi_rmi_client_t *rmi = nullptr;
    /** The task's bind stack. */
    qvi_task_bind_stack_t stack;

    static qvi_task_id_t
    me(void)
    {
        return {QVI_TASK_TYPE_THREAD, qvi_gettid()};
    }

    int
    connect_to_server(void)
    {
        std::string url;
        const int rc = qvi_url(url);
        if (rc != QV_SUCCESS) {
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
        if (rc != QV_SUCCESS) return rc;

        stack.push(current_bind);
        return rc;
    }
    /** Default constructor. */
    qvi_task_s(void)
    {
        int rc = qvi_rmi_client_new(&rmi);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
        // Connect to our server.
        rc = connect_to_server();
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
        // Initialize our bind stack.
        rc = bind_stack_init();
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
    }
    /** Copy constructor. */
    qvi_task_s(const qvi_task_s &src) = delete;
    /** Destructor. */
    ~qvi_task_s(void)
    {
        while (!stack.empty()) {
            hwloc_bitmap_free(stack.top());
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
        hwloc_cpuset_t bitmap_copy = nullptr;
        int rc = qvi_hwloc_bitmap_dup(cpuset, &bitmap_copy);
        if (rc != QV_SUCCESS) return rc;
        // Change policy
        rc = qvi_rmi_task_set_cpubind_from_cpuset(
            rmi, me(), bitmap_copy
        );
        if (rc != QV_SUCCESS) goto out;
        // Push bitmap onto stack.
        stack.push(bitmap_copy);
    out:
        if (rc != QV_SUCCESS) {
            hwloc_bitmap_free(bitmap_copy);
        }
        return rc;
    }
    /** */
    int
    bind_pop(void)
    {
        hwloc_bitmap_free(stack.top());
        stack.pop();

        return qvi_rmi_task_set_cpubind_from_cpuset(
            rmi, me(), stack.top()
        );
    }
    /** */
    int
    bind_string(
        qv_bind_string_format_t format,
        char **str
    ) {
        char *istr = nullptr;

        hwloc_cpuset_t cpuset = nullptr;
        int rc = qvi_rmi_task_get_cpubind(
            rmi, me(), &cpuset
        );
        if (rc != QV_SUCCESS) goto out;

        switch (format) {
            case QV_BIND_STRING_AS_BITMAP:
                rc = qvi_hwloc_bitmap_asprintf(&istr, cpuset);
                break;
            case QV_BIND_STRING_AS_LIST:
                rc = qvi_hwloc_bitmap_list_asprintf(&istr, cpuset);
                break;
            default:
                rc = QV_ERR_INVLD_ARG;
        }
    out:
        hwloc_bitmap_free(cpuset);
        *str = istr;
        return rc;
    }
};

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

qvi_task_id_t
qvi_task_id(void)
{
    return qvi_task_s::me();
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
qvi_task_bind_string(
    qvi_task_t *task,
    qv_bind_string_format_t format,
    char **str
) {
    return task->bind_string(format, str);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
