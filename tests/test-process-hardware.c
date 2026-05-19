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
    int rc = QV_SUCCESS;

    const int pid = getpid();

    qv_scope_t *base_scope;
    rc = qv_process_scope_get(
        QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_USER) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int n_pus;
    rc = qv_scope_hw_obj_count(
        base_scope, QV_HW_OBJ_PU, &n_pus
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of PUs in base_scope is %d\n", pid, n_pus);


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
