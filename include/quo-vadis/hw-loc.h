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

#ifndef QUO_VADIS_HW_LOC_H
#define QUO_VADIS_HW_LOC_H

#include "quo-vadis/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations. */
struct qvi_hwloc_t;
typedef struct qvi_hwloc_t qvi_hwloc_t;

/**
 *
 */
int
qvi_hwloc_construct(
    qvi_hwloc_t **hwl
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
