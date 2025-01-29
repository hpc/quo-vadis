/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2024-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-pthread.cc
 */

#include "qvi-pthread.h"
#include "qvi-bbuff.h"
#include "qvi-task.h" // IWYU pragma: keep
#include "qvi-utils.h"

int
qvi_pthread_group::m_init_by_a_single_thread(
    qvi_pthread_group_context *ctx,
    int group_size
) {
    m_context = ctx;

    m_size = group_size;

    int rc = pthread_barrier_init(&m_barrier, NULL, m_size);
    if (qvi_unlikely(rc != 0)) throw qvi_runtime_error();

    m_data_g = new qvi_bbuff *[m_size]();
    for (int i = 0 ; i < group_size ; i++) {
        const int rc = qvi_bbuff_new(&m_data_g[i]);
        if (qvi_unlikely(rc != 0)) throw qvi_runtime_error();
    }
    m_data_s = new qvi_bbuff**();
    m_ckrs = new qvi_subgroup_color_key_rank[m_size]();

    return rc;
}

/* static */ int
qvi_pthread_group::m_finish_init_by_all_threads(
    qvi_pthread_group *group
) {
    int rc = QV_SUCCESS;

    const pid_t mytid = qvi_gettid();
    // Let the threads add their TIDs to the vector.
    {
        std::lock_guard<std::mutex> guard(group->m_mutex);
        group->m_tids.push_back(mytid);
    }
    // Make sure they all contribute before continuing.
    pthread_barrier_wait(&group->m_barrier);
    // Elect one thread to be the worker.
    bool worker = false;
    {
        std::lock_guard<std::mutex> guard(group->m_mutex);
        worker = (group->m_tids.at(0) == mytid);
    }
    // The worker populates the TID to rank mapping, while the others wait.
    if (worker) {
        std::sort(group->m_tids.begin(), group->m_tids.end());

        for (int i = 0; i < group->m_size; ++i) {
            const pid_t tidi = group->m_tids[i];
            group->m_tid2rank.insert({tidi, i});
        }
    }
    pthread_barrier_wait(&group->m_barrier);
    // Everyone can now create their task and populate the mapping table.
    {
        std::lock_guard<std::mutex> guard(group->m_mutex);
        qvi_task *task = nullptr;
        // Don't check rc and return here to avoid hangs in error paths.
        rc = qvi_new(&task);
        group->m_tid2task.insert({mytid, task});
    }
    // Make sure they all finish before returning.
    pthread_barrier_wait(&group->m_barrier);
    return rc;
}

qvi_pthread_group::qvi_pthread_group(
    qvi_pthread_group_context *ctx,
    int group_size
) {
    const int rc = m_init_by_a_single_thread(ctx, group_size);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    // TODO(skg) Add to group table context.
}

qvi_pthread_group::qvi_pthread_group(
    qvi_pthread_group *parent_group,
    const qvi_subgroup_info &sginfo
) {
    assert(sginfo.rank == 0);

    std::lock_guard<std::mutex> guard(parent_group->m_mutex);
    const int rc = m_init_by_a_single_thread(parent_group->m_context, sginfo.size);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();

    const qvi_group_id_t mygid = parent_group->m_subgroup_gids[sginfo.index];
    m_context->groupid2thgroup.insert({mygid, this});
}

void *
qvi_pthread_group::call_first_from_pthread_create(
    void *arg
) {
    auto args = (qvi_pthread_group_pthread_create_args *)arg;
    const qvi_pthread_routine_fun_ptr_t thread_routine = args->throutine;
    void *const th_routine_argp = args->throutine_argp;

    const int rc = m_finish_init_by_all_threads(args->group);
    // TODO(skg) Is this the correct thing to do? Shall we return something?
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    // Free the provided argument container.
    qvi_delete(&args);
    // Finally, call the specified thread routine.
    return thread_routine(th_routine_argp);
}

qvi_pthread_group::~qvi_pthread_group(void)
{
    // TODO(skg)
    return;

    std::lock_guard<std::mutex> guard(m_mutex);
    for (auto &tt : m_tid2task) {
        qvi_delete(&tt.second);
    }
    pthread_barrier_destroy(&m_barrier);

    if (m_data_g) {
        for (int i = 0; i < m_size; ++i) {
            qvi_bbuff_delete(&m_data_g[i]);
        }
        delete[] m_data_g;
    }

    //C++ flavor
    /*
    for (auto &tt : m_data) {
        qvi_delete(&tt);
    }
    */
    delete m_data_s;
    delete[] m_ckrs;
}

int
qvi_pthread_group::size(void)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_size;
}

int
qvi_pthread_group::rank(void)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    assert(!m_tid2rank.empty());
    return m_tid2rank.at(qvi_gettid());
}

qvi_task *
qvi_pthread_group::task(void)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    assert(!m_tid2task.empty());
    return m_tid2task.at(qvi_gettid());
}

int
qvi_pthread_group::barrier(void)
{
    const int rc = pthread_barrier_wait(&m_barrier);
    if (qvi_unlikely((rc != 0) && (rc != PTHREAD_BARRIER_SERIAL_THREAD))) {
        return QV_ERR_INTERNAL;
    }
    return QV_SUCCESS;
}

