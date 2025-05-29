/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*qv_policy_t
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
 * @file quo-vadis-thread.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "quo-vadis-thread.h"
#include "qvi-group-pthread.h"
#include "qvi-scope.h"
#include "qvi-utils.h"

struct qvi_pthread_args {
    qv_scope_t *scope = nullptr;
    qvi_pthread_routine_fun_ptr_t th_routine = nullptr;
    void *th_routine_argp = nullptr;
    /** Default constructor. */
    qvi_pthread_args(void) = delete;
    /** Constructor. */
    qvi_pthread_args(
        qv_scope_t *scope_a,
        qvi_pthread_routine_fun_ptr_t th_routine_a,
        void *th_routine_argp_a
    ) : scope(scope_a)
      , th_routine(th_routine_a)
      , th_routine_argp(th_routine_argp_a) { }
};

static void *
qvi_pthread_start_routine(
    void *arg
) {
    qvi_pthread_args *args = (qvi_pthread_args *)arg;

    const int rc = args->scope->bind_push();
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_log_error(
            "An error occurred in bind_push(): {} ({})", rc, qv_strerr(rc)
        );
        pthread_exit(nullptr);
    }

    void *const ret = args->th_routine(args->th_routine_argp);
    qvi_delete(&args);
    pthread_exit(ret);
}

static int
split_color_fixup(
    int *kcolors,
    int k,
    std::vector<int> &kcolorsp
) {
    int real_color = QV_SCOPE_SPLIT_UNDEFINED;

    if (kcolors == QV_THREAD_SCOPE_SPLIT_PACKED) {
        real_color = QV_SCOPE_SPLIT_PACKED;
    }
    else if (kcolors == QV_THREAD_SCOPE_SPLIT_SPREAD) {
        real_color = QV_SCOPE_SPLIT_SPREAD;
    }
    else if (kcolors == QV_THREAD_SCOPE_SPLIT_AFFINITY_PRESERVING) {
        real_color = QV_SCOPE_SPLIT_AFFINITY_PRESERVING;
    }
    // Nothing to do. An automatic coloring was not requested.
    if (real_color == QV_SCOPE_SPLIT_UNDEFINED) {
        return QV_SUCCESS;
    }
    // An automatic coloring was requested.
    kcolorsp.resize(k);
    std::fill(kcolorsp.begin(), kcolorsp.end(), real_color);
    return QV_SUCCESS;
}

int
qv_thread_scope_split(
    qv_scope_t *scope,
    int npieces,
    int *kcolors,
    int k,
    qv_scope_t ***subscopes
) {
    const bool invalid_args = !scope || npieces < 0 || !kcolors ||
                              k < 0 || !subscopes;
    if (qvi_unlikely(invalid_args)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::vector<int> color_fixup;
        const int rc = split_color_fixup(kcolors, k, color_fixup);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        // Set the colors array to the appropriate data.
        int *kcolorsp = color_fixup.empty() ? kcolors : color_fixup.data();
        return scope->thread_split(
            npieces, kcolorsp, k, QV_HW_OBJ_LAST, subscopes
        );
    }
    qvi_catch_and_return();
}

int
qv_thread_scope_split_at(
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int *kcolors,
    int k,
    qv_scope_t ***subscopes
) {
    if (qvi_unlikely(!scope || !kcolors || k < 0 || !subscopes)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::vector<int> color_fixup;
        const int rc = split_color_fixup(kcolors, k, color_fixup);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        // Set the colors array to the appropriate data.
        int *kcolorsp = color_fixup.empty() ? kcolors : color_fixup.data();
        return scope->thread_split_at(type, kcolorsp, k, subscopes);
    }
    qvi_catch_and_return();
}

int
qv_thread_scopes_free(
    int nscopes,
    qv_scope_t **scopes
) {
    if (qvi_unlikely(nscopes < 0 || !scopes)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        qv_scope::thread_destroy(&scopes, nscopes);
        return QV_SUCCESS;
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
    qvi_pthread_args *pthread_start_args = nullptr;
    int rc = qvi_new(&pthread_start_args, scope, thread_routine, arg);
    // Since this is meant to behave similarly to
    // pthread_create(), return a reasonable errno.
    if (qvi_unlikely(rc != QV_SUCCESS)) return ENOMEM;
    // Note: The provided scope should have been created by
    // qv_pthread_scope_split*. That is why we can safely cast the scope's
    // underlying group it to a qvi_group_pthread *.
    auto group = dynamic_cast<qvi_group_pthread *>(&scope->group());
    qvi_pthread_create_args *cargs = nullptr;
    rc = qvi_new(
        &cargs, group, qvi_pthread_start_routine, pthread_start_args
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&pthread_start_args);
        return ENOMEM;
    }
    return pthread_create(
        thread, attr, qvi_group_pthread::call_first_from_pthread_create, cargs
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
