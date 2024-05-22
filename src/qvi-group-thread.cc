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

int
qvi_group_thread_s::self(
    qvi_group_t **child
) {
    int rc = QV_SUCCESS;

    qvi_group_thread_s *ichild = qvi_new qvi_group_thread_s();
    if (!ichild) {
        rc = QV_ERR_OOR;
        goto out;
    }
    // Initialize the child with the parent's thread instance.
    rc = ichild->initialize(th);
    if (rc != QV_SUCCESS) goto out;

    // Create a group containing a single thread
    rc = qvi_thread_group_create_single(
        th, &ichild->th_group
    );
out:
    if (rc != QV_SUCCESS) {
        delete ichild;
        ichild = nullptr;
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
    int rc = QV_SUCCESS;

    qvi_group_thread_s *ichild = qvi_new qvi_group_thread_s();
    if (!ichild) {
        rc = QV_ERR_OOR;
        goto out;
    }
    // Initialize the child with the parent's MPI instance.
    rc = ichild->initialize(th);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_thread_group_create_from_split(
        th, th_group, color,
        key, &ichild->th_group
    );
out:
    if (rc != QV_SUCCESS) {
        delete ichild;
        ichild = nullptr;
    }
    *child = ichild;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
