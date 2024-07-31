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
#include "qvi-task.h" // IWYU pragma: keep
#include "qvi-utils.h"

qvi_pthread_group_s::qvi_pthread_group_s(
    int group_size
) : m_size(group_size)
{
    const int rc = pthread_barrier_init(&m_barrier, NULL, group_size);
    if (qvi_unlikely(rc != 0)) throw qvi_runtime_error();
}

void *
qvi_pthread_group_s::call_first_from_pthread_create(
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
        pthread_barrier_wait(&group->m_barrier);
    }
    else {
        pthread_barrier_wait(&group->m_barrier);
    }
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

qvi_pthread_group_s::~qvi_pthread_group_s(void)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    for (auto &tt : m_tid2task) {
        qvi_delete(&tt.second);
    }
    pthread_barrier_destroy(&m_barrier);
}

int
qvi_pthread_group_s::size(void)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_size;
}

int
qvi_pthread_group_s::rank(void)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_tid2rank.at(qvi_gettid());
}

qvi_task *
qvi_pthread_group_s::task(void)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_tid2task.at(qvi_gettid());
}

int
qvi_pthread_group_s::barrier(void)
{
    const int rc = pthread_barrier_wait(&(m_barrier));
    if (qvi_unlikely((rc != 0) && (rc != PTHREAD_BARRIER_SERIAL_THREAD))) {
        return QV_ERR_INTERNAL;
    }
    return QV_SUCCESS;
}

int
qvi_pthread_group_s::split(
    int,
    int,
    qvi_pthread_group_s **
) {
    // TODO(skg)
    return QV_ERR_NOT_SUPPORTED;
}

int
qvi_pthread_group_s::gather(
    qvi_bbuff *,
    int,
    bool *,
    qvi_bbuff ***
) {
    // TODO(skg)
    return QV_ERR_NOT_SUPPORTED;
}

int
qvi_pthread_group_s::scatter(
    qvi_bbuff **,
    int,
    qvi_bbuff **
) {
    // TODO(skg)
    return QV_ERR_NOT_SUPPORTED;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
