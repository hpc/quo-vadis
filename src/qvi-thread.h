/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 * Copyright (c) 2022      Inria. All rights reserved.
 * Copyright (c) 2022      Bordeaux INP. All rights reserved.
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
  
// Forward declarations.
  
struct qvi_thread_group_shared_s;  
typedef struct qvi_thread_group_shared_s qvi_thread_group_shared_t;

struct qvi_thread_group_s;
typedef struct qvi_thread_group_s qvi_thread_group_t;

struct qvi_thread_s;
typedef struct qvi_thread_s qvi_thread_t;

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
    const qvi_thread_group_shared_t *group
);

/**
 *
 */
int
qvi_thread_group_new(
    qvi_thread_group_shared_t **group
);

/**
 *
 */
void
qvi_thread_group_free(
    qvi_thread_group_shared_t **group
);

/**
 *
 */
int
qvi_thread_group_id(
    const qvi_thread_group_shared_t *group
);

/**
 *
 */
int
qvi_thread_group_create(
    qvi_thread_t *proc,
    qvi_thread_group_shared_t **group
);

/**
 *
 */
int
qvi_thread_group_create_single(
    qvi_thread_t *proc,
    qvi_thread_group_t **group
);

/**
 *
 */
int
qvi_thread_group_create_single(
    qvi_thread_t *proc,
    qvi_thread_group_t **group
);

/**
 *
 */
int
qvi_thread_group_barrier(
    qvi_thread_group_shared_t *group
);

/**
 *
 */
int
qvi_thread_group_create_from_split(
    qvi_thread_t *th,
    const qvi_thread_group_t *parent,
    int color,
    int key,
    qvi_thread_group_t **child
);

/**
 *
 */
int
qvi_thread_group_create_from_split(
    qvi_thread_t *th,
    const qvi_thread_group_t *parent,
    int color,
    int key,
    qvi_thread_group_t **child
);

/**
 *
 */
int
qvi_thread_group_gather_bbuffs(
    qvi_thread_group_shared_t *group,
    qvi_bbuff_t *txbuff,
    int root,
    qvi_bbuff_t ***rxbuffs,
    int *shared_alloc
);

/**
 *
 */
int
qvi_thread_group_scatter_bbuffs(
    qvi_thread_group_shared_t *group,
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
