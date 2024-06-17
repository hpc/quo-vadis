/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group.h
 */

#ifndef QVI_GROUP_H
#define QVI_GROUP_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-bbuff.h"
#include "qvi-task.h"

/** Group ID type. */
typedef uint64_t qvi_group_id_t;

/**
 * Virtual base group class.
 */
struct qvi_group_s {
    /** Constructor. */
    qvi_group_s(void) = default;
    /** Virtual destructor. */
    virtual ~qvi_group_s(void) = default;
    /** Returns pointer to the caller's task information. */
    virtual qvi_task_t *task(void) = 0;
    /** Returns the caller's group ID. */
    virtual int id(void) = 0;
    /** Returns the number of members in this group. */
    virtual int size(void) = 0;
    /** Performs node-local group barrier. */
    virtual int barrier(void) = 0;
    /** Creates a new intrinsic group from an intrinsic identifier. */
    virtual int
    intrinsic(
        qv_scope_intrinsic_t intrinsic,
        qvi_group_s **group
    ) = 0;
    /**
     * Creates a new self group with a single member: the caller.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    self(
        qvi_group_s **child
    ) = 0;
    /**
     * Creates new groups by splitting this group based on color, key.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    split(
        int color,
        int key,
        qvi_group_s **child
    ) = 0;
    /** Gathers bbuffs to specified root. */
    virtual int
    gather(
        qvi_bbuff_t *txbuff,
        int root,
        qvi_bbuff_t ***rxbuffs,
        int *shared
    ) = 0;
    /** Scatters bbuffs from specified root. */
    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    ) = 0;
    /** Returns a unique group ID after each call. */
    static int
    next_id(
        qvi_group_id_t *gid
    ) {
        // Global group ID. Note that we pad its initial value so that other
        // infrastructure (e.g., QVI_MPI_GROUP_INTRINSIC_END) will never
        // equal or exceed this value.
        static std::atomic<qvi_group_id_t> group_id(64);
        if (group_id == UINT64_MAX) {
            qvi_log_error("Group ID space exhausted");
            return QV_ERR_OOR;
        }
        *gid = group_id++;
        return QV_SUCCESS;
    }
};
typedef struct qvi_group_s qvi_group_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
