/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-threads.c
 */

#include "common-test-utils.h"
#include "quo-vadis-thread.h"
#include <omp.h>

typedef struct {
    int nthreads;
    qv_scope_t **th_scopes;
} scopei;

static void
scopei_free(
    scopei *sinfo
) {
    // Wait for all threads to complete before freeing the scopes.
    #pragma omp barrier
    char *ers = NULL;
    const int rc = qv_thread_scopes_free(sinfo->nthreads, sinfo->th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scopes_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

/**
 * Creates the execution policy scopes.
 */
static void
scopei_ep(
    scopei *sinfo
) {
    char *ers = NULL;

    qv_scope_t *base_scope;
    int rc = qv_process_scope_get(
        QV_SCOPE_PROCESS, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scope_get() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Use the number of cores to determine how many thread scopes to create.
    rc = qv_scope_nobjs(base_scope, QV_HW_OBJ_CORE, &sinfo->nthreads);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int *thread_coloring = QV_THREAD_SCOPE_SPLIT_AFFINITY_PRESERVING;
    rc = qv_thread_scope_split_at(
        base_scope, QV_HW_OBJ_CORE, thread_coloring,
        sinfo->nthreads, &sinfo->th_scopes
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

static void
scopei_ep_push(
    scopei *sinfo,
    int rank
) {
    const int rc = qv_scope_bind_push(sinfo->th_scopes[rank]);
    if (rc != QV_SUCCESS) {
        char *ers = "qv_scope_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

static void
scopei_ep_pop(
    scopei *sinfo,
    int rank
) {
    const int rc = qv_scope_bind_pop(sinfo->th_scopes[rank]);
    if (rc != QV_SUCCESS) {
        char *ers = "qv_scope_bind_pop() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

static void
emit_iter_info(
    scopei *sinfo,
    int rank,
    int i
) {
    char const *ers = NULL;

    scopei_ep_push(sinfo, rank);

    char *binds;
    const int rc = qv_scope_bind_string(
        sinfo->th_scopes[rank], QV_BIND_STRING_LOGICAL, &binds
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf(
        "[%d]: thread=%d of nthread=%d handling iter %d on %s\n",
        ctu_gettid(), omp_get_thread_num(), omp_get_num_threads(), i, binds
    );
    free(binds);

    scopei_ep_pop(sinfo, rank);
}

int
main(void)
    {
    const double tick = omp_get_wtime();
    scopei ep_sinfo;
    scopei_ep(&ep_sinfo);
    const double tock = omp_get_wtime();

    omp_set_num_threads(ep_sinfo.nthreads);

    #pragma omp parallel
    #pragma omp master
    {
        printf("# Starting OpenMP Test (nthreads=%d)\n", omp_get_num_threads());
        printf("# Scope creation took %lf seconds\n", tock - tick);
    }

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < ep_sinfo.nthreads * 128; ++i) {
        emit_iter_info(&ep_sinfo, omp_get_thread_num(), i);
    }

    scopei_free(&ep_sinfo);
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
