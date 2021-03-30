/*
 * Copyright (c)      2021 Triad National Security, LLC
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

#include "qvi-common.h"
#include "qvi-bind.h"

using qvi_bitmap_stack_t = std::stack<hwloc_bitmap_t>;

// Type definition
struct qvi_bind_stack_s {
    /** Initialized task instance. */
    qv_task_t *task = nullptr;
    /** Initialized hwloc instance. */
    qvi_hwloc_t *hwloc = nullptr;
    /** The bind stack. */
    qvi_bitmap_stack_t *stack = nullptr;
};

int
qvi_bind_stack_new(
    qvi_bind_stack_t **bstack
) {
    int rc = QV_SUCCESS;

    qvi_bind_stack_t *ibstack = qvi_new qvi_bind_stack_t;
    if (!ibstack) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ibstack->stack = qvi_new qvi_bitmap_stack_t;
    if (!ibstack->stack) {
        rc = QV_ERR_OOR;
        goto out;
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_bind_stack_free(&ibstack);
    }
    *bstack = ibstack;
    return rc;
}

void
qvi_bind_stack_free(
    qvi_bind_stack_t **bstack
) {
    qvi_bind_stack_t *ibstack = *bstack;
    if (!ibstack) return;
    // The contents of the stack are NOT owned by us.
    delete ibstack->stack;
    delete ibstack;
    *bstack = nullptr;
}

int
qvi_bind_stack_init(
    qvi_bind_stack_t *bstack,
    qv_task_t *task,
    qvi_hwloc_t *hwloc
) {
    // Cache pointer to initialized infrastructure.
    bstack->task = task;
    bstack->hwloc = hwloc;
    // Cache current binding.
    hwloc_bitmap_t current_bind;
    int rc = qvi_hwloc_task_get_cpubind(
        hwloc,
        qvi_task_pid(task),
        &current_bind
    );
    if (rc != QV_SUCCESS) return rc;
    bstack->stack->push(current_bind);

    return QV_SUCCESS;
}

int
qvi_bind_push(
    qvi_bind_stack_t *bstack,
    hwloc_bitmap_t bitmap
) {
    // Change policy
    int rc = qvi_hwloc_set_cpubind_from_bitmap(
        bstack->hwloc,
        bitmap
    );
    if (rc != QV_SUCCESS) {
        return rc;
    }

    bstack->stack->push(bitmap);

    return QV_SUCCESS;
}

int
qvi_bind_pop(
    qvi_bind_stack_t *bstack
) {
    hwloc_bitmap_t current_bind = bstack->stack->top();
    hwloc_bitmap_free(current_bind);
    bstack->stack->pop();

    int rc = qvi_hwloc_set_cpubind_from_bitmap(
        bstack->hwloc,
        bstack->stack->top()
    );
    if (rc != QV_SUCCESS) {
        return rc;
    }

    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
