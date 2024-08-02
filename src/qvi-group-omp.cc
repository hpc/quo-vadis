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
#include "qvi-task.h" // IWYU pragma: keep
#include "qvi-utils.h"
#include <omp.h>

qvi_group_omp::qvi_group_omp(void)
{
    const int rc = qvi_new(&m_task);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
}

qvi_group_omp::~qvi_group_omp(void)
{
    qvi_omp_group::destroy(&m_ompgroup);
    qvi_delete(&m_task);
}

int
qvi_group_omp::make_intrinsic(
    qv_scope_intrinsic_t
) {
    const int group_size = omp_get_num_threads();
    const int group_rank = omp_get_thread_num();
    // NOTE: the provided scope doesn't affect how
    // we create the thread group, so we ignore it.
    return qvi_omp_group::create(group_size, group_rank, &m_ompgroup);
}

int
qvi_group_omp::self(
    qvi_group **child
) {
    const int group_size = 1;
    const int group_rank = 0;
    qvi_group_omp *ichild = nullptr;
    int rc = qvi_new(&ichild);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // Create a group containing a single thread.
    rc = qvi_omp_group::create(group_size, group_rank, &ichild->m_ompgroup);
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_group_omp::split(
    int color,
    int key,
    qvi_group **child
) {
    qvi_group_omp *ichild = nullptr;
    int rc = qvi_new(&ichild);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;

    rc = m_ompgroup->split(color, key, &ichild->m_ompgroup);
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
