/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2024      Triad National Security, LLC
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

qvi_pthread_group::qvi_pthread_group(
    int group_size,
    int //rank_in_group //unused for now
) : m_size(group_size)
{
    const int rc = pthread_barrier_init(&m_barrier, NULL, group_size);
    if (qvi_unlikely(rc != 0)) throw qvi_runtime_error();

    //C++ flavor
    /*
    m_data_g.resize(m_size);
    for (auto &p : m_data_g) {
        qvi_new(&p);
    }
    */

    m_data_g = new qvi_bbuff *[m_size]();
    for(int i = 0 ; i < group_size ; i++){
        const int rc = qvi_bbuff_new(&m_data_g[i]);
        if (qvi_unlikely(rc != 0)) throw qvi_runtime_error();
    }
    m_data_s = new qvi_bbuff**();
    m_ckrs = new qvi_subgroup_color_key_rank[m_size]();
}

void *
qvi_pthread_group::call_first_from_pthread_create(
    void *arg
) {
    const pid_t mytid = qvi_gettid();
    auto args = (qvi_pthread_group_pthread_create_args_s *)arg;
    qvi_pthread_group_t *const group = args->group;
    const qvi_pthread_routine_fun_ptr_t thread_routine = args->throutine;
    void *const th_routine_argp = args->throutine_argp;
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
        const int rc = qvi_new(&task);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
        group->m_tid2task.insert({mytid, task});
    }
    // Make sure they all finish before continuing.
    pthread_barrier_wait(&group->m_barrier);
    // Free the provided argument container.
    qvi_delete(&args);
    // Finally, call the specified thread routine.
    return thread_routine(th_routine_argp);
}

qvi_pthread_group::~qvi_pthread_group(void)
{
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

    if(m_tid2rank.empty()){
        fprintf(stdout,"[%i]=========== Empty map ! %s @ %i\n",qvi_gettid(),__FILE__,__LINE__);
        return -111;
    }
    
    return m_tid2rank.at(qvi_gettid());
}

qvi_task *
qvi_pthread_group::task(void)
{
    std::lock_guard<std::mutex> guard(m_mutex);
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
    qvi_subgroup_info *sginfo
) {
    int rank = qvi_pthread_group::rank();
    int master_rank = 0; // Choosing 0 as master.
    // Gather colors and keys from ALL threads.
    m_ckrs[rank].color = color;
    m_ckrs[rank].key   = key;
    m_ckrs[rank].rank  = rank;
    // Barrier to be sure that all threads have contributed their values.
    pthread_barrier_wait(&m_barrier);
    // Since these data are shared, only the master thread has to sort them.
    // The same goes for calculating the number of distinct colors provided.
    if(rank == master_rank){
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
        m_ckrs[rank].ncolors = color_set.size();
    }
    //All threads wait for the number of colors to be computed.
    pthread_barrier_wait(&m_barrier);
    // The number of distinct colors is the number of subgroups.
    sginfo->ngroups = m_ckrs[master_rank].ncolors;
    // Compute my sub-group size and sub-group rank.
    int group_rank = 0;
    int group_size = 0;
    for (int i = 0; i < m_size; ++i) {
        if (color != m_ckrs[i].color) continue;
        // Else we found the start of my color group.
        const int current_color = m_ckrs[i].color;
        for (int j = i; j < m_size && current_color == m_ckrs[j].color; ++j) {
            if (m_ckrs[j].rank == rank) {
                sginfo->rank = group_rank;
            }
            group_size++;
            group_rank++;
        }
        sginfo->size = group_size;
        break;
    }
    return QV_SUCCESS;
}

int
qvi_pthread_group::split(
    int color,
    int key,
    qvi_pthread_group_t **child
) {
    qvi_pthread_group_t *ichild = nullptr;

    fprintf(stdout,"[%i] ==============  pthread group split with color = %i, key = %i |  %s @ %i\n",qvi_gettid(),color, key, __FILE__,__LINE__);            
    
    qvi_subgroup_info sginfo;
    int rc = m_subgroup_info(color, key, &sginfo);

    fprintf(stdout,"[%i] ==============  pthread group split with sginfo.size = %i, sginfo.rank = %i |  %s @ %i\n",qvi_gettid(), sginfo.size, sginfo.rank, __FILE__,__LINE__);            

    
    if (qvi_likely(rc == QV_SUCCESS)) {
        rc = qvi_new(&ichild, sginfo.size, sginfo.rank);
    }
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
    qvi_alloc_type_t *shared_alloc,
    qvi_bbuff ***rxbuffs
) {
    const int rc = qvi_bbuff_copy(*txbuff, m_data_g[rank()]);
    // Need to ensure that all threads have contributed to m_data_g
    pthread_barrier_wait(&m_barrier);
    *shared_alloc = ALLOC_SHARED_GLOBAL;

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
    const int myrank = rank();

    if (rootid == myrank) {
        *m_data_s = txbuffs;
    }
    pthread_barrier_wait(&m_barrier);

    qvi_bbuff *mybbuff = nullptr;
    const int rc = qvi_bbuff_dup( *((*m_data_s)[myrank]), &mybbuff);
    pthread_barrier_wait(&m_barrier);

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
