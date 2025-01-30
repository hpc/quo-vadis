/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-pthread.cc
 */

#include "qvi-group-pthread.h"
#include "qvi-utils.h"

qvi_group_pthread::qvi_group_pthread(
    qvi_pthread_group_context *ctx,
    int group_size
) {
    int rc = QV_SUCCESS;
    // A context pointer was not provided, so create a new one.
    if (!ctx) {
        rc = qvi_new(&m_context);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    }
    else {
        // Else a context pointer was provided, so use it.
        m_context = ctx;
        m_context->retain();
    }
    //
    rc = qvi_new(&thgroup, m_context, group_size);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
}

qvi_group_pthread::qvi_group_pthread(
    qvi_pthread_group_context *ctx,
    qvi_pthread_group *thread_group
) {
    assert(ctx && thread_group);
    m_context = ctx;
    m_context->retain();
    // No retain() here because the calling side takes care of this. See
    // qvi_pthread_group::split() by way of qvi_group_pthread::split().
    thgroup = thread_group;
}

qvi_group_pthread::~qvi_group_pthread(void)
{
    thgroup->release();
    m_context->release();
}

int
qvi_group_pthread::self(
    qvi_group **
) {
    return QV_ERR_NOT_SUPPORTED;
}

int
qvi_group_pthread::split(
    int color,
    int key,
    qvi_group **child
) {
    // NOTE: This is a collective call across
    // ALL threads in the parent thread group.
    qvi_group_pthread *ichild = nullptr;

    qvi_pthread_group *ithgroup = nullptr;
    int rc = thgroup->split(color, key, &ithgroup);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;

    rc = qvi_new(&ichild, m_context, ithgroup);
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
