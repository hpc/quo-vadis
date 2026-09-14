/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-api.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

int
main(
    int argc,
    char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

    ctu_mpi_check(MPI_Init(&argc, &argv), "MPI_Init");

    int wsize = 0;
    ctu_mpi_check(MPI_Comm_size(comm, &wsize), "MPI_Comm_size");

    int wrank = 0;
    ctu_mpi_check(MPI_Comm_rank(comm, &wrank), "MPI_Comm_rank");

    int vmajor, vminor, vpatch;
    ctu_check(qv_version(&vmajor, &vminor, &vpatch), "qv_version");

    qv_scope_t *world_scope = NULL;
    ctu_check(
        qv_mpi_scope(
            comm, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &world_scope
        ),
        "qv_mpi_scope"
    );

    ctu_pemit(
        world_scope, CTU_SCOPE_KIND_MPI, wrank == 0,
        "QV Version: %d.%d.%d\n", vmajor, vminor, vpatch
    );

    MPI_Comm wscope_comm = MPI_COMM_NULL;
    ctu_check(qv_mpi_comm_dup(world_scope, &wscope_comm), "qv_mpi_comm_dup");

    int wscope_size = 0;
    ctu_mpi_check(MPI_Comm_size(wscope_comm, &wscope_size), "MPI_Comm_size");

    int wscope_rank = 0;
    ctu_mpi_check(MPI_Comm_rank(wscope_comm, &wscope_rank), "MPI_Comm_rank");

    if (wscope_size != wsize) {
        ers = "MPI communicator size mismatch!";
        ctu_panic("%s", ers);
    }

    if (wscope_rank != wrank) {
        ers = "MPI communicator rank mismatch!";
        ctu_panic("%s", ers);
    }

    qv_scope_t *sub_scope = NULL;
    ctu_check(
        qv_split(
            world_scope, wsize, wrank, &sub_scope
        ),
        "qv_split"
    );

    MPI_Comm split_wscope_comm = MPI_COMM_NULL;
    ctu_check(
        qv_mpi_comm_dup(sub_scope, &split_wscope_comm),
        "qv_mpi_comm_dup"
    );

    int split_wscope_size = 0;
    ctu_mpi_check(
        MPI_Comm_size(split_wscope_comm, &split_wscope_size),
        "MPI_Comm_size"
    );

    ctu_pemit(
        world_scope, CTU_SCOPE_KIND_MPI, wrank == 0,
        "Size of MPI_COMM_WORLD = %d\n", wsize
    );
    ctu_pemit(
        world_scope, CTU_SCOPE_KIND_MPI, wrank == 0,
        "Size of World Scope    = %d\n", wscope_size
    );

    float expected_result = (float)1 / wsize;
    float result = (float)split_wscope_size / wscope_size;

    if (expected_result != result) {
        ctu_panic("Unexpected result!");
    }

    ctu_pemit(
        world_scope, CTU_SCOPE_KIND_MPI, wrank == 0,
        "Size of Split World Scope = %d (1/%d of World Scope)\n",
        split_wscope_size, wsize
    );

    ctu_check(qv_free(sub_scope), "qv_free");

    ctu_check(qv_free(world_scope), "qv_free");

    ctu_mpi_check(MPI_Comm_free(&wscope_comm), "MPI_Comm_free");

    ctu_mpi_check(MPI_Comm_free(&split_wscope_comm), "MPI_Comm_free");

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
