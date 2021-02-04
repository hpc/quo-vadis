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
#include "private/qvi-log.h"
#include "private/qvi-task.h"
#include "private/qvi-mpi.h"

#include "quo-vadis/qv-mpi.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// NOTE: Public API definition, so carefully check input arguments. ////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Type definition
struct qv_context_s {
    qv_task_t *task = nullptr;
    qvi_mpi_t *mpi = nullptr;
};

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

    rc = qvi_mpi_init(ictx->mpi, ictx->task, comm);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_init() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        *ctx = nullptr;
        return rc;
    }
    *ctx = ictx;
    return rc;
}

int
qv_mpi_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    qvi_task_destruct(&ctx->task);
    qvi_mpi_destruct(&ctx->mpi);
    delete ctx;

    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
