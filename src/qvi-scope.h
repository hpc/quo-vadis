/*
 * Copyright (c)      2021 Triad National Security, LLC
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
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
int
qvi_scope_new(
    qv_scope_t **scope,
    qv_context_t *ctx
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
hwloc_bitmap_t
qvi_scope_bitmap_get(
    qv_scope_t *scope
);

/**
 *
 */
int
qvi_scope_bitmap_set(
    qv_scope_t *scope,
    hwloc_const_bitmap_t bitmap
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
