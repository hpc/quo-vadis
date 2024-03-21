/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis-process.cc
 */

#include "quo-vadis-process.h"
#include "qvi-context.h"
#include "qvi-zgroup-process.h"

int
qv_process_context_create(
    qv_context_t **ctx
) {
    if (!ctx) {
        return QV_ERR_INVLD_ARG;
    }

    int rc = QV_SUCCESS;
    qv_context_t *ictx = nullptr;
    qvi_zgroup_process_t *izgroup = nullptr;
    // Create base context.
    rc = qvi_context_new(&ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Create and initialize the base group.
    rc = qvi_zgroup_process_new(&izgroup);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Save zgroup instance pointer to context.
    ictx->zgroup = izgroup;

    rc = izgroup->initialize();
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
        qvi_process_task_get(izgroup->zproc),
        ictx->rmi
    );
out:
    if (rc != QV_SUCCESS) {
        (void)qv_process_context_free(ictx);
        ictx = nullptr;
    }
    *ctx = ictx;
    return rc;
}

int
qv_process_context_free(
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
