/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvb-process.c
 *
 * Process-scope micro-benchmarks. Reuses the shared driver (qvb_run_common)
 * for all scope-kind-agnostic functions and only adds process-specific scope
 * creation timing here.
 */

#include "quo-vadis.h"
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

// Body that benchmarks process scope creation + free as a cycle.
static void
body_process_scope(void *v)
{
    (void)v;
    qv_scope_t *scope = NULL;
    qvb_check(
        qv_process_scope(QV_SCOPE_PROCESS, QV_SCOPE_FLAG_NONE, &scope),
        "qv_process_scope"
    );
    qvb_check(qv_free(scope), "qv_free");
}

// Global, non-scope entry points shared by all APIs; benchmarked once here
// since the process suite always builds.
static void
body_version(void *v)
{
    (void)v;
    int major = 0, minor = 0, patch = 0;
    qvb_check(qv_version(&major, &minor, &patch), "qv_version");
}

static void
body_strerr(void *v)
{
    (void)v;
    volatile const char *s = qv_strerr(QV_SUCCESS);
    (void)s;
}

int
main(void)
{
    qvb_reporter_t reporter;
    qvb_reporter_init(&reporter, QVB_KIND_PROCESS, /*active=*/true);

    qvb_backend_t backend = {
        .kind = QVB_KIND_PROCESS,
        .reporting = true,
        .make_root_scope = make_root_scope,
        .data = NULL
    };

    // Process-specific entry points.
    qvb_measure(
        &reporter, "qv_process_scope", qvb_iters(), body_process_scope, NULL
    );

    // Global, non-scope entry points.
    qvb_measure(&reporter, "qv_version", qvb_iters(), body_version, NULL);
    qvb_measure(&reporter, "qv_strerr", qvb_iters(), body_strerr, NULL);

    // Everything else is shared.
    qvb_run_common(&backend, &reporter);

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
