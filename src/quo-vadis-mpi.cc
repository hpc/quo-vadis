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
 * @file quo-vadis-mpi.cc
 */

#include "quo-vadis-mpi.h"
#include "qvi-group-mpi.h"
#include "qvi-scope.h"
#include "qvi-utils.h"

/**
 * Simply a wrapper for our Fortran interface to C interface. No need to expose
 * in a header at this point, since they are only used by our Fortran module.
 */
#ifdef __cplusplus
extern "C" {
#endif

int
qvi_mpi_scope_get_f2c(
    MPI_Fint comm,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    MPI_Comm c_comm = MPI_Comm_f2c(comm);
    return qv_mpi_scope_get(c_comm, iscope, scope);
}

int
qvi_mpi_scope_comm_dup_f2c(
    qv_scope_t *scope,
    MPI_Fint *comm
) {
    MPI_Comm c_comm = MPI_COMM_NULL;
    const int rc = qv_mpi_scope_comm_dup(scope, &c_comm);
    *comm = MPI_Comm_c2f(c_comm);
    return rc;
}

#ifdef __cplusplus
}
#endif

int
qvi_mpi_scope_get(
    MPI_Comm comm,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    *scope = nullptr;
    // Create and initialize the base group.
    qvi_zgroup_mpi_s *izgroup = nullptr;
    const int rc = qvi_new(&izgroup, comm);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return qv_scope_s::make_intrinsic(izgroup, iscope, scope);
}

int
qv_mpi_scope_get(
    MPI_Comm comm,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    if (qvi_unlikely(comm == MPI_COMM_NULL || !scope)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_mpi_scope_get(comm, iscope, scope);
    }
    qvi_catch_and_return();
}

static int
qvi_mpi_scope_comm_dup(
    qv_scope_t *scope,
    MPI_Comm *comm
) {
    return dynamic_cast<qvi_group_mpi_t *>(scope->group())->comm_dup(comm);
}

int
qv_mpi_scope_comm_dup(
    qv_scope_t *scope,
    MPI_Comm *comm
) {
    if (qvi_unlikely(!scope || !comm)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_mpi_scope_comm_dup(scope, comm);
    }
    qvi_catch_and_return();
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
