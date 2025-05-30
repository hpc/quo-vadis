/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-threads.c
 */

#include "quo-vadis-mpi.h"
#include "quo-vadis-thread.h"
#include "common-test-utils.h"
#include "omp.h"

void *
thread_work(
    void *arg
) {
    // Do work.
    printf("Hello from pid=%d,tid=%d\n", getpid(), ctu_gettid());
    ctu_emit_task_bind(arg);
    return NULL;
}

int
main(
    int argc, char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    int wsize;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    int wrank;
    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }
    ////////////////////////////////////////////////////////////////////////////
    // Use the process interface for NUMA.
    ////////////////////////////////////////////////////////////////////////////
    // Get the base scope: RM-given resources.
    qv_scope_t *base_scope;
    rc = qv_mpi_scope_get(comm, QV_SCOPE_USER, &base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope_get() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int nnumas;
    rc = qv_scope_nobjs(base_scope, QV_HW_OBJ_NUMANODE, &nnumas);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Split at NUMA domains.
    qv_scope_t *numa_scope;
    rc = qv_scope_split_at(
        base_scope, QV_HW_OBJ_NUMANODE,
        wrank % nnumas, &numa_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // When there's more tasks than NUMAs,
    // make sure each task has exclusive resources.
    int lrank;
    rc = qv_scope_group_rank(numa_scope, &lrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ntasks_per_numa;
    rc = qv_scope_group_size(numa_scope, &ntasks_per_numa);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *subnuma;
    rc = qv_scope_split(
        numa_scope, ntasks_per_numa,
        lrank % ntasks_per_numa, &subnuma
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get the number of cores and pus per NUMA part.
    int ncores;
    rc = qv_scope_nobjs(subnuma, QV_HW_OBJ_CORE, &ncores);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int npus;
    rc = qv_scope_nobjs(subnuma, QV_HW_OBJ_PU, &npus);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    ////////////////////////////////////////////////////////////////////////////
    // OpenMP: Launch one thread per core.
    ////////////////////////////////////////////////////////////////////////////
    const int nthreads = ncores;
    int *thread_coloring = NULL; // Default thread assignment.
    qv_scope_t **th_scopes;
    rc = qv_thread_scope_split_at(
        subnuma, QV_HW_OBJ_CORE, thread_coloring, nthreads, &th_scopes
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    omp_set_num_threads(nthreads);
    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        // Each thread works with its new hardware affinity.
        qv_scope_bind_push(th_scopes[tid]);
        thread_work(th_scopes[tid]);
    }
    // When we are done with the scope, clean up.
    rc = qv_thread_scopes_free(nthreads, th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scopes_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    ////////////////////////////////////////////////////////////////////////////
    // POSIX threads:
    // * Launch one thread per hardware thread
    // *   Policy-based placement
    // *   Note num_threads < num_places on SMT
    ////////////////////////////////////////////////////////////////////////////
    thread_coloring = QV_THREAD_SCOPE_SPLIT_PACKED,
    rc = qv_thread_scope_split_at(
        subnuma, QV_HW_OBJ_PU, thread_coloring, nthreads, &th_scopes
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    pthread_t *pthrds = calloc(nthreads, sizeof(pthread_t));
    if (!pthrds) {
        ers = "calloc() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    for (int i = 0; i < nthreads; i++) {
        qv_pthread_create(
            &pthrds[i], NULL, thread_work,
            th_scopes[i], th_scopes[i]
        );
    }

    for (int i = 0; i < nthreads; i++) {
        void *ret = NULL;
        if (pthread_join(pthrds[i], &ret) != 0) {
            printf("Thread exited with %p\n", ret);
        }
    }
    free(pthrds);
    // When we are done with the scope, clean up.
    rc = qv_thread_scopes_free(nthreads, th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scopes_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Clean up.
    qv_scope_free(subnuma);
    qv_scope_free(numa_scope);
    qv_scope_free(base_scope);

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
