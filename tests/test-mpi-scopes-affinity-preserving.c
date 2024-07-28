/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-mpi-scopes-affinity-preserving.c
 */

#include "quo-vadis-mpi.h"
#include "qvi-test-common.h"

#include <signal.h>

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

    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    setbuf(stdout, NULL);

    int wsize;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    int wrank;
    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    qv_scope_t *base_scope;
    rc = qv_mpi_scope_get(
        comm,
        QV_SCOPE_USER,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope_get() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_test_scope_report(base_scope, "base_scope");

    int base_scope_ntasks;
    rc = qv_scope_ntasks(
        base_scope,
        &base_scope_ntasks
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_ntasks() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int base_scope_rank;
    rc = qv_scope_group_rank(
        base_scope,
        &base_scope_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int n_pu;
    rc = qv_scope_nobjs(
        base_scope,
        QV_HW_OBJ_PU,
        &n_pu
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of PUs in base_scope is %d\n", wrank, n_pu);

    const int npieces = 2;
    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        base_scope,
        npieces,
        QV_SCOPE_SPLIT_AFFINITY_PRESERVING,
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_test_scope_report(sub_scope, "sub_scope");

    qvi_test_change_bind(sub_scope);

    rc = qv_scope_nobjs(
        sub_scope, QV_HW_OBJ_PU, &n_pu
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of PUs in sub_scope is %d\n", wrank, n_pu);

    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
