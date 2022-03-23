/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
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

#include "qvi-group.h"
#include "qvi-mpi.h"

struct qvi_group_mpi_s : public qvi_group_s {
    /** Initialized qvi_mpi instance embedded in group instances. */
    qvi_mpi_t *mpi = nullptr;
    /** Underlying group instance. */
    qvi_mpi_group_t *mpi_group = nullptr;
    /** Base constructor that does minimal work. */
    qvi_group_mpi_s(void) = default;
    /** Virtual destructor. */
    virtual ~qvi_group_mpi_s(void);
    /** The real 'constructor' that can possibly fail. */
    virtual int create(void);
    /** Initializes the instance. */
    int initialize(qvi_mpi_t *mpi);
    /** The caller's group ID. */
    virtual int id(void);
    /** The number of members in this group. */
    virtual int size(void);
    /** Group barrier. */
    virtual int barrier(void);
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
    /**
     * Gathers bbuffs to specified root.
     */
    virtual int
    gather(
        qvi_bbuff_t *txbuff,
        int root,
        qvi_bbuff_t ***rxbuffs
    );
    /**
     * Scatters bbuffs from specified root.
     */
    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    );
    /**
     * Returns a duplicate of the underlying MPI group communicator.
     */
    int
    comm_dup(
        MPI_Comm *comm
    );
};
typedef qvi_group_mpi_s qvi_group_mpi_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
