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
 * @file qvi-zgroup.h
 *
 * Zeroth group used for bootstrapping operations that may require
 * group-level participation from the tasks composing the context.
 */

#ifndef QVI_ZGROUP_H
#define QVI_ZGROUP_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-task.h"
#include "qvi-group.h"
#include "qvi-utils.h"

/**
 * Virtual base zgroup class.
 */
struct qvi_zgroup_s {
    /** Constructor. */
    qvi_zgroup_s(void) = default;
    /** Virtual destructor. */
    virtual ~qvi_zgroup_s(void) = default;
    /** Returns pointer to the caller's task information. */
    virtual qvi_task_t *task(void) = 0;
    /** Creates an intrinsic group from an intrinsic identifier. */
    virtual int group_create_intrinsic(
        qv_scope_intrinsic_t intrinsic,
        qvi_group_t **group
    ) = 0;
    /** Node-local task barrier. */
    virtual int barrier(void) = 0;
};
typedef qvi_zgroup_s qvi_zgroup_t;

static inline void
qvi_zgroup_free(
    qvi_zgroup_t **zgroup
) {
    qvi_delete(zgroup);
}

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
