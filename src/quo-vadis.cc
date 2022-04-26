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
#include "qvi-utils.h"

int
qvi_context_create(
    qv_context_t **ctx
) {
    if (!ctx) {
        return QV_ERR_INVLD_ARG;
    }

    int rc = QV_SUCCESS;

    qv_context_t *ictx = qvi_new qv_context_t();
    if (!ictx) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_rmi_client_new(&ictx->rmi);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    rc = qvi_bind_stack_new(&ictx->bind_stack);
out:
    if (rc != QV_SUCCESS) {
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
qvi_context_connect_to_server(
    qv_context_t *ctx
) {
    char *url = nullptr;
    int rc = qvi_url(&url);
    if (rc != QV_SUCCESS) {
        qvi_log_error("{}", qvi_conn_ers());
        return rc;
    }

    rc = qvi_rmi_client_connect(ctx->rmi, url);
    if (url) free(url);
    return rc;
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

    hwloc_cpuset_t cpuset = nullptr;
    int rc = qvi_rmi_task_get_cpubind(
        ctx->rmi,
        qvi_task_pid(ctx->zgroup->task()),
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
    int rc = qvi_scope_split(
        scope, npieces, color, &isubscope
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
