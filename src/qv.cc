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
 * @file qv.cc
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

    rc = qvi_rmi_client_new(&ictx->rmi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_client_new() failed";
        goto out;
    }

    rc = qvi_bind_stack_new(&ictx->bind_stack);
    if (rc != QV_SUCCESS) {
        ers = "qvi_bind_stack_new() failed";
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
    qvi_bind_stack_free(&ctx->bind_stack);
    qvi_rmi_client_free(&ctx->rmi);
    delete ctx;

    return QV_SUCCESS;
}

int
qv_bind_push(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) return QV_ERR_INVLD_ARG;

    return qvi_bind_push(
        ctx->bind_stack,
        qvi_scope_bitmap_get(scope)
    );
}

int
qv_bind_pop(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    return qvi_bind_pop(ctx->bind_stack);
}

int
qv_bind_get_as_string(
    qv_context_t *ctx,
    char **bitmaps
) {
    if (!ctx || !bitmaps) return QV_ERR_INVLD_ARG;

    return qvi_hwloc_task_get_cpubind_as_string(
        ctx->hwloc,
        qvi_task_pid(ctx->task),
        bitmaps
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
