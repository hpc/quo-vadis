/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

int
main(
    int argc,
    char **argv
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

    // Get base scope: RM-given resources.
    qv_scope_t *base_scope;
    rc = qv_mpi_scope_get(
        comm,
        QV_SCOPE_USER,
        QV_SCOPE_FLAG_NONE,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope_get() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (wrank == 0) {
        ctu_emit_device_info(base_scope, QV_HW_OBJ_GPU, "Base Scope");
    }

    // Get total number of GPUs in base_scope.
    int ngpus;
    rc = qv_scope_hw_obj_count(base_scope, QV_HW_OBJ_GPU, &ngpus);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    if (wrank == 0) {
        printf("[%d]: Base scope has %d GPUs\n", wrank, ngpus);
    }

    // Split the base scope evenly across workers.
    qv_scope_t *rank_scope;
    rc = qv_scope_split(
            base_scope,
            wsize,
            wrank,
            &rank_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Get number of GPUs in my rank_scope.
    int rank_ngpus;
    rc = qv_scope_hw_obj_count(
        rank_scope,
        QV_HW_OBJ_GPU,
        &rank_ngpus
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    printf("[%d]: Local scope has %d GPUs\n", wrank, rank_ngpus);

    //ctu_emit_device_info(rank_scope, QV_HW_OBJ_GPU, "Rank Scope");

    int total_ngpus;
    MPI_Reduce(&rank_ngpus, &total_ngpus, 1, MPI_INT, MPI_SUM, 0, comm);

    if (wrank == 0) {
        if (ngpus == total_ngpus) {
            printf("PASS: Number of GPUs match\n");
        }
        else {
            printf(
                "FAIL: Base GPUs=%d do not match aggregate GPUs=%d\n",
                ngpus, total_ngpus
            );
        }
    }

    qv_scope_free(rank_scope);
    qv_scope_free(base_scope);

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
