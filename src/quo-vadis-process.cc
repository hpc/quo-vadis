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
#include "qvi-group-process.h"
#include "qvi-utils.h"

static int
qvi_process_context_create(
    qv_context_t **ctx
) {
    int rc = QV_SUCCESS;
    // Create base context.
    qv_context_t *ictx = nullptr;
    rc = qvi_new_rc(&ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Create and initialize the base group.
    qvi_zgroup_process_s *izgroup;
    rc = qvi_new_rc(&izgroup);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    ictx->zgroup = izgroup;
    // Connect to RMI server.
    rc = qvi_context_connect_to_server(ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }

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
qv_process_context_create(
    qv_context_t **ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;
    try {
        return qvi_process_context_create(ctx);
    }
    qvi_catch_and_return();
}

static int
qvi_process_context_free(
    qv_context_t *ctx
) {
    qvi_delete(&ctx);
    return QV_SUCCESS;
}

int
qv_process_context_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;
    try {
        return qvi_process_context_free(ctx);
    }
    qvi_catch_and_return();
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
