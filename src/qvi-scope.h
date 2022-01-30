/*
 * Copyright (c) 2021-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-scope.h
 */

#ifndef QVI_SCOPE_H
#define QVI_SCOPE_H

#include "qvi-common.h"
#include "qvi-group.h"
#include "qvi-hwpool.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
int
qvi_scope_new(
    qv_scope_t **scope
);

/**
 *
 */
void
qvi_scope_free(
    qv_scope_t **scope
);

/**
 *
 */
int
qvi_scope_init(
    qv_scope_t *scope,
    qvi_group_t *group,
    qvi_hwpool_t *hwrespool
);

/**
 *
 */
hwloc_const_cpuset_t
qvi_scope_cpuset_get(
    qv_scope_t *scope
);

/**
 *
 */
const qvi_hwpool_t *
qvi_scope_hwpool_get(
    qv_scope_t *scope
);

/**
 *
 */
qvi_group_t *
qvi_scope_group_get(
    qv_scope_t *scope
);

/**
 *
 */
int
qvi_scope_taskid(
    qv_scope_t *scope,
    int *taskid
);

/**
 *
 */
int
qvi_scope_ntasks(
    qv_scope_t *scope,
    int *ntasks
);

/**
 *
 */
int
qvi_scope_barrier(
    qv_scope_t *scope
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
