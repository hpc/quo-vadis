/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
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
#include "qvi-scope.h"

int
qvi_context_create(
    qv_context_t **ctx
) {
    if (!ctx) {
        return QV_ERR_INVLD_ARG;
    }

    int rc = QV_SUCCESS;
    cstr_t ers = nullptr;

    qv_context_t *ictx = qvi_new qv_context_t;
    if (!ictx) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
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
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_context_free(&ictx);
    }
    *ctx = ictx;
    return rc;
}

void
qvi_context_free(
    qv_context_t **ctx
) {
    if (!ctx) return;
    qv_context_t *ictx = *ctx;
    if (!ictx) goto out;
    qvi_bind_stack_free(&ictx->bind_stack);
    qvi_rmi_client_free(&ictx->rmi);
    delete ictx;
out:
    *ctx = nullptr;
}

int
qv_bind_push(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) {
        return QV_ERR_INVLD_ARG;
    }

    return qvi_bind_push(
        ctx->bind_stack, qvi_scope_cpuset_get(scope)
    );
}

int
qv_bind_pop(
    qv_context_t *ctx
) {
    if (!ctx) {
        return QV_ERR_INVLD_ARG;
    }

    return qvi_bind_pop(ctx->bind_stack);
}

int
qv_bind_get_as_string(
    qv_context_t *ctx,
    char **str
) {
    if (!ctx || !str) {
        return QV_ERR_INVLD_ARG;
    }

    hwloc_cpuset_t cpuset = nullptr;
    int rc = qvi_rmi_task_get_cpubind(
        ctx->rmi,
        qvi_task_pid(ctx->zgroup->task()),
        &cpuset
    );
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwloc_bitmap_asprintf(str, cpuset);
out:
    hwloc_bitmap_free(cpuset);
    return rc;
}

int
qv_context_barrier(
    qv_context_t *ctx
) {
    if (!ctx) {
        return QV_ERR_INVLD_ARG;
    }

    return ctx->zgroup->barrier();
}

int
qv_scope_free(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) {
        return QV_ERR_INVLD_ARG;
    }

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
    if (!ctx || !scope || !n) {
        return QV_ERR_INVLD_ARG;
    }

    return qvi_scope_nobjs(scope, obj, n);
}

int
qv_scope_taskid(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int *taskid
) {
    if (!ctx || !scope || !taskid) {
        return QV_ERR_INVLD_ARG;
    }

    return qvi_scope_taskid(scope, taskid);
}

int
qv_scope_ntasks(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int *ntasks
) {
    if (!ctx || !scope || !ntasks) {
        return QV_ERR_INVLD_ARG;
    }

    return qvi_scope_ntasks(scope, ntasks);
}

int
qv_scope_get(
    qv_context_t *ctx,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    if (!ctx || !scope) {
        return QV_ERR_INVLD_ARG;
    }

    return qvi_scope_get(
        ctx->zgroup, ctx->rmi, iscope, scope
    );
}

int
qv_scope_barrier(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) return QV_ERR_INVLD_ARG;

    return qvi_scope_barrier(scope);
}

// TODO(skg) Make sure to document all the paths that may result in an error.
// Take a look at the RPC code to get a sense of where all the paths occur.
// TODO(skg) We also need to harden the RPC code so that failures don't result
// int hangs.
int
qv_scope_split(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int npieces,
    int color,
    qv_scope_t **subscope
) {
    if (!ctx || !scope || !subscope) {
        return QV_ERR_INVLD_ARG;
    }

    static const cstr_t epref = "qv_scope_split Error: ";
    int rc = QV_SUCCESS;

    if (npieces <= 0 ) {
        qvi_log_error("{} n <= 0 (n = {})", epref, npieces);
        rc = QV_ERR_INVLD_ARG;
        goto out;
    }
    // TODO(skg) This will have to change because
    // our grouping algorithms will be in this space.
    if (color < 0) {
        qvi_log_error("{} group_id < 0 (group_id = {})", epref, color);
        rc = QV_ERR_INVLD_ARG;
        goto out;
    }

    rc = qvi_scope_split(scope, npieces, color, subscope);
out:
    if (rc != QV_SUCCESS) {
        qvi_scope_free(subscope);
    }
    return rc;
}

/*
 * TODO(skg) This should also be in the server code and retrieved via RPC?
 * TODO(skg) This needs to be fixed. Lots of work needed.
 */
int
qv_scope_split_at(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int group_id,
    qv_scope_t **subscope
) {
    if (!ctx || !scope || !subscope) {
        return QV_ERR_INVLD_ARG;
    }
#if 0

    int ntype;
    // TODO(skg) Update how we do this.
    int qvrc = qvi_hwloc_get_nobjs_in_cpuset(
        ctx->hwloc,
        type,
        qvi_scope_cpuset_get(scope),
        &ntype
    );
    if (qvrc != QV_SUCCESS) return qvrc;

    return qv_scope_split(ctx, scope, ntype, group_id, subscope);
#endif
    QVI_UNUSED(type); QVI_UNUSED(group_id);
    return QV_ERR_NOT_SUPPORTED;
}

int
qv_scope_get_device(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int i,
    qv_device_id_type_t id_type,
    char **dev_id
) {
    if (!ctx || !scope || i < 0 || !dev_id) {
        return QV_ERR_INVLD_ARG;
    }

    return qvi_scope_get_device(
        scope, dev_obj, i,
        id_type, dev_id
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
