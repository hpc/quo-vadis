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
#include "qvi-task.h"
#include "qvi-group.h"
#include "qvi-bbuff.h"

struct qvi_group_process : public qvi_group {
private:
    /** Size of group. This is fixed. */
    static constexpr int m_size = 1;
    /** ID (rank) in group. This is fixed. */
    static constexpr int m_rank = 0;
    /** Task associated with this group. */
    qvi_task m_task;
public:
    /** Constructor. */
    qvi_group_process(void)
    {
        const int rc = m_task.connect_to_server();
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    }
    /** Destructor. */
    virtual ~qvi_group_process(void) = default;

    virtual qvi_task &
    task(void)
    {
        return m_task;
    }

    virtual int
    size(void) const
    {
        return m_size;
    }

    virtual int
    rank(void) const
    {
        return m_rank;
    }

    virtual std::vector<pid_t>
    pids(void) const
    {
        return { m_task.mytid() };
    }

    virtual int
    barrier(void) const
    {
        // Nothing to do since process groups contain a single member.
        return QV_SUCCESS;
    }

    virtual int
    make_intrinsic(
        qv_scope_intrinsic_t
    ) {
        // NOTE: the provided scope doesn't affect how
        // we create the process group, so we ignore it.
        return QV_SUCCESS;
    }

    virtual int
    self(
        qvi_group **child
    ) {
        // Because this is in the context of a process, the concept of splitting
        // doesn't really apply here, so just create another process group.
        qvi_group_process *ichild = nullptr;
        const int rc = qvi_new(&ichild);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            qvi_delete(&ichild);
        }
        *child = ichild;
        return rc;
    }

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
    ) const {
        const int group_size = size();
        // Make sure that we are dealing with a valid process group.
        // If not, this is an internal development error, so abort.
        if (root != 0 || group_size != 1) {
            qvi_abort();
        }
        // Zero initialize array of pointers to nullptr.
        qvi_bbuff **bbuffs = new qvi_bbuff *[group_size]();

        const int rc = qvi_dup(*txbuff, &bbuffs[0]);
        if (rc != QV_SUCCESS) {
            if (bbuffs) {
                qvi_delete(&bbuffs[0]);
                delete[] bbuffs;
            }
            bbuffs = nullptr;
        }
        *rxbuffs = bbuffs;
        *alloc_type = QVI_BBUFF_ALLOC_PRIVATE;
        return rc;
    }

    virtual int
    scatter(
        qvi_bbuff **txbuffs,
        int root,
        qvi_bbuff **rxbuff
    ) const {
        const int group_size = size();
        // Make sure that we are dealing with a valid process group.
        // If not, this is an internal development error, so abort.
        if (qvi_unlikely(root != 0 || group_size != 1)) {
            qvi_abort();
        }
        // There should always be only one at the root (us).
        qvi_bbuff *mybbuff = nullptr;
        const int rc = qvi_dup(*txbuffs[root], &mybbuff);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            qvi_delete(&mybbuff);
        }
        *rxbuff = mybbuff;
        return rc;
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
