/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvb.h
 *
 * Shared micro-benchmark harness for the quo-vadis public APIs.
 *
 * The overwhelming majority of the public API operates on an opaque
 * qv_scope_t *, regardless of whether that scope was created from a process,
 * thread, or MPI context. This header captures that commonality: a backend
 * supplies a "root" scope plus a few identifying bits, and the shared driver
 * (qvb_run_common) benchmarks every scope-kind-agnostic public function once.
 * Scope-kind-specific functions (e.g. qv_mpi_comm_dup, qv_thread_split) are
 * benchmarked by the individual backends via the same timing primitives, so
 * there is no measurement/reporting code duplicated across the three suites.
 */

#ifndef QVB_H
#define QVB_H

#include "quo-vadis.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default number of timed iterations per function. Override with QVB_ITERS. */
#define QVB_DEFAULT_ITERS 10

/** Scope kind, used only for labeling output. */
typedef enum {
    QVB_KIND_PROCESS = 0,
    QVB_KIND_THREAD,
    QVB_KIND_MPI
} qvb_kind_t;

static inline const char *
qvb_kind_name(qvb_kind_t kind)
{
    switch (kind) {
        case QVB_KIND_PROCESS: return "process";
        case QVB_KIND_THREAD:  return "thread";
        case QVB_KIND_MPI:     return "mpi";
        default:               return "?";
    }
}

/**
 * Fatal-error helper shared by all benchmarks.
 */
#define qvb_panic(...)                                                         \
do {                                                                           \
    fprintf(stderr, "\nqvb error %s@%d: ", __func__, __LINE__);                \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)

/**
 * Asserts that a quo-vadis call returned QV_SUCCESS.
 */
#define qvb_check(rc, what)                                                    \
do {                                                                           \
    const int qvb_rc_ = (rc);                                                  \
    if (qvb_rc_ != QV_SUCCESS) {                                               \
        qvb_panic("%s failed (rc=%s)", (what), qv_strerr(qvb_rc_));            \
    }                                                                          \
} while (0)

/**
 * Monotonic wall-clock in nanoseconds.
 */
static inline uint64_t
qvb_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * Returns the number of timed iterations to use (QVB_ITERS env override).
 */
static inline long
qvb_iters(void)
{
    const char *s = getenv("QVB_ITERS");
    if (s) {
        const long v = strtol(s, NULL, 10);
        if (v > 0) return v;
    }
    return QVB_DEFAULT_ITERS;
}

/**
 * Timing accumulator + reporter. A single instance is shared by the common
 * driver and each backend so all functions are reported through one table.
 */
typedef struct {
    qvb_kind_t kind;
    bool active;   /**< Only the "reporting" rank prints (rank 0 for MPI). */
    bool header_emitted;
} qvb_reporter_t;

static inline void
qvb_reporter_init(qvb_reporter_t *r, qvb_kind_t kind, bool active)
{
    r->kind = kind;
    r->active = active;
    r->header_emitted = false;
}

static inline void
qvb_emit_header(qvb_reporter_t *r)
{
    if (!r->active || r->header_emitted) return;
    printf(
        "# quo-vadis micro-benchmarks [%s scope]\n", qvb_kind_name(r->kind)
    );
    printf(
        "# %-24s %10s %14s %14s %14s\n",
        "function", "iters", "avg (ns)", "min (ns)", "max (ns)"
    );
    r->header_emitted = true;
}

static inline void
qvb_report(
    qvb_reporter_t *r,
    const char *name,
    long iters,
    uint64_t total_ns,
    uint64_t min_ns,
    uint64_t max_ns
) {
    if (!r->active) return;
    qvb_emit_header(r);
    const double avg = iters > 0 ? (double)total_ns / (double)iters : 0.0;
    printf(
        "  %-24s %10ld %14.2f %14llu %14llu\n",
        name, iters, avg,
        (unsigned long long)min_ns, (unsigned long long)max_ns
    );
    fflush(stdout);
}

