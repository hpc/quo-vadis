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
    if (!scope || npieces < 0 || !color_array || nthreads < 0 || !subscope) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_ksplit(
            scope, npieces, color_array, nthreads,
            QV_HW_OBJ_LAST, subscope
        );
    }
    qvi_catch_and_return();
}

int
qv_pthread_scope_split_at(
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscopes
) {
    if (!scope || !color_array || nthreads < 0 || !subscopes) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_ksplit_at(
            scope, type, color_array, nthreads, subscopes
        );
    }
    qvi_catch_and_return();
}

void *
qv_pthread_routine(
    void *arg
) {
    qv_pthread_args_t *arg_ptr = (qv_pthread_args_t *)arg;
    qvi_scope_bind_push(arg_ptr->scope);

    void *ret = arg_ptr->thread_routine(arg_ptr->arg);
    qvi_delete(&arg_ptr);
    pthread_exit(ret);
}

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
) {
    // Memory will be freed in qv_pthread_routine to avoid memory leaks.
    qv_pthread_args_t *arg_ptr = nullptr;
    const int rc = qvi_new(&arg_ptr);
    // Since this is meant to behave similarly to
    // pthread_create(), return a reasonable errno.
    if (rc != QV_SUCCESS) return ENOMEM;

    arg_ptr->scope = scope;
    arg_ptr->thread_routine = thread_routine;
    arg_ptr->arg = arg;
    return pthread_create(thread, attr, qv_pthread_routine, arg_ptr);
}

int
qv_pthread_scope_free(
    int nscopes,
    qv_scope_t **scopes
) {
    if (nscopes < 0 || !scopes) return QV_ERR_INVLD_ARG;
    try {
        qvi_scope_kfree(&scopes, nscopes);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
