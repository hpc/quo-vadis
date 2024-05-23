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
    /**
     * Initialized qvi_process_t instance
     * embedded in process group instances.
     */
    qvi_process_t *proc = nullptr;
    /** Underlying group instance. */
    qvi_process_group_t *proc_group = nullptr;
    /** Base constructor that does minimal work. */
    qvi_group_process_s(void) = default;
    /** Virtual destructor. */
    virtual ~qvi_group_process_s(void)
    {
        qvi_process_group_free(&proc_group);
    }
    /** Initializes the instance. */
    int
    initialize(qvi_process_t *proc_a)
    {
        if (!proc_a) qvi_abort();

        proc = proc_a;
        return QV_SUCCESS;
    }
    /** Returns the caller's task_id. */
    virtual qvi_task_id_t
    task_id(void)
    {
        return qvi_task_task_id(qvi_process_task_get(proc));
    }
    /** Returns the caller's group ID. */
    virtual int
    id(void)
    {
        return qvi_process_group_id(proc_group);
    }
    /** Returns the number of members in this group. */
    virtual int
    size(void)
    {
        return qvi_process_group_size(proc_group);
    }
    /** Performs node-local group barrier. */
    virtual int
    barrier(void)
    {
        return qvi_process_group_barrier(proc_group);
    }
    /**
     * Creates a new self group with a single member: the caller.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    self(
        qvi_group_s **child
    );
    /**
     * Creates new groups by splitting this group based on color, key.
     * Returns the appropriate newly created child group to the caller.
     */
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
    /** Gathers bbuffs to specified root. */
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
    /** Scatters bbuffs from specified root. */
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

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
