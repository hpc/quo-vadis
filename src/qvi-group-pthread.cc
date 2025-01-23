/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
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

qvi_group_pthread::qvi_group_pthread(
    int group_size
) {
    const int rc = qvi_new(&thgroup, group_size, 0);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
}

qvi_group_pthread::~qvi_group_pthread(void)
{
    qvi_delete(&thgroup);
}

int
qvi_group_pthread::self(
    qvi_group **
) {
    // TODO(skg)
    return QV_ERR_NOT_SUPPORTED;
}

int
qvi_group_pthread::split(
    int color,
    int key,
    qvi_group **child
) {
    qvi_group_pthread *ichild = nullptr;
    int rc  = qvi_new(&ichild);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;

    rc = thgroup->split(color, key, &ichild->thgroup);
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
