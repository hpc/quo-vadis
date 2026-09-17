/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-omp.c
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
    ctu_check(
        qv_thread_free(sinfo->th_scopes, sinfo->nthreads),
        "qv_thread_free"
    );
}

/**
 * Creates the execution policy scopes.
 */
static void
scopei_ep(
    scopei *sinfo
) {
    qv_scope_t *base_scope;
    ctu_check(
        qv_process_scope(QV_SCOPE_PROCESS, QV_SCOPE_FLAG_NONE, &base_scope),
        "qv_process_scope"
    );
    // Use the number of cores to determine how many thread scopes to create.
    ctu_check(
        qv_hw_count(base_scope, QV_HW_CORE, &sinfo->nthreads),
        "qv_hw_count"
    );

    int *thread_coloring = QV_THREAD_SPLIT_CLOSE;
    ctu_check(
        qv_thread_split_at(
            base_scope, QV_HW_CORE, thread_coloring,
            sinfo->nthreads, &sinfo->th_scopes
        ),
        "qv_thread_split_at"
    );

    ctu_check(qv_free(base_scope), "qv_free");
}

static void
scopei_ep_push(
    scopei *sinfo,
    int rank
) {
    ctu_check(qv_bind_push(sinfo->th_scopes[rank]), "qv_bind_push");
}

static void
scopei_ep_pop(
    scopei *sinfo,
    int rank
) {
    ctu_check(qv_bind_pop(sinfo->th_scopes[rank]), "qv_bind_pop");
}

static void
emit_iter_info(
    scopei *sinfo,
    int rank,
    int i
) {
    char *binds;
    ctu_check(
        qv_bind_string(sinfo->th_scopes[rank], QV_BIND_STRING_LOGICAL, &binds),
        "qv_bind_string"
    );
    printf(
        "[%d]: thread=%03d of nthread=%03d handling iter %03d on %s\n",
        ctu_gettid(), omp_get_thread_num(), omp_get_num_threads(), i, binds
    );
    free(binds);
}

int
main(void)
    {
    const double tick = omp_get_wtime();
    scopei ep_sinfo;
    scopei_ep(&ep_sinfo);
    const double tock = omp_get_wtime();
    const int niters = ep_sinfo.nthreads * 4;

    omp_set_num_threads(ep_sinfo.nthreads);

    #pragma omp parallel
    #pragma omp master
    {
        printf("# Starting OpenMP Test (nthreads=%d)\n", omp_get_num_threads());
        printf("# Scope creation took %lf seconds\n", tock - tick);
    }

    // First, set the thread affinities based on the computed execution policy.
    #pragma omp parallel
    scopei_ep_push(&ep_sinfo, omp_get_thread_num());

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < niters; ++i) {
        emit_iter_info(&ep_sinfo, omp_get_thread_num(), i);
    }

    // Done with our calculation, so undo the threads' execution policy.
    #pragma omp parallel
    scopei_ep_pop(&ep_sinfo, omp_get_thread_num());

    #pragma omp parallel
    #pragma omp master
    printf("\n# Now running without a QV execution policy\n\n");

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < niters; ++i) {
        emit_iter_info(&ep_sinfo, omp_get_thread_num(), i);
    }

    scopei_free(&ep_sinfo);
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
