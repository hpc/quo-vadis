/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2025 Triad National Security, LLC
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

#include "qvi-common.h"
#include "qvi-utils.h"
#include "qvi-bbuff.h"

/** Group ID type. */
using qvi_group_id_t = uint64_t;

/**
 * Virtual base group class. Notice that groups are reference counted.
 */
struct qvi_group : qvi_refc {
    /** Constructor. */
    qvi_group(void) = default;
    /** Virtual destructor. */
    virtual ~qvi_group(void) = default;
    /** Returns pointer to the caller's task information. */
    virtual qvi_task *
    task(void) = 0;
    /** Returns pointer to the task's hwloc information. */
    qvi_hwloc_t *
    hwloc(void);
    /** Returns the number of members in this group. */
    virtual int
    size(void) const = 0;
    /** Returns the caller's group rank. */
    virtual int
    rank(void) const = 0;
    /** Performs node-local group barrier. */
    virtual int
    barrier(void) const = 0;
    /** Makes the calling instance an intrinsic group. */
    virtual int
    make_intrinsic(
        qv_scope_intrinsic_t intrinsic
    ) = 0;
    /**
     * Creates a new self group with a single member: the caller.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    self(
        qvi_group **child
    ) = 0;
    /**
     * Creates a new thread group by splitting off of the calling process'
     * group.
     */
    virtual int
    thread_split(
        int nthreads,
        qvi_group **child
    );
    /**
     * Creates new groups by splitting this group based on color, key.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    split(
        int color,
        int key,
        qvi_group **child
    ) = 0;
    /** Gathers bbuffs to specified root. */
    virtual int
    gather(
        qvi_bbuff *txbuff,
        int root,
        qvi_bbuff_alloc_type_t *alloc_type,
        qvi_bbuff ***rxbuffs
    ) const = 0;
    /** Scatters bbuffs from specified root. */
    virtual int
    scatter(
        qvi_bbuff **txbuffs,
        int root,
        qvi_bbuff **rxbuff
    ) const = 0;
    /** Returns a unique group ID after each call. */
    static int
    next_id(
        qvi_group_id_t *gid
    );
    /** Populates gids with n unique group IDs after each call. */
    static int
    next_ids(
        size_t n,
        std::vector<qvi_group_id_t> &gids
    );
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
