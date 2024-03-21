/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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

#include "qvi-context.h" // IWYU pragma: keep
#include "qvi-scope.h"

int
qv_version(
    int *major,
    int *minor,
    int *patch
) {
    if (!major || !minor || !patch) {
        return QV_ERR_INVLD_ARG;
    }

    *major = PACKAGE_VERSION_MAJOR;
    *minor = PACKAGE_VERSION_MINOR;
    *patch = PACKAGE_VERSION_PATCH;

    return QV_SUCCESS;
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
qv_bind_string(
    qv_context_t *ctx,
    qv_bind_string_format_t format,
    char **str
) {
    if (!ctx || !str) {
        return QV_ERR_INVLD_ARG;
    }

    *str = nullptr;

    hwloc_cpuset_t cpuset = nullptr;
    int rc = qvi_rmi_task_get_cpubind(
        ctx->rmi,
        qvi_task_task_id(ctx->zgroup->task()),
        &cpuset
    );
    if (rc != QV_SUCCESS) goto out;

    switch (format) {
        case QV_BIND_STRING_AS_BITMAP:
            rc = qvi_hwloc_bitmap_asprintf(str, cpuset);
            break;
        case QV_BIND_STRING_AS_LIST:
            rc = qvi_hwloc_bitmap_list_asprintf(str, cpuset);
            break;
        default:
            rc = QV_ERR_INVLD_ARG;
    }
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
    int *nobjs
) {
    if (!ctx || !scope || !nobjs) {
        return QV_ERR_INVLD_ARG;
    }

    return qvi_scope_nobjs(scope, obj, nobjs);
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

    qv_scope_t *scopei = nullptr;
    int rc = qvi_scope_get(
        ctx->zgroup, ctx->rmi, iscope, &scopei
    );
    if (rc != QV_SUCCESS) {
        qvi_scope_free(&scopei);
    }
    *scope = scopei;
    return rc;
}

int
qv_scope_barrier(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) {
        return QV_ERR_INVLD_ARG;
    }

    return qvi_scope_barrier(scope);
}

// TODO(skg) Add Fortran interface.
int
qv_scope_create(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int nobjs,
    qv_scope_create_hint_t hint,
    qv_scope_t **subscope
) {
    if (!ctx || !scope || (nobjs < 0) || !subscope) {
        return QV_ERR_INVLD_ARG;
    }

    qv_scope_t *isubscope = nullptr;
    int rc = qvi_scope_create(
        scope, type, nobjs, hint, &isubscope
    );
    if (rc != QV_SUCCESS) {
        qvi_scope_free(&isubscope);
    }
    *subscope = isubscope;
    return rc;
}

int
qv_scope_split(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int npieces,
    int color,
    qv_scope_t **subscope
) {
    if (!ctx || !scope || (npieces <= 0) | !subscope) {
        return QV_ERR_INVLD_ARG;
    }

    qv_scope_t *isubscope = nullptr;
    // We use the sentinel value QV_HW_OBJ_LAST to differentiate between calls
    // from split() and split_at(). Since this call doesn't have a hardware type
    // argument, we use QV_HW_OBJ_LAST as the hardware type.
    int rc = qvi_scope_split(
        scope, npieces, color,
        QV_HW_OBJ_LAST, &isubscope
    );
    if (rc != QV_SUCCESS) {
        qvi_scope_free(&isubscope);
    }
    *subscope = isubscope;
    return rc;
}

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

    qv_scope_t *isubscope = nullptr;
    int rc = qvi_scope_split_at(
        scope, type, group_id, &isubscope
    );
    if (rc != QV_SUCCESS) {
        qvi_scope_free(&isubscope);
    }
    *subscope = isubscope;
    return rc;
}

int
qv_scope_get_device_id(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int dev_index,
    qv_device_id_type_t id_type,
    char **dev_id
) {
    if (!ctx || !scope || (dev_index < 0) || !dev_id) {
        return QV_ERR_INVLD_ARG;
    }

    return qvi_scope_get_device_id(
        scope, dev_obj, dev_index, id_type, dev_id
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
