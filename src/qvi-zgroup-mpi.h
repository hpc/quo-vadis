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
 * @file qvi-zgroup-mpi.h
 *
 * MPI context group used for bootstrapping operations.
 */

#ifndef QVI_ZGROUP_MPI_H
#define QVI_ZGROUP_MPI_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-zgroup.h"
#include "qvi-mpi.h"

struct qvi_zgroup_mpi_s : public qvi_zgroup_s {
    /** Internal qvi_mpi instance maintained by this zgroup. */
    qvi_mpi_t *mpi = nullptr;
    /** Constructor. */
    qvi_zgroup_mpi_s(void)
    {
        const int rc = qvi_mpi_new(&mpi);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
    }
    /** Virtual destructor. */
    virtual ~qvi_zgroup_mpi_s(void)
    {
        qvi_mpi_free(&mpi);
    }
    /** The real 'constructor' that can possibly fail. */
    virtual int create(void)
    {
        return QV_SUCCESS;
    }
    /** Initializes the MPI group. */
    int initialize(MPI_Comm comm)
    {
        return qvi_mpi_init(mpi, comm);
    }
    /** Returns a pointer to the caller's task information. */
    virtual qvi_task_t *task(void)
    {
        return qvi_mpi_task_get(mpi);
    }
    /** Creates an intrinsic group from an intrinsic identifier. */
    virtual int group_create_intrinsic(
        qv_scope_intrinsic_t intrinsic,
        qvi_group_t **group
    );
    /** Node-local task barrier. */
    virtual int barrier(void)
    {
        return qvi_mpi_node_barrier(mpi);
    }
};
typedef qvi_zgroup_mpi_s qvi_zgroup_mpi_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
int
qvi_zgroup_mpi_new(
    qvi_zgroup_mpi_t **zgroup
);

/**
 *
 */
void
qvi_zgroup_mpi_free(
    qvi_zgroup_mpi_t **zgroup
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
