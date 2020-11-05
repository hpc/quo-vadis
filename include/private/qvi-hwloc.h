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
 * @file qvi-hwloc.h
 */

#ifndef QVI_HWLOC_H
#define QVI_HWLOC_H

#include <unistd.h>

#include "hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qvi_hwloc_s;
typedef struct qvi_hwloc_s qvi_hwloc_t;

// Convenience typedefs
struct qvi_hwloc_bitmap_s;
typedef struct qvi_hwloc_bitmap_s * qvi_hwloc_bitmap_t;

struct qvi_hwloc_topology_s;
typedef struct qvi_hwloc_topology_s * qvi_hwloc_topology_t;

/**
 *
 */
int
qvi_hwloc_construct(
    qvi_hwloc_t **hwl
);

/**
 *
 */
void
qvi_hwloc_destruct(
    qvi_hwloc_t *hwl
);

/**
 *
 */
int
qvi_hwloc_init(
    qvi_hwloc_t *hwl
);

/**
 *
 */
int
qvi_hwloc_topo_load(
    qvi_hwloc_t *hwl
);

/**
 *
 */
int
qvi_hwloc_topology_dup(
    qvi_hwloc_topology_t *newtopology,
    qvi_hwloc_topology_t oldtopology
);

/**
 *
 */
int
qvi_hwloc_task_get_cpubind(
    qvi_hwloc_t *hwl,
    pid_t who,
    qvi_hwloc_bitmap_t *out_bitmap
);

/**
 *
 */
qvi_hwloc_bitmap_t
qvi_hwloc_bitmap_alloc(void);

/**
 *
 */
int
qvi_hwloc_bitmap_free(
    qvi_hwloc_bitmap_t bitmap
);

/**
 *
 */
int
qvi_hwloc_bitmap_snprintf(
    char *buf,
    size_t buflen,
    qvi_hwloc_bitmap_t bitmap,
    int *result
);

/**
 *
 */
int
qvi_hwloc_bitmap_asprintf(
    qvi_hwloc_bitmap_t bitmap,
    char **result
);

/**
 *
 */
int
qvi_hwloc_bitmap_sscanf(
    qvi_hwloc_bitmap_t bitmap,
    const char *string
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
