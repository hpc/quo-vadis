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
#include "qvi-subgroup.h"

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

struct qvi_omp_group {
private:
    /** Group size. */
    int m_size = 0;
    /** ID (rank) in group: this ID is unique to each thread. */
    int m_rank = 0;
    /** */
    int
    subgroup_info(
        int color,
        int key,
        qvi_subgroup_info_s *sginfo
    );
public:
    /** Constructor. */
    qvi_omp_group(void) = delete;
    /** Constructor. */
    qvi_omp_group(
        int group_size,
        int group_rank
    );

    static int
    create(
        int group_size,
        int group_rank,
        qvi_omp_group **group
    );

    static void
    destroy(
        qvi_omp_group **group
    );

    int
    size(void);

    int
    rank(void);

    int
    barrier(void);

    int
    split(
        int color,
        int key,
        qvi_omp_group **child
    );

    int
    gather(
        qvi_bbuff *txbuff,
        int root,
        bool *shared,
        qvi_bbuff ***rxbuffs
    );

    int
    scatter(
        qvi_bbuff **txbuffs,
        int root,
        qvi_bbuff **rxbuff
    );
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
