/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-mpi-api.c
 */

#include "quo-vadis-mpi.h"
#include "qvi-test-common.h"

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
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    int wsize = 0;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    int wrank = 0;
    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    int vmajor, vminor, vpatch;
    rc = qv_version(&vmajor, &vminor, &vpatch);
    if (rc != QV_SUCCESS) {
        ers = "qv_version() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (wrank == 0) {
        printf("QV Version: %d.%d.%d\n", vmajor, vminor, vpatch);
    }

    qv_context_t *ctx = NULL;
    rc = qv_mpi_context_create(comm, &ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *world_scope = NULL;
    rc = qv_scope_get(
        ctx, QV_SCOPE_USER, &world_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    MPI_Comm wscope_comm = MPI_COMM_NULL;
    rc = qv_mpi_scope_comm_dup(ctx, world_scope, &wscope_comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope_comm_dup() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int wscope_size = 0;
    rc = MPI_Comm_size(wscope_comm, &wscope_size);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    int wscope_rank = 0;
    rc = MPI_Comm_rank(wscope_comm, &wscope_rank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    if (wscope_size != wsize) {
        ers = "MPI communicator size mismatch!";
        qvi_test_panic("%s", ers);
    }

    if (wscope_rank != wrank) {
        ers = "MPI communicator rank mismatch!";
        qvi_test_panic("%s", ers);
    }

    qv_scope_t *sub_scope = NULL;
    rc = qv_scope_split(
        ctx, world_scope, wsize, wrank, &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    MPI_Comm split_wscope_comm = MPI_COMM_NULL;
    rc = qv_mpi_scope_comm_dup(ctx, sub_scope, &split_wscope_comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope_comm_dup() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int split_wscope_size = 0;
    rc = MPI_Comm_size(split_wscope_comm, &split_wscope_size);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    if (wrank == 0) {
        printf("Size of MPI_COMM_WORLD = %d\n", wsize);
        printf("Size of World Scope    = %d\n", wscope_size);
        printf(
            "Size of Split World Scope = %d (1/%d of World Scope)\n",
            split_wscope_size, wsize
        );
    }

    rc = qv_scope_free(ctx, sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(ctx, world_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_mpi_context_free(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_free() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    rc = MPI_Comm_free(&wscope_comm);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_free() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    rc = MPI_Comm_free(&split_wscope_comm);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_free() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    if (wrank == 0) {
        printf("Succcess!\n");
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
