/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-task.cc
 */

#include "qvi-common.h"
#include "qvi-task.h"
#include "qvi-log.h"

static const int qvi_task_id_invalid = -1;

// Type definition
struct qv_task_s {
    /** Global task ID */
    int64_t gid = qvi_task_id_invalid;
    /** Node-local task ID */
    int lid = qvi_task_id_invalid;
    /** Process ID */
    pid_t pid = 0;
    // TODO(skg) Handle to children?
};

int
qvi_task_new(
    qv_task_t **task
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qv_task_t *itask = qvi_new qv_task_t;
    if (!itask) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
        qvi_task_free(&itask);
    }
    *task = itask;
    return rc;
}

void
qvi_task_free(
    qv_task_t **task
) {
    qv_task_t *itask = *task;
    if (!itask) return;
    delete itask;
    *task = nullptr;
}

int
qvi_task_init(
    qv_task_t *task,
    pid_t pid,
    int64_t gid,
    int lid
) {
    task->pid = pid;
    task->gid = gid;
    task->lid = lid;
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
qvi_task_lid(
    qv_task_t *task
) {
    return task->lid;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
