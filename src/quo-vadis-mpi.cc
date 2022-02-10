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

// TODO(skg) This should probably be in a common area because other
// infrastructure will likely use something similar.
static int
connect_to_server(
    qv_context_t *ctx
) {
    char *url = nullptr;
    int rc = qvi_url(&url);
    if (rc != QV_SUCCESS) {
        qvi_log_error("{}", qvi_conn_ers());
        return rc;
    }

    rc = qvi_rmi_client_connect(ctx->rmi, url);
    if (rc != QV_SUCCESS) {
        goto out;
    }
out:
    if (url) free(url);
    return rc;
}

int
qv_mpi_context_create(
    qv_context_t **ctx,
    MPI_Comm comm
) {
    if (!ctx || comm == MPI_COMM_NULL) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    cstr ers = nullptr;
    // Create base context.
    qv_context_t *ictx = nullptr;
    rc = qvi_context_create(&ictx);
    if (rc != QV_SUCCESS) {
        ers = "qvi_context_create() failed";
        goto out;
    }
    // Create and initialize context group.
    qvi_zgroup_mpi_t *izgroup;
    rc = qvi_zgroup_mpi_new(&izgroup);
    if (rc != QV_SUCCESS) {
        ers = "qvi_zgroup_mpi_new() failed";
        goto out;
    }
    // Save zgroup instance pointer to context.
    ictx->zgroup = izgroup;

    rc = izgroup->initialize(comm);
    if (rc != QV_SUCCESS) {
        ers = "group->initialize() failed";
        goto out;
    }
    // Connect to RMI server.
    rc = connect_to_server(ictx);
    if (rc != QV_SUCCESS) {
        ers = "connect_to_server() failed";
        goto out;
    }

    rc = qvi_bind_stack_init(
        ictx->bind_stack,
        qvi_mpi_task_get(izgroup->mpi),
        ictx->rmi
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_bind_stack_init() failed";
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
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
    if (ctx->zgroup) delete ctx->zgroup;
    qvi_context_free(&ctx);
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
