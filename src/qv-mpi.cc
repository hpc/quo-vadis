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
 * @file qv-mpi.cc
 */

#include "private/qvi-common.h"
#include "private/qvi-context.h"
#include "private/qvi-task.h"
#include "private/qvi-rmi.h"
#include "private/qvi-mpi.h"

#include "mpi.h"

// TODO(skg) Have infrastructure-specific init (e.g., MPI), but have
// infrastructure-agnostic free(), etc (where possible).

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// NOTE: Public API definition, so carefully check input arguments. ////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// TODO(skg) Consider implementing a connection manager that will deal with all
// the low-level details regarding connection setup, discovery, etc.
static int
connect_to_server(
    qv_context_t *ctx
) {
    int rc = qvi_rmi_client_connect(
        ctx->rmi,
        "tcp://127.0.0.1:55995"
    );
    return rc;
}

int
qv_mpi_create(
    qv_context_t **ctx,
    MPI_Comm comm
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qv_context_t *ictx = qvi_new qv_context_t;
    if (!ictx) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_mpi_construct(&ictx->mpi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_construct() failed";
        goto out;
    }

    rc = qvi_task_construct(&ictx->task);
    if (rc != QV_SUCCESS) {
        ers = "qvi_task_construct() failed";
        goto out;
    }

    rc = qvi_rmi_client_construct(&ictx->rmi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_client_construct() failed";
        goto out;
    }

    rc = qvi_mpi_init(ictx->mpi, ictx->task, comm);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_init() failed";
        goto out;
    }

    rc = connect_to_server(ictx);
    if (rc != QV_SUCCESS) {
        ers = "connect_to_server() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        (void)qv_free(ictx);
        *ctx = nullptr;
        return rc;
    }
    *ctx = ictx;
    return rc;
}

// TODO(skg) Move.
int
qv_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    qvi_task_destruct(&ctx->task);
    qvi_mpi_destruct(&ctx->mpi);
    qvi_rmi_client_destruct(&ctx->rmi);
    delete ctx;

    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
