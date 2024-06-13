/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-mpi.h
 *
 */

#ifndef QVI_GROUP_MPI_H
#define QVI_GROUP_MPI_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-group.h"
#include "qvi-mpi.h"

struct qvi_group_mpi_s : public qvi_group_s {
    /** Initialized qvi_mpi_t instance embedded in MPI group instances. */
    qvi_mpi_t *mpi = nullptr;
    /** Underlying group instance. */
    qvi_mpi_group_t *mpi_group = nullptr;
    /** Constructor. */
    qvi_group_mpi_s(void) = default;
    /** Destructor. */
    virtual ~qvi_group_mpi_s(void)
    {
        qvi_mpi_group_free(&mpi_group);
    }
    /** Initializes the instance. */
    int
    initialize(
        qvi_mpi_t *mpi_a
    ) {
        if (!mpi_a) qvi_abort();

        mpi = mpi_a;
        return QV_SUCCESS;
    }

    virtual qvi_task_t *
    task(void)
    {
        return qvi_mpi_task_get(mpi);
    }

    virtual int
    id(void)
    {
        return qvi_mpi_group_id(mpi_group);
    }

    virtual int
    size(void)
    {
        return qvi_mpi_group_size(mpi_group);
    }

    virtual int
    barrier(void)
    {
        return qvi_mpi_group_barrier(mpi_group);
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
        return qvi_mpi_group_gather_bbuffs(
            mpi_group, txbuff, root, rxbuffs, shared
        );
    }

    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    ) {
        return qvi_mpi_group_scatter_bbuffs(
            mpi_group, txbuffs, root, rxbuff
        );
    }
    /** Returns a duplicate of the underlying MPI group communicator. */
    int
    comm_dup(
        MPI_Comm *comm
    ) {
        return qvi_mpi_group_comm_dup(mpi_group, comm);
    }
};
typedef qvi_group_mpi_s qvi_group_mpi_t;

struct qvi_zgroup_mpi_s : public qvi_group_mpi_s {
    /** Constructor. */
    qvi_zgroup_mpi_s(void)
    {
        const int rc = qvi_mpi_new(&mpi);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
    }
    /** Destructor. */
    virtual ~qvi_zgroup_mpi_s(void)
    {
        qvi_mpi_free(&mpi);
    }
    /** Initializes the MPI group with the provided communicator. */
    int
    initialize(
        MPI_Comm comm
    ) {
        return qvi_mpi_init(mpi, comm);
    }
    /** Node-local task barrier. */
    virtual int
    barrier(void)
    {
        return qvi_mpi_node_barrier(mpi);
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
