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
    char *ers = NULL;
    int rc = QV_SUCCESS;

#pragma omp parallel
#pragma omp single
    printf("# Starting OpenMP Test (nthreads=%d)\n", omp_get_num_threads());

#pragma omp parallel private(ers, rc)
{
    qv_scope_t *base_scope = NULL;
    rc = qv_thread_scope_get(
        QV_SCOPE_PROCESS,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scope_get() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_test_scope_report(base_scope, "base_scope");

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
