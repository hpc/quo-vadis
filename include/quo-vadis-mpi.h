/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis-mpi.h
 */

#ifndef QUO_VADIS_MPI_H
#define QUO_VADIS_MPI_H

#include "quo-vadis.h"

#include "mpi.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Convenience definition. */
#define QUO_VADIS_MPI 1

/**
 *
 */
int
qv_mpi_context_create(
    qv_context_t **ctx,
    MPI_Comm comm
);

/**
 *
 */
int
qv_mpi_context_free(
    qv_context_t *ctx
);

/**
 * Returns a duplicate of the underlying MPI communicator associated with the
 * provided scope. The provided communicator must be freed by MPI_Comm_free().
 */
int
qv_mpi_scope_comm_dup(
    qv_context_t *ctx,
    qv_scope_t *scope,
    MPI_Comm *comm
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
