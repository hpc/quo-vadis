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
 * @file qvi-mpi.h
 */

#ifndef QVI_MPI_H
#define QVI_MPI_H

#include "qvi-common.h"
#include "qvi-group.h"
#include "quo-vadis-mpi.h" // IWYU pragma: keep
#include "qvi-bbuff.h"

// Forward declarations.
struct qvi_mpi_group;
struct qvi_mpi;

/**
 * Intrinsic group IDs.
 */
static constexpr qvi_group_id_t QVI_MPI_GROUP_NULL = 0;
static constexpr qvi_group_id_t QVI_MPI_GROUP_SELF = 1;
static constexpr qvi_group_id_t QVI_MPI_GROUP_NODE = 2;
static constexpr qvi_group_id_t QVI_MPI_GROUP_WORLD = 3;

struct qvi_mpi_comm {
    friend qvi_mpi_group;
    friend qvi_mpi;
private:
    /** Underlying MPI communicator. */
    MPI_Comm m_mpi_comm = MPI_COMM_NULL;
    /** Communicator size. */
    int m_size = 0;
    /** Communicator rank. */
    int m_rank = 0;
public:
    /** Constructor. */
    qvi_mpi_comm(void) = default;
    /** Constructor. */
    qvi_mpi_comm(
        MPI_Comm comm,
        bool dup
    ) {
        int rc;
        if (dup) {
            rc = MPI_Comm_dup(comm, &m_mpi_comm);
            if (qvi_unlikely(rc != MPI_SUCCESS)) {
                throw qvi_runtime_error(QV_ERR_MPI);
            }
        }
        else {
            m_mpi_comm = comm;
        }
        rc = MPI_Comm_size(m_mpi_comm, &m_size);
        if (qvi_unlikely(rc != MPI_SUCCESS)) {
            throw qvi_runtime_error(QV_ERR_MPI);
        }
        rc = MPI_Comm_rank(m_mpi_comm, &m_rank);
        if (qvi_unlikely(rc != MPI_SUCCESS)) {
            throw qvi_runtime_error(QV_ERR_MPI);
        }
    }
    /** Destructor. */
    ~qvi_mpi_comm(void) = default;
    /** Frees the resources associated with the provided instance. */
    static void
    free(
        qvi_mpi_comm &comm
    ) {
        MPI_Comm mpi_comm = comm.m_mpi_comm;
        if (mpi_comm != MPI_COMM_NULL) {
            MPI_Comm_free(&mpi_comm);
            comm.m_mpi_comm = MPI_COMM_NULL;
        }
    }
};

struct qvi_mpi_group {
    /** The group's communicator info. */
    qvi_mpi_comm qvcomm = {};
    /** Constructor. */
    qvi_mpi_group(void) = default;
    /** Constructor. */
    qvi_mpi_group(
        const qvi_mpi_comm &comm
    ) : qvcomm(comm) { }
    /** Returns the size of the group. */
    int
    size(void) const
    {
        return qvcomm.m_size;
    }
    /** Returns the rank of the caller in the group. */
    int
    rank(void) const
    {
        return qvcomm.m_rank;
    }
    /** Returns the PIDs of all the group members. */
    std::vector<pid_t>
    pids(void) const;
    /** Performs a low-noise, high-latency barrier. */
    int
    barrier(void) const;
    /** Gathers bbuffs. */
    int
    gather_bbuffs(
        const qvi_bbuff &txbuff,
        int root,
        std::vector<qvi_bbuff> &rxbuffs
    ) const;
    /** Scatters bbuffs. */
    int
    scatter_bbuffs(
        const std::vector<qvi_bbuff> &txbuffs,
        int root,
        qvi_bbuff &rxbuff
    ) const;
    /** Duplicates the underlying group communicator and returns it. */
    int
    comm_dup(
        MPI_Comm *comm
    ) const;
};

using qvi_mpi_group_tab = std::unordered_map<
    qvi_group_id_t, qvi_mpi_group
>;

struct qvi_mpi {
private:
    /** Node representative communicator. Only valid for elected processes. */
    qvi_mpi_comm m_node_rep_comm;
    /** Duplicate of MPI_COMM_SELF. */
    qvi_mpi_comm m_self_comm;
    /** Node communicator. */
    qvi_mpi_comm m_node_comm;
    /** Duplicate of initializing communicator. */
    qvi_mpi_comm m_world_comm;
    /** Group table (ID to internal structure mapping). */
    qvi_mpi_group_tab m_group_tab;
    /** Creates intrinsic communicators. */
    int
    m_create_intrinsic_comms(
        MPI_Comm comm
    );
    /** */
    int
    m_create_intrinsic_groups(void);
    /** Creates internal, administrative communicators. */
    int
    m_create_admin_comms(void);
    /** */
    static int
    get_portno_from_env(
        const qvi_mpi_comm &comm,
        int &portno
    );
    /** */
    int
    m_start_daemons(void);
public:
    /** Constructor. */
    qvi_mpi(void) = delete;
    /** Constructor. */
    qvi_mpi(
        MPI_Comm comm
    );
    /** Destructor. */
    ~qvi_mpi(void);
    /** Adds a new group to the group table. */
    int
    add_group(
        const qvi_mpi_group &group,
        qvi_group_id_t given_id = QVI_MPI_GROUP_NULL
    );
    /** */
    int
    group_from_group_id(
        qvi_group_id_t id,
        qvi_mpi_group &group
    );
    /** */
    int
    group_from_mpi_comm(
        MPI_Comm comm,
        qvi_mpi_group &group
    );
    /** */
    int
    group_from_split(
        const qvi_mpi_group &parent,
        int color,
        int key,
        qvi_mpi_group &child
    );
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
