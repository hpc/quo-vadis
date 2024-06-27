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
#include "qvi-context.h"
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
    const int rc = qv_mpi_scope_comm_dup(ctx, scope, &c_comm);
    *comm = MPI_Comm_c2f(c_comm);
    return rc;
}

#ifdef __cplusplus
}
#endif

static int
qvi_mpi_context_create(
    MPI_Comm comm,
    qv_context_t **ctx
) {
    // Create base context.
    qv_context_t *ictx = nullptr;
    int rc = qvi_new_rc(&ictx);
    if (rc != QV_SUCCESS) goto out;
    // Create and initialize the base group.
    qvi_zgroup_mpi_s *izgroup;
    rc = qvi_new_rc(&izgroup);
    if (rc != QV_SUCCESS) goto out;

    rc = izgroup->initialize(comm);
    if (rc != QV_SUCCESS) goto out;

    ictx->zgroup = izgroup;
    // Connect to RMI server.
    rc = qvi_context_connect_to_server(ictx);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_bind_stack_init(
        ictx->bind_stack,
        ictx->zgroup->task(),
        ictx->rmi
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&ictx);
    }
    *ctx = ictx;
    return rc;
}

int
qv_mpi_context_create(
    MPI_Comm comm,
    qv_context_t **ctx
) {
    if (!ctx || comm == MPI_COMM_NULL) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_mpi_context_create(comm, ctx);
    }
    qvi_catch_and_return();
}

int
qv_mpi_context_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;
    try {
        qvi_delete(&ctx);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

static int
qvi_mpi_scope_comm_dup(
    qv_context_t *,
    qv_scope_t *scope,
    MPI_Comm *comm
) {
    qvi_group_mpi_t *mpi_group = dynamic_cast<qvi_group_mpi_t *>(
        qvi_scope_group_get(scope)
    );
    return mpi_group->comm_dup(comm);
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
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_mpi_scope_comm_dup(ctx, scope, comm);
    }
    qvi_catch_and_return();
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
