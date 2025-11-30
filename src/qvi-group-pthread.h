/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-pthread.h
 */

#ifndef QVI_GROUP_PTHREAD_H
#define QVI_GROUP_PTHREAD_H

#include "qvi-group-thread.h"

typedef void *(*qvi_pthread_routine_fun_ptr_t)(void *);

struct qvi_group_pthread : public qvi_group_thread {
    /** Default constructor. */
    qvi_group_pthread(void) = delete;
    /**
     * Constructor that is called by the parent process to setup
     * base infrastructure required during a thread_split().
     */
    qvi_group_pthread(
        qv_scope_flags_t flags,
        int group_size,
        const std::vector<int> &colors
    ) : qvi_group_thread(flags, group_size, colors) { }
    /** Destructor. */
    virtual ~qvi_group_pthread(void) = default;
    /** */
    static void *
    call_first_from_pthread_create(
        void *arg
    );
};

struct qvi_pthread_create_args {
    /** Thread group. */
    qvi_group_thread *group = nullptr;
    /** The routine to call after group construction. */
    qvi_pthread_routine_fun_ptr_t throutine = nullptr;
    /** Thread routine arguments. */
    void *throutine_argp = nullptr;
    /** Default constructor. */
    qvi_pthread_create_args(void) = delete;
    /** Constructor. */
    qvi_pthread_create_args(
        qvi_group_thread *group_a,
        qvi_pthread_routine_fun_ptr_t throutine_a,
        void *throutine_argp_a
    ) : group(group_a)
      , throutine(throutine_a)
      , throutine_argp(throutine_argp_a) { }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
