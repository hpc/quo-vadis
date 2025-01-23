/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2025 Triad National Security, LLC
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
 * @file quo-vadis-pthread.h
 */

#ifndef QUO_VADIS_PTHREAD_H
#define QUO_VADIS_PTHREAD_H

#include "quo-vadis.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Mapping policies types.
 */
// Intel policies (KMP_AFFINITY) are :
// - disabled: prevents the runtime library from making any affinity-related
//             system calls (to avoid interference with other platform affinity mechanisms).
// - compact: threads are placed as close together as possible.
// - scatter: threads are distributed as evenly as possible across the entire system.
//            (opposite of compact).
// - explicit: threads are placed according to a list of OS proc IDs (required)
typedef enum {
  QV_POLICY_PACKED     = 1,
  QV_POLICY_COMPACT    = 1,
  QV_POLICY_CLOSE      = 1,
  QV_POLICY_SPREAD     = 2,
  QV_POLICY_DISTRIBUTE = 3,
  QV_POLICY_ALTERNATE  = 3,
  QV_POLICY_CORESFIRST = 3,
  QV_POLICY_SCATTER    = 4,
  QV_POLICY_CHOOSE     = 5,
} qv_policy_t;
// TODO(skg) Consider updating prefix to qv_pthread_ if this is only used for
// Pthread code.

/**
 * Similar to pthread_create(3).
 */
int
qv_pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*thread_routine)(void *arg),
    void *arg,
    qv_scope_t *scope
);

int
qv_pthread_scope_split(
    qv_scope_t *scope,
    int npieces,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscope
);

int
qv_pthread_scope_split_at(
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscopes
);

/**
 * Frees resources allocated by calls to qv_pthread_scope_split*.
 */
int
qv_pthread_scopes_free(
    int nscopes,
    qv_scope_t **scopes
);

/**
 * Fills color array used in qv_pthread_scope_split*.
 */
int
qv_pthread_colors_fill(
    int *color_array,
    int array_size,
    qv_policy_t policy,
    int stride,
    int npieces
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
