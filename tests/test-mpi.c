/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-mpi.c
 */

#include "private/qvi-mpi.h"
#include "private/qvi-utils.h"

#include "quo-vadis.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(
    int argc,
    char **argv
) {
    int rc = QV_SUCCESS;
    char const *ers = NULL;

    qvi_mpi_t *mpi = NULL;

    int gid = 0;
    int lid = 0;
    int wsize = 0;
    int nsize = 0;

    rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        fprintf(stderr, "%s\n", ers);
        return EXIT_FAILURE;
    }

    rc = qvi_mpi_construct(&mpi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_construct() failed";
        goto out;
    }

    rc = qvi_mpi_init(mpi, MPI_COMM_WORLD);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_init() failed";
        goto out;
    }

    gid = qvi_mpi_world_id(mpi);
    lid = qvi_mpi_node_id(mpi);
    wsize = qvi_mpi_world_size(mpi);
    nsize = qvi_mpi_node_size(mpi);

    printf(
        "Hello from gid=%d (lid=%d, nsize=%d) of wsize=%d\n",
        gid,
        lid,
        nsize,
        wsize
    );

    rc = qvi_mpi_finalize(mpi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_finalize() failed";
        goto out;
    }
out:
    qvi_mpi_destruct(mpi);
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
