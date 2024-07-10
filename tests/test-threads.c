/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-threads.c
 */

#include "qvi-test-common.h"
#include "quo-vadis-thread.h"
#include <omp.h>

int
main(void)
{
#pragma omp parallel
#pragma omp single
    printf("# Starting OpenMP Test (nthreads=%d)\n", omp_get_num_threads());

#pragma omp parallel
{
    char *ers = NULL;
    int rc = QV_SUCCESS;
    qv_scope_t *base_scope = NULL;
    rc = qv_thread_scope_get(
        QV_SCOPE_PROCESS,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scope_get() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int taskid = 0;
    rc = qv_scope_taskid(base_scope, &taskid);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_test_scope_report(base_scope, "base_scope");
#if 1
    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        base_scope, 2, taskid, &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_test_scope_report(sub_scope, "sub_scope");

    rc = qv_scope_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#endif
    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