/**
 * The signature every benchmarked body must have. The body performs exactly
 * ONE logical invocation of the function under test. ctx is backend-defined.
 */
typedef void (*qvb_body_fn)(void *ctx);

/**
 * Times `iters` invocations of `body` and reports avg/min/max under `name`.
 * This is the single place where measurement happens, so backends only ever
 * write the one-line body they want to measure.
 */
static inline void
qvb_measure(
    qvb_reporter_t *r,
    const char *name,
    long iters,
    qvb_body_fn body,
    void *ctx
) {
    // Warm-up (not timed) to amortize first-call effects.
    body(ctx);

    uint64_t total = 0, min = UINT64_MAX, max = 0;
    for (long i = 0; i < iters; ++i) {
        const uint64_t t0 = qvb_now_ns();
        body(ctx);
        const uint64_t dt = qvb_now_ns() - t0;
        total += dt;
        if (dt < min) min = dt;
        if (dt > max) max = dt;
    }
    if (min == UINT64_MAX) min = 0;
    qvb_report(r, name, iters, total, min, max);
}

/**
 * Times two paired sub-steps that must execute together to keep external state
 * balanced (e.g. qv_bind_push / qv_bind_pop), reporting each under its own
 * name. Both bodies run every iteration; `body_a` is timed and reported as
 * `name_a`, `body_b` as `name_b`. This preserves the "measurement happens in
 * one place" design while still separating the cost of each call.
 */
static inline void
qvb_measure2(
    qvb_reporter_t *r,
    const char *name_a,
    qvb_body_fn body_a,
    const char *name_b,
    qvb_body_fn body_b,
    long iters,
    void *ctx
) {
    // Warm-up (not timed) to amortize first-call effects.
    body_a(ctx);
    body_b(ctx);

    uint64_t total_a = 0, min_a = UINT64_MAX, max_a = 0;
    uint64_t total_b = 0, min_b = UINT64_MAX, max_b = 0;
    for (long i = 0; i < iters; ++i) {
        const uint64_t ta0 = qvb_now_ns();
        body_a(ctx);
        const uint64_t da = qvb_now_ns() - ta0;
        total_a += da;
        if (da < min_a) min_a = da;
        if (da > max_a) max_a = da;

        const uint64_t tb0 = qvb_now_ns();
        body_b(ctx);
        const uint64_t db = qvb_now_ns() - tb0;
        total_b += db;
        if (db < min_b) min_b = db;
        if (db > max_b) max_b = db;
    }
    if (min_a == UINT64_MAX) min_a = 0;
    if (min_b == UINT64_MAX) min_b = 0;
    qvb_report(r, name_a, iters, total_a, min_a, max_a);
    qvb_report(r, name_b, iters, total_b, min_b, max_b);
}

/**
 * Backend abstraction. A backend knows how to produce a "root" scope for a
 * given scope kind; everything else (the common function benchmarks) is
 * shared. This is what removes duplication across process/thread/MPI suites.
 */
typedef struct qvb_backend_s {
    qvb_kind_t kind;
    /** True if this rank/instance should print results. */
    bool reporting;
    /**
     * Creates a fresh root scope. Used both to obtain the scope the common
     * benchmarks operate on and (by timing this call) to benchmark scope
     * creation itself. Must not return NULL.
     */
    qv_scope_t *(*make_root_scope)(struct qvb_backend_s *self);
    /** Backend-private data. */
    void *data;
} qvb_backend_t;

/**
 * Benchmarks every scope-kind-agnostic public function exactly once, using a
 * scope produced by the backend. Called by all three suites; the only thing
 * that differs between suites is the backend passed in.
 *
 * Note: qv_scope creation and qv_free are also benchmarked here since they are
 * shared entry points; kind-specific creation (qv_mpi_scope, qv_process_scope,
 * ...) is timed by the backends themselves.
 */
void
qvb_run_common(qvb_backend_t *backend, qvb_reporter_t *reporter);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
