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
    /** Virtual destructor. */
    virtual ~qvi_group_mpi_s(void)
    {
        qvi_mpi_group_free(&mpi_group);
    }
    /** Initializes the instance. */
    int
    initialize(qvi_mpi_t *mpi_a)
    {
        if (!mpi_a) qvi_abort();

        mpi = mpi_a;
        return QV_SUCCESS;
    }
    /** Returns the caller's task_id. */
    virtual qvi_task_id_t
    task_id(void)
    {
        return qvi_task_task_id(qvi_mpi_task_get(mpi));
    }
    /** Returns the caller's group ID. */
    virtual int
    id(void)
    {
        return qvi_mpi_group_id(mpi_group);
    }
    /** Returns the number of members in this group. */
    virtual int
    size(void)
    {
        return qvi_mpi_group_size(mpi_group);
    }
    /** Performs node-local group barrier. */
    virtual int
    barrier(void)
    {
        return qvi_mpi_group_barrier(mpi_group);
    }
    /**
     * Creates a new self group with a single member: the caller.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    self(
        qvi_group_s **child
    );
    /**
     * Creates new groups by splitting this group based on color, key.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    split(
        int color,
        int key,
        qvi_group_s **child
    );
    /** Gathers bbuffs to specified root. */
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
    /** Scatters bbuffs from specified root. */
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

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
