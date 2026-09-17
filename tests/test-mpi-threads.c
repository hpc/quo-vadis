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
    ctu_emit_task_bind(arg, CTU_SCOPE_KIND_THREAD);
    return NULL;
}

int
main(
    int argc, char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

    ctu_mpi_check(MPI_Init(&argc, &argv), "MPI_Init");

    int wsize;
    ctu_mpi_check(MPI_Comm_size(comm, &wsize), "MPI_Comm_size");

    int wrank;
    ctu_mpi_check(MPI_Comm_rank(comm, &wrank), "MPI_Comm_rank");
    ////////////////////////////////////////////////////////////////////////////
    // Use the process interface for NUMA.
    ////////////////////////////////////////////////////////////////////////////
    // Get the base scope: RM-given resources.
    qv_scope_t *base_scope;
    ctu_check(
        qv_mpi_scope(comm, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &base_scope),
        "qv_mpi_scope"
    );

    int nnumas;
    ctu_check(
        qv_hw_count(base_scope, QV_HW_NUMANODE, &nnumas),
        "qv_hw_count"
    );
    // Split at NUMA domains.
    qv_scope_t *numa_scope;
    ctu_check(
        qv_split_at(
            base_scope, QV_HW_NUMANODE,
            wrank % nnumas, &numa_scope
        ),
        "qv_split_at"
    );
    // When there's more tasks than NUMAs,
    // make sure each task has exclusive resources.
    int lrank;
    ctu_check(qv_group_rank(numa_scope, &lrank), "qv_group_rank");

    int ntasks_per_numa;
    ctu_check(qv_group_size(numa_scope, &ntasks_per_numa), "qv_group_size");

    qv_scope_t *subnuma;
    ctu_check(
        qv_split(
            numa_scope, ntasks_per_numa,
            lrank % ntasks_per_numa, &subnuma
        ),
        "qv_split"
    );
    // Get the number of cores and pus per NUMA part.
    int ncores;
    ctu_check(
        qv_hw_count(subnuma, QV_HW_CORE, &ncores),
        "qv_hw_count"
    );

    int npus;
    ctu_check(
        qv_hw_count(subnuma, QV_HW_PU, &npus),
        "qv_hw_count"
    );
    ////////////////////////////////////////////////////////////////////////////
    // OpenMP: Launch one thread per core.
    ////////////////////////////////////////////////////////////////////////////
    const int nthreads = ncores;
    ctu_pemit(
        base_scope, CTU_SCOPE_KIND_MPI, wrank == 0,
        "# Starting OpenMP test (nthreads/process=%d)\n", nthreads
    );
    int *thread_coloring = NULL; // Default thread assignment.
    qv_scope_t **th_scopes;
    ctu_check(
        qv_thread_split_at(
            subnuma, QV_HW_CORE, thread_coloring, nthreads, &th_scopes
        ),
        "qv_thread_split_at"
    );

    omp_set_num_threads(nthreads);
    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        // Each thread works with its new hardware affinity.
        qv_bind_push(th_scopes[tid]);
        thread_work(th_scopes[tid]);
    }
    // When we are done with the scope, clean up.
    ctu_check(qv_thread_free(th_scopes, nthreads), "qv_thread_free");
    ctu_pemit(
        base_scope, CTU_SCOPE_KIND_MPI, wrank == 0,
        "# Done!\n"
    );
    ////////////////////////////////////////////////////////////////////////////
    // POSIX threads:
    // * Launch one thread per hardware thread
    // *   Policy-based placement
    // *   Note num_threads < num_places on SMT
    ////////////////////////////////////////////////////////////////////////////
    ctu_pemit(
        base_scope, CTU_SCOPE_KIND_MPI, wrank == 0,
        "# Starting Pthread test (nthreads/process=%d)\n", nthreads
    );
    thread_coloring = QV_THREAD_SPLIT_PACKED;
    int rc = qv_thread_split_at(
        subnuma, QV_HW_PU, thread_coloring, nthreads, &th_scopes
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_split_at() failed";
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
    ctu_pemit(
        base_scope, CTU_SCOPE_KIND_MPI, wrank == 0,
        "# Done!\n"
    );
    // When we are done with the scope, clean up.
    ctu_check(qv_thread_free(th_scopes, nthreads), "qv_thread_free");
    // Clean up.
    qv_free(subnuma);
    qv_free(numa_scope);
    qv_free(base_scope);

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
