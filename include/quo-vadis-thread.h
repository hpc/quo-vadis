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
 * Creates a thread context.
 */
int
qv_thread_context_create(
    qv_context_t **ctx
);

/**
 * Frees resources associated with a context created by
 * qv_thread_context_create().
 */
int
qv_thread_context_free(
    qv_context_t *ctx
);

typedef struct {
    qv_context_t *ctx;
    qv_scope_t *scope;
    void *(*thread_routine)(void *);
    void *arg;
} qv_thread_args_t;

void *
qv_thread_routine(
    void *arg
);

int
qv_pthread_create(
    pthread_t *thread,
    pthread_attr_t *attr,
    void *(*thread_routine)(void *arg),
    void *arg,
    qv_context_t *ctx,
    qv_scope_t *scope
);

int
qv_thread_scope_split_at(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscopes
);

int
qv_thread_scope_split(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int npieces,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscope
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
