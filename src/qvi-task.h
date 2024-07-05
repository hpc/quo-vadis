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
 * @file qvi-task.h
 */

#ifndef QVI_TASK_H
#define QVI_TASK_H

#include "qvi-common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
int
qvi_task_new(
    qvi_task_t **task
);

void
qvi_task_free(
    qvi_task_t **task
);

qvi_rmi_client_t *
qvi_task_rmi(
    qvi_task_t *task
);

pid_t
qvi_task_id(void);

int
qvi_task_bind_push(
    qvi_task_t *task,
    hwloc_const_cpuset_t cpuset
);

int
qvi_task_bind_pop(
    qvi_task_t *task
);

int
qvi_task_bind_string(
    qvi_task_t *task,
    qv_bind_string_format_t format,
    char **str
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
