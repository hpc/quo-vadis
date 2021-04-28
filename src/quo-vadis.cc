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
 * @file quo-vadis.cc
 */

#include "qvi-common.h"
#include "qvi-context.h"

int
qv_create(
    qv_context_t **ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    qv_context_t *ictx = qvi_new qv_context_t;
    if (!ictx) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_task_new(&ictx->task);
    if (rc != QV_SUCCESS) {
        ers = "qvi_task_new() failed";
        goto out;
    }

    rc = qvi_rmi_client_new(&ictx->rmi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_client_new() failed";
        goto out;
    }

    rc = qvi_bind_stack_new(&ictx->bind_stack);
    if (rc != QV_SUCCESS) {
        ers = "qvi_bind_stack_new() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        (void)qv_free(ictx);
        ictx = nullptr;
    }
    *ctx = ictx;
    return rc;
}

int
qv_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    qvi_task_free(&ctx->task);
    qvi_bind_stack_free(&ctx->bind_stack);
    qvi_rmi_client_free(&ctx->rmi);
    delete ctx;

    return QV_SUCCESS;
}

int
qv_bind_push(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) return QV_ERR_INVLD_ARG;

    return qvi_bind_push(
        ctx->bind_stack,
        qvi_scope_bitmap_get(scope)
    );
}

int
qv_bind_pop(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    return qvi_bind_pop(ctx->bind_stack);
}

int
qv_bind_get_as_string(
    qv_context_t *ctx,
    char **bitmaps
) {
    if (!ctx || !bitmaps) return QV_ERR_INVLD_ARG;

    return qvi_hwloc_task_get_cpubind_as_string(
        ctx->hwloc,
        qvi_task_pid(ctx->task),
        bitmaps
    );
}

// TODO(skg) Add support for other, non-MPI barriers.
int
qv_barrier(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    return qvi_mpi_node_barrier(ctx->mpi);
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
        qvi_scope_bitmap_get(scope),
        (unsigned *)n
    );
}

int
qv_scope_get(
    qv_context_t *ctx,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    if (!ctx || !scope) return QV_ERR_INVLD_ARG;

    qv_scope_t *qvs;
    int rc = qvi_scope_new(&qvs);
    if (rc != QV_SUCCESS) return rc;

    hwloc_bitmap_t cpuset;
    rc = qvi_hwloc_bitmap_alloc(&cpuset);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_rmi_scope_get_intrinsic_scope_cpuset(
        ctx->rmi,
        iscope,
        cpuset
    );
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_scope_bitmap_set(qvs, cpuset);
    if (rc != QV_SUCCESS) goto out;
out:
    hwloc_bitmap_free(cpuset);
    if (rc != QV_SUCCESS) qvi_scope_free(&qvs);
    *scope = qvs;
    return rc;
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
        qvi_scope_bitmap_get(scope),
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
    hwloc_bitmap_t bitm;
    qvrc = qvi_hwloc_bitmap_alloc(&bitm);
    if (qvrc != QV_SUCCESS) return qvrc;

    hwloc_bitmap_zero(bitm);
    for (int i = base; i < extent; ++i) {
        hwloc_obj_t dobj;
        qvrc = qvi_hwloc_get_obj_in_cpuset_by_depth(
            ctx->hwloc,
            qvi_scope_bitmap_get(scope),
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
    qvrc = qvi_scope_new(&isubscope);
    if (qvrc != QV_SUCCESS) goto out;

    rc = hwloc_bitmap_copy(
        qvi_scope_bitmap_get(isubscope),
        bitm
    );
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

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