int
qvi_pthread_group::m_subgroup_info(
    int color,
    int key,
    qvi_subgroup_info &sginfo
) {
    int rc = QV_SUCCESS;
    const int master_rank = 0;
    const int my_rank = rank();
    // Gather colors and keys from ALL threads in the parent group.
    // NOTE: this (i.e., the caller) is a member of the parent group).
    m_ckrs[my_rank].color = color;
    m_ckrs[my_rank].key = key;
    m_ckrs[my_rank].rank = my_rank;
    // Barrier to be sure that all threads have contributed their values.
    pthread_barrier_wait(&m_barrier);
    // Since these data are shared, only the master thread has to sort them.
    // The same goes for calculating the number of distinct colors provided.
    if (my_rank == master_rank) {
        // Sort the color/key/rank array. First according to color, then by key,
        // but in the same color realm. If color and key are identical, sort by
        // the rank from given group.
        std::sort(m_ckrs, m_ckrs + m_size, qvi_subgroup_color_key_rank::by_color);
        std::sort(m_ckrs, m_ckrs + m_size, qvi_subgroup_color_key_rank::by_key);
        std::sort(m_ckrs, m_ckrs + m_size, qvi_subgroup_color_key_rank::by_rank);
        // Calculate the number of distinct colors provided.
        std::set<int> color_set;
        for (int i = 0; i < m_size; ++i) {
            color_set.insert(m_ckrs[i].color);
        }
        const size_t ncolors = color_set.size();
        m_ckrs[my_rank].ncolors = ncolors;
        // Now that we know the number of distinct colors, populate the
        // sub-group IDs. Set rc here and continue until return so we don't hang
        // on an error path.
        rc = qvi_group::next_ids(ncolors, m_subgroup_gids);
    }
    // All threads wait for the number of colors to be computed.
    pthread_barrier_wait(&m_barrier);
    // Make sure errors didn't occur above. Do this after the barrier.
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // The number of distinct colors is the number of subgroups.
    m_ckrs[my_rank].ncolors = m_ckrs[master_rank].ncolors;
    sginfo.ngroups = m_ckrs[master_rank].ncolors;
    // Compute my sub-group id, size, and sub-group rank.
    int subgroup_index = 0;
    int subgroup_rank = 0;
    int subgroup_size = 0;
    int current_color = m_ckrs[0].color;
    for (int i = 0; i < m_size; ++i) {
        // Calculate subgroup ID based on new colors seen.
        if (current_color != m_ckrs[i].color) {
            current_color = m_ckrs[i].color;
            subgroup_index++;
        }
        if (color != m_ckrs[i].color) continue;
        // Else we found the start of my color group.
        for (int j = i; j < m_size && color == m_ckrs[j].color; ++j) {
            if (m_ckrs[j].rank == my_rank) {
                sginfo.rank = subgroup_rank;
            }
            subgroup_size++;
            subgroup_rank++;
        }
        sginfo.index = subgroup_index;
        sginfo.size = subgroup_size;
        break;
    }
    return rc;
}

int
qvi_pthread_group::split(
    int color,
    int key,
    qvi_pthread_group **child
) {
    qvi_pthread_group *ichild = nullptr;

    qvi_subgroup_info sginfo;
    int rc = m_subgroup_info(color, key, sginfo);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    if (sginfo.rank == 0) {
        // Recall this is the parent group.
        rc = qvi_new(&ichild, this, sginfo);
        barrier();
    }
    else {
        barrier();
        const qvi_group_id_t mygid = m_subgroup_gids[sginfo.index];
        ichild = m_context->groupid2thgroup.at(mygid);
    }
    // Now we can check if errors happened above.
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;

    rc = m_finish_init_by_all_threads(ichild);
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_pthread_group::gather(
    qvi_bbuff *txbuff,
    int,
    qvi_bbuff_alloc_type_t *alloc_type,
    qvi_bbuff ***rxbuffs
) {
    const int myrank = rank();
    // I'm not sure why this barrier is needed, but it seems to help...
    barrier();
    int rc = QV_SUCCESS;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        rc = qvi_bbuff_copy(*txbuff, m_data_g[myrank]);
        *alloc_type = QVI_BBUFF_ALLOC_SHARED_GLOBAL;
    }
    // Need to ensure that all threads have contributed to m_data_g
    barrier();

    if (qvi_unlikely(rc != QV_SUCCESS)) {
        *rxbuffs = nullptr;
        return QV_ERR_INTERNAL;
    }
    *rxbuffs = m_data_g;
    return rc;
}

int
qvi_pthread_group::scatter(
    qvi_bbuff **txbuffs,
    int rootid,
    qvi_bbuff **rxbuff
) {
    int rc = QV_SUCCESS;
    const int myrank = rank();
    qvi_bbuff *mybbuff = nullptr;

    if (rootid == myrank) {
        *m_data_s = txbuffs;
    }

    barrier();
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        rc = qvi_bbuff_dup(*((*m_data_s)[myrank]), &mybbuff);
    }
    barrier();

    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_bbuff_delete(&mybbuff);
        return QV_ERR_INTERNAL;
    }
    *rxbuff = mybbuff;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
