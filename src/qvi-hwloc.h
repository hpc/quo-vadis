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
 * @file qvi-hwloc.h
 */

#ifndef QVI_HWLOC_H
#define QVI_HWLOC_H

#include "qvi-common.h"
#include "hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qvi_hwloc_s;
typedef struct qvi_hwloc_s qvi_hwloc_t;

/**
 *
 */
int
qvi_hwloc_new(
    qvi_hwloc_t **hwl
);

/**
 *
 */
void
qvi_hwloc_free(
    qvi_hwloc_t **hwl
);

/**
 *
 */
int
qvi_hwloc_topology_load(
    qvi_hwloc_t *hwl
);

/**
 *
 */
hwloc_topology_t
qvi_hwloc_topo_get(
    qvi_hwloc_t *hwl
);

/**
 *
 */
int
qvi_hwloc_get_nobjs_by_type(
   qvi_hwloc_t *hwloc,
   qv_hwloc_obj_type_t target_type,
   int *out_nobjs
);

/**
 *
 */
int
qvi_hwloc_bitmap_asprintf(
    char **result,
    hwloc_const_bitmap_t bitmap
);

/**
 *
 */
int
qvi_hwloc_task_get_cpubind(
    qvi_hwloc_t *hwl,
    pid_t who,
    hwloc_bitmap_t *out_bitmap
);

/**
 *
 */
int
qvi_hwloc_task_intersects_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hwloc_obj_type_t type,
    pid_t who,
    unsigned type_index,
    int *result
);

/**
 *
 */
int
qvi_hwloc_task_isincluded_in_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hwloc_obj_type_t type,
    pid_t who,
    unsigned type_index,
    int *result
);

/**
 *
 */
int
qvi_hwloc_topology_export(
    qvi_hwloc_t *hwl,
    const char *base_path,
    char **path
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
