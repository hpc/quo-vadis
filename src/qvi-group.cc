/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group.cc
 */

#include "qvi-group.h"
#include "qvi-group-pthread.h"
#include "qvi-task.h" // IWYU pragma: keep
#include "qvi-utils.h"

qvi_hwloc_t *
qvi_group_s::hwloc(void)
{
    return task()->hwloc();
}

int
qvi_group_s::thsplit(
    int nthreads,
    qvi_group_s **child
) {
    qvi_group_pthread_t *ichild = nullptr;
    const int rc = qvi_new(&ichild, nthreads);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_group_s::next_id(
    qvi_group_id_t *gid
) {
    // Global group ID. Note that we pad its initial value so that other
    // infrastructure (e.g., QVI_MPI_GROUP_WORLD) will never equal or exceed
    // this value.
    static std::atomic<qvi_group_id_t> group_id(64);
    if (group_id == UINT64_MAX) {
        qvi_log_error("Group ID space exhausted");
        return QV_ERR_OOR;
    }
    *gid = group_id++;
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
