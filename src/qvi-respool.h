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
 * @file qvi-respool.h
 *
 * Resource Pool
 */

#ifndef QVI_RESPOOL_H
#define QVI_RESPOOL_H

#include "qvi-common.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qvi_respool_s;
typedef struct qvi_respool_s qvi_respool_t;

/**
 *
 */
int
qvi_respool_new(
    qvi_respool_t **rpool
);

/**
 *
 */
void
qvi_respool_free(
    qvi_respool_t **rpool
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
