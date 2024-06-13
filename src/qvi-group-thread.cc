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
 * @file qvi-group-thread.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-group-thread.h"
#include "qvi-utils.h"

int
qvi_group_thread_s::intrinsic(
    qv_scope_intrinsic_t,
    qvi_group_s **group
) {
    // NOTE: the provided scope doesn't affect how
    // we create the thread group, so we ignore it.
    qvi_group_thread_t *igroup = nullptr;
    int rc = qvi_new_rc(&igroup);
    if (rc != QV_SUCCESS) goto out;

    rc = igroup->initialize(th);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_thread_group_create(
        th, &igroup->th_group
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&igroup);
    }
    *group = igroup;
    return rc;
}

int
qvi_group_thread_s::self(
    qvi_group_t **child
) {
    qvi_group_thread_t *ichild = nullptr;
    int rc = qvi_new_rc(&ichild);
    if (rc != QV_SUCCESS) goto out;
    // Initialize the child with the parent's thread instance.
    rc = ichild->initialize(th);
    if (rc != QV_SUCCESS) goto out;
    // Create a group containing a single thread
    rc = qvi_thread_group_create_single(
        th, &ichild->th_group
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_group_thread_s::split(
    int color,
    int key,
    qvi_group_t **child
) {
    qvi_group_thread_t *ichild = nullptr;
    int rc = qvi_new_rc(&ichild);
    if (rc != QV_SUCCESS) goto out;
    // Initialize the child with the parent's MPI instance.
    rc = ichild->initialize(th);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_thread_group_create_from_split(
        th, th_group, color,
        key, &ichild->th_group
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
