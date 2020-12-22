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
 * @file qv-task.cc
 */

#include "private/qvi-common.h"
#include "private/qvi-task.h"
#include "private/qvi-log.h"

#include "quo-vadis/qv-task.h"

static const int qvi_task_id_invalid = -1;

// Type definition
struct qv_task_s {
    /** Global task ID */
    int64_t gid = qvi_task_id_invalid;
    /** Node-local task ID */
    int id = qvi_task_id_invalid;
    /** Process ID */
    pid_t pid = 0;
    // TODO(skg) Handle to children?
};

int
qvi_task_construct(
    qv_task_t **task
) {
    if (!task) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qv_task_t *itask = qvi_new qv_task_t;
    if (!itask) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }
    *task = itask;
out:
    if (ers) {
        qvi_log_error(ers);
        qvi_task_destruct(itask);
        *task = nullptr;
    }
    return rc;
}

void
qvi_task_destruct(
    qv_task_t *task
) {
    if (!task) return;

    delete task;
}

int
qvi_task_init(
    qv_task_t *task,
    pid_t pid,
    int64_t gid,
    int id
) {
    task->pid = pid;
    task->gid = gid;
    task->id = id;
    return QV_SUCCESS;
}

pid_t
qvi_task_pid(
    qv_task_t *task
) {
    return task->pid;
}

int64_t
qvi_task_gid(
    qv_task_t *task
) {
    return task->gid;
}

int
qvi_task_id(
    qv_task_t *task
) {
    return task->id;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
