/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-taskman.h
 *
 * Task management abstraction for processes and threads.
 */

#ifndef QVI_TASKMAN_H
#define QVI_TASKMAN_H

#include "qvi-task.h"
#include "qvi-scope.h"
// TODO(skg) Do we need RMI here?
#include "qvi-rmi.h"

/**
 * Virtual base taskman class.
 */
struct qvi_taskman_s {
    /** Base constructor that does minimal work. */
    qvi_taskman_s(void) = default;
    /** Virtual destructor. */
    virtual ~qvi_taskman_s(void) = default;
    /** The real 'constructor' that can possibly fail. */
    virtual int initialize(void) = 0;
    /** Returns pointer to the caller's task information. */
    virtual qvi_task_t *task(void) = 0;
    /** Creates a new scope from an intrinsic scope. */
    virtual int scope_create_from_intrinsic(
        qvi_rmi_client_t *rmi,
        qv_scope_intrinsic_t intrinsic,
        qv_scope_t **scope
    ) = 0;
    // TODO(skg) This shouldn't be here. Scopes should know how to split
    // themselves.
    /** Creates a new scope from a scope split. */
    virtual int scope_create_from_split(
        qvi_hwloc_t *hwloc,
        qvi_rmi_client_t *rmi,
        qv_scope_t *parent,
        int ncolors,
        int color,
        qv_scope_t **child
    ) = 0;
    /** Frees provided group. */
    virtual void group_free(qvi_group_t *group) = 0;
    /** Node-local task barrier. */
    virtual int barrier(void) = 0;
};
typedef qvi_taskman_s qvi_taskman_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
