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
 * public entry points from quo-vadis-mpi.h. Every rank times the functions
 * independently and the avg/min/max are reduced across all ranks with MPI
 * collectives; only rank 0 reports the aggregate.
 */

#include "quo-vadis-mpi.h"
#include "qvb.h"

#include "mpi.h"

/**
 * Asserts that an MPI call returned MPI_SUCCESS, mirroring qvb_check for the
 * quo-vadis calls. On failure it prints the MPI error string (when available)
 * and aborts.
 */
#define qvb_mpi_check(rc, what)                                                \
do {                                                                           \
    const int qvb_mpi_rc_ = (rc);                                              \
    if (qvb_mpi_rc_ != MPI_SUCCESS) {                                          \
        char qvb_mpi_estr_[MPI_MAX_ERROR_STRING] = {0};                        \
        int qvb_mpi_elen_ = 0;                                                 \
        if (MPI_Error_string(                                                  \
                qvb_mpi_rc_, qvb_mpi_estr_, &qvb_mpi_elen_                     \
            ) != MPI_SUCCESS) {                                                \
            qvb_mpi_estr_[0] = '\0';                                           \
        }                                                                      \
        qvb_panic("%s failed (rc=%d: %s)", (what), qvb_mpi_rc_, qvb_mpi_estr_);\
    }                                                                          \
} while (0)

typedef struct {
    MPI_Comm comm;
} mpi_ctx_t;

/**
 * Cross-rank reduction (qvb_reduce_fn) for the MPI suite. Every rank times the
 * same functions independently; this collapses those per-rank samples into
 * suite-wide statistics using MPI collectives so the reported avg/min/max span
 * ALL ranks rather than just the reporting rank:
 *
 *   - total: MPI_SUM of each rank's summed iteration time (feeds the average),
 *   - min:   MPI_MIN of each rank's fastest iteration,
 *   - max:   MPI_MAX of each rank's slowest iteration,
 *   - ninst: the communicator size (number of participating ranks).
 *
 * Allreduce is used so every rank stays in lock-step; only the reporting rank
 * consumes the result.
 */
static void
mpi_reduce_samples(
    void *ctx,
    const qvb_sample_t *local,
    uint64_t *out_total,
    uint64_t *out_min,
    uint64_t *out_max,
    long *out_ninst
) {
    mpi_ctx_t *c = (mpi_ctx_t *)ctx;

    qvb_mpi_check(
        MPI_Allreduce(
            &local->total_ns, out_total, 1, MPI_UINT64_T, MPI_SUM, c->comm
        ),
        "MPI_Allreduce(total)"
    );
    qvb_mpi_check(
        MPI_Allreduce(
            &local->min_ns, out_min, 1, MPI_UINT64_T, MPI_MIN, c->comm
        ),
        "MPI_Allreduce(min)"
    );
    qvb_mpi_check(
        MPI_Allreduce(
            &local->max_ns, out_max, 1, MPI_UINT64_T, MPI_MAX, c->comm
        ),
        "MPI_Allreduce(max)"
    );

    int size = 1;
    qvb_mpi_check(MPI_Comm_size(c->comm, &size), "MPI_Comm_size");
    *out_ninst = (long)size;
}

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
    qvb_mpi_check(MPI_Comm_free(&dup), "MPI_Comm_free");
}

int
main(int argc, char **argv)
{
    qvb_mpi_check(MPI_Init(&argc, &argv), "MPI_Init");

    int wrank = 0;
    qvb_mpi_check(MPI_Comm_rank(MPI_COMM_WORLD, &wrank), "MPI_Comm_rank");

    // Only rank 0 prints the table; all ranks execute so collective calls
    // (e.g. qv_barrier, qv_split) don't deadlock.
    const bool reporting = (wrank == 0);

    qvb_reporter_t reporter;
    qvb_reporter_init(&reporter, QVB_KIND_MPI, reporting);

    mpi_ctx_t mctx = { .comm = MPI_COMM_WORLD };

    // Reduce avg/min/max across ALL ranks (not just the reporting rank) via
    // MPI collectives. Every rank runs the same benchmarks, so every rank must
    // participate in the reduction; only rank 0 prints the aggregate.
    qvb_reporter_set_reduce(&reporter, mpi_reduce_samples, &mctx);

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

    qvb_mpi_check(MPI_Finalize(), "MPI_Finalize");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
