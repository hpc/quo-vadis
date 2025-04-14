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
qvi_pthread_group::m_start_init_by_a_single_thread(
    qvi_pthread_group_context *ctx,
    int group_size
) {
    assert(ctx && group_size > 0);

    m_context = ctx;
    m_size = group_size;

    int rc = pthread_barrier_init(&m_barrier, nullptr, m_size);
    if (qvi_unlikely(rc != 0)) return rc;

    m_gather_data = new qvi_bbuff *[m_size];
    for (int i = 0 ; i < group_size ; i++) {
        rc = qvi_new(&m_gather_data[i]);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    }
    m_scatter_data = new qvi_bbuff**;
    m_ckrs.resize(m_size);

    return QV_SUCCESS;
}

int
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
    group->barrier();
    // Elect one thread to be the worker.
    bool worker = false;
    {
        std::lock_guard<std::mutex> guard(group->m_mutex);
        worker = (group->m_tids.at(0) == mytid);
    }
    // The worker populates the TID to rank mapping, while the others wait.
    if (worker) {
        std::lock_guard<std::mutex> guard(group->m_mutex);
        std::sort(group->m_tids.begin(), group->m_tids.end());

        for (int i = 0; i < group->m_size; ++i) {
            const pid_t tidi = group->m_tids[i];
            group->m_tid2rank.insert({tidi, i});
        }
    }
    group->barrier();
    // Everyone can now create their task and populate the mapping table.
    {
        std::lock_guard<std::mutex> guard(group->m_mutex);
        qvi_task *task = nullptr;
        // Don't check rc and return here to avoid hangs in error paths.
        rc = qvi_new(&task);
        group->m_tid2task.insert({mytid, task});
    }
    // Make sure they all finish before returning.
    group->barrier();
    return rc;
}

// TODO(skg) Verify that the ckr information is used properly during
// thread_split() and during split(). Should we set the other infos here?
static int
initialize_ckrs(
    const std::vector<int> &colors,
    std::vector<qvi_subgroup_color_key_rank> &ckrs
) {
    for (uint_t i = 0; i < colors.size(); ++i) {
        ckrs.at(i).color = colors.at(i);
    }
    return QV_SUCCESS;
}

qvi_pthread_group::qvi_pthread_group(
    qvi_pthread_group_context *ctx,
    int group_size,
    const std::vector<int> &colors
) {
    int rc = m_start_init_by_a_single_thread(ctx, group_size);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    rc = initialize_ckrs(colors, m_ckrs);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
}

qvi_pthread_group::qvi_pthread_group(
    qvi_pthread_group *parent_group,
    const qvi_subgroup_info &sginfo
) {
    assert(sginfo.rank == qvi_subgroup_info::master_rank);

    std::lock_guard<std::mutex> guard(parent_group->m_mutex);
    const int rc = m_start_init_by_a_single_thread(
        parent_group->m_context, sginfo.size
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();

    const qvi_group_id_t mygid = parent_group->m_subgroup_gids[sginfo.index];
    m_context->groupid2thgroup.insert({mygid, this});
}

qvi_pthread_group::~qvi_pthread_group(void)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    for (auto &tt : m_tid2task) {
        qvi_delete(&tt.second);
    }

    if (m_gather_data) {
        for (int i = 0; i < m_size; ++i) {
            qvi_delete(&m_gather_data[i]);
        }
        delete[] m_gather_data;
    }

    delete m_scatter_data;

    pthread_barrier_destroy(&m_barrier);
}

void *
qvi_pthread_group::call_first_from_pthread_create(
    void *arg
) {
    auto args = (qvi_pthread_group_pthread_create_args *)arg;
    const qvi_pthread_routine_fun_ptr_t thread_routine = args->throutine;
    void *const th_routine_argp = args->throutine_argp;

    const int rc = m_finish_init_by_all_threads(args->group);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_log_error(
            "An error occurred in m_finish_init_by_all_threads(): {} ({})",
            rc, qv_strerr(rc)
        );
        pthread_exit(nullptr);
    }
    // Free the provided argument container.
    qvi_delete(&args);
    // Finally, call the specified thread routine.
    return thread_routine(th_routine_argp);
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

// TODO(skg) Cleanup barrier exit paths to avoid hangs in error paths.
// TODO(skg) Audit thread safety here.
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
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_ckrs[my_rank].color = color;
        m_ckrs[my_rank].key = key;
        m_ckrs[my_rank].rank = my_rank;
    }
    // Barrier to be sure that all threads have contributed their values.
    barrier();
    // Since these data are shared, only the master thread has to sort them.
    // The same goes for calculating the number of distinct colors provided.
    if (my_rank == master_rank) {
        std::lock_guard<std::mutex> guard(m_mutex);
        // Sort the color/key/rank array. First according to color, then by key,
        // but in the same color realm. If color and key are identical, sort by
        // the rank from given group.
        std::sort(m_ckrs.begin(), m_ckrs.end(), qvi_subgroup_color_key_rank::by_color);
        std::sort(m_ckrs.begin(), m_ckrs.end(), qvi_subgroup_color_key_rank::by_key);
        std::sort(m_ckrs.begin(), m_ckrs.end(), qvi_subgroup_color_key_rank::by_rank);
        // Calculate the number of distinct colors provided.
        std::set<int> color_set;
        for (int i = 0; i < m_size; ++i) {
            color_set.insert(m_ckrs[i].color);
        }
        const size_t ncolors = color_set.size();
        m_ckrs[my_rank].ncolors = ncolors;
        // Now that we know the number of distinct colors, populate the
        // sub-group IDs. Set rc here and continue until after the next barrier
        // so we don't hang on an error path.
        rc = qvi_group::next_ids(ncolors, m_subgroup_gids);
    }
    // All threads wait for the number of colors to be computed.
    barrier();
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
    // One thread creates the child group. The rest wait for the instance and
    // later grab a pointer to their group based on the sub-group index.
    if (sginfo.rank == qvi_subgroup_info::master_rank) {
        // Recall that 'this' is the parent group.
        rc = qvi_new(&ichild, this, sginfo);
        barrier();
    }
    else {
        barrier();
        const qvi_group_id_t mygid = m_subgroup_gids[sginfo.index];
        ichild = m_context->groupid2thgroup.at(mygid);
        ichild->retain();
    }
    // Now we can check if errors happened above.
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // Collectively finish child instance initialization.
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
    int rc = QV_SUCCESS;
    const int myrank = rank();

    barrier();
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        rc = qvi_copy(*txbuff, m_gather_data[myrank]);
        *alloc_type = QVI_BBUFF_ALLOC_SHARED_GLOBAL;
    }
    // Ensure that all threads have contributed to m_gather_data.
    barrier();

    if (qvi_unlikely(rc != QV_SUCCESS)) {
        *rxbuffs = nullptr;
        return rc;
    }
    *rxbuffs = m_gather_data;
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
        *m_scatter_data = txbuffs;
    }

    barrier();
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        rc = qvi_dup(*((*m_scatter_data)[myrank]), &mybbuff);
    }
    barrier();

    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&mybbuff);
        return rc;
    }
    *rxbuff = mybbuff;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
