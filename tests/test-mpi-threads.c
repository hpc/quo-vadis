/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-threads.c
 */

// TODO(skg)
// * qv_thread_scope_split_at() allow NULL for default colors.

#include "quo-vadis-mpi.h"
// TODO(skg) rename
#include "quo-vadis-pthread.h"

#include "common-test-utils.h"

#include "omp.h"

void *
thread_work(
    void *arg
) {
    // Do work.
    printf("hi from %d(%d)\n", getpid(), ctu_gettid());
    return arg;
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
    int tid;
    qv_scope_t *th_scope;
    omp_set_num_threads(nthreads);
    #pragma omp parallel private(tid, th_scope)
    {
        rc = qv_scope_split_at(
            subnuma, QV_HW_OBJ_CORE, QV_SCOPE_SPLIT_PACKED, &th_scope
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_split_at() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
        //qv_scope_bind_push(th_scopes);
        tid = omp_get_thread_num();
        // Each thread works.
        thread_work(&tid);
        /* When we are done, clean up */
        qv_scope_free(th_scope);
    }
    /************************************************
     * POSIX threads:
     *   Launch one thread per hardware thread
     *   Policy-based placement
     *   Note num_threads < num_places on SMT
     ************************************************/

    void *ret;
    qv_scope_t **th_scopes;
    int *th_color = QV_PTHREAD_SCOPE_SPLIT_PACKED;
    // TODO(skg)
    qv_pthread_scope_split_at(
        subnuma, QV_HW_OBJ_PU, th_color, nthreads, &th_scopes
    );

    pthread_t *thid = malloc(nthreads * sizeof(pthread_t));
    pthread_attr_t *attr = NULL;

    for (int i=0; i<nthreads; i++)
        qv_pthread_create(&thid[i], attr, thread_work, &i, th_scopes[i]);

    for (int i=0; i<nthreads; i++)
        if (pthread_join(thid[i], &ret) != 0)
            printf("Thread exited with %p\n", ret);

    for (int i=0; i<nthreads; i++)
        qv_scope_free(th_scopes[i]);
    /************************************************
     * Clean up
     ************************************************/
    qv_scope_free(subnuma);
    qv_scope_free(numa_scope);

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
