/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file hw-loc.h
 */

#ifndef QUO_VADIS_HWLOC_H
#define QUO_VADIS_HWLOC_H

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qv_hwloc_s;
typedef struct qv_hwloc_s qv_hwloc_t;
// Convenience typedefs
typedef void * qv_hwloc_bitmap_t;

/**
 *
 */
int
qv_hwloc_construct(
    qv_hwloc_t **hwl
);

/**
 *
 */
void
qv_hwloc_destruct(
    qv_hwloc_t *hwl
);

/**
 *
 */
int
qv_hwloc_init(
    qv_hwloc_t *hwl
);

/**
 *
 */
int
qv_hwloc_topo_load(
    qv_hwloc_t *hwl
);

/**
 *
 */
int
qv_hwloc_task_get_cpubind(
    qv_hwloc_t *hwl,
    pid_t who,
    qv_hwloc_bitmap_t *out_bitmap
);

/**
 *
 */
qv_hwloc_bitmap_t
qv_hwloc_bitmap_alloc(void);

/**
 *
 */
int
qv_hwloc_bitmap_free(
    qv_hwloc_bitmap_t bitmap
);

/**
 *
 */
int
qv_hwloc_bitmap_asprintf(
    qv_hwloc_bitmap_t bitmap,
    char **result
);

/**
 *
 */
int
qv_hwloc_bitmap_sscanf(
    qv_hwloc_bitmap_t bitmap,
    const char *string
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
