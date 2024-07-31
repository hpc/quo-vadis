/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2024      Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-pthread.h
 */

#ifndef QVI_PTHREAD_H
#define QVI_PTHREAD_H

#include "qvi-common.h"

typedef void *(*qvi_pthread_routine_fun_ptr_t)(void *);

struct qvi_pthread_group_s;
typedef struct qvi_pthread_group_s qvi_pthread_group_t;

struct qvi_pthread_group_pthread_create_args_s {
    /** Thread group. */
    qvi_pthread_group_t *group = nullptr;
    /** The routine to call after group construction. */
    qvi_pthread_routine_fun_ptr_t throutine = nullptr;
    /** Thread routine arguments. */
    void *throutine_argp = nullptr;
    /** Constructor. */
    qvi_pthread_group_pthread_create_args_s(void) = delete;
    /** Constructor. */
    qvi_pthread_group_pthread_create_args_s(
        qvi_pthread_group_t *group_a,
        qvi_pthread_routine_fun_ptr_t throutine_a,
        void *throutine_argp_a
    ) : group(group_a)
      , throutine(throutine_a)
      , throutine_argp(throutine_argp_a) { }
};

struct qvi_pthread_group_s {
private:
    /** Group size. */
    int m_size = 0;
    /** Holds the thread TIDs in this group. */
    std::vector<pid_t> m_tids;
    /** Holds TID to rank mapping. */
    std::map<pid_t, int> m_tid2rank;
    /** Holds TID to task mapping. */
    std::map<pid_t, qvi_task *> m_tid2task;
    /** Used for mutexy things. */
    std::mutex m_mutex;
    /** Used for barrier things. */
    pthread_barrier_t m_barrier;
public:
    /** Constructor. */
    qvi_pthread_group_s(void) = delete;
    /**
     * Constructor. This is called by the parent process to construct the
     * maximum amount of infrastructure possible. The rest of group construction
     * has to be performed after pthread_create() time. See
     * call_first_from_pthread_create() for more details.
     */
    qvi_pthread_group_s(
        int group_size
    );
    /**
     * This function shall be called by pthread_create() to finish group
     * construction. This function is called by the pthreads and NOT their
     * parent.
     */
    static void *
    call_first_from_pthread_create(
        void *arg
    );
    /** Destructor. */
    ~qvi_pthread_group_s(void);

    qvi_task *
    task(void);

    int
    size(void);

    int
    rank(void);

    int
    barrier(void);

    int
    split(
        int color,
        int key,
        qvi_pthread_group_s **child
    );

    int
    gather(
        qvi_bbuff *txbuff,
        int root,
        bool *shared,
        qvi_bbuff ***rxbuffs
    );

    int
    scatter(
        qvi_bbuff **txbuffs,
        int root,
        qvi_bbuff **rxbuff
    );
};
typedef struct qvi_pthread_group_s qvi_pthread_group_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
