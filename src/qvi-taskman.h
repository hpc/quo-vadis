/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
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
 * Task management: processes, threads.
 */

#ifndef QVI_TASKMAN_H
#define QVI_TASKMAN_H

#include "qvi-group.h"

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
    /** */
    virtual int group_create_base(qvi_group_t **group) = 0;
    /** */
    virtual void group_free(qvi_group_t **group) = 0;
    // TODO(skg) This will change to support scope-specific barriers.
    /** Node-local task barrier. */
    virtual int barrier(void) = 0;
};
typedef qvi_taskman_s qvi_taskman_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */