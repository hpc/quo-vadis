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
#include "qvi-rmi.h"

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

    int wrank;
    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        fprintf(stderr, "%s\n", ers);
        return EXIT_FAILURE;
    }

    setbuf(stdout, NULL);

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

    int n_cores;
    rc = qv_scope_nobjs(
        ctx,
        base_scope,
        QV_HW_OBJ_NUMANODE,
        &n_cores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        goto out;
    }
    printf("[%d] Number of NUMA in base_scope is %d\n", wrank, n_cores);

    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        ctx,
        base_scope,
        2,
        wrank,
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        goto out;
    }

    rc = qv_scope_nobjs(
        ctx,
        sub_scope,
        QV_HW_OBJ_NUMANODE,
        &n_cores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        goto out;
    }
    printf("[%d] Number of NUMA in sub_scope is %d\n", wrank, n_cores);

    char *binds;
    rc = qv_bind_get_as_string(ctx, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_as_string() failed";
        goto out;
    }
    printf("[%d] Current cpubind is %s\n", wrank, binds);
    free(binds);

    rc = qv_bind_push(ctx, sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        goto out;
    }

    char *bind1s;
    rc = qv_bind_get_as_string(ctx, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_as_string() failed";
        goto out;
    }
    printf("[%d] New cpubind is %s\n", wrank, bind1s);
    free(bind1s);

    rc = qv_bind_pop(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        goto out;
    }

    char *bind2s;
    rc = qv_bind_get_as_string(ctx, &bind2s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_as_string() failed";
        goto out;
    }
    printf("[%d] Popped cpubind is %s\n", wrank, bind2s);
    free(bind2s);

    // TODO(skg) Add test to make popped is same as original.

    qv_scope_t *sub_sub_scope;
    rc = qv_scope_split(
        ctx,
        sub_scope,
        2,
        wrank,
        &sub_sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        goto out;
    }

    rc = qv_scope_nobjs(
        ctx,
        sub_sub_scope,
        QV_HW_OBJ_NUMANODE,
        &n_cores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        goto out;
    }
    printf("[%d] Number of NUMA in sub_sub_scope is %d\n", wrank, n_cores);

    rc = qv_scope_free(ctx, base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        goto out;
    }

    rc = qv_scope_free(ctx, sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        goto out;
    }

    rc = qv_scope_free(ctx, sub_sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        goto out;
    }

    rc = qv_barrier(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_barrier() failed";
        goto out;
    }
out:
    if (qv_free(ctx) != QV_SUCCESS) {
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
