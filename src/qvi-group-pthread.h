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
#include "qvi-bbuff.h"

struct qvi_group_pthread : public qvi_group {
    /** Underlying group instance. */
    qvi_pthread_group_t *thgroup = nullptr;
    /** Constructor. */
    qvi_group_pthread(void) = default;
    /** Constructor. */
    qvi_group_pthread(
        int group_size
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
        // Nothing to do.
        return QV_SUCCESS;
    }

    virtual int
    self(
        qvi_group **child
    );

    virtual int
    thsplit(
        int,
        qvi_group **
    ) {
        // TODO(skg)
        return QV_ERR_NOT_SUPPORTED;
    }

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
        qvi_alloc_type_t  *shared,
        qvi_bbuff ***rxbuffs
    ) {
        return thgroup->gather(
           txbuff, root, shared, rxbuffs
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
