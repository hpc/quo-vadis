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
    int rc = qvi_new_rc(ctx);
    return rc;
    //return qvi_new_rc(ctx);
}

void
qvi_context_free(
    qv_context_t **ctx
) {
    qvi_delete(ctx);
}

int
qvi_context_connect_to_server(
    qv_context_t *ctx
) {
    std::string url;
    const int rc = qvi_url(url);
    if (rc != QV_SUCCESS) {
        qvi_log_error("{}", qvi_conn_ers());
        return rc;
    }
    return qvi_rmi_client_connect(ctx->rmi, url);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
