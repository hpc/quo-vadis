/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-bind.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-bind.h"
#include "qvi-utils.h"

using qvi_bind_bitmap_stack_t = std::stack<hwloc_cpuset_t>;

// Type definition
struct qvi_bind_stack_s {
    int qvim_rc = QV_SUCCESS;
    /** Initialized task instance. */
    qvi_task_t *task = nullptr;
    /** Client RMI instance. */
    qvi_rmi_client_t *rmi = nullptr;
    /** The bind stack. */
    qvi_bind_bitmap_stack_t stack;
    /** Constructor */
    qvi_bind_stack_s(void) = default;
    /** Destructor */
    ~qvi_bind_stack_s(void)
    {
        while (!stack.empty()) {
            hwloc_bitmap_free(stack.top());
            stack.pop();
        }
    }
};

int
qvi_bind_stack_new(
    qvi_bind_stack_t **bstack
) {
    return qvi_new_rc(bstack);
}

void
qvi_bind_stack_free(
    qvi_bind_stack_t **bstack
) {
    qvi_delete(bstack);
}

int
qvi_bind_stack_init(
    qvi_bind_stack_t *bstack,
    qvi_task_t *task,
    qvi_rmi_client_t *rmi
) {
    // Cache pointer to initialized infrastructure.
    bstack->task = task;
    bstack->rmi = rmi;
    // Cache current binding.
    hwloc_cpuset_t current_bind = nullptr;
    int rc = qvi_rmi_task_get_cpubind(
        rmi,
        qvi_task_task_id(task),
        &current_bind
    );
    if (rc != QV_SUCCESS) goto out;

    bstack->stack.push(current_bind);
out:
    if (rc != QV_SUCCESS) {
        hwloc_bitmap_free(current_bind);
    }
    return rc;
}

int
qvi_bind_push(
    qvi_bind_stack_t *bstack,
    hwloc_const_cpuset_t cpuset
) {
    // Copy input bitmap because we don't want to directly modify it.
    hwloc_cpuset_t bitmap_copy = nullptr;
    int rc = qvi_hwloc_bitmap_calloc(&bitmap_copy);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwloc_bitmap_copy(cpuset, bitmap_copy);
    if (rc != QV_SUCCESS) goto out;
    // Change policy
    rc = qvi_rmi_task_set_cpubind_from_cpuset(
        bstack->rmi,
        qvi_task_task_id(bstack->task),
        bitmap_copy
    );
    if (rc != QV_SUCCESS) goto out;
    // Push bitmap onto stack.
    bstack->stack.push(bitmap_copy);
out:
    if (rc != QV_SUCCESS) {
        hwloc_bitmap_free(bitmap_copy);
    }
    return rc;
}

int
qvi_bind_pop(
    qvi_bind_stack_t *bstack
) {
    hwloc_bitmap_free(bstack->stack.top());
    bstack->stack.pop();

    return qvi_rmi_task_set_cpubind_from_cpuset(
        bstack->rmi,
        qvi_task_task_id(bstack->task),
        bstack->stack.top()
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
