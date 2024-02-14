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

#include "qvi-common.h" // IWYU pragma: keep

#include "quo-vadis-mpi.h"

#include "qvi-context.h"
#include "qvi-group-mpi.h"
#include "qvi-zgroup-mpi.h"
#include "qvi-scope.h"

/**
 * Simply a wrapper for our Fortran interface to C interface. No need to expose
 * in a header at this point, since they are only used by our Fortran module.
 */
#ifdef __cplusplus
extern "C" {
#endif

int
qvi_mpi_context_create_f2c(
    MPI_Fint comm,
    qv_context_t **ctx
) {
    MPI_Comm c_comm = MPI_Comm_f2c(comm);
    return qv_mpi_context_create(c_comm, ctx);
}

int
qvi_mpi_scope_comm_dup_f2c(
    qv_context_t *ctx,
    qv_scope_t *scope,
    MPI_Fint *comm
) {
    MPI_Comm c_comm = MPI_COMM_NULL;
    int rc = qv_mpi_scope_comm_dup(ctx, scope, &c_comm);
    *comm = MPI_Comm_c2f(c_comm);
    return rc;
}

#ifdef __cplusplus
}
#endif

int
qv_mpi_context_create(
    MPI_Comm comm,
    qv_context_t **ctx
) {
    if (!ctx || comm == MPI_COMM_NULL) {
        return QV_ERR_INVLD_ARG;
    }

    int rc = QV_SUCCESS;
    qv_context_t *ictx = nullptr;
    qvi_zgroup_mpi_t *izgroup = nullptr;
    // Create base context.
    rc = qvi_context_new(&ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Create and initialize the base group.
    rc = qvi_zgroup_mpi_new(&izgroup);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Save zgroup instance pointer to context.
    ictx->zgroup = izgroup;

    rc = izgroup->initialize(comm);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Connect to RMI server.
    rc = qvi_context_connect_to_server(ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    rc = qvi_bind_stack_init(
        ictx->bind_stack,
        qvi_mpi_task_get(izgroup->mpi),
        ictx->rmi
    );
out:
    if (rc != QV_SUCCESS) {
        (void)qv_mpi_context_free(ictx);
        ictx = nullptr;
    }
    *ctx = ictx;
    return rc;
}

int
qv_mpi_context_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;
    delete ctx->zgroup;
    qvi_context_free(&ctx);
    return QV_SUCCESS;
}

int
qv_mpi_scope_comm_dup(
    qv_context_t *ctx,
    qv_scope_t *scope,
    MPI_Comm *comm
) {
    if (!ctx || !scope || !comm) {
        return QV_ERR_INVLD_ARG;
    }

    qvi_group_mpi_t *mpi_group = dynamic_cast<qvi_group_mpi_t *>(
        qvi_scope_group_get(scope)
    );
    return mpi_group->comm_dup(comm);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
