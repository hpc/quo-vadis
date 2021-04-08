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
    delete iscope;
    *scope = nullptr;
}

hwloc_bitmap_t
qvi_scope_bitmap_get(
    qv_scope_t *scope
) {
    return scope->bitmap;
}

/**
 * TODO(skg) This should probably live in the daemon/server because it knows
 * about jobs, users, etc.
 */
static int
obj_from_intrinsic_scope(
    qv_context_t *ctx,
    qv_scope_intrinsic_t iscope,
    qv_scope_t *scope
) {
    // TODO(skg) Implement the rest.
    switch (iscope) {
        case QV_SCOPE_SYSTEM: {
            hwloc_bitmap_copy(
                scope->bitmap,
                hwloc_get_root_obj(ctx->topo)->cpuset
            );
            // TODO(skg) Needs work.
            //scope->obj = hwloc_get_root_obj(ctx->topo);
            //scope->obj = hwloc_get_obj_by_type(ctx->topo, HWLOC_OBJ_NUMANODE, 0);
            break;
        }
        case QV_SCOPE_EMPTY:
            return QV_ERR_INVLD_ARG;
        case QV_SCOPE_USER:
        case QV_SCOPE_JOB:
        case QV_SCOPE_PROCESS:
            return QV_ERR_INTERNAL;
        default:
            return QV_ERR_INVLD_ARG;
    }
    return QV_SUCCESS;
}

int
qv_scope_get(
    qv_context_t *ctx,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    if (!ctx || !scope) return QV_ERR_INVLD_ARG;

    qv_scope_t *qvs;
    int rc = qvi_scope_new(&qvs, ctx);
    if (rc != QV_SUCCESS) return rc;

    rc = obj_from_intrinsic_scope(ctx, iscope, qvs);
    if (rc != QV_SUCCESS) goto out;
out:
    if (rc != QV_SUCCESS) qvi_scope_free(&qvs);
    *scope = qvs;
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

/*
 * TODO(skg) Is this call collective? Are we going to verify input parameter
 * consistency across processes?
 *
 * TODO(skg) This should also be in the server code and retrieved via RPC?
 */
int
qv_scope_split(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int n,
    int group_id,
    qv_scope_t **subscope
) {
    static const cstr epref = "qv_scope_split Error: ";

    if (!ctx || !scope || !subscope) {
        return QV_ERR_INVLD_ARG;
    }
    if (n <= 0 ) {
        qvi_log_error("{} n <= 0 (n = {})", epref, n);
        return QV_ERR_INVLD_ARG;
    }
    if (group_id < 0) {
        qvi_log_error("{} group_id < 0 (group_id = {})", epref, group_id);
        return QV_ERR_INVLD_ARG;
    }
    unsigned npus;
    int qvrc = qvi_hwloc_get_nobjs_in_cpuset(
        ctx->hwloc,
        QV_HW_OBJ_PU,
        scope->bitmap,
        &npus
    );
    if (qvrc != QV_SUCCESS) return qvrc;
    // We use PUs to split resources.  Each set bit represents a PU. The number
    // of bits set represents the number of PUs present on the system. The
    // least-significant (right-most) bit represents logical ID 0.
    int pu_depth;
    qvrc = qvi_hwloc_obj_type_depth(
        ctx->hwloc,
        QV_HW_OBJ_PU,
        &pu_depth
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    int rc;
    const int chunk = npus / n;
    // This happens when n > npus. We can't support that split.
    if (chunk == 0) {
        qvi_log_error("{} n > npus ({} > {})", epref, n, npus);
        return QV_ERR_SPLIT;
    }
    // Group IDs must be < n: 0, 1, ... , n-1.
    if (group_id >= n) {
        qvi_log_error("{} group_id >= n ({} >= {})", epref, group_id, n);
        return QV_ERR_SPLIT;
    }
    // Calculate base and extent of split.
    const int base = chunk * group_id;
    const int extent = base + chunk;
    // Allocate and zero-out a new bitmap that will encode the split.
    hwloc_bitmap_t bitm = hwloc_bitmap_alloc();
    if (!bitm) return QV_ERR_OOR;
    hwloc_bitmap_zero(bitm);
    for (int i = base; i < extent; ++i) {
        hwloc_obj_t dobj;
        qvrc = qvi_hwloc_get_obj_in_cpuset_by_depth(
            ctx->hwloc,
            scope->bitmap,
            pu_depth,
            i,
            &dobj
        );
        if (qvrc != QV_SUCCESS) goto out;

        rc = hwloc_bitmap_or(bitm, bitm, dobj->cpuset);
        if (rc != 0) {
            qvrc = QV_ERR_HWLOC;
            goto out;
        }
    }
    // Create new sub-scope.
    qv_scope_t *isubscope;
    qvrc = qvi_scope_new(&isubscope, ctx);
    if (qvrc != QV_SUCCESS) goto out;

    rc = hwloc_bitmap_copy(isubscope->bitmap, bitm);
    if (rc != 0) {
        qvrc = QV_ERR_HWLOC;
        goto out;
    }
out:
    hwloc_bitmap_free(bitm);
    if (qvrc != QV_SUCCESS) {
        qvi_scope_free(&isubscope);
    }
    *subscope = isubscope;
    return qvrc;
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
