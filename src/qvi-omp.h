/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Inria
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Bordeaux INP
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-omp.h
 */

#ifndef QVI_OMP_H
#define QVI_OMP_H

#include "qvi-common.h"

// Forward declarations.
struct qvi_omp_group_s;
typedef struct qvi_omp_group_s qvi_omp_group_t;

#if 0
/**
 * Mapping policies types.
 */
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

/**
 * Layout for fine-grain binding
 * with default behaviour
 */
typedef struct qv_layout_s {
  qv_policy_t policy;
  qv_hw_obj_type_t obj_type;
  int stride;
} qv_layout_t;
#endif

int
qvi_omp_group_new(
    int group_size,
    int group_rank,
    qvi_omp_group_t **group
);

void
qvi_omp_group_delete(
    qvi_omp_group_t **group
);

int
qvi_omp_group_size(
    const qvi_omp_group_t *group
);

int
qvi_omp_group_id(
    const qvi_omp_group_t *group
);

int
qvi_omp_group_barrier(
    qvi_omp_group_t *group
);

int
qvi_omp_group_create_from_split(
    qvi_omp_group_t *parent,
    int color,
    int key,
    qvi_omp_group_t **child
);

int
qvi_omp_group_gather_bbuffs(
    qvi_omp_group_t *group,
    qvi_bbuff_t *txbuff,
    int,
    bool *shared_alloc,
    qvi_bbuff_t ***rxbuffs
);

int
qvi_omp_group_scatter_bbuffs(
    qvi_omp_group_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
