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
 */

#ifndef QVI_GROUP_MPI_H
#define QVI_GROUP_MPI_H

#include "qvi-common.h"
#include "qvi-group.h"
#include "qvi-mpi.h"

struct qvi_group_mpi_s : public qvi_group_s {
    /** Points to the base MPI context information. */
    qvi_mpi_t *mpi = nullptr;
    /** Underlying group instance. */
    qvi_mpi_group_t *mpi_group = nullptr;
    /** Default constructor. */
    qvi_group_mpi_s(void) = default;
    /** Constructor. */
    qvi_group_mpi_s(
        qvi_mpi_t *mpi_ctx
    ) {
        if (!mpi_ctx) throw qvi_runtime_error();
        mpi = mpi_ctx;
    }
    /** Destructor. */
    virtual ~qvi_group_mpi_s(void)
    {
        qvi_mpi_group_free(&mpi_group);
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
    /** Default constructor. */
    qvi_zgroup_mpi_s(void) = delete;
    /** Constructor. */
    qvi_zgroup_mpi_s(
        MPI_Comm comm
    ) {
        int rc = qvi_mpi_new(&mpi);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
        /** Initialize the MPI group with the provided communicator. */
        rc = qvi_mpi_init(mpi, comm);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
    }
    /** Destructor. */
    virtual ~qvi_zgroup_mpi_s(void)
    {
        qvi_mpi_free(&mpi);
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
