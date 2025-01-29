/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2024-2025 Triad National Security, LLC
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
#include "qvi-group.h"
#include "qvi-subgroup.h"
#include "qvi-bbuff.h"

typedef void *(*qvi_pthread_routine_fun_ptr_t)(void *);
// Forward declaration.
struct qvi_pthread_group;

/**
 * This structure holds Pthread group context information that is relevant to
 * all threads that are spawned by a common parent process.
 */
struct qvi_pthread_group_context : public qvi_refc {
    /** */
    std::unordered_map<qvi_group_id_t, qvi_pthread_group *> groupid2thgroup;
};

struct qvi_pthread_group_pthread_create_args {
    /** Thread group. */
    qvi_pthread_group *group = nullptr;
    /** The routine to call after group construction. */
    qvi_pthread_routine_fun_ptr_t throutine = nullptr;
    /** Thread routine arguments. */
    void *throutine_argp = nullptr;
    /** Default constructor. */
    qvi_pthread_group_pthread_create_args(void) = delete;
    /** Constructor. */
    qvi_pthread_group_pthread_create_args(
        qvi_pthread_group *group_a,
        qvi_pthread_routine_fun_ptr_t throutine_a,
        void *throutine_argp_a
    ) : group(group_a)
      , throutine(throutine_a)
      , throutine_argp(throutine_argp_a) { }
};

struct qvi_pthread_group {
private:
    /** Context information. */
    qvi_pthread_group_context *m_context = nullptr;
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
    /** Used for monitory things. */
    std::condition_variable m_condition;
    /** Used for barrier things. */
    pthread_barrier_t m_barrier;
    /** Used for gather exchanges. */
    qvi_bbuff **m_data_g = nullptr;
    /** Used for scatter exchanges. */
    qvi_bbuff ***m_data_s = nullptr;
    /** Shared color, key, rank scratch pad used for splitting. */
    qvi_subgroup_color_key_rank *m_ckrs = nullptr;
    /** Shared sub-group IDs. */
    std::vector<qvi_group_id_t> m_subgroup_gids;
    /** */
    int
    m_init_by_a_single_thread(
        qvi_pthread_group_context *ctx,
        int group_size
    );
    /** */
    static int
    m_finish_init_by_all_threads(
        qvi_pthread_group *group
    );
    /** */
    int
    m_subgroup_info(
        int color,
        int key,
        qvi_subgroup_info &sginfo
    );
public:
    /** Default constructor. */
    qvi_pthread_group(void) = delete;
    /**
     * Constructor. Called by sginfo.rank == 0.
     */
    qvi_pthread_group(
        qvi_pthread_group *parent_group,
        const qvi_subgroup_info &sginfo
    );
    /**
     * Constructor. This is called by the parent process to construct the
     * maximum amount of infrastructure possible. The rest of group construction
     * has to be performed after pthread_create() or during
     * qvi_pthread_group::split() time. See call_first_from_pthread_create() for
     * more details.
     */
    qvi_pthread_group(
        qvi_pthread_group_context *ctx,
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
    ~qvi_pthread_group(void);

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
        qvi_pthread_group **child
    );

    int
    gather(
        qvi_bbuff *txbuff,
        int root,
        qvi_bbuff_alloc_type_t *alloc_type,
        qvi_bbuff ***rxbuffs
    );

    int
    scatter(
        qvi_bbuff **txbuffs,
        int root,
        qvi_bbuff **rxbuff
    );
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
