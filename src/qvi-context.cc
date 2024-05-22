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
 * @file qvi-context.cc
 */

#include "qvi-context.h"
#include "qvi-utils.h"

int
qvi_context_new(
    qv_context_t **ctx
) {
    qv_context_t *ictx = new qv_context_t();

    int rc = qvi_rmi_client_new(&ictx->rmi);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_bind_stack_new(&ictx->bind_stack);
out:
    if (rc != QV_SUCCESS) {
        qvi_context_free(&ictx);
    }
    *ctx = ictx;
    return rc;
}

void
qvi_context_free(
    qv_context_t **ctx
) {
    if (!ctx) return;
    qv_context_t *ictx = *ctx;
    if (!ictx) goto out;
    qvi_bind_stack_free(&ictx->bind_stack);
    qvi_rmi_client_free(&ictx->rmi);
    delete ictx;
out:
    *ctx = nullptr;
}

int
qvi_context_connect_to_server(
    qv_context_t *ctx
) {
    std::string url;
    int rc = qvi_url(url);
    if (rc != QV_SUCCESS) {
        qvi_log_error("{}", qvi_conn_ers());
        return rc;
    }

    rc = qvi_rmi_client_connect(ctx->rmi, url.c_str());
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
