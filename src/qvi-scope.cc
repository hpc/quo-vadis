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
 * @file qvi-scope.cc
 */

#include "qvi-common.h"

#include "qvi-scope.h"
#include "qvi-hwpool.h"

// Type definition
struct qv_scope_s {
    /** Task group associated with this scope instance. */
    qvi_group_t *group = nullptr;
    /** Hardware resource pool. */
    qvi_hwpool_t *hwrespool = nullptr;
};

int
qvi_scope_new(
    qv_scope_t **scope
) {
    int rc = QV_SUCCESS;

    qv_scope_t *iscope = qvi_new qv_scope_t;
    if (!scope) rc = QV_ERR_OOR;
    // hwrespool and group will be initialized in qvi_scope_init().
    if (rc != QV_SUCCESS) qvi_scope_free(&iscope);
    *scope = iscope;
    return rc;
}

void
qvi_scope_free(
    qv_scope_t **scope
) {
    if (!scope) return;
    qv_scope_t *iscope = *scope;
    if (!iscope) goto out;
    qvi_group_free(&iscope->group);
    qvi_hwpool_free(&iscope->hwrespool);
    delete iscope;
out:
    *scope = nullptr;
}

int
qvi_scope_init(
    qv_scope_t *scope,
    qvi_group_t *group,
    qvi_hwpool_t *hwrespool
) {
    scope->group = group;
    scope->hwrespool = hwrespool;
    return QV_SUCCESS;
}

hwloc_const_cpuset_t
qvi_scope_cpuset_get(
    qv_scope_t *scope
) {
    return qvi_hwpool_cpuset_get(scope->hwrespool);
}

const qvi_hwpool_t *
qvi_scope_hwpool_get(
    qv_scope_t *scope
) {
    return scope->hwrespool;
}

qvi_group_t *
qvi_scope_group_get(
    qv_scope_t *scope
) {
    return scope->group;
}

int
qvi_scope_taskid(
    qv_scope_t *scope,
    int *taskid
) {
    *taskid = scope->group->id();
    return QV_SUCCESS;
}

int
qvi_scope_ntasks(
    qv_scope_t *scope,
    int *ntasks
) {
    *ntasks = scope->group->size();
    return QV_SUCCESS;
}

int
qvi_scope_barrier(
    qv_scope_t *scope
) {
    return scope->group->barrier();
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
