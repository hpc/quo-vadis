/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-pthread.cc
 */

#include "qvi-group-pthread.h"

void *
qvi_group_pthread::call_first_from_pthread_create(
    void *arg
) {
    auto args = (qvi_pthread_create_args *)arg;
    const qvi_pthread_routine_fun_ptr_t thread_routine = args->throutine;
    void *const th_routine_argp = args->throutine_argp;
    // Free the provided argument container.
    qvi_delete(&args);
    // Finally, call the specified thread routine.
    return thread_routine(th_routine_argp);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
