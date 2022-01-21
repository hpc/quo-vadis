/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwrespool.h
 *
 * Hwardware Resource Pool
 */

#ifndef QVI_HWRESPOOL_H
#define QVI_HWRESPOOL_H

#include "qvi-common.h"
#include "qvi-hwres.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qvi_hwrespool_s;
typedef struct qvi_hwrespool_s qvi_hwrespool_t;

/**
 *
 */
int
qvi_hwrespool_new(
    qvi_hwrespool_t **rpool
);

/**
 *
 */
void
qvi_hwrespool_free(
    qvi_hwrespool_t **rpool
);

/**
 *
 */
int
qvi_hwrespool_add(
    qvi_hwrespool_t *rpool,
    qvi_hwres_t *res
);

/**
 *
 */
int
qvi_hwrespool_remove(
    qvi_hwrespool_t *rpool,
    qvi_hwres_t *res
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
