/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-mpi-init.c
 */

#include "quo-vadis-mpi.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
        fprintf(stderr, "%s\n", ers);
        return EXIT_FAILURE;
    }
    int wsize;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        fprintf(stderr, "%s\n", ers);
        return EXIT_FAILURE;
    }

    qv_context_t *ctx = NULL;
    rc = qv_mpi_context_create(&ctx, comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_create() failed";
        goto out;
    }

out:
    rc = qv_mpi_context_free(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_free() failed";
    }
    MPI_Finalize();
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
