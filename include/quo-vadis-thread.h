/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2026 Triad National Security, LLC
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
 * @file quo-vadis-thread.h
 */

#ifndef QUO_VADIS_THREAD_H
#define QUO_VADIS_THREAD_H

#include "quo-vadis.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Automatic grouping options for qv_thread_scope_split() and
 * qv_thread_scope_split_at(). The following values can be used instead of
 * group_id to influence how automatic task grouping is accomplished.
 */
int *const QV_THREAD_SCOPE_SPLIT_PACKED              = (int *)0x00000001;
int *const QV_THREAD_SCOPE_SPLIT_SPREAD              = (int *)0x00000002;
int *const QV_THREAD_SCOPE_SPLIT_AFFINITY_PRESERVING = (int *)0x00000003;

int
qv_thread_scope_split(
    qv_scope_t *scope,
    int npieces,
    int *kcolors,
    int k,
    qv_scope_t ***subscopes
);

int
qv_thread_scope_split_at(
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int *kcolors,
    int k,
    qv_scope_t ***subscopes
);

/**
 * Frees resources allocated by calls to qv_thread_scope_split*.
 */
// TODO(skg) flip ordering.
int
qv_thread_scopes_free(
    int nscopes,
    qv_scope_t **scopes
);

/* ////////////////////////////////////////////////////////////////////////// */
/* Pthread-specific calls.                                                    */
/* ////////////////////////////////////////////////////////////////////////// */

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

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
