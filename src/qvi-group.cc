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
#include "qvi-utils.h"

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

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
