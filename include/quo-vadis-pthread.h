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
 * @file quo-vadis-pthread.h
 */

#ifndef QUO_VADIS_PTHREAD_H
#define QUO_VADIS_PTHREAD_H

#include "quo-vadis.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    qv_scope_t *scope;
    void *(*thread_routine)(void *);
    void *arg;
} qv_pthread_args_t;

void *
qv_pthread_routine(
    void *arg
);

int
qv_pthread_create(
    pthread_t *thread,
    pthread_attr_t *attr,
    void *(*thread_routine)(void *arg),
    void *arg,
    qv_scope_t *scope
);

int
qv_pthread_scope_split_at(
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscopes
);

int
qv_pthread_scope_split(
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
