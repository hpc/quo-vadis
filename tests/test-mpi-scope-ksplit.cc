/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-mpi-scope-ksplit.cc
 */

#include "mpi.h"
#include "quo-vadis-mpi.h"
#include "qvi-scope.h"
#include "qvi-test-common.h"

int
main(void)
{
    cstr_t ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

    /* Initialization */
    int rc = MPI_Init(nullptr, nullptr);
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
    // Helps with flushing MPI process output.
    setbuf(stdout, NULL);

    qv_context_t *ctx = nullptr;
    rc = qv_mpi_context_create(comm, &ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_create() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *base_scope = nullptr;
    rc = qv_scope_get(
        ctx, QV_SCOPE_USER, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ncores = 0;
    rc = qv_scope_nobjs(
        ctx, base_scope, QV_HW_OBJ_CORE, &ncores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Test internal APIs
    const uint_t npieces = ncores / 2;
    std::vector<int> colors(npieces * 2);
    std::fill_n(
        colors.begin(), colors.size(),
        QV_SCOPE_SPLIT_AFFINITY_PRESERVING
    );
    //std::iota(colors.begin(), colors.end(), 0);

    std::vector<qv_scope_t *> subscopes;
    rc = qvi_scope_ksplit(
        base_scope, npieces, colors, subscopes
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_scope_ksplit() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    for (uint_t i = 0; i < subscopes.size(); ++i) {
        qvi_test_scope_report(
            ctx, subscopes[i], std::to_string(i).c_str()
        );
        qvi_test_bind_push(
            ctx, subscopes[i]
        );
        qvi_test_bind_pop(
            ctx, subscopes[i]
        );
    }

    qvi_scope_kfree(subscopes);

    rc = qv_scope_free(ctx, base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_mpi_context_free(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_free() failed";
        qvi_test_panic("%s", ers);
    }

    rc = MPI_Finalize();
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Finalize() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    if (wrank == 0) {
        qvi_log_info("Test Passed");
    }

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
