/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-pthread.cc
 */

#include "qvi-group-pthread.h"
#include "qvi-utils.h"

int
qvi_group_pthread_s::self(
    qvi_group_t **child
) {
    constexpr int group_size = 1;
    qvi_group_pthread_t *ichild = nullptr;
    // Create a group containing a single thread.
    const int rc = qvi_new(&ichild, group_size);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_group_pthread_s::split(
    int,
    int,
    qvi_group_t **
) {
    // TODO(skg) Test to see if we can do this.
    return QV_ERR_NOT_SUPPORTED;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
