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
 * @file qv-task.h
 */

#ifndef QUO_VADIS_TASK_H
#define QUO_VADIS_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Global ID type */
typedef int64_t qv_task_gid_t;

struct qv_task_s;
typedef struct qv_task_s qv_task_t;

/**
 *
 */
qv_task_gid_t
qv_task_gid(
    qv_task_t *task
);

/**
 *
 */
int
qv_task_id(
    qv_task_t *task
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
