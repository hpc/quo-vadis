/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2023 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-process-scopes.c
 */

#include "quo-vadis.h"
#include "common-test-utils.h"

int
main(void)
{
    char const *ers = NULL;
    char const *scope_label = "Base Scope";

    qv_scope_t *base_scope;
    int rc = qv_process_scope_get(
        QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_USER) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_host_hw_info(base_scope, scope_label);

    ctu_emit_gpu_info(base_scope, scope_label);

    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
