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

#include "quo-vadis-thread.h"
#include "qvi-context.h"
#include "qvi-group-thread.h"
#include "qvi-scope.h"
#include "qvi-utils.h"
#ifdef OPENMP_FOUND
#include <omp.h>
#endif

int
qv_thread_context_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;
    try {
        qvi_delete(&ctx);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

static int
qvi_thread_context_create(
    qv_context_t **ctx
) {
    // Create base context.
    qv_context_t *ictx = nullptr;
    int rc = qvi_new_rc(&ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Create and initialize the base group.
    qvi_zgroup_thread_s *izgroup;
    rc = qvi_new_rc(&izgroup);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    ictx->zgroup = izgroup;
    // Connect to RMI server.
    rc = qvi_context_connect_to_server(ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    rc = qvi_bind_stack_init(
        ictx->bind_stack,
        ictx->zgroup->task(),
        ictx->rmi
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&ictx);
    }
    *ctx = ictx;
    return rc;
}

int
qv_thread_context_create(
    qv_context_t **ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;
    try {
        return qvi_thread_context_create(ctx);
    }
    qvi_catch_and_return();
}

int
qv_thread_scope_split(
    qv_context_t *,
    qv_scope_t *scope,
    int npieces,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscope
){
    qv_hw_obj_type_t type;
    const int rc = qvi_scope_obj_type(scope, npieces, &type);
    if (rc != QV_SUCCESS) return rc;
    //qvi_log_info("=================== Type for split is {}",(int)type);
    return qvi_scope_ksplit_at(scope, type, color_array, nthreads, subscope);
}

int
qv_thread_scope_split_at(
    qv_context_t *,
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscopes
) {
    return qvi_scope_ksplit_at(scope, type, color_array, nthreads, subscopes);
}

void *
qv_thread_routine(
    void * arg
) {
    qv_thread_args_t *arg_ptr = (qv_thread_args_t *)arg;
    int rc = qv_bind_push(arg_ptr->ctx, arg_ptr->scope);
    if (rc != QV_SUCCESS) {
        qvi_log_error("qv_bind_push() failed");
        pthread_exit(NULL);
    }

    void *ret = arg_ptr->thread_routine(arg_ptr->arg);
    // Free memory allocated in qv_pthread_create
    delete arg_ptr;
    pthread_exit(ret);
}

int
qv_pthread_create(
     pthread_t *thread,
     pthread_attr_t *attr,
     void *(*thread_routine)(void *arg),
     void *arg,
     qv_context_t *ctx,
     qv_scope_t *scope
) {
     // Memory will be freed in qv_thread_routine to avoid memory leaks.
     qv_thread_args_t *arg_ptr = nullptr;
     int rc = qvi_new_rc(&arg_ptr);
     if (rc != QV_SUCCESS) return rc;

     arg_ptr->ctx = ctx;
     arg_ptr->scope = scope;
     arg_ptr->thread_routine = thread_routine;
     arg_ptr->arg = arg;

    return pthread_create(thread, attr, qv_thread_routine, arg_ptr);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
