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
    //
    std::unordered_map<pid_t, size_t> tid2index;
    /** Holds the appropriate number of task instances. */
    std::vector<qvi_task> m_tasks;
public:
    /** Deleted default constructor. */
    qvi_group_thread(void) = delete;
    /** Constructor. */
    qvi_group_thread(
        int nthreads,
        const std::vector<int> &
    ) : m_tasks(nthreads) {
        // Note the unused vector of ints are colors. We don't currently use
        // that information because we don't support group splits within a
        // threaded context, so ignore it. See qvi_group::thread_split().
    }
    /** Virtual destructor. */
    virtual ~qvi_group_thread(void) = default;
    /**
     * TODO(skg) Reimplement as an LRU cache of a bound size.
     * This implements a dynamic, transient TID mapping to tasks.
     * This is geared for runtimes like OpenMP.
     */
    virtual qvi_task *
    task(void) {
        static std::atomic<size_t> next_index(0);
        const pid_t mytid = qvi_gettid();

        std::lock_guard<std::mutex> guard(m_mutex);
        auto got = tid2index.find(mytid);
        if (got == tid2index.end()) {
            const size_t myindex = next_index++ % m_tasks.size();
            tid2index.insert({mytid, myindex});
            return &m_tasks.at(myindex);
        }
        return &m_tasks.at(got->second);
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
        qvi_bbuff *,
        int,
        qvi_bbuff_alloc_type_t *,
        qvi_bbuff ***
    ) const {
        // By default thread groups do not support this operation.
        return QV_ERR_NOT_SUPPORTED;
    }

    virtual int
    scatter(
        qvi_bbuff **,
        int,
        qvi_bbuff **
    ) const {
        // By default thread groups do not support this operation.
        return QV_ERR_NOT_SUPPORTED;
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
