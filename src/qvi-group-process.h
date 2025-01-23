/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-process.h
 */

#ifndef QVI_GROUP_PROCESS_H
#define QVI_GROUP_PROCESS_H

#include "qvi-common.h"
#include "qvi-group.h"
#include "qvi-process.h"
#include "qvi-bbuff.h"

struct qvi_group_process : public qvi_group {
protected:
    /** Task associated with this group. */
    qvi_task *m_task = nullptr;
    /** Underlying group instance. */
    qvi_process_group_t *m_proc_group = nullptr;
public:
    /** Constructor. */
    qvi_group_process(void);
    /** Destructor. */
    virtual ~qvi_group_process(void);

    virtual qvi_task *
    task(void)
    {
        return m_task;
    }

    virtual int
    rank(void) const
    {
        return qvi_process_group_id(m_proc_group);
    }

    virtual int
    size(void) const
    {
        return qvi_process_group_size(m_proc_group);
    }

    virtual int
    barrier(void)
    {
        return qvi_process_group_barrier(m_proc_group);
    }

    virtual int
    make_intrinsic(
        qv_scope_intrinsic_t
    ) {
        // NOTE: the provided scope doesn't affect how
        // we create the process group, so we ignore it.
        return qvi_process_group_new(&m_proc_group);
    }

    virtual int
    self(
        qvi_group **child
    );

    virtual int
    split(
        int,
        int,
        qvi_group **child
    ) {
        // NOTE: The concept of coloring with a provided key doesn't apply here,
        // so ignore.  Also, because this is in the context of a process, the
        // concept of splitting doesn't really apply here, so just create
        // another process group, self will suffice.
        return self(child);
    }

    virtual int
    gather(
        qvi_bbuff *txbuff,
        int root,
        qvi_bbuff_alloc_type_t *alloc_type,
        qvi_bbuff ***rxbuffs
    ) {
        return qvi_process_group_gather_bbuffs(
            m_proc_group, txbuff, root, alloc_type, rxbuffs
        );
    }

    virtual int
    scatter(
        qvi_bbuff **txbuffs,
        int root,
        qvi_bbuff **rxbuff
    ) {
        return qvi_process_group_scatter_bbuffs(
            m_proc_group, txbuffs, root, rxbuff
        );
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
