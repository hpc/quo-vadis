/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-task.h
 */

#ifndef QVI_TASK_H
#define QVI_TASK_H

#include "qvi-common.h"
#include "qvi-rmi.h"
#include "qvi-hwloc.h"

using qvi_task_bind_stack = std::stack<qvi_hwloc_bitmap>;

struct qvi_task {
private:
    /** Client-side connection to the RMI. */
    qvi_rmi_client m_rmi;
    /** The task's bind stack. */
    qvi_task_bind_stack m_stack;
    /** Implements the RMI server connection. */
    int
    m_connect_to_server(void);
    /** Initializes the bind stack. */
    int
    m_init_bind_stack(void);
public:
    /** Returns the caller's thread ID. */
    static pid_t
    mytid(void);
    /** Constructor. */
    qvi_task(void) = default;
    /** Copy constructor. */
    qvi_task(const qvi_task &src) = delete;
    /** Assignment operator. */
    void
    operator=(const qvi_task &src) = delete;
    /** Destructor. */
    ~qvi_task(void) = default;
    /** Connects to the server. */
    int
    connect_to_server(void);
    /** Returns a reference to the task's RMI. */
    qvi_rmi_client &
    rmi(void);
    /** Returns a reference to the task's hwloc. */
    qvi_hwloc &
    hwloc(void);
    /**
     * Changes the task's affinity based on the provided cpuset.
     * Also stores the cpuset to the top of the task's bind stack.
     */
    int
    bind_push(
        const qvi_hwloc_bitmap &cpuset
    );
    /**
     * Removes the cpuset from the top of the bind stack
     * and changes the task's affinity to that value.
     */
    int
    bind_pop(void);
    /** Returns the task's current cpuset. */
    int
    bind_top(
        qvi_hwloc_bitmap &result
    );
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
