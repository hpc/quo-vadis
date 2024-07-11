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
 * @file quo-vadis-pthread.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "quo-vadis-pthread.h"
#include "qvi-scope.h"
#include "qvi-utils.h"

int
qv_pthread_scope_split(
    qv_scope_t *scope,
    int npieces,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscope
) {
    return qvi_scope_ksplit(
        scope, npieces, color_array, nthreads, QV_HW_OBJ_LAST, subscope
    );
}

int
qv_pthread_scope_split_at(
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscopes
) {
    return qvi_scope_ksplit_at(
        scope, type, color_array, nthreads, subscopes
    );
}

void *
qv_pthread_routine(
    void *arg
) {
    qv_pthread_args_t *arg_ptr = (qv_pthread_args_t *)arg;
    qvi_scope_bind_push(arg_ptr->scope);

    void *ret = arg_ptr->thread_routine(arg_ptr->arg);
    // Free memory allocated in qv_pthread_create
    // TODO(skg) Should we consider adding a free call-back? Are there any data
    // here that may require user-level control?
    delete arg_ptr;
    pthread_exit(ret);
}

int
qv_pthread_create(
     pthread_t *thread,
     pthread_attr_t *attr,
     void *(*thread_routine)(void *arg),
     void *arg,
     qv_scope_t *scope
) {
     // Memory will be freed in qv_pthread_routine to avoid memory leaks.
     qv_pthread_args_t *arg_ptr = nullptr;
     int rc = qvi_new(&arg_ptr);
     if (rc != QV_SUCCESS) return rc;

     arg_ptr->scope = scope;
     arg_ptr->thread_routine = thread_routine;
     arg_ptr->arg = arg;

    return pthread_create(thread, attr, qv_pthread_routine, arg_ptr);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
