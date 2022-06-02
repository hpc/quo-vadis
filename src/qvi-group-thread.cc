/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-thread.cc
 *
 */

#include "qvi-common.h"
#include "qvi-group-thread.h"

qvi_group_thread_s::~qvi_group_thread_s(void)
{
    qvi_thread_group_free(&th_group);
}

int
qvi_group_thread_s::create(void)
{
    return qvi_thread_group_new(&th_group);
}

int
qvi_group_thread_s::initialize(
    qvi_thread_t *th
) {
    assert(th);
    if (!th) return QV_ERR_INTERNAL;

    this->th = th;
    return QV_SUCCESS;
}

int
qvi_group_thread_s::id(void)
{
    return qvi_thread_group_id(th_group);
}

int
qvi_group_thread_s::size(void)
{
    return qvi_thread_group_size(th_group);
}

int
qvi_group_thread_s::barrier(void)
{
    return qvi_thread_group_barrier(th_group);
}

int
qvi_group_thread_s::self(
    qvi_group_t **child
) {
    int rc = QV_SUCCESS;

    qvi_group_thread_t *ichild = qvi_new qvi_group_thread_t();
    if (!ichild) {
        rc = QV_ERR_OOR;
        goto out;
    }
    // Initialize the child with the parent's thread instance.
    rc = ichild->initialize(th);
    if (rc != QV_SUCCESS) goto out;
    // Because this is in the context of a thread, the concept of splitting
    // doesn't really apply here, so just create another thread group.
    rc = qvi_thread_group_create(
        th, &ichild->th_group
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
qvi_group_thread_s::split(
    int,
    int,
    qvi_group_t **child
) {
    // NOTE: The concept of coloring with a provided key doesn't apply here, so
    // ignore.  Also, because this is in the context of a thread, the concept
    // of splitting doesn't really apply here, so just create another thread
    // group, self will suffice.
    return self(child);
}

int
qvi_group_thread_s::gather(
    qvi_bbuff_t *txbuff,
    int root,
    qvi_bbuff_t ***rxbuffs
) {
    return qvi_thread_group_gather_bbuffs(
        th_group, txbuff, root, rxbuffs
    );
}

int
qvi_group_thread_s::scatter(
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
) {
    return qvi_thread_group_scatter_bbuffs(
        th_group, txbuffs, root, rxbuff
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
