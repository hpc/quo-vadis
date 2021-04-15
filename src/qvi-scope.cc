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
 * @file qvi-scope.cc
 */

#include "qvi-common.h"
#include "qvi-scope.h"
#include "qvi-context.h"
#include "qvi-bind.h"

// Type definition
struct qv_scope_s {
    /** The group structure. */
    // TODO(skg) This will need work: needs to support more than just MPI.
    qvi_mpi_group_t *group = nullptr;
    /** Points to cached hwloc instance. */
    qvi_hwloc_t *hwloc = nullptr;
    /** Points to cached hwloc topology. */
    hwloc_topology_t topo = nullptr;
    /** Bitmap associated with this scope instance. */
    hwloc_bitmap_t bitmap = nullptr;
};

int
qvi_scope_new(
    qv_scope_t **scope,
    qv_context_t *ctx
) {
    int rc = QV_SUCCESS;

    qv_scope_t *iscope = qvi_new qv_scope_t;
    if (!scope) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_mpi_group_new(&iscope->group);
    if (rc != QV_SUCCESS) goto out;

    iscope->bitmap = hwloc_bitmap_alloc();
    if (!iscope->bitmap) {
        rc = QV_ERR_OOR;
        goto out;
    }
    iscope->hwloc = ctx->hwloc;
    iscope->topo = ctx->topo;
out:
    if (rc != QV_SUCCESS) {
        qvi_scope_free(&iscope);
    }
    *scope = iscope;
    return rc;
}

void
qvi_scope_free(
    qv_scope_t **scope
) {
    qv_scope_t *iscope = *scope;
    if (!iscope) return;
    hwloc_bitmap_free(iscope->bitmap);
    qvi_mpi_group_free(&iscope->group);
    delete iscope;
    *scope = nullptr;
}

hwloc_bitmap_t
qvi_scope_bitmap_get(
    qv_scope_t *scope
) {
    return scope->bitmap;
}

int
qvi_scope_bitmap_set(
    qv_scope_t *scope,
    hwloc_const_bitmap_t bitmap
) {
    int rc = QV_SUCCESS;

    if (hwloc_bitmap_copy(scope->bitmap, bitmap) != 0) {
        rc = QV_ERR_HWLOC;
    }
    return rc;
}

int
qv_scope_free(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) return QV_ERR_INVLD_ARG;

    qvi_scope_free(&scope);

    return QV_SUCCESS;
}

int
qv_scope_nobjs(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t obj,
    int *n
) {
    if (!ctx || !scope || !n) return QV_ERR_INVLD_ARG;

    return qvi_hwloc_get_nobjs_in_cpuset(
        ctx->hwloc,
        obj,
        scope->bitmap,
        (unsigned *)n
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
