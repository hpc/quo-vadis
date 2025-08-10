/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
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
#include "qvi-task.h"
#include "qvi-group.h"
#include "qvi-mpi.h"
#include "qvi-bbuff.h"

struct qvi_group_mpi : public qvi_group {
protected:
    /** Task associated with this group. */
    qvi_task m_task;
    /** Points to the base MPI context information. */
    qvi_mpi_t *m_mpi = nullptr;
    /** Underlying group instance. */
    qvi_mpi_group_t *m_mpi_group = nullptr;
public:
    /** Default constructor. */
    qvi_group_mpi(void) = default;
    /** Constructor. */
    qvi_group_mpi(
        qvi_mpi_t *mpi_ctx
    );
    /** Destructor. */
    virtual ~qvi_group_mpi(void);

    virtual qvi_task &
    task(void)
    {
        return m_task;
    }

    virtual int
    size(void) const
    {
        return qvi_mpi_group_size(m_mpi_group);
    }

    virtual int
    rank(void) const
    {
        return qvi_mpi_group_rank(m_mpi_group);
    }

    virtual std::vector<pid_t>
    pids(void) const {
        return qvi_mpi_group_pids(m_mpi_group);
    }

    virtual int
    barrier(void) const
    {
        return qvi_mpi_group_barrier(m_mpi_group);
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
    split(
        int color,
        int key,
        qvi_group **child
    );

    virtual int
    gather(
        const qvi_bbuff &txbuff,
        int root,
        std::vector<qvi_bbuff> &rxbuffs
    ) const {
        return qvi_mpi_group_gather_bbuffs(
            m_mpi_group, txbuff, root, rxbuffs
        );
    }

    virtual int
    scatter(
        const std::vector<qvi_bbuff> &txbuffs,
        int root,
        qvi_bbuff &rxbuff
    ) const {
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

struct qvi_zgroup_mpi_s : public qvi_group_mpi {
    /** Default constructor. */
    qvi_zgroup_mpi_s(void) = delete;
    /** Constructor. */
    qvi_zgroup_mpi_s(
        MPI_Comm comm
    ) {
        int rc = qvi_mpi_new(&m_mpi);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
        // Initialize the MPI group with the provided communicator.
        rc = qvi_mpi_init(m_mpi, comm);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
        // Finish task initialization after we finish MPI initialization because
        // the server daemon may have been started during qvi_mpi_init().
        rc = m_task.connect_to_server();
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
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
