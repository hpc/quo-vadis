/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 * Copyright (c) 2022      Inria. All rights reserved.
 * Copyright (c) 2022      Bordeaux INP. All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-thread.h
 */

#ifndef QVI_THREAD_H
#define QVI_THREAD_H

#include "qvi-common.h"

// Forward declarations.

struct qvi_thread_group_shared_s;
typedef struct qvi_thread_group_shared_s qvi_thread_group_shared_t;

struct qvi_thread_group_s;
typedef struct qvi_thread_group_s qvi_thread_group_t;

struct qvi_thread_s;
typedef struct qvi_thread_s qvi_thread_t;

/**
 * Mapping policies types.
 */
/*
typedef enum qv_policy_s {
  QV_POLICY_PACKED     = 1,
  QV_POLICY_COMPACT    = 1,
  QV_POLICY_CLOSE      = 1,
  QV_POLICY_SPREAD     = 2,
  QV_POLICY_DISTRIBUTE = 3,
  QV_POLICY_ALTERNATE  = 3,
  QV_POLICY_CORESFIRST = 3,
  QV_POLICY_SCATTER    = 4,
  QV_POLICY_CHOOSE     = 5,
} qv_policy_t;
*/
/**
 * Layout for fine-grain binding
 * with default behaviour
 */
/*
typedef struct qv_layout_s {
  qv_policy_t policy;
  qv_hw_obj_type_t obj_type;
  int stride;
} qv_layout_t;
*/

int
qvi_thread_new(
    qvi_thread_t **th
);

void
qvi_thread_free(
    qvi_thread_t **th
);

int
qvi_thread_group_size(
    const qvi_thread_group_t *group
);

int
qvi_thread_group_new(
    qvi_thread_group_t **group
);

void
qvi_thread_group_free(
    qvi_thread_group_t **group
);

int
qvi_thread_group_id(
    const qvi_thread_group_t *group
);

int
qvi_thread_group_create(
    qvi_thread_t *proc,
    qvi_thread_group_t **group
);

int
qvi_thread_group_create_single(
    qvi_thread_t *proc,
    qvi_thread_group_t **group
);

int
qvi_thread_group_create_from_split(
    qvi_thread_t *th,
    const qvi_thread_group_t *parent,
    int color,
    int key,
    qvi_thread_group_t **child
);

int
qvi_thread_group_gather_bbuffs(
    qvi_thread_group_t *group,
    qvi_bbuff_t *txbuff,
    int root,
    qvi_bbuff_t ***rxbuffs,
    int *shared_alloc
);

int
qvi_thread_group_scatter_bbuffs(
    qvi_thread_group_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
