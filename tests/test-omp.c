/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-threads.c
 */

#include "qvi-test-common.h"
#include "quo-vadis-omp.h"
#include <omp.h>

typedef struct {
    qv_scope_t *scope;
    int size;
    int id;
} scopei;

static void
emit_iter_info(
    scopei *sinfo,
    int i
) {
    char const *ers = NULL;
    char *binds;
    const int rc = qv_scope_bind_string(
        sinfo->scope, QV_BIND_STRING_AS_LIST, &binds
    );
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

static void
scopei_fill(
    scopei *sinfo
) {
    char *ers = NULL;

    int rc = qv_scope_ntasks(sinfo->scope, &sinfo->size);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_ntasks() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_taskid(sinfo->scope, &sinfo->id);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

static void
scopei_free(
    scopei *sinfo
) {
    char *ers = NULL;
    const int rc = qv_scope_free(sinfo->scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

static void
scopei_base(
    scopei *sinfo
) {
    char *ers = NULL;
    const int rc = qv_omp_scope_get(
        QV_SCOPE_PROCESS, &sinfo->scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scope_get() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    scopei_fill(sinfo);
}

/**
 * Creates the execution policy scope.
 */
static void
scopei_ep(
    scopei *sinfo
) {
    // Get the base (parent) scope.
    scopei pinfo;
    scopei_base(&pinfo);

    char *ers = NULL;
    int rc = qv_scope_split(
        pinfo.scope, 2, pinfo.id, &sinfo->scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    scopei_fill(sinfo);
    // We don't need this anymore.
    scopei_free(&pinfo);
}

static void
scopei_ep_push(
    scopei *sinfo
) {
    char *ers = NULL;
    const int rc = qv_scope_bind_push(sinfo->scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_bind_push() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

static void
scopei_ep_pop(
    scopei *sinfo
) {
    // Make sure everyone is done with the execution policy before we pop.
    #pragma omp barrier
    char *ers = NULL;
    const int rc = qv_scope_bind_pop(sinfo->scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_bind_pop() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
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
        scopei ep_sinfo;
        scopei_ep(&ep_sinfo);

        scopei_ep_push(&ep_sinfo);
        #pragma omp task
        #pragma omp parallel for num_threads(ep_sinfo.size)
        for (int i = 0; i < 8; ++i) {
            emit_iter_info(&ep_sinfo, i);
        }
        scopei_ep_pop(&ep_sinfo);

        scopei_free(&ep_sinfo);
    }
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
