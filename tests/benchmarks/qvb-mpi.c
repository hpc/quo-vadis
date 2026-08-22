/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvb-mpi.c
 *
 * MPI-scope micro-benchmarks. Reuses the shared driver (qvb_run_common) for
 * all scope-kind-agnostic functions and adds timing for the MPI-specific
 * public entry points from quo-vadis-mpi.h. Only rank 0 reports.
 */

#include "quo-vadis-mpi.h"
#include "qvb.h"

#include "mpi.h"

typedef struct {
    MPI_Comm comm;
} mpi_ctx_t;

static qv_scope_t *
make_root_scope(qvb_backend_t *self)
{
    mpi_ctx_t *c = (mpi_ctx_t *)self->data;
    qv_scope_t *scope = NULL;
    qvb_check(
        qv_mpi_scope(c->comm, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &scope),
        "qv_mpi_scope"
    );
    return scope;
}

// qv_mpi_scope + qv_free cycle.
static void
body_mpi_scope(void *v)
{
    mpi_ctx_t *c = (mpi_ctx_t *)v;
    qv_scope_t *scope = NULL;
    qvb_check(
        qv_mpi_scope(c->comm, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &scope),
        "qv_mpi_scope"
    );
    qvb_check(qv_free(scope), "qv_free");
}

// qv_mpi_comm_dup + MPI_Comm_free cycle against a persistent scope.
typedef struct {
    qv_scope_t *scope;
} mpi_dup_ctx_t;

static void
body_mpi_comm_dup(void *v)
{
    mpi_dup_ctx_t *c = (mpi_dup_ctx_t *)v;
    MPI_Comm dup = MPI_COMM_NULL;
    qvb_check(qv_mpi_comm_dup(c->scope, &dup), "qv_mpi_comm_dup");
    MPI_Comm_free(&dup);
}

int
main(int argc, char **argv)
{
    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        qvb_panic("MPI_Init failed (rc=%d)", rc);
    }

    int wrank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &wrank);

    // Only rank 0 prints the table; all ranks execute so collective calls
    // (e.g. qv_barrier, qv_split) don't deadlock.
    const bool reporting = (wrank == 0);

    qvb_reporter_t reporter;
    qvb_reporter_init(&reporter, QVB_KIND_MPI, reporting);

    mpi_ctx_t mctx = { .comm = MPI_COMM_WORLD };

    qvb_backend_t backend = {
        .kind = QVB_KIND_MPI,
        .reporting = reporting,
        .make_root_scope = make_root_scope,
        .data = &mctx
    };

    const long iters = qvb_iters();

    // MPI-specific entry points.
    qvb_measure(&reporter, "qv_mpi_scope", iters, body_mpi_scope, &mctx);

    qv_scope_t *dup_scope = NULL;
    qvb_check(
        qv_mpi_scope(
            MPI_COMM_WORLD, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &dup_scope
        ),
        "qv_mpi_scope"
    );
    mpi_dup_ctx_t dctx = { .scope = dup_scope };
    qvb_measure(&reporter, "qv_mpi_comm_dup", iters, body_mpi_comm_dup, &dctx);
    qvb_check(qv_free(dup_scope), "qv_free");

    // Everything scope-kind-agnostic is shared.
    qvb_run_common(&backend, &reporter);

    MPI_Finalize();
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
