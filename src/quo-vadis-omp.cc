/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022-2024 Inria
 *                         All rights reserved.
 *
 * Copyright (c) 2022-2024 Bordeaux INP
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis-thread.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "quo-vadis-omp.h"
#include "qvi-group-omp.h"
#include "qvi-scope.h"
#include "qvi-utils.h"

int
qv_omp_scope_get(
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    // Create the base process group.
    qvi_group_omp_s *zgroup = nullptr;
    const int rc = qvi_new(&zgroup);
    if (rc != QV_SUCCESS) {
        *scope = nullptr;
        return rc;
    }
    return qvi_scope_get(zgroup, iscope, scope);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
