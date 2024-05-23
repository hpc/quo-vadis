/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-zgroup-process.h
 *
 * Process context 'group' used for bootstrapping operations.
 * In actuality, a process is a standalone member of its group.
 */

#ifndef QVI_ZGROUP_PROCESS_H
#define QVI_ZGROUP_PROCESS_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-zgroup.h"
#include "qvi-process.h"

struct qvi_zgroup_process_s : public qvi_zgroup_s {
    /** Internal qvi_process_t instance maintained by this zgroup. */
    qvi_process_t *zproc = nullptr;
    /** Constructor. */
    qvi_zgroup_process_s(void)
    {
        const int rc = qvi_process_new(&zproc);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
    }
    /** Virtual destructor. */
    virtual ~qvi_zgroup_process_s(void)
    {
        qvi_process_free(&zproc);
    }
    /** Initializes the process group. */
    int
    initialize(void)
    {
        return qvi_process_init(zproc);
    }
    /** Returns a pointer to the caller's task information. */
    virtual qvi_task_t *
    task(void)
    {
        return qvi_process_task_get(zproc);
    }
    /** Creates an intrinsic group from an intrinsic identifier. */
    virtual int
    group_create_intrinsic(
        qv_scope_intrinsic_t intrinsic,
        qvi_group_t **group
    );
    /** Node-local task barrier. */
    virtual int
    barrier(void)
    {
        return qvi_process_node_barrier(zproc);
    }
};
typedef qvi_zgroup_process_s qvi_zgroup_process_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
int
qvi_zgroup_process_new(
    qvi_zgroup_process_t **zgroup
);

/**
 *
 */
void
qvi_zgroup_process_free(
    qvi_zgroup_process_t **zgroup
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
