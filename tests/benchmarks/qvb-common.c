/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvb-common.c
 *
 * Shared driver that benchmarks every scope-kind-agnostic public function in
 * quo-vadis.h exactly once. Reused as-is by the process, thread, and MPI
 * benchmark binaries; the only per-suite variation is the backend that
 * supplies the root scope.
 */

#include "qvb.h"

/*
 * Each benchmarked function gets a tiny "body" performing a single logical
 * call. The body signature is uniform (qvb_body_fn), so qvb_measure() does all
 * the timing and reporting. The context struct below carries whatever state a
 * body needs.
 */
typedef struct {
    qv_scope_t *scope;
    // Reusable output slots so bodies don't allocate/measure allocation.
    int i_out;
    char *str_out;
    char *devid_out;
    qv_hw_obj_type_t obj_type;
    qv_device_id_type_t devid_type;
} qvb_common_ctx_t;

static void
body_group_rank(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    qvb_check(qv_group_rank(c->scope, &c->i_out), "qv_group_rank");
}

static void
body_group_size(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    qvb_check(qv_group_size(c->scope, &c->i_out), "qv_group_size");
}

static void
body_hw_obj_count(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    qvb_check(
        qv_hw_obj_count(c->scope, c->obj_type, &c->i_out), "qv_hw_obj_count"
    );
}

static void
body_barrier(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    qvb_check(qv_barrier(c->scope), "qv_barrier");
}

static void
body_bind_string(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    qvb_check(
        qv_bind_string(c->scope, QV_BIND_STRING_LOGICAL, &c->str_out),
        "qv_bind_string"
    );
    free(c->str_out);
    c->str_out = NULL;
}

static void
body_bind_push(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    qvb_check(qv_bind_push(c->scope), "qv_bind_push");
}

static void
body_bind_pop(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    qvb_check(qv_bind_pop(c->scope), "qv_bind_pop");
}

static void
body_device_id(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    const int rc = qv_device_id(
        c->scope, c->obj_type, 0, c->devid_type, &c->devid_out
    );
    // Devices may be absent; only free on success.
    if (rc == QV_SUCCESS) {
        free(c->devid_out);
        c->devid_out = NULL;
    }
}

/*
 * Split-family functions produce a subscope that must be freed each iteration
 * to keep state balanced; the free is intentionally included in the timing to
 * reflect a realistic split/free cycle. qv_create_scope is treated similarly.
 */
static void
body_split(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    qv_scope_t *sub = NULL;
    qvb_check(qv_split(c->scope, 1, 0, &sub), "qv_split");
    qvb_check(qv_free(sub), "qv_free");
}

static void
body_split_at(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    qv_scope_t *sub = NULL;
    qvb_check(
        qv_split_at(c->scope, QV_HW_OBJ_CORE, 0, &sub), "qv_split_at"
    );
    qvb_check(qv_free(sub), "qv_free");
}

static void
body_create_scope(void *v)
{
    qvb_common_ctx_t *c = (qvb_common_ctx_t *)v;
    qv_scope_t *sub = NULL;
    qvb_check(
        qv_create_scope(c->scope, QV_SCOPE_FLAG_NONE, QV_HW_OBJ_CORE, 1, &sub),
        "qv_create_scope"
    );
    qvb_check(qv_free(sub), "qv_free");
}

void
qvb_run_common(qvb_backend_t *backend, qvb_reporter_t *reporter)
{
    const long iters = qvb_iters();

    // Obtain a root scope from the backend; the shared benchmarks all run
    // against it. This is the sole scope-kind-specific bit in this file.
    qv_scope_t *root = backend->make_root_scope(backend);
    if (!root) {
        qvb_panic("backend->make_root_scope() returned NULL");
    }

    qvb_common_ctx_t ctx = {
        .scope = root,
        .i_out = 0,
        .str_out = NULL,
        .devid_out = NULL,
        .obj_type = QV_HW_OBJ_CORE,
        .devid_type = QV_DEVICE_ID_ORDINAL
    };

    // The shared function coverage table. Adding a public scope function here
    // benchmarks it for process, thread, AND MPI at once.
    struct {
        const char *name;
        qvb_body_fn body;
    } table[] = {
        {"qv_group_rank",   body_group_rank},
        {"qv_group_size",   body_group_size},
        {"qv_hw_obj_count", body_hw_obj_count},
        {"qv_bind_string",  body_bind_string},
        {"qv_device_id",    body_device_id},
        {"qv_barrier",      body_barrier},
        {"qv_split",        body_split},
        {"qv_split_at",     body_split_at},
        {"qv_create_scope", body_create_scope},
    };
    const size_t n = sizeof(table) / sizeof(table[0]);

    for (size_t i = 0; i < n; ++i) {
        qvb_measure(reporter, table[i].name, iters, table[i].body, &ctx);
    }

    // qv_bind_push / qv_bind_pop must run as a matched pair each iteration to
    // keep the binding stack balanced, but are timed separately.
    qvb_measure2(
        reporter,
        "qv_bind_push", body_bind_push,
        "qv_bind_pop", body_bind_pop,
        iters, &ctx
    );

    qvb_check(qv_free(root), "qv_free");
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
