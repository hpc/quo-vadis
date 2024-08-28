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
 * @file qvi-group-omp.h
 */

#ifndef QVI_GROUP_OMP_H
#define QVI_GROUP_OMP_H

#include "qvi-common.h"
#include "qvi-group.h"
#include "qvi-omp.h"
#include "qvi-bbuff.h"

struct qvi_group_omp : public qvi_group {
private:
    /** Task associated with this group. */
    qvi_task *m_task = nullptr;
    /** Underlying group instance. */
    qvi_omp_group *m_ompgroup = nullptr;
public:
    /** Constructor. */
    qvi_group_omp(void);
    /** Destructor. */
    virtual ~qvi_group_omp(void);

    virtual qvi_task *
    task(void)
    {
        return m_task;
    }

    virtual int
    rank(void) const
    {
        return m_ompgroup->rank();
    }

    virtual int
    size(void) const
    {
        return m_ompgroup->size();
    }

    virtual int
    barrier(void)
    {
        return m_ompgroup->barrier();
    }

    virtual int
    make_intrinsic(
        qv_scope_intrinsic_t intrinsic
    );

    virtual int
    self(
        qvi_group **child
    );

    virtual int
    thsplit(
        int,
        qvi_group **
    ) {
        // TODO(skg) Need to test this.
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
        qvi_alloc_type_t *shared,
        qvi_bbuff ***rxbuffs
    ) {
        return m_ompgroup->gather(txbuff, root, shared, rxbuffs);
    }

    virtual int
    scatter(
        qvi_bbuff **txbuffs,
        int root,
        qvi_bbuff **rxbuff
    ) {
        return m_ompgroup->scatter(txbuffs, root, rxbuff);
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
