/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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
#include "qvi-hwloc.h"

using qvi_task_bind_stack_t = std::stack<qvi_hwloc_bitmap_s>;

struct qvi_task_s {
private:
    /** Client-side connection to the RMI. */
    qvi_rmi_client_t *myrmi = nullptr;
    /** The task's bind stack. */
    qvi_task_bind_stack_t mystack;
    /** Connects to the RMI server. */
    int
    connect_to_server(void);
    /** Initializes the bind stack. */
    int
    bind_stack_init(void);
public:
    /** Returns the caller's thread ID. */
    static pid_t
    mytid(void);
    /** Default constructor. */
    qvi_task_s(void);
    /** Copy constructor. */
    qvi_task_s(const qvi_task_s &src) = delete;
    /** Destructor. */
    ~qvi_task_s(void);
    /** Returns a pointer to the task's RMI. */
    qvi_rmi_client_t *
    rmi(void);
    /** Changes the task's affinity. */
    int
    bind_push(
        hwloc_const_cpuset_t cpuset
    );
    /** */
    int
    bind_pop(void);
    /** Returns the task's current cpuset. */
    int
    bind_top(
        hwloc_cpuset_t *dest
    );
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
