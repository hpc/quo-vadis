/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
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

#include "qvi-common.h"

#include "qvi-zgroup-thread.h"
#include "qvi-group-thread.h"

qvi_zgroup_thread_s::~qvi_zgroup_thread_s(void)
{
    qvi_thread_free(&zth);
}

int
qvi_zgroup_thread_s::create(void)
{
    return qvi_thread_new(&zth);
}

int
qvi_zgroup_thread_s::initialize(void)
{
    return qvi_thread_init(zth);
}

qvi_task_t *
qvi_zgroup_thread_s::task(void)
{
    return qvi_thread_task_get(zth);
}

int
qvi_zgroup_thread_s::group_create_intrinsic(
    qv_scope_intrinsic_t,
    qvi_group_t **group
) {
    // NOTE: the provided scope doesn't affect how
    // we create the thread group, so we ignore it.
    int rc = QV_SUCCESS;
    qvi_group_thread_t *igroup = NULL;

    igroup = qvi_new qvi_group_thread_t();
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
qvi_zgroup_thread_s::barrier(void)
{
    return qvi_thread_node_barrier(zth);
}

int
qvi_zgroup_thread_new(
    qvi_zgroup_thread_t **zgroup
) {
    int rc = QV_SUCCESS;

    qvi_zgroup_thread_t *izgroup = qvi_new qvi_zgroup_thread_t();
    if (!izgroup) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = izgroup->create();
out:
    if (rc != QV_SUCCESS) {
        qvi_zgroup_thread_free(&izgroup);
    }
    *zgroup = izgroup;
    return rc;
}

void
qvi_zgroup_thread_free(
    qvi_zgroup_thread_t **zgroup
) {
    if (!zgroup) return;
    qvi_zgroup_thread_t *izgroup = *zgroup;

    if (!izgroup) goto out;
    delete izgroup;
out:
    *zgroup = nullptr;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
