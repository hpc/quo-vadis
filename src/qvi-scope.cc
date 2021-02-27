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
#include "qvi-hwloc.h"

// Type definition
struct qv_scope_s {
    /** Points to already initialized hwloc instance. */
    qvi_hwloc_t *hwloc = nullptr;
};

/**
 *
 */
void
qvi_scope_free(
    qv_scope_t **scope
) {
    qv_scope_t *iscope = *scope;
    if (!iscope) return;
    delete iscope;
    *scope = nullptr;
}

/**
 *
 */
int
qvi_scope_new(
    qv_scope_t **scope,
    qv_context_t *ctx
) {
    char const *ers = nullptr;
    int rc = QV_SUCCESS;

    qv_scope_t *iscope = qvi_new qv_scope_t;
    if (!scope) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }
    iscope->hwloc = ctx->hwloc;
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_scope_free(&iscope);
    }
    *scope = iscope;
    return rc;
}

int
qv_scope_get(
    qv_context_t *ctx,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    if (!ctx || !scope) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    qv_scope_t *qvs;
    // TODO(skg) Should we cache these?
    int rc = qvi_scope_new(&qvs, ctx);
    if (rc != QV_SUCCESS) {
        ers = "qvi_scope_new() failed";
        return rc;
        //goto out;
    }

    hwloc_obj_t root = hwloc_get_root_obj(qvi_hwloc_topo_get(qvs->hwloc));
    char *root_cpus;
    qvi_hwloc_bitmap_asprintf(&root_cpus, root->cpuset);
    qvi_log_info("-->{}", root_cpus);

    hwloc_obj_t obj = hwloc_get_obj_inside_cpuset_by_type(
        qvi_hwloc_topo_get(qvs->hwloc),
        root->cpuset,
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


out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_scope_free(&qvs);
    }
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
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
