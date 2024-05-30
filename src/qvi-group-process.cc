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
#include "qvi-utils.h"

int
qvi_group_process_s::self(
    qvi_group_t **child
) {
    qvi_group_process_t *ichild = new qvi_group_process_t();
    // Initialize the child with the parent's process instance.
    int rc = ichild->initialize(proc);
    if (rc != QV_SUCCESS) goto out;
    // Because this is in the context of a process, the concept of splitting
    // doesn't really apply here, so just create another process group.
    rc = qvi_process_group_create(
        proc, &ichild->proc_group
    );
out:
    if (rc != QV_SUCCESS) {
        delete ichild;
        ichild = nullptr;
    }
    *child = ichild;
    return rc;
}

int
qvi_group_process_new(
    qvi_group_process_t **group
) {
    return qvi_new_rc(group);
}

void
qvi_group_process_free(
    qvi_group_process_t **group
) {
    qvi_delete(group);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
