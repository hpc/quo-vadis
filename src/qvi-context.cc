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
 * @file qvi-context.cc
 */

#include "qvi-common.h"
#include "qvi-context.h"

int
qv_create(
    qv_context_t **ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    qv_context_t *ictx = qvi_new qv_context_t;
    if (!ictx) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_task_new(&ictx->task);
    if (rc != QV_SUCCESS) {
        ers = "qvi_task_new() failed";
        goto out;
    }

    rc = qvi_hwloc_new(&ictx->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_new() failed";
        goto out;
    }

    rc = qvi_rmi_client_new(&ictx->rmi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_client_new() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        (void)qv_free(ictx);
        ictx = nullptr;
    }
    *ctx = ictx;
    return rc;
}

int
qv_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    qvi_task_free(&ctx->task);
    qvi_rmi_client_free(&ctx->rmi);
    delete ctx;

    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
