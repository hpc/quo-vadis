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

struct qvi_group_omp_s : public qvi_group_s {
protected:
    /** Task associated with this group. */
    qvi_task_t *m_task = nullptr;
    /** Underlying group instance. */
    qvi_omp_group_t *m_ompgroup = nullptr;
public:
    /** Constructor. */
    qvi_group_omp_s(void);
    /** Destructor. */
    virtual ~qvi_group_omp_s(void);

    virtual qvi_task_t *
    task(void)
    {
        return m_task;
    }

    virtual int
    rank(void) const
    {
        return qvi_omp_group_id(m_ompgroup);
    }

    virtual int
    size(void) const
    {
        return qvi_omp_group_size(m_ompgroup);
    }

    virtual int
    barrier(void)
    {
        return qvi_omp_group_barrier(m_ompgroup);
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
    thsplit(
        int,
        qvi_group_s **
    ) {
        // TODO(skg) Need to test this.
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
        return qvi_omp_group_gather_bbuffs(
           m_ompgroup, txbuff, root, shared, rxbuffs
        );
    }

    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    ) {
        return qvi_omp_group_scatter_bbuffs(
            m_ompgroup, txbuffs, root, rxbuff
        );
    }
};
typedef qvi_group_omp_s qvi_group_omp_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
