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
#include "qvi-bbuff.h"
#include "qvi-utils.h"

void *
qvi_pthread_group_s::call_first_from_pthread_create(
    void *arg
) {
    // TODO(skg) Cleanup.
    auto args = (qvi_pthread_group_pthread_create_args_s *)arg;
    auto group = args->group;
    auto thread_routine = args->th_routine;
    auto th_routine_argp = args->th_routine_argp;
    // Let the threads add their TIDs to the vector.
    {
        std::lock_guard<std::mutex> guard(group->m_mutex);
        group->m_tids.push_back(qvi_gettid());
    }
    // Make sure they all contribute before continuing.
    pthread_barrier_wait(&group->m_barrier);
    // Elect one thread to be the worker.
    bool worker = false;
    {
        std::lock_guard<std::mutex> guard(group->m_mutex);
        worker = group->m_tids.at(0) == qvi_gettid();
    }
    // The worker populates the TID to rank mapping, while the others wait.
    if (worker) {
        std::sort(group->m_tids.begin(), group->m_tids.end());

        for (int i = 0; i < group->m_size; ++i) {
            const pid_t tid = group->m_tids[i];
            group->m_tid2rank.insert({tid, i});
        }
        pthread_barrier_wait(&group->m_barrier);
    }
    else {
        pthread_barrier_wait(&group->m_barrier);
    }
    // Everyone can now create their task and populate the mapping table.
    {
        std::lock_guard<std::mutex> guard(group->m_mutex);
        qvi_task_t *task = nullptr;
        const int rc = qvi_new(&task);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
        group->m_tid2task.insert({qvi_gettid(), task});
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

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
