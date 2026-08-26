/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
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
 * Creates a context containing the MPI processes contained within the provided
 * communicator.
 *
 * @param[in] comm The MPI communicator whose processes comprise the resulting
 * scope's group. All processes in the communicator must call this function
 * because it is collective over comm.
 *
 * @param[in] iscope The intrinsic scope type used to determine the hardware
 * resources included in the resulting scope.
 *
 * @param[in] flags Flags that influence how the scope is created.
 *
 * @param[out] scope Address of a pointer that will receive the newly created
 * scope. The caller is responsible for freeing this scope with qv_free().
 *
 * @retval QV_SUCCESS if the operation completed successfully.
 */
int
qv_mpi_scope(
    MPI_Comm comm,
    qv_scope_intrinsic_t iscope,
    qv_scope_flags_t flags,
    qv_scope_t **scope
);

/**
 * Returns a duplicate of the underlying MPI communicator associated with the
 * provided scope. The returned communicator must be freed by MPI_Comm_free().
 *
 * @param[in] scope The scope whose underlying MPI communicator is duplicated.
 *
 * @param[out] comm Address of an MPI_Comm that will receive the duplicated
 * communicator. The caller is responsible for freeing this communicator with
 * MPI_Comm_free().
 *
 * @retval QV_SUCCESS if the operation completed successfully.
 */
int
qv_mpi_comm_dup(
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
