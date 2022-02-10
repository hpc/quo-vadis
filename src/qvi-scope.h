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

#include "qvi-zgroup.h"
#include "qvi-rmi.h"

#include "hwloc.h"

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
qvi_scope_get(
    qvi_zgroup_t *zgroup,
    qvi_rmi_client_t *rmi,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
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

/**
 *
 */
int
qvi_scope_split(
    qv_scope_t *parent,
    int ncolors,
    int color,
    qv_scope_t **child
);

/**
 *
 */
int
qvi_scope_nobjs(
    qv_scope_t *scope,
    qv_hw_obj_type_t obj,
    int *n
);

int
qvi_scope_get_device(
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int i,
    qv_device_id_type_t id_type,
    char **dev_id
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
