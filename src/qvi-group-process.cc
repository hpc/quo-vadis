/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-process.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-group-process.h"

qvi_group_process_s::~qvi_group_process_s(void)
{
    qvi_process_group_free(&proc_group);
}

int
qvi_group_process_s::create(void)
{
    return qvi_process_group_new(&proc_group);
}

int
qvi_group_process_s::initialize(
    qvi_process_t *proc
) {
    assert(proc);
    if (!proc) return QV_ERR_INTERNAL;

    this->proc = proc;
    return QV_SUCCESS;
}

qvi_task_id_t
qvi_group_process_s::task_id(void)
{
    return qvi_task_task_id(qvi_process_task_get(proc));
}

int
qvi_group_process_s::id(void)
{
    return qvi_process_group_id(proc_group);
}

int
qvi_group_process_s::size(void)
{
    return qvi_process_group_size(proc_group);
}

int
qvi_group_process_s::barrier(void)
{
    return qvi_process_group_barrier(proc_group);
}

int
qvi_group_process_s::self(
    qvi_group_t **child
) {
    int rc = QV_SUCCESS;

    qvi_group_process_t *ichild = qvi_new qvi_group_process_t();
    if (!ichild) {
        rc = QV_ERR_OOR;
        goto out;
    }
    // Initialize the child with the parent's process instance.
    rc = ichild->initialize(proc);
    if (rc != QV_SUCCESS) goto out;
    // Because this is in the context of a process, the concept of splitting
    // doesn't really apply here, so just create another process group.
    rc = qvi_process_group_create(
        proc, &ichild->proc_group
    );
out:
    if (rc != QV_SUCCESS) {
        delete ichild;
        ichild = nullptr;
    }
    *child = ichild;
    return rc;
}

int
qvi_group_process_s::split(
    int,
    int,
    qvi_group_t **child
) {
    // NOTE: The concept of coloring with a provided key doesn't apply here, so
    // ignore.  Also, because this is in the context of a process, the concept
    // of splitting doesn't really apply here, so just create another process
    // group, self will suffice.
    return self(child);
}

int
qvi_group_process_s::gather(
    qvi_bbuff_t *txbuff,
    int root,
    qvi_bbuff_t ***rxbuffs,
    int *shared
) {
    return qvi_process_group_gather_bbuffs(
        proc_group, txbuff, root, rxbuffs, shared
    );
}

int
qvi_group_process_s::scatter(
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
) {
    return qvi_process_group_scatter_bbuffs(
        proc_group, txbuffs, root, rxbuff
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
