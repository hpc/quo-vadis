/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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

struct qvi_group_pthread_s : public qvi_group_s {
    /** Underlying group instance. */
    qvi_pthread_group_t *thgroup = nullptr;
    /** Constructor. */
    qvi_group_pthread_s(void) = delete;
    /** Constructor. */
    qvi_group_pthread_s(
        int group_size
    );
    /** Destructor. */
    virtual ~qvi_group_pthread_s(void);

    virtual qvi_task_t *
    task(void)
    {
        return thgroup->task();
    }

    virtual int
    rank(void)
    {
        return thgroup->rank();
    }

    virtual int
    size(void)
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
        // Nothing to do.
        return QV_SUCCESS;
    }

    virtual int
    self(
        qvi_group_s **child
    );

    virtual int
    thsplit(
        int,
        qvi_group_s **
    ) {
        // TODO(skg)
        return QV_ERR_NOT_SUPPORTED;
    }

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
        bool *shared,
        qvi_bbuff_t ***rxbuffs
    ) {
        return thgroup->gather(
           txbuff, root, shared, rxbuffs
        );
    }

    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    ) {
        return thgroup->scatter(
            txbuffs, root, rxbuff
        );
    }
};
typedef qvi_group_pthread_s qvi_group_pthread_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
