/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-threads.c
 */

#include "qvi-test-common.h"
#include "quo-vadis-thread.h"
#include <omp.h>

static void
emit_iter_info(
    qv_scope_t *scope,
    int i
) {
    char const *ers = NULL;
    char *binds;
    const int rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf(
        "[%d]: thread=%d of nthread=%d of thread=%d handling iter %d on %s\n",
        qvi_test_gettid(), omp_get_thread_num(), omp_get_team_size(2),
        omp_get_ancestor_thread_num(1), i, binds
    );
    free(binds);
}

int
main(void)
{
    // Make sure nested parallism is on.
    omp_set_nested(1);
#pragma omp parallel
#pragma omp master
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

    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        base_scope, 2, taskid, &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int nsubtasks = 0;
    rc = qv_scope_ntasks(base_scope, &nsubtasks);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_ntasks() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_bind_push(sub_scope);
    #pragma omp task
    #pragma omp parallel for num_threads(nsubtasks)
    for (int i = 0; i < 8; ++i) {
        emit_iter_info(sub_scope, i);
    }
    qv_scope_bind_pop(sub_scope);

    qv_scope_bind_push(sub_scope);
    #pragma omp task
    #pragma omp parallel num_threads(1)
    {
        //qvi_test_emit_task_bind(sub_scope);
    }
    #pragma omp barrier
    qv_scope_bind_pop(sub_scope);

    rc = qv_scope_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

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
