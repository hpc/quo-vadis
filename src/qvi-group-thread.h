/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c)      2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-thread.h
 */

#ifndef QVI_GROUP_THREAD_H
#define QVI_GROUP_THREAD_H

#include "qvi-common.h"
#include "qvi-group.h"
#include "qvi-task.h"

/**
 * Base thread group class.
 */
struct qvi_group_thread : qvi_group {
private:
    /** Used for mutexy things. */
    std::mutex m_mutex;
    /** Holds the appropriate number of task instances. */
    std::vector<qvi_task> m_tasks;
    //
    qvi_lru_cache<pid_t, size_t> m_tid2index;
public:
    /** Deleted default constructor. */
    qvi_group_thread(void) = delete;
    /** Constructor. */
    qvi_group_thread(
        int nthreads,
        const std::vector<int> &
    ) : m_tasks(nthreads)
      , m_tid2index(nthreads * 8) {
        // Note the unused vector of ints are colors. We don't currently use
        // that information because we don't support group splits within a
        // threaded context, so ignore it. See qvi_group::thread_split().

        // Initialize the tasks.
        for (auto &task : m_tasks) {
            const int rc = task.connect_to_server();
            if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
        }
    }
    /** Virtual destructor. */
    virtual ~qvi_group_thread(void) = default;
    /**
     * This implements a dynamic, transient TID mapping to tasks. This is geared
     * for runtimes like OpenMP where respective parallel regions may spawn new
     * threads.
     */
    virtual qvi_task &
    task(void) {
        static std::atomic<size_t> next_index(0);
        const pid_t mytid = qvi_gettid();

        std::lock_guard<std::mutex> guard(m_mutex);

        size_t myindex = 0;
        const int rc = m_tid2index.get(mytid, myindex);

        switch (rc) {
            // Found my index.
            case QV_SUCCESS:
                break;
            // Not found, so create new index and cache it.
            case QV_ERR_NOT_FOUND:
                myindex = next_index++ % m_tasks.size();
                m_tid2index.put(mytid, myindex);
                break;
            default:
                throw qvi_runtime_error();
        }
        return m_tasks.at(myindex);
    }

    virtual int
    size(void) const {
        // By default thread groups do not support this operation.
        return QV_ERR_NOT_SUPPORTED;
    }

    virtual int
    rank(void) const {
        // By default thread groups do not support this operation.
        return QV_ERR_NOT_SUPPORTED;
    }

    virtual std::vector<pid_t>
    pids(void) const
    {
        // Use getpid() because we want to return the caller's parent's PID.
        // We can't reliably get thread TIDs because they can change during
        // execution (e.g., in OpenMP), so the best we can do is share the
        // parent process' PID. Note: don't go through task.mytid() because
        // it returns a thread ID.
        return { getpid() };
    }

    virtual int
    barrier(void) const {
        // By default thread groups do not support this operation.
        return QV_ERR_NOT_SUPPORTED;
    }

    virtual int
    make_intrinsic(
        qv_scope_intrinsic_t
    ) {
        // Not supported because a thread group cannot be created outside of
        // another group. For example, a thread_split can be called from a
        // process context, which can be an intrinsic group, but not from a
        // threaded context alone.
        return QV_ERR_NOT_SUPPORTED;
    }

    virtual int
    self(
        qvi_group **
    ) {
        // By default thread groups do not support this operation.
        return QV_ERR_NOT_SUPPORTED;
    }

    virtual int
    thread_split(
        int,
        const std::vector<int> &,
        qvi_group **
    ) {
        // By default thread groups do not support this operation.
        return QV_ERR_NOT_SUPPORTED;
    }

    virtual int
    split(
        int,
        int,
        qvi_group **
    ) {
        // By default thread groups do not support this operation.
        return QV_ERR_NOT_SUPPORTED;
    }

    virtual int
    gather(
        const qvi_bbuff &,
        int,
        std::vector<qvi_bbuff> &
    ) const {
        // By default thread groups do not support this operation.
        return QV_ERR_NOT_SUPPORTED;
    }

    virtual int
    scatter(
        const std::vector<qvi_bbuff> &,
        int,
        qvi_bbuff &
    ) const {
        // By default thread groups do not support this operation.
        return QV_ERR_NOT_SUPPORTED;
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
