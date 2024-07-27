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
 * @file quo-vadis-omp.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "quo-vadis-omp.h"
#include "qvi-group-omp.h"
#include "qvi-scope.h"
#include "qvi-utils.h"

static int
qvi_omp_scope_get(
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    // Create the base process group.
    qvi_group_omp_s *zgroup = nullptr;
    const int rc = qvi_new(&zgroup);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        *scope = nullptr;
        return rc;
    }
    return qv_scope_s::makei(zgroup, iscope, scope);
}

int
qv_omp_scope_get(
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    if (qvi_unlikely(!scope)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_omp_scope_get(iscope, scope);
    }
    qvi_catch_and_return();
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
