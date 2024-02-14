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

#include "qvi-common.h" // IWYU pragma: keep

#include "qvi-zgroup-process.h"
#include "qvi-group-process.h"

qvi_zgroup_process_s::~qvi_zgroup_process_s(void)
{
    qvi_process_free(&zproc);
}

int
qvi_zgroup_process_s::create(void)
{
    return qvi_process_new(&zproc);
}

int
qvi_zgroup_process_s::initialize(void)
{
    return qvi_process_init(zproc);
}

qvi_task_t *
qvi_zgroup_process_s::task(void)
{
    return qvi_process_task_get(zproc);
}

int
qvi_zgroup_process_s::group_create_intrinsic(
    qv_scope_intrinsic_t,
    qvi_group_t **group
) {
    // NOTE: the provided scope doesn't affect how
    // we create the process group, so we ignore it.
    int rc = QV_SUCCESS;

    qvi_group_process_t *igroup = qvi_new qvi_group_process_t();
    if (!igroup) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = igroup->initialize(zproc);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_process_group_create(
        zproc, &igroup->proc_group
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
qvi_zgroup_process_s::barrier(void)
{
    return qvi_process_node_barrier(zproc);
}

int
qvi_zgroup_process_new(
    qvi_zgroup_process_t **zgroup
) {
    int rc = QV_SUCCESS;

    qvi_zgroup_process_t *izgroup = qvi_new qvi_zgroup_process_t();
    if (!izgroup) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = izgroup->create();
out:
    if (rc != QV_SUCCESS) {
        qvi_zgroup_process_free(&izgroup);
    }
    *zgroup = izgroup;
    return rc;
}

void
qvi_zgroup_process_free(
    qvi_zgroup_process_t **zgroup
) {
    if (!zgroup) return;
    qvi_zgroup_process_t *izgroup = *zgroup;
    if (!izgroup) goto out;
    delete izgroup;
out:
    *zgroup = nullptr;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
