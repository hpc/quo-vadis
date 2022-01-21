/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwres.h
 *
 * Hardware Resource
 */

#ifndef QVI_HWRES_H
#define QVI_HWRES_H

#include "qvi-common.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qvi_hwres_s;
typedef struct qvi_hwres_s qvi_hwres_t;

/**
 *
 */
int
qvi_hwres_new(
    qvi_hwres_t **hwres,
    qv_hw_obj_type_t type
);

/**
 *
 */
void
qvi_hwres_free(
    qvi_hwres_t **hwres
);

/**
 *
 */
qv_hw_obj_type_t
qvi_hwres_type(
    qvi_hwres_t *hwres
);

/**
 *
 */
int
qvi_hwres_add_from_bitmap(
    qvi_hwres_t *hwres,
    hwloc_const_cpuset_t rmap
);

/**
 *
 */
int
qvi_hwres_add(
    qvi_hwres_t *to,
    qvi_hwres_t *from
);

/**
 *
 */
int
qvi_hwres_remove_from_bitmap(
    qvi_hwres_t *hwres,
    hwloc_const_cpuset_t rmap
);

/**
 *
 */
int
qvi_hwres_remove(
    qvi_hwres_t *from,
    qvi_hwres_t *what
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
