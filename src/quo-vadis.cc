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
        qvi_context_free(ictx);
        ictx = nullptr;
    }
    *ctx = ictx;
    return rc;
}

void
qvi_context_free(
    qv_context_t *ctx
) {
    if (!ctx) return;

    qvi_task_free(&ctx->task);
    qvi_bind_stack_free(&ctx->bind_stack);
    qvi_rmi_client_free(&ctx->rmi);
    delete ctx;
}

int
qv_bind_push(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) return QV_ERR_INVLD_ARG;

    return qvi_bind_push(
        ctx->bind_stack,
        qvi_scope_cpuset_get(scope)
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
    char **str
) {
    if (!ctx || !str) return QV_ERR_INVLD_ARG;

    hwloc_cpuset_t cpuset = nullptr;

    int rc = qvi_rmi_task_get_cpubind(
        ctx->rmi,
        qvi_task_pid(ctx->task),
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
    if (!ctx) return QV_ERR_INVLD_ARG;

    return ctx->taskman->barrier();
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

    int nu = 0;
    // TODO(skg) We should update how we do this.
    int rc = qvi_rmi_get_nobjs_in_cpuset(
        ctx->rmi,
        obj,
        qvi_scope_cpuset_get(scope),
        &nu
    );
    *n = nu;
    return rc;
}

int
qv_scope_taskid(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int *taskid
) {
    if (!ctx || !scope || !taskid) return QV_ERR_INVLD_ARG;

    return qvi_scope_taskid(scope, taskid);
}

int
qv_scope_ntasks(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int *ntasks
) {
    if (!ctx || !scope || !ntasks) return QV_ERR_INVLD_ARG;

    return qvi_scope_ntasks(scope, ntasks);
}

int
qv_scope_get(
    qv_context_t *ctx,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    if (!ctx || !scope) return QV_ERR_INVLD_ARG;
    *scope = nullptr;

    qv_scope_t *qvs = nullptr;
    qvi_hwpool_t *hwpool = nullptr;

    int rc = qvi_scope_new(&qvs);
    if (rc != QV_SUCCESS) return rc;

    qvi_group_t *group;
    rc = ctx->taskman->group_create_from_intrinsic_scope(&group, iscope);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_rmi_scope_get_intrinsic_scope_hwpool(
        ctx->rmi,
        qvi_task_pid(ctx->task),
        iscope,
        &hwpool
    );
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_scope_init(qvs, group, hwpool);
    if (rc != QV_SUCCESS) goto out;
out:
    if (rc != QV_SUCCESS) qvi_scope_free(&qvs);
    *scope = qvs;
    return rc;
}

int
qv_scope_barrier(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) return QV_ERR_INVLD_ARG;

    return qvi_scope_barrier(scope);
}

static int
create_new_subgroup_by_color(
    qv_context_t *ctx,
    qvi_group_t *pargroup,
    int color,
    qvi_group_t **subgroup
) {
    const int split_key = qvi_group_id(pargroup);

    return ctx->taskman->group_create_from_split(
        pargroup,
        color,
        split_key,
        subgroup
    );
}

// TODO(skg) Make sure to document all the paths that may result in an error.
// Take a look at the RPC code to get a sense of where all the paths occur.
int
qv_scope_split(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int n,
    int group_id,
    qv_scope_t **subscope
) {
    // TODO(skg) Implement
    static const cstr epref = "qv_scope_split Error: ";

    int rc = QV_SUCCESS;
    qvi_hwpool_t *hwpool = nullptr;
    qv_scope_t *isubscope = nullptr;
    qvi_group_t *subgroup = nullptr;

    if (!ctx || !scope || !subscope) {
        rc = QV_ERR_INVLD_ARG;
        goto out;
    }
    if (n <= 0 ) {
        qvi_log_error("{} n <= 0 (n = {})", epref, n);
        rc = QV_ERR_INVLD_ARG;
        goto out;
    }
    // TODO(skg) This will have to change because
    // our grouping algorithms will be in this space.
    if (group_id < 0) {
        qvi_log_error("{} group_id < 0 (group_id = {})", epref, group_id);
        rc = QV_ERR_INVLD_ARG;
        goto out;
    }

    rc = qvi_rmi_split_hwpool_by_group(
        ctx->rmi,
        qvi_scope_hwpool_get(scope),
        n,
        group_id,
        &hwpool
    );
    if (rc != QV_SUCCESS) goto out;
    // Create new sub-scope.
    rc = qvi_scope_new(&isubscope);
    if (rc != QV_SUCCESS) goto out;
    // Create new sub-group.
    rc = create_new_subgroup_by_color(
        ctx,
        qvi_scope_group_get(scope),
        group_id,
        &subgroup
    );
    if (rc != QV_SUCCESS) goto out;
    // Initialize new sub-scope.
    rc = qvi_scope_init(isubscope, subgroup, hwpool);
    if (rc != QV_SUCCESS) goto out;
out:
    if (rc != QV_SUCCESS) {
        // Don't explicitly free subgroup here. Hope
        // that qvi_group_free() will do the job for us.
        qvi_scope_free(&isubscope);
    }
    *subscope = isubscope;
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
}

int
qv_scope_get_device(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int i,
    qv_device_id_type_t dev_id_type,
    char **dev_id
) {
    if (!ctx || !scope || i < 0 || !dev_id) return QV_ERR_INVLD_ARG;

    // TODO(skg) Update how we do this.
    return qvi_rmi_get_device_in_cpuset(
        ctx->rmi,
        dev_obj,
        i,
        qvi_scope_cpuset_get(scope),
        dev_id_type,
        dev_id
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
