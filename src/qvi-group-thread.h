/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Inria
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Bordeaux INP
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-thread.h
 */

#ifndef QVI_GROUP_THREAD_H
#define QVI_GROUP_THREAD_H

#include "qvi-group.h"
#include "qvi-thread.h"

struct qvi_group_thread_s : public qvi_group_s {
    /**
     * Initialized qvi_thread_t instance
     * embedded in thread group instances.
     */
    qvi_thread_t *th = nullptr;
    /** Underlying group instance. */
    qvi_thread_group_t *th_group = nullptr;
    /** Constructor. */
    qvi_group_thread_s(void)
    {
        int rc = qvi_thread_new(&th);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
    }
    /** Destructor. */
    virtual ~qvi_group_thread_s(void)
    {
        qvi_thread_group_free(&th_group);
    }

    virtual int
    id(void)
    {
        return qvi_thread_group_id(th_group);
    }

    virtual int
    size(void)
    {
        return qvi_thread_group_size(th_group);
    }

    virtual int
    barrier(void)
    {
        // TODO(skg) FIXME
        return QV_SUCCESS;
    }

    virtual int
    make_intrinsic(
        qv_scope_intrinsic_t intrinsic
    );

    virtual int
    self(
        qvi_group_s **child
    );

    virtual int
    split(
        int color,
        int key,
        qvi_group_s **child
    );

    virtual int
    gather(
        qvi_bbuff_t *txbuff,
        int root,
        qvi_bbuff_t ***rxbuffs,
        int *shared
    ) {
        return qvi_thread_group_gather_bbuffs(
           th_group, txbuff, root, rxbuffs, shared
        );
    }

    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    ) {
        return qvi_thread_group_scatter_bbuffs(
            th_group, txbuffs, root, rxbuff
        );
    }
};
typedef qvi_group_thread_s qvi_group_thread_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
