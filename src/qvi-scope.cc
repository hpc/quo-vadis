/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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
    qvi_hwpool *hwpool
) : m_group(group)
  , m_hwpool(hwpool) { }

qv_scope::~qv_scope(void)
{
    qvi_delete(&m_hwpool);
    m_group->release();
}

void
qv_scope::destroy(
    qv_scope_t **scope
) {
    qvi_delete(scope);
}

void
qv_scope::thdestroy(
    qv_scope_t ***kscopes,
    uint_t k
) {
    if (!kscopes) return;
    qv_scope_t **ikscopes = *kscopes;
    for (uint_t i = 0; i < k; ++i) {
        qv_scope::destroy(&ikscopes[i]);
    }
    delete[] ikscopes;
    *kscopes = nullptr;
}

int
qv_scope::make_intrinsic(
    qvi_group *group,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    *scope = nullptr;
    // Get the requested intrinsic group.
    int rc = group->make_intrinsic(iscope);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Get the requested intrinsic hardware pool.
    qvi_hwpool *hwpool = nullptr;
    rc = qvi_rmi_get_intrinsic_hwpool(
        group->task()->rmi(), qvi_task::mytid(), iscope, &hwpool
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Create and initialize the scope.
    rc = qvi_new(scope, group, hwpool);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qv_scope::destroy(scope);
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
    qvi_hwpool *hwpool = nullptr;
    rc = qvi_new(&hwpool);
    if (rc != QV_SUCCESS) {
        qvi_delete(&group);
        return rc;
    }
    // Get the appropriate cpuset based on the caller's request.
    hwloc_cpuset_t cpuset = nullptr;
    rc = qvi_rmi_get_cpuset_for_nobjs(
        m_group->task()->rmi(),
        m_hwpool->cpuset().cdata(),
        type, nobjs, &cpuset
    );
    if (rc != QV_SUCCESS) {
        qvi_delete(&group);
        qvi_delete(&hwpool);
        return rc;
    }
    // Now that we have the desired cpuset,
    // initialize the new hardware pool.
    rc = hwpool->initialize(m_group->hwloc(), cpuset);
    if (rc != QV_SUCCESS) {
        qvi_delete(&group);
        qvi_delete(&hwpool);
        return rc;
    }
    // No longer needed.
    qvi_hwloc_bitmap_delete(&cpuset);
    // Create and initialize the new scope.
    qv_scope_t *ichild = nullptr;
    rc = qvi_new(&ichild, group, hwpool);
    if (rc != QV_SUCCESS) {
        qv_scope::destroy(&ichild);
    }
    *child = ichild;
    return rc;
}

qvi_group *
qv_scope::group(void) const
{
    return m_group;
}

qvi_hwpool *
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
    m_hwpool->nobjects(m_group->hwloc(), obj, &result);
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
    for (const auto &dinfo : m_hwpool->devices()) {
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
    return m_group->task()->bind_push(
        m_hwpool->cpuset().cdata()
    );
}

int
qv_scope::bind_pop(void)
{
    return m_group->task()->bind_pop();
}

int
qv_scope::bind_string(
    qv_bind_string_format_t format,
    char **result
) {
    hwloc_cpuset_t bitmap = nullptr;
    int rc = m_group->task()->bind_top(&bitmap);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = qvi_hwloc_bitmap_string(bitmap, format, result);
    qvi_hwloc_bitmap_delete(&bitmap);
    return rc;
}

int
qv_scope::split(
    int npieces,
    int color,
    qv_hw_obj_type_t maybe_obj_type,
    qv_scope_t **child
) {
    int rc = QV_SUCCESS, colorp = 0;
    qvi_hwpool *hwpool = nullptr;
    qvi_group *group = nullptr;
    qv_scope_t *ichild = nullptr;
    // Split the hardware resources based on the provided split parameters.
    qvi_hwsplit_coll chwsplit(
        this, npieces, color, maybe_obj_type
    );
    rc = chwsplit.split(&colorp, &hwpool);
    if (rc != QV_SUCCESS) goto out;

    // Split underlying group. Notice the use of colorp here.
    rc = m_group->split(
        colorp, m_group->rank(), &group
    );
    if (rc != QV_SUCCESS) goto out;
    // Create and initialize the new scope.
    rc = qvi_new(&ichild, group, hwpool);
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&hwpool);
        qvi_delete(&group);
        qv_scope::destroy(&ichild);
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
    // Split the hardware, get the hardare pools.
    qvi_hwpool **hwpools = nullptr;
    int rc = qvi_hwsplit::thread_split(
        this, npieces, kcolors, k, maybe_obj_type, &hwpools
    );
    if (rc != QV_SUCCESS) return rc;
    // Split off from our parent group. This call is called from a context in
    // which a process is splitting its resources across threads, so create a
    // new thread group for each child.
    qvi_group *thgroup = nullptr;
    rc = m_group->thsplit(group_size, &thgroup);
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
    if (rc != QV_SUCCESS) {
        qv_scope::thdestroy(&ithchildren, k);
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
qv_scope::thsplit_at(
    qv_hw_obj_type_t type,
    int *kgroup_ids,
    uint_t k,
    qv_scope_t ***kchildren
) {
    return thread_split(hwpool_nobjects(type), kgroup_ids, k, type, kchildren);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
