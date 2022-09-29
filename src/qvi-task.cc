/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
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

static const int qvi_task_id_invalid = -1;

struct qvi_task_s {
    /** Task ID */
    qvi_task_id_t task_id;
    /** Global task ID */
    int64_t gid = qvi_task_id_invalid;
    /** Node-local task ID */
    int lid = qvi_task_id_invalid;
};

int
qvi_task_new(
    qvi_task_t **task
) {
    int rc = QV_SUCCESS;

    qvi_task_t *itask = qvi_new qvi_task_t();
    if (!itask) {
        rc = QV_ERR_OOR;
    }
    if (rc != QV_SUCCESS) {
        qvi_task_free(&itask);
    }
    *task = itask;
    return rc;
}

void
qvi_task_free(
    qvi_task_t **task
) {
    if (!task) return;
    qvi_task_t *itask = *task;
    if (!itask) goto out;
    delete itask;
out:
    *task = nullptr;
}

int
qvi_task_init(
    qvi_task_t *task,
    qvi_task_type_t type,
    pid_t who,
    int64_t gid,
    int lid
) {
    task->task_id.type = type;
    task->task_id.who  = who;
    task->gid = gid;
    task->lid = lid;
    return QV_SUCCESS;
}

qvi_task_id_t
qvi_task_task_id(
    qvi_task_t *task
) {
    return task->task_id;
}

qvi_task_type_t
qvi_task_type(
    qvi_task_t *task
) {
    return qvi_task_id_get_type(task->task_id);
}

pid_t
qvi_task_pid(
    qvi_task_t *task
) {
    return qvi_task_id_get_pid(task->task_id);
}

int64_t
qvi_task_gid(
    qvi_task_t *task
) {
    return task->gid;
}

int
qvi_task_lid(
    qvi_task_t *task
) {
    return task->lid;
}

qvi_task_type_t
qvi_task_id_get_type(
    qvi_task_id_t task_id
) {
    return task_id.type;
}

pid_t
qvi_task_id_get_pid(
    qvi_task_id_t task_id
) {
    return task_id.who;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
