/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(
    int argc,
    char **argv
) {
    MPI_Comm comm = MPI_COMM_WORLD;

    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) abort();

    int wsize = 0;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) abort();
    // This test should be run with -n 2.
    const int esize = 2;
    if (wsize != esize) {
        fprintf(stderr, "MPI seems to be broken\n");
        abort();
    }
    printf("pid %d says it looks good: comm_size==%d\n", getpid(), esize);
    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
