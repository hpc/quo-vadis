/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*qv_policy_t
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
#include "qvi-pthread.h"
#include "qvi-group-pthread.h"
#include "qvi-scope.h"
#include "qvi-utils.h"


struct qvi_pthread_args_s {
    qv_scope_t *scope = nullptr;
    qvi_pthread_routine_fun_ptr_t th_routine = nullptr;
    void *th_routine_argp = nullptr;
    /** Constructor. */
    qvi_pthread_args_s(void) = delete;
    /** Constructor. */
    qvi_pthread_args_s(
        qv_scope_t *scope_a,
        qvi_pthread_routine_fun_ptr_t th_routine_a,
        void *th_routine_argp_a
    ) : scope(scope_a)
      , th_routine(th_routine_a)
      , th_routine_argp(th_routine_argp_a) { }
};

static void *
qvi_pthread_routine(
    void *arg
) {
    qvi_pthread_args_s *arg_ptr = (qvi_pthread_args_s *)arg;
    // TODO(skg) Check return code.
    arg_ptr->scope->bind_push();

    void *const ret = arg_ptr->th_routine(arg_ptr->th_routine_argp);
    qvi_delete(&arg_ptr);
    pthread_exit(ret);
}

int
qv_pthread_scope_split(
    qv_scope_t *scope,
    int npieces,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscope
) {
    const bool invld_args = !scope || npieces < 0 || !color_array ||
                            nthreads < 0 || !subscope;
    if (qvi_unlikely(invld_args)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return scope->thread_split(
            npieces, color_array, nthreads, QV_HW_OBJ_LAST, subscope
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
    if (qvi_unlikely(!scope || !color_array || nthreads < 0 || !subscopes)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return scope->thsplit_at(
            type, color_array, nthreads, subscopes
        );
    }
    qvi_catch_and_return();
}

int
qv_pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr,
    qvi_pthread_routine_fun_ptr_t thread_routine,
    void *arg,
    qv_scope_t *scope
) {
    // Memory will be freed in qv_pthread_routine to avoid memory leaks.
    qvi_pthread_args_s *arg_ptr = nullptr;
    int rc = qvi_new(&arg_ptr, scope, thread_routine, arg);
    // Since this is meant to behave similarly to
    // pthread_create(), return a reasonable errno.
    if (qvi_unlikely(rc != QV_SUCCESS)) return ENOMEM;

    auto group = dynamic_cast<qvi_group_pthread *>(scope->group());
    qvi_pthread_group_pthread_create_args_s *cargs = nullptr;
    rc = qvi_new(&cargs, group->thgroup, qvi_pthread_routine, arg_ptr);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&arg_ptr);
        return ENOMEM;
    }
    return pthread_create(
        thread, attr, qvi_pthread_group::call_first_from_pthread_create, cargs
    );
}

int
qv_pthread_scopes_free(
    int nscopes,
    qv_scope_t **scopes
) {
    if (qvi_unlikely(nscopes < 0 || !scopes)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        qv_scope::thdestroy(&scopes, nscopes);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

int
qv_pthread_colors_fill(
   int *color_array,
   int array_size,
   qv_policy_t policy,
   int stride,
   int npieces
){
    int rc = QV_SUCCESS;

    if(stride < 1)
        return QV_ERR_INVLD_ARG;

    switch(policy){
    case QV_POLICY_SPREAD:
        {
            break;
        }

    case QV_POLICY_DISTRIBUTE:
    //case QV_POLICY_ALTERNATE:
    //case QV_POLICY_CORESFIRST:
        {
            break;
        }

    case QV_POLICY_SCATTER:
        {


            break;
        }

    case QV_POLICY_CHOOSE:
        {
            break;
        }

    case QV_POLICY_PACKED:
    //case QV_POLICY_COMPACT:
    //case QV_POLICY_CLOSE:
    default:
        {
            for(int idx = 0 ; idx < array_size ; idx++){
                //    color_array[idx] = (idx+idx*(stride-1))%(npieces);
                color_array[idx] = (idx*stride)%(npieces);
            }
            break;
        }
    }

    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
