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

#include "quo-vadis-process.h"
#include "qvi-test-common.h"

int
main(void)
{
    char const *ers = NULL;
    int rc = QV_SUCCESS;

    const int pid = getpid();

    qv_context_t *ctx = NULL;
    rc = qv_process_context_create(&ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_process_context_create() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *self_scope = NULL;
    rc = qv_scope_get(
        ctx, QV_SCOPE_PROCESS, &self_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_PROCESS) failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_test_scope_report(ctx, self_scope, "self_scope");

    rc = qv_scope_free(ctx, self_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *base_scope;
    rc = qv_scope_get(
        ctx, QV_SCOPE_USER, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_USER) failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_test_scope_report(ctx, base_scope, "base_scope");

    int taskid;
    rc = qv_scope_taskid(ctx, base_scope, &taskid);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    if (taskid != 0) {
        ers = "Invalid task ID detected";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ntasks;
    rc = qv_scope_ntasks(ctx, base_scope, &ntasks);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_ntasks() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    if (ntasks != 1) {
        ers = "Invalid number of tasks detected";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int n_numa;
    rc = qv_scope_nobjs(
        ctx, base_scope, QV_HW_OBJ_NUMANODE, &n_numa
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of NUMA in base_scope is %d\n", pid, n_numa);

    const int npieces = 2;
    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        ctx, base_scope, npieces, taskid, &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_nobjs(
        ctx, sub_scope, QV_HW_OBJ_NUMANODE, &n_numa
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of NUMA in sub_scope is %d\n", pid, n_numa);

    qvi_test_scope_report(ctx, sub_scope, "sub_scope");

    qvi_test_change_bind(ctx, sub_scope);

    qv_scope_t *sub_sub_scope;
    rc = qv_scope_split(
        ctx, sub_scope, npieces, taskid, &sub_sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_nobjs(
        ctx, sub_sub_scope, QV_HW_OBJ_NUMANODE, &n_numa
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of NUMA in sub_sub_scope is %d\n", pid, n_numa);

    rc = qv_scope_free(ctx, base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(ctx, sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(ctx, sub_sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_context_barrier(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_context_barrier() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_process_context_free(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_process_context_free() failed";
        qvi_test_panic("%s", ers);
    }

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
