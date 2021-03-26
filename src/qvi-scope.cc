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
    /** Hardware object associated with a scope instance. */
    hwloc_obj_t obj = nullptr;
};

void
qvi_scope_free(
    qv_scope_t **scope
) {
    qv_scope_t *iscope = *scope;
    if (!iscope) return;
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
        case QV_SCOPE_SYSTEM:
            scope->obj = hwloc_get_root_obj(ctx->topo);
            break;
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

#if 0
int
test(void) {
    char *root_cpus;
    qvi_hwloc_bitmap_asprintf(&root_cpus, qvs->obj->cpuset);
    qvi_log_info("-->{}", root_cpus);

    hwloc_obj_t obj = hwloc_get_obj_inside_cpuset_by_type(
        qvs->topo,
        qvs->obj->cpuset,
        HWLOC_OBJ_PACKAGE,
        3
    );

    char *objcpus;
    qvi_hwloc_bitmap_asprintf(&objcpus, obj->cpuset);
    qvi_log_info("-->{}", objcpus);

    hwloc_obj_t obj2 = hwloc_get_first_largest_obj_inside_cpuset(
        qvi_hwloc_topo_get(qvs->hwloc),
        obj->cpuset
    );

    char *obj2cpus;
    qvi_hwloc_bitmap_asprintf(&obj2cpus, obj2->cpuset);
    qvi_log_info("-->{} {}", obj2cpus, obj2->logical_index);
}
#endif

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
 *
 * Operations Needed:
 * Counting bits
 * Combining n-bits
 */

int
qv_scope_split(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int n,
    qv_scope_t **subscope
) {
    char *root_cpus;
    qvi_hwloc_bitmap_asprintf(&root_cpus, scope->obj->cpuset);
    qvi_log_info("Root Scope is {}", root_cpus);
    unsigned long ulongs[4];
    hwloc_bitmap_to_ulongs(scope->obj->cpuset, 4, ulongs);
    for (int i = 0; i < 4; ++i) {
        qvi_log_info("{} ulong {}", i, ulongs[i]);
    }

    unsigned npus = -1;
    qvi_hwloc_calc_nobjs_in_cpuset(
        ctx->hwloc,
        QV_HW_OBJ_PU,
        scope->obj->cpuset,
        &npus
    );
    qvi_log_info("Number of PUs is {}", npus);
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
