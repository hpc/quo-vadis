/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvb-thread.c
 *
 * Thread-scope micro-benchmarks. Reuses the shared driver (qvb_run_common) for
 * all scope-kind-agnostic functions and adds timing for the thread-specific
 * public entry points from quo-vadis-thread.h.
 */

#include "quo-vadis.h"
#include "quo-vadis-thread.h"
#include "qvb.h"

static qv_scope_t *
make_root_scope(qvb_backend_t *self)
{
    (void)self;
    qv_scope_t *scope = NULL;
    qvb_check(
        qv_process_scope(QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &scope),
        "qv_process_scope"
    );
    return scope;
}

typedef struct {
    qv_scope_t *base;
    int nthreads;
} thread_ctx_t;

// A no-op thread routine used to benchmark qv_pthread_create.
static void *
noop_routine(void *arg)
{
    (void)arg;
    return NULL;
}

// qv_thread_split + qv_thread_free as a balanced cycle.
static void
body_thread_split(void *v)
{
    thread_ctx_t *c = (thread_ctx_t *)v;
    qv_scope_t **subs = NULL;
    qvb_check(
        qv_thread_split(
            c->base, 1, QV_THREAD_SCOPE_SPLIT_PACKED, c->nthreads, &subs
        ),
        "qv_thread_split"
    );
    qvb_check(qv_thread_free(c->nthreads, subs), "qv_thread_free");
}

// qv_thread_split_at + qv_thread_free as a balanced cycle.
static void
body_thread_split_at(void *v)
{
    thread_ctx_t *c = (thread_ctx_t *)v;
    qv_scope_t **subs = NULL;
    qvb_check(
        qv_thread_split_at(
            c->base, QV_HW_OBJ_CORE,
            QV_THREAD_SCOPE_SPLIT_PACKED, c->nthreads, &subs
        ),
        "qv_thread_split_at"
    );
    qvb_check(qv_thread_free(c->nthreads, subs), "qv_thread_free");
}

// qv_pthread_create + join, using a scope from a fresh split each iteration.
static void
body_pthread_create(void *v)
{
    thread_ctx_t *c = (thread_ctx_t *)v;
    qv_scope_t **subs = NULL;
    qvb_check(
        qv_thread_split(
            c->base, 1, QV_THREAD_SCOPE_SPLIT_PACKED, 1, &subs
        ),
        "qv_thread_split"
    );

    pthread_t tid;
    const int rc = qv_pthread_create(&tid, NULL, noop_routine, NULL, subs[0]);
    if (rc != 0) {
        qvb_panic("qv_pthread_create failed (rc=%d)", rc);
    }
    pthread_join(tid, NULL);

    qvb_check(qv_thread_free(1, subs), "qv_thread_free");
}

int
main(void)
{
    qvb_reporter_t reporter;
    qvb_reporter_init(&reporter, QVB_KIND_THREAD, /*active=*/true);

    qvb_backend_t backend = {
        .kind = QVB_KIND_THREAD,
        .reporting = true,
        .make_root_scope = make_root_scope,
        .data = NULL
    };

    // A base scope for the thread-specific splits.
    qv_scope_t *base = NULL;
    qvb_check(
        qv_process_scope(QV_SCOPE_PROCESS, QV_SCOPE_FLAG_NONE, &base),
        "qv_process_scope"
    );

    int ncores = 0;
    qvb_check(
        qv_hw_obj_count(base, QV_HW_OBJ_CORE, &ncores), "qv_hw_obj_count"
    );
    if (ncores < 1) ncores = 1;

    thread_ctx_t tctx = { .base = base, .nthreads = ncores };

    const long iters = qvb_iters();
    qvb_measure(&reporter, "qv_thread_split", iters, body_thread_split, &tctx);
    qvb_measure(
        &reporter, "qv_thread_split_at", iters, body_thread_split_at, &tctx
    );
    qvb_measure(
        &reporter, "qv_pthread_create", iters, body_pthread_create, &tctx
    );

    qvb_check(qv_free(base), "qv_free");

    // Everything scope-kind-agnostic is shared.
    qvb_run_common(&backend, &reporter);

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
