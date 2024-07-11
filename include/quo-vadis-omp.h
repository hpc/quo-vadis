/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
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
 * @file quo-vadis-omp.h
 */

#ifndef QUO_VADIS_OMP_H
#define QUO_VADIS_OMP_H

#include "quo-vadis.h"

#ifdef __cplusplus
extern "C" {
#endif

int
qv_omp_scope_get(
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
