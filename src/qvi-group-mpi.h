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
protected:
    /** Task associated with this group. */
    qvi_task_t *m_task = nullptr;
    /** Points to the base MPI context information. */
    qvi_mpi_t *m_mpi = nullptr;
    /** Underlying group instance. */
    qvi_mpi_group_t *m_mpi_group = nullptr;
public:
    /** Default constructor. */
    qvi_group_mpi_s(void);
    /** Constructor. */
    qvi_group_mpi_s(
        qvi_mpi_t *mpi_ctx
    );
    /** Destructor. */
    virtual ~qvi_group_mpi_s(void);

    virtual qvi_task_t *
    task(void)
    {
        return m_task;
    }

    virtual int
    rank(void) const
    {
        return qvi_mpi_group_rank(m_mpi_group);
    }

    virtual int
    size(void) const
    {
        return qvi_mpi_group_size(m_mpi_group);
    }

    virtual int
    barrier(void)
    {
        return qvi_mpi_group_barrier(m_mpi_group);
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
        bool *shared,
        qvi_bbuff_t ***rxbuffs
    ) {
        return qvi_mpi_group_gather_bbuffs(
            m_mpi_group, txbuff, root, shared, rxbuffs
        );
    }

    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    ) {
        return qvi_mpi_group_scatter_bbuffs(
            m_mpi_group, txbuffs, root, rxbuff
        );
    }
    /** Returns a duplicate of the underlying MPI group communicator. */
    int
    comm_dup(
        MPI_Comm *comm
    ) {
        return qvi_mpi_group_comm_dup(m_mpi_group, comm);
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
        int rc = qvi_mpi_new(&m_mpi);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
        /** Initialize the MPI group with the provided communicator. */
        rc = qvi_mpi_init(m_mpi, comm);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
    }
    /** Destructor. */
    virtual ~qvi_zgroup_mpi_s(void)
    {
        qvi_mpi_delete(&m_mpi);
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
