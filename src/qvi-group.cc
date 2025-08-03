/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group.cc
 */

#include "qvi-group.h"
#include "qvi-group-thread.h"
#include "qvi-utils.h"

qvi_hwloc *
qvi_group::hwloc(void)
{
    return task()->hwloc();
}

int
qvi_group::thread_split(
    int nthreads,
    const std::vector<int> &colors,
    qvi_group **child
) {
    // This is the entry point for creating a new thread group. Also note this is
    // called by a single thread of execution (i.e., the parent process).
    qvi_group_thread *ichild = nullptr;
    const int rc = qvi_new(&ichild, nthreads, colors);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_group::next_id(
    qvi_group_id_t *gid
) {
    // Global group ID. Note that we pad its initial value so that other
    // infrastructure (e.g., QVI_MPI_GROUP_WORLD) will never equal or exceed
    // this value.
    static std::atomic<qvi_group_id_t> group_id(64);
    if (qvi_unlikely(group_id == UINT64_MAX)) {
        qvi_log_error("Group ID space exhausted.");
        return QV_ERR_OOR;
    }
    *gid = group_id++;
    return QV_SUCCESS;
}

int
qvi_group::next_ids(
    size_t n,
    std::vector<qvi_group_id_t> &gids
) {
    gids.resize(n);
    for (size_t i = 0; i < n; ++i) {
        qvi_group_id_t gid = 0;
        const int rc = next_id(&gid);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        gids[i] = gid;
    }
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
