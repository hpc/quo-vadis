/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-process.h
 *
 */

#ifndef QVI_GROUP_PROCESS_H
#define QVI_GROUP_PROCESS_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-group.h"
#include "qvi-process.h"

struct qvi_group_process_s : public qvi_group_s {
    /** Process instance pointer. */
    qvi_process_t *proc = nullptr;
    /** Underlying group instance. */
    qvi_process_group_t *proc_group = nullptr;
    /** Constructor. */
    qvi_group_process_s(void) = default;
    /** Destructor. */
    virtual ~qvi_group_process_s(void)
    {
        qvi_process_group_free(&proc_group);
    }

    int
    initialize(
        qvi_process_t *proc_a
    ) {
        if (!proc_a) qvi_abort();

        proc = proc_a;
        return QV_SUCCESS;
    }

    virtual qvi_task_t *
    task(void)
    {
        return qvi_process_task_get(proc);
    }

    virtual int
    id(void)
    {
        return qvi_process_group_id(proc_group);
    }

    virtual int
    size(void)
    {
        return qvi_process_group_size(proc_group);
    }

    virtual int
    barrier(void)
    {
        return qvi_process_group_barrier(proc_group);
    }

    virtual int
    intrinsic(
        qv_scope_intrinsic_t intrinsic,
        qvi_group_s **group
    );

    virtual int
    self(
        qvi_group_s **child
    );

    virtual int
    split(
        int,
        int,
        qvi_group_s **child
    ) {
        // NOTE: The concept of coloring with a provided key doesn't apply here,
        // so ignore.  Also, because this is in the context of a process, the
        // concept of splitting doesn't really apply here, so just create
        // another process group, self will suffice.
        return self(child);
    }

    virtual int
    gather(
        qvi_bbuff_t *txbuff,
        int root,
        qvi_bbuff_t ***rxbuffs,
        int *shared
    ) {
        return qvi_process_group_gather_bbuffs(
            proc_group, txbuff, root, rxbuffs, shared
        );
    }

    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    ) {
        return qvi_process_group_scatter_bbuffs(
            proc_group, txbuffs, root, rxbuff
        );
    }
};
typedef qvi_group_process_s qvi_group_process_t;

struct qvi_zgroup_process_s : public qvi_group_process_s {
    /** Constructor. */
    qvi_zgroup_process_s(void)
    {
        const int rc = qvi_process_new(&proc);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
    }
    /** Destructor. */
    virtual ~qvi_zgroup_process_s(void)
    {
        qvi_process_free(&proc);
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
