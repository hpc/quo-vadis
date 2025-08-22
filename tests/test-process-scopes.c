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

    qv_scope_t *self_scope = NULL;
    rc = qv_process_scope_get(
        QV_SCOPE_PROCESS, QV_SCOPE_FLAG_NONE, &self_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_PROCESS) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_scope_report(self_scope, "self_scope");

    rc = qv_scope_free(self_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *base_scope;
    rc = qv_process_scope_get(
        QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_USER) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_scope_report(base_scope, "base_scope");

    int srank;
    rc = qv_scope_group_rank(base_scope, &srank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    if (srank != 0) {
        ers = "Invalid task ID detected";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int sgsize;
    rc = qv_scope_group_size(base_scope, &sgsize);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    if (sgsize != 1) {
        ers = "Invalid number of tasks detected";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int n_cores;
    rc = qv_scope_hw_obj_count(
        base_scope, QV_HW_OBJ_CORE, &n_cores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of cores in base_scope is %d\n", pid, n_cores);

    const int npieces = 2;
    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        base_scope, npieces, srank, &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_hw_obj_count(
        sub_scope, QV_HW_OBJ_CORE, &n_cores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of cores in sub_scope is %d\n", pid, n_cores);

    ctu_scope_report(sub_scope, "sub_scope");

    ctu_change_bind(sub_scope);

    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
