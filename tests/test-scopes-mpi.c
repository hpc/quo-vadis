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
 * @file test-scopes-mpi.c
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

    qv_context_t *ctx;
    rc = qv_mpi_create(&ctx, comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_create() failed";
        goto out;
    }

    qv_scope_t *base_scope;
    rc = qv_scope_get(
        ctx,
        QV_SCOPE_SYSTEM,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        goto out;
    }

    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        ctx,
        base_scope,
        2,
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        goto out;
    }

    rc = qv_scope_free(ctx, base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        goto out;
    }

out:
    rc = qv_free(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
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
