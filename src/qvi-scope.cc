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
#include "qvi-context.h"

// Type definition
struct qv_scope_s {
    /** Points to cached hwloc instance. */
    qvi_hwloc_t *hwloc = nullptr;
    /** Points to cached hwloc topology. */
    hwloc_topology_t topo = nullptr;
    /** Bitmap associated with this scope instance. */
    hwloc_bitmap_t cpuset = nullptr;
};

void
qvi_scope_free(
    qv_scope_t **scope
) {
    qv_scope_t *iscope = *scope;
    if (!iscope) return;
    hwloc_bitmap_free(iscope->cpuset);
    delete iscope;
    *scope = nullptr;
}

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
    iscope->cpuset = hwloc_bitmap_alloc();
    if (!iscope->cpuset) {
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

/**
 * TODO(skg) This should probably live in the daemon/server because it knows
 * about jobs, users, etc.
 */
int
obj_from_intrinsic_scope(
    qv_context_t *ctx,
    qv_scope_intrinsic_t iscope,
    qv_scope_t *scope
) {
    // TODO(skg) Implement the rest.
    switch (iscope) {
        case QV_SCOPE_SYSTEM: {
            hwloc_bitmap_copy(
                scope->cpuset,
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
    // TODO(skg) Should we cache these?
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
 * Bitmaps:
 * Each set bit represents a PU. I think the number of bits set represents the
 * number of PUs present on the system. The least-significant (right-most) bit
 * probably represents logical ID 0.
 */

/*
 * TODO(skg) Is this call collective? Are we going to verify input parameter
 * consistency across processes?
 */
int
qv_scope_split(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int n,
    qv_scope_t **subscope
) {
    char *root_cpus;
    qvi_hwloc_bitmap_asprintf(&root_cpus, scope->cpuset);
    qvi_log_info("Root Scope is {}", root_cpus);

    unsigned npus;
    qvi_hwloc_get_nobjs_in_cpuset(
        ctx->hwloc,
        QV_HW_OBJ_PU,
        scope->cpuset,
        &npus
    );
    qvi_log_info("Number of PUs is {}, split is {}", npus, n);

    int depth;
    qvi_hwloc_obj_type_depth(ctx->hwloc, QV_HW_OBJ_PU, &depth);
    // TODO(skg) We need to deal with corner cases. For example, fewer resources
    // than request, etc.
    const int local_id = qvi_task_lid(ctx->task);
    const int chunk = npus / n;
    hwloc_bitmap_t ncpus = hwloc_bitmap_alloc();
    const int base = chunk * local_id;
    const int extent = base + chunk;
    hwloc_bitmap_zero(ncpus);
    for (int i = base; i < extent; ++i) {
        hwloc_obj_t dobj;
        qvi_hwloc_get_obj_in_cpuset_by_depth(
            ctx->hwloc,
            scope->cpuset,
            depth,
            i,
            &dobj
        );
        char *dobjs;
        qvi_hwloc_bitmap_asprintf(&dobjs, dobj->cpuset);
        qvi_log_info("{} OBJ cpuset at depth {} is {}", local_id, depth, dobjs);
        hwloc_bitmap_or(ncpus, ncpus, dobj->cpuset);
    }
    char *news;
    qvi_hwloc_bitmap_asprintf(&news, ncpus);
    qvi_log_info("{} New cpuset is {}", local_id, news);
    qv_scope_t *isubscope;
    int rc = qvi_scope_new(&isubscope, ctx);
    if (rc != QV_SUCCESS) return rc;
    hwloc_bitmap_copy(isubscope->cpuset, ncpus);
    *subscope = isubscope;
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
        scope->cpuset,
        (unsigned *)n
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
