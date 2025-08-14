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
    qvi_mpi *m_mpi = nullptr;
    /** Underlying group instance. */
    qvi_mpi_group m_mpi_group;
public:
    /** Default constructor. */
    qvi_group_mpi(void) = default;
    /** Constructor. */
    qvi_group_mpi(
        qvi_mpi *mpi_ctx
    );
    /** Destructor. */
    virtual ~qvi_group_mpi(void) = default;

    virtual qvi_task &
    task(void)
    {
        return m_task;
    }

    virtual int
    size(void) const
    {
        return m_mpi_group.size();
    }

    virtual int
    rank(void) const
    {
        return m_mpi_group.rank();
    }

    virtual std::vector<pid_t>
    pids(void) const {
        return m_mpi_group.pids();
    }

    virtual int
    barrier(void) const
    {
        return m_mpi_group.barrier();
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
        return m_mpi_group.gather_bbuffs(
            txbuff, root, rxbuffs
        );
    }

    virtual int
    scatter(
        const std::vector<qvi_bbuff> &txbuffs,
        int root,
        qvi_bbuff &rxbuff
    ) const {
        return m_mpi_group.scatter_bbuffs(
            txbuffs, root, rxbuff
        );
    }
    /** Returns a duplicate of the underlying MPI group communicator. */
    int
    comm_dup(
        MPI_Comm *comm
    ) const {
        return m_mpi_group.comm_dup(comm);
    }
};

struct qvi_zgroup_mpi : public qvi_group_mpi {
    /** Default constructor. */
    qvi_zgroup_mpi(void) = delete;
    /** Constructor. */
    qvi_zgroup_mpi(
        MPI_Comm comm
    ) {
        int rc = qvi_new(&m_mpi, comm);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
        // Finish task initialization after we finish MPI initialization because
        // the server daemon may have been started during qvi_mpi_init().
        rc = m_task.connect_to_server();
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
    }
    /** Destructor. */
    virtual ~qvi_zgroup_mpi(void)
    {
        qvi_delete(&m_mpi);
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
