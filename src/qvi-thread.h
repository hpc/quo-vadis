/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright © Inria 2022.  All rights reserved.                                                              
 * Copyright © Bordeaux INP 2022. All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-thread.h
 */

#ifndef QVI_THREAD_H
#define QVI_THREAD_H

#include "qvi-common.h"

#include "qvi-task.h"
#include "qvi-bbuff.h"

#ifdef __cplusplus
extern "C" {
#endif

// Type definitions.
typedef uint64_t qvi_thread_group_id_t;

// Forward declarations.
struct qvi_thread_s;
typedef struct qvi_thread_s qvi_thread_t;

struct qvi_thread_group_s;
typedef struct qvi_thread_group_s qvi_thread_group_t;

/**
 *
 */
int
qvi_thread_new(
    qvi_thread_t **th
);

/**
 *
 */
void
qvi_thread_free(
    qvi_thread_t **th
);

/**
 *
 */
int
qvi_thread_init(
    qvi_thread_t *th
);

/**
 *
 */
int
qvi_thread_finalize(
    qvi_thread_t *th
);

/**
 *
 */
int
qvi_thread_node_barrier(
    qvi_thread_t *th
);

/**
 *
 */
qvi_task_t *
qvi_thread_task_get(
    qvi_thread_t *th
);

/**
 *
 */
int
qvi_thread_group_size(
    const qvi_thread_group_t *group
);

/**
 *
 */
int
qvi_thread_group_new(
    qvi_thread_group_t **group
);

/**
 *
 */
void
qvi_thread_group_free(
    qvi_thread_group_t **group
);

/**
 *
 */
int
qvi_thread_group_id(
    const qvi_thread_group_t *group
);

/**
 *
 */
int
qvi_thread_group_create(
    qvi_thread_t *proc,
    qvi_thread_group_t **group
);

/**
 *
 */
int
qvi_thread_group_barrier(
    qvi_thread_group_t *group
);

/**
 *
 */
int
qvi_thread_group_gather_bbuffs(
    qvi_thread_group_t *group,
    qvi_bbuff_t *txbuff,
    int root,
    qvi_bbuff_t ***rxbuffs
);

/**
 *
 */
int
qvi_thread_group_scatter_bbuffs(
    qvi_thread_group_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
