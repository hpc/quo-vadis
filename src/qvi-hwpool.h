/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwpool.h
 *
 * Hardware Resource Pool
 */

#ifndef QVI_HWPOOL_H
#define QVI_HWPOOL_H

#include "qvi-common.h"
#include "qvi-line.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qvi_hwpool_devinfo_s;
typedef struct qvi_hwpool_devinfo_s qvi_hwpool_devinfo_t;

struct qvi_hwpool_s;
typedef struct qvi_hwpool_s qvi_hwpool_t;

/**
 *
 */
int
qvi_hwpool_new(
    qvi_hwpool_t **rpool
);

/**
 *
 */
int
qvi_hwpool_new_from_line(
    qvi_line_hwpool_t *line,
    qvi_hwpool_t **rpool
);

/**
 *
 */
int
qvi_hwpool_new_line_from_hwpool(
    const qvi_hwpool_t *rpool,
    qvi_line_hwpool_t **line
);

/**
 *
 */
int
qvi_hwpool_init(
    qvi_hwpool_t *rpool,
    hwloc_const_bitmap_t cpuset
);

/**
 *
 */
void
qvi_hwpool_free(
    qvi_hwpool_t **rpool
);

/**
 *
 */
int
qvi_hwpool_add_device(
    qvi_hwpool_t *rpool,
    qv_hw_obj_type_t type,
    int id,
    hwloc_const_cpuset_t affinity
);

/**
 *
 */
hwloc_const_cpuset_t
qvi_hwpool_cpuset_get(
    qvi_hwpool_t *rpool
);

/**
 *
 */
int
qvi_hwpool_obtain_by_cpuset(
    qvi_hwpool_t *pool,
    qvi_hwloc_t *hwloc,
    hwloc_const_cpuset_t cpuset,
    qvi_hwpool_t **opool
);

/**
 * TODO(skg) This will likely go away.
 */
int
qvi_hwpool_obtain_split_by_group(
    qvi_hwpool_t *pool,
    qvi_hwloc_t *hwloc,
    int npieces,
    int group_id,
    qvi_hwpool_t **opool
);

/**
 *
 */
int
qvi_hwpool_split_devices(
    qvi_hwpool_t **pools,
    int npools,
    qvi_hwloc_t *hwloc,
    int ncolors,
    int color
);

/**
 *
 */
int
qvi_hwpool_pack(
    const qvi_hwpool_t *hwp,
    qvi_bbuff_t *buff
);

/**
 *
 */
int
qvi_hwpool_unpack(
    void *buff,
    qvi_hwpool_t **hwp
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */