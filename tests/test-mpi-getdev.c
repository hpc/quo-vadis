#include <stdlib.h>
#include <stdio.h>
#include "quo-vadis-mpi.h"

#define panic(vargs...)                                                        \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, vargs);                                                    \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)


int main(int argc, char **argv)
{
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    int wsize;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    int wrank;
    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    /* Create a QV context */
    qv_context_t *ctx;
    rc = qv_mpi_create(&ctx, comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_create() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Get base scope: RM-given resources */
    qv_scope_t *base_scope;
    rc = qv_scope_get(ctx, QV_SCOPE_USER, &base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Get num GPUs */
    int ngpus;
    rc = qv_scope_nobjs(ctx, base_scope, QV_HW_OBJ_GPU, &ngpus);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    if (wrank == 0) {
        printf("[%d]: Base scope has %d GPUs\n", wrank, ngpus);
    }

    /* Split the base scope evenly across workers */
    qv_scope_t *rank_scope;
    rc = qv_scope_split(
            ctx,
            base_scope,
            wsize,        // Number of workers
            wrank,        // My group color
            &rank_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Move to my subscope */
    rc = qv_bind_push(ctx, rank_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Get num GPUs */
    int rank_ngpus;
    rc = qv_scope_nobjs(ctx, rank_scope, QV_HW_OBJ_GPU, &rank_ngpus);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d]: Local scope has %d GPUs\n", wrank, rank_ngpus);

    int total_ngpus;
    MPI_Reduce(&rank_ngpus, &total_ngpus, 1, MPI_INT, MPI_SUM, 0, comm);

    if (wrank == 0) {
        if (ngpus == total_ngpus) {
            printf("PASS: Number of GPUs match\n");
        }
        else {
            printf("FAIL: Base GPUs=%d do not match aggregate GPUs=%d\n",
                    ngpus, total_ngpus);
        }
    }

    return 0;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
