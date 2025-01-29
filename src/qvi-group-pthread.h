/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-pthread.h
 */

#ifndef QVI_GROUP_PTHREAD_H
#define QVI_GROUP_PTHREAD_H

#include "qvi-common.h"
#include "qvi-group.h"
#include "qvi-pthread.h"
#include "qvi-bbuff.h"

struct qvi_group_pthread : public qvi_group {
protected:
    /**
     * Points to per-process, per-thread_split()
     * information that maintains Pthread group context.
     */
    qvi_pthread_group_context *m_context = nullptr;
public:
    /** Underlying group instance. */
    qvi_pthread_group *thgroup = nullptr;
    /** Default constructor. */
    qvi_group_pthread(void) = delete;
    /**
     * Constructor that is called by the parent process to setup
     * base infrastructure required during a thread_split().
     */
    qvi_group_pthread(
        qvi_pthread_group_context *ctx,
        int group_size
    );
    /**
     * Constructor that is collective across ALL threads in the parent group.
     */
    qvi_group_pthread(
        qvi_pthread_group_context *ctx,
        qvi_pthread_group *thread_group
    );
    /** Destructor. */
    virtual ~qvi_group_pthread(void);

    virtual qvi_task *
    task(void)
    {
        return thgroup->task();
    }

    virtual int
    rank(void) const
    {
        return thgroup->rank();
    }

    virtual int
    size(void) const
    {
        return thgroup->size();
    }

    virtual int
    barrier(void)
    {
        return thgroup->barrier();
    }

    virtual int
    make_intrinsic(
        qv_scope_intrinsic_t
    ) {
        // Nothing to do here because a Pthread group cannot be created outside
        // of another group. For example, a thread_split can be called from a
        // process context, which can be an intrinsic group, but not from a
        // threaded context alone.
        return QV_SUCCESS;
    }

    virtual int
    self(
        qvi_group **child
    );

    virtual int
    thread_split(
        int,
        qvi_group **
    ) {
        return QV_ERR_NOT_SUPPORTED;
    }
    /**
     * This is a collective call across all threads in the parent thread group.
     */
    virtual int
    split(
        int color,
        int key,
        qvi_group **child
    );

    virtual int
    gather(
        qvi_bbuff *txbuff,
        int root,
        qvi_bbuff_alloc_type_t *alloc_type,
        qvi_bbuff ***rxbuffs
    ) {
        return thgroup->gather(
           txbuff, root, alloc_type, rxbuffs
        );
    }

    virtual int
    scatter(
        qvi_bbuff **txbuffs,
        int root,
        qvi_bbuff **rxbuff
    ) {
        return thgroup->scatter(
            txbuffs, root, rxbuff
        );
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
