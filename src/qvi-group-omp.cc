/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Inria.
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Bordeaux INP.
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-omp.cc
 */

#include "qvi-group-omp.h"
#include "qvi-utils.h"
#include <omp.h>

int
qvi_group_omp_s::make_intrinsic(
    qv_scope_intrinsic_t
) {
    const int group_size = omp_get_num_threads();
    const int group_rank = omp_get_thread_num();
    // NOTE: the provided scope doesn't affect how
    // we create the thread group, so we ignore it.
    return qvi_omp_group_new(group_size, group_rank, &th_group);
}

int
qvi_group_omp_s::self(
    qvi_group_t **child
) {
    const int group_size = 1;
    const int group_rank = 0;
    qvi_group_omp_t *ichild = nullptr;
    int rc = qvi_new(&ichild);
    if (rc != QV_SUCCESS) goto out;
    // Create a group containing a single thread.
    rc = qvi_omp_group_new(group_size, group_rank, &ichild->th_group);
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_group_omp_s::split(
    int color,
    int key,
    qvi_group_t **child
) {
    qvi_group_omp_t *ichild = nullptr;
    int rc = qvi_new(&ichild);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_omp_group_create_from_split(
        th_group, color, key, &ichild->th_group
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
