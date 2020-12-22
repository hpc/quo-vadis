/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-task.h
 */

#ifndef QVI_TASK_H
#define QVI_TASK_H

#include "private/qvi-common.h"
#include "quo-vadis/qv-task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
int
qvi_task_construct(
    qv_task_t **task
);

/**
 *
 */
void
qvi_task_destruct(
    qv_task_t *task
);

/**
 *
 */
int
qvi_task_init(
    qv_task_t *task,
    pid_t pid,
    int64_t gid,
    int id
);

/**
 *
 */
pid_t
qvi_task_pid(
    qv_task_t *task
);

/**
 *
 */
int64_t
qvi_task_gid(
    qv_task_t *task
);

/**
 *
 */
int
qvi_task_id(
    qv_task_t *task
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
