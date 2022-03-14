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
 * @file quo-vadis-mpi.cc
 */


#include "qvi-common.h"

#include "qvi-context.h"
#include "qvi-zgroup-mpi.h"
#include "qvi-utils.h"

/**
 * Simply a wrapper for our Fortran interface to C interface.
 * in a header at this point, since it is only used by our Fortran module.
 */
#ifdef __cplusplus
extern "C" {
#endif
int
qv_mpi_context_create_f2c(
    qv_context_t **ctx,
    MPI_Fint comm
) {
    MPI_Comm c_comm = MPI_Comm_f2c(comm);
    return qv_mpi_context_create(ctx, c_comm);
}
#ifdef __cplusplus
}
#endif

int
qv_mpi_context_create(
    qv_context_t **ctx,
    MPI_Comm comm
) {
    if (!ctx || comm == MPI_COMM_NULL) {
        return QV_ERR_INVLD_ARG;
    }

    int rc = QV_SUCCESS;
    qv_context_t *ictx = nullptr;
    qvi_zgroup_mpi_t *izgroup = nullptr;
    // Create base context.
    rc = qvi_context_create(&ictx);
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

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
