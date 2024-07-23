/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-process.cc
 */

#include "qvi-group-process.h"
#include "qvi-task.h" // IWYU pragma: keep
#include "qvi-utils.h"

qvi_group_process_s::qvi_group_process_s(void)
{
    const int rc = qvi_new(&m_task);
    if (rc != QV_SUCCESS) throw qvi_runtime_error();
}

qvi_group_process_s::~qvi_group_process_s(void)
{
    qvi_process_group_free(&m_proc_group);
    qvi_delete(&m_task);
}

int
qvi_group_process_s::self(
    qvi_group_t **child
) {
    qvi_group_process_t *ichild = nullptr;
    int rc = qvi_new(&ichild);
    if (rc != QV_SUCCESS) goto out;
    // Because this is in the context of a process, the concept of splitting
    // doesn't really apply here, so just create another process group.
    rc = qvi_process_group_new(&ichild->m_proc_group);
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
