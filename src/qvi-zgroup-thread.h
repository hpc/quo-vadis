/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Inria.
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Bordeaux INP.
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-zgroup-thread.h
 *
 * Thread context 'group' used for bootstrapping operations.
 * In actuality, a thread is a standalone member of its group.
 */

#ifndef QVI_ZGROUP_THREAD_H
#define QVI_ZGROUP_THREAD_H

#include "qvi-zgroup.h"
#include "qvi-thread.h"

struct qvi_zgroup_thread_s : public qvi_zgroup_s {
    /** Internal qvi_thread_t instance maintained by this zgroup. */
    qvi_thread_t *zth = nullptr;
    /** Constructor. */
    qvi_zgroup_thread_s(void)
    {
        const int rc = qvi_thread_new(&zth);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
    }
    /** Virtual destructor. */
    virtual ~qvi_zgroup_thread_s(void)
    {
        qvi_thread_free(&zth);
    }
    /** Initializes the thread group. */
    int initialize(void)
    {
        return qvi_thread_init(zth);
    }
    /** Returns a pointer to the caller's task information. */
    virtual qvi_task_t *task(void)
    {
        return qvi_thread_task_get(zth);
    }
    /** Creates an intrinsic group from an intrinsic identifier. */
    virtual int group_create_intrinsic(
        qv_scope_intrinsic_t intrinsic,
        qvi_group_t **group
    );
    /** Node-local task barrier. */
    virtual int barrier(void)
    {
        return qvi_thread_node_barrier(zth);
    }
};
typedef qvi_zgroup_thread_s qvi_zgroup_thread_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
int
qvi_zgroup_thread_new(
    qvi_zgroup_thread_t **zgroup
);

/**
 *
 */
void
qvi_zgroup_thread_free(
    qvi_zgroup_thread_t **zgroup
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
