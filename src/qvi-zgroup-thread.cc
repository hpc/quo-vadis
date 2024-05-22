/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Inria.
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Bordeaux INP.
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-zgroup-thread.cc
 */

#include "qvi-common.h" // IWYU pragma: keep

#include "qvi-zgroup-thread.h"
#include "qvi-group-thread.h"
#include "qvi-utils.h"

int
qvi_zgroup_thread_s::group_create_intrinsic(
    qv_scope_intrinsic_t,
    qvi_group_t **group
) {
    // NOTE: the provided scope doesn't affect how
    // we create the thread group, so we ignore it.
    int rc = QV_SUCCESS;
    qvi_group_thread_s *igroup = NULL;

    igroup = qvi_new qvi_group_thread_s();
    if (!igroup) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = igroup->initialize(zth);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_thread_group_create(
        zth, &igroup->th_group
    );
out:
    if (rc != QV_SUCCESS) {
        delete igroup;
        igroup = nullptr;
    }
    *group = igroup;
    return rc;
}

int
qvi_zgroup_thread_new(
    qvi_zgroup_thread_t **zgroup
) {
    return qvi_new_rc(zgroup);
}

void
qvi_zgroup_thread_free(
    qvi_zgroup_thread_t **zgroup
) {
    qvi_delete(zgroup);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
