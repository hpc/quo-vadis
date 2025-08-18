/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-scope.cc
 */

#include "qvi-scope.h"
#include "qvi-rmi.h"
#include "qvi-task.h"
#include "qvi-hwpool.h"
#include "qvi-hwsplit.h"
#include "qvi-utils.h"

qv_scope::qv_scope(
    qvi_group *group,
    qvi_hwpool &hwpool
) : m_group(group)
  , m_hwpool(hwpool) { }

qv_scope::~qv_scope(void)
{
    m_group->release();
}

void
qv_scope::thread_destroy(
    qv_scope_t ***kscopes,
    uint_t k
) {
    if (!kscopes) return;
    qv_scope_t **ikscopes = *kscopes;
    for (uint_t i = 0; i < k; ++i) {
        qvi_delete(&ikscopes[i]);
    }
    delete[] ikscopes;
    *kscopes = nullptr;
}

int
qv_scope::make_intrinsic(
    qvi_group *group,
    qv_scope_intrinsic_t iscope,
    qv_scope_flags_t flags,
    qv_scope_t **scope
) {
    *scope = nullptr;
    // Get the requested intrinsic group.
    int rc = group->make_intrinsic(iscope, flags);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Get the requested intrinsic hardware pool.
    qvi_hwpool hwpool;
    rc = group->task().rmi().get_intrinsic_hwpool(
        group->pids(), iscope, flags, hwpool
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Create and initialize the scope.
    rc = qvi_new(scope, group, hwpool);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(scope);
    }
    return rc;
}

// TODO(skg) Implement use of hints.
int
qv_scope::create(
    qv_hw_obj_type_t type,
    int nobjs,
    qv_scope_create_hints_t,
    qv_scope_t **child
) {
    *child = nullptr;
    // Create underlying group. Notice the use of self here.
    qvi_group *group = nullptr;
    int rc = m_group->self(&group);
    if (rc != QV_SUCCESS) return rc;
    // Create the hardware pool.
    qvi_hwpool hwpool;
    // Get the appropriate cpuset based on the caller's request.
    qvi_hwloc_bitmap cpuset;
    rc = m_group->task().rmi().get_cpuset_for_nobjs(
        m_hwpool.cpuset().cdata(), type, nobjs, cpuset
    );
    if (rc != QV_SUCCESS) {
        qvi_delete(&group);
        return rc;
    }
    // Now that we have the desired cpuset,
    // initialize the new hardware pool.
    rc = hwpool.initialize(m_group->hwloc(), cpuset);
    if (rc != QV_SUCCESS) {
        qvi_delete(&group);
        return rc;
    }
    // Create and initialize the new scope.
    qv_scope_t *ichild = nullptr;
    rc = qvi_new(&ichild, group, hwpool);
    if (rc != QV_SUCCESS) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

qvi_group &
qv_scope::group(void) const
{
    return *m_group;
}

const qvi_hwpool &
qv_scope::hwpool(void) const
{
    return m_hwpool;
}

int
qv_scope::group_size(void) const
{
    return m_group->size();
}

int
qv_scope::group_rank(void) const
{
    return m_group->rank();
}

int
qv_scope::hwpool_nobjects(
    qv_hw_obj_type_t obj
) const {
    int result = 0;
    const int rc = m_hwpool.nobjects(m_group->hwloc(), obj, &result);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
    return result;
}

int
qv_scope::device_id(
    qv_hw_obj_type_t dev_type,
    int dev_index,
    qv_device_id_type_t format,
    char **result
) const {
    *result = nullptr;
    // Look for the requested device.
    int id = 0;
    qvi_hwpool_dev *finfo = nullptr;
    for (const auto &dinfo : m_hwpool.devices()) {
        if (dev_type != dinfo.first) continue;
        if (id++ == dev_index) {
            finfo = dinfo.second.get();
            break;
        }
    }
    if (qvi_unlikely(!finfo)) return QV_ERR_NOT_FOUND;
    // Format the device ID based on the caller's request.
    return finfo->id(format, result);
}

int
qv_scope::group_barrier(void)
{
    return m_group->barrier();
}

int
qv_scope::bind_push(void)
{
    return m_group->task().bind_push(m_hwpool.cpuset());
}

int
qv_scope::bind_pop(void)
{
    return m_group->task().bind_pop();
}

int
qv_scope::bind_string(
    qv_bind_string_flags_t flags,
    char **result
) {
    *result = nullptr;

    qvi_hwloc_bitmap bitmap;
    const int rc = m_group->task().bind_top(bitmap);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return m_group->hwloc().bind_string(bitmap.cdata(), flags, result);
}

int
qv_scope::split(
    int npieces,
    int color,
    qv_hw_obj_type_t maybe_obj_type,
    qv_scope_t **child
) {
    qvi_group *group = nullptr;
    qv_scope_t *ichild = nullptr;
    // Split the hardware resources based on the provided split parameters.
    int colorp = 0;
    qvi_hwpool hwpool;
    int rc = qvi_hwsplit::split(
        this, npieces, color, maybe_obj_type, &colorp, hwpool
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // Split underlying group. Notice the use of colorp here.
    rc = m_group->split(colorp, m_group->rank(), &group);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // Create and initialize the new scope.
    rc = qvi_new(&ichild, group, hwpool);
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&group);
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qv_scope::split_at(
    qv_hw_obj_type_t type,
    int color,
    qv_scope_t **child
) {
    return split(hwpool_nobjects(type), color, type, child);
}

int
qv_scope::thread_split(
    uint_t npieces,
    int *kcolors,
    uint_t k,
    qv_hw_obj_type_t maybe_obj_type,
    qv_scope_t ***thchildren
) {
    *thchildren = nullptr;
    const uint_t group_size = k;
    // Split the hardware, get the hardware pools.
    std::vector<qvi_hwpool> hwpools;
    std::vector<int> colorps;
    int rc = qvi_hwsplit::thread_split(
        this, npieces, kcolors, k, maybe_obj_type, colorps, hwpools
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Split off from our parent group. This call is called from a context in
    // which a process is splitting its resources across threads, so create a
    // new thread group that will be shared with each child (see below).
    qvi_group *thgroup = nullptr;
    rc = m_group->thread_split(group_size, colorps, &thgroup);
    if (rc != QV_SUCCESS) return rc;
    // Now create and populate the children.
    qv_scope_t **ithchildren = new qv_scope_t *[group_size];
    for (uint_t i = 0; i < group_size; ++i) {
        // Create and initialize the new scope.
        qv_scope_t *child = nullptr;
        rc = qvi_new(&child, thgroup, hwpools[i]);
        if (rc != QV_SUCCESS) break;
        thgroup->retain();
        ithchildren[i] = child;
    }
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qv_scope::thread_destroy(&ithchildren, k);
    }
    else {
        // Subtract one to account for the parent's
        // implicit retain during construct.
        thgroup->release();
    }
    *thchildren = ithchildren;
    return rc;
}

int
qv_scope::thread_split_at(
    qv_hw_obj_type_t type,
    int *kcolors,
    uint_t k,
    qv_scope_t ***kchildren
) {
    return thread_split(hwpool_nobjects(type), kcolors, k, type, kchildren);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
