/*
 * Copyright (c) 2021-2022 Triad National Security, LLC
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

#include "qvi-bbuff.h"
#include "qvi-task.h"

#ifdef __cplusplus
/**
 * Virtual base group class.
 */
struct qvi_group_s {
    /** Base constructor that does minimal work. */
    qvi_group_s(void) = default;
    /** Virtual destructor. */
    virtual ~qvi_group_s(void) = default;
    /** The real 'constructor' that can possibly fail. */
    virtual int create(void) = 0;
    /** Returns the caller's task_id. */
    virtual qvi_task_id_t task_id(void) = 0;
    /** Returns the caller's group ID. */
    virtual int id(void) = 0;
    /** Returns the number of members in this group. */
    virtual int size(void) = 0;
    /** Performs node-local group barrier. */
    virtual int barrier(void) = 0;
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
    /**
     * Gathers bbuffs to specified root.
     */
    virtual int
    gather(
        qvi_bbuff_t *txbuff,
        int root,
        qvi_bbuff_t ***rxbuffs,
        int *shared
    ) = 0;
    /**
     * Scatters bbuffs from specified root.
     */
    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    ) = 0;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qvi_group_s;
typedef struct qvi_group_s qvi_group_t;

/** Group ID. */
typedef uint64_t qvi_group_id_t;

/**
 *
 */
void
qvi_group_free(
    qvi_group_t **group
);

/**
 *
 */
int
qvi_group_next_id(
    qvi_group_id_t *gid
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
