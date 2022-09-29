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
 * @file qvi-group-thread.h
 *
 */

#ifndef QVI_GROUP_THREAD_H
#define QVI_GROUP_THREAD_H

#include "qvi-group.h"
#include "qvi-thread.h"

struct qvi_group_thread_s : public qvi_group_s {
    /**
     * Initialized qvi_thread_t instance
     * embedded in thread group instances.
     */
    qvi_thread_t *th = nullptr;
    /** Underlying group instance. */
    qvi_thread_group_t *th_group = nullptr;
    /** Base constructor that does minimal work. */
    qvi_group_thread_s(void) = default;
    /** Virtual destructor. */
    virtual ~qvi_group_thread_s(void);
    /** The real 'constructor' that can possibly fail. */
    virtual int create(void);
    /** Initializes the instance. */
    int initialize(qvi_thread_t *th);
    /** Returns the caller's task_id. */
    virtual qvi_task_id_t task_id(void);
    /** Returns the caller's group ID. */
    virtual int id(void);
    /** Returns the number of members in this group. */
    virtual int size(void);
    /** Performs node-local group barrier. */
    virtual int barrier(void);
    /**
     * Creates a new self group with a single member: the caller.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    self(
        qvi_group_s **child
    );
    /**
     * Creates new groups by splitting this group based on color, key.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    split(
        int color,
        int key,
        qvi_group_s **child
    );
    /**
     * Gathers bbuffs to specified root.
     */
    virtual int
    gather(
        qvi_bbuff_t *txbuff,
        int root,
        qvi_bbuff_t ***rxbuffs,
        int *shared
    );
    /**
     * Scatters bbuffs from specified root.
     */
    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    );
};
typedef qvi_group_thread_s qvi_group_thread_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
