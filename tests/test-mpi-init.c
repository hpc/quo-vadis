/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-init.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

int
main(
    int argc,
    char **argv
) {
    MPI_Comm comm = MPI_COMM_WORLD;

    ctu_mpi_check(MPI_Init(&argc, &argv));

    qv_scope_t *scope = NULL;
    ctu_check(qv_mpi_scope(comm, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &scope));

    ctu_check(qv_free(scope));

    ctu_mpi_check(MPI_Finalize());

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
