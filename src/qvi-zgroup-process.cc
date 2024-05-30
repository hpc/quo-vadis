/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-zgroup-process.cc
 */

#include "qvi-zgroup-process.h"
#include "qvi-group-process.h"
#include "qvi-utils.h"

int
qvi_zgroup_process_s::group_create_intrinsic(
    qv_scope_intrinsic_t,
    qvi_group_t **group
) {
    // NOTE: the provided scope doesn't affect how
    // we create the process group, so we ignore it.
    qvi_group_process_t *igroup = nullptr;
    int rc = qvi_group_process_new(&igroup);
    if (rc != QV_SUCCESS) goto out;

    rc = igroup->initialize(zproc);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_process_group_create(
        zproc, &igroup->proc_group
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_group_process_free(&igroup);
    }
    *group = igroup;
    return rc;
}

int
qvi_zgroup_process_new(
    qvi_zgroup_process_t **zgroup
) {
    return qvi_new_rc(zgroup);
}

void
qvi_zgroup_process_free(
    qvi_zgroup_process_t **zgroup
) {
    qvi_delete(zgroup);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
