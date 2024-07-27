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

// TODO(skg) Use distance API for device affinity.
// TODO(skg) Add RMI to acquire/release resources.

#include "qvi-scope.h"
#include "qvi-rmi.h"
#include "qvi-task.h"
#include "qvi-hwpool.h"
#include "qvi-split.h"
#include "qvi-utils.h"

qv_scope_s::qv_scope_s(
    qvi_group_t *group_a,
    qvi_hwpool_s *hwpool_a
) : group(group_a)
  , hwpool(hwpool_a) { }

qv_scope_s::~qv_scope_s(void)
{
    qvi_delete(&hwpool);
    group->release();
}

static int
scope_new(
    qvi_group_t *group,
    qvi_hwpool_s *hwpool,
    qv_scope_t **scope
) {
    return qvi_new(scope, group, hwpool);
}

int
qvi_scope_get(
    qvi_group_t *group,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    *scope = nullptr;
    // Get the requested intrinsic group.
    int rc = group->make_intrinsic(iscope);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Get the requested intrinsic hardware pool.
    qvi_hwpool_s *hwpool = nullptr;
    rc = qvi_rmi_get_intrinsic_hwpool(
        group->task()->rmi(), qvi_task_s::mytid(), iscope, &hwpool
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Create and initialize the scope.
    rc = scope_new(group, hwpool, scope);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_scope_delete(scope);
    }
    return rc;
}

// TODO(skg) Implement use of hints.
int
qvi_scope_create(
    qv_scope_t *parent,
    qv_hw_obj_type_t type,
    int nobjs,
    qv_scope_create_hints_t,
    qv_scope_t **child
) {
    *child = nullptr;
    // Create underlying group. Notice the use of self here.
    qvi_group_t *group = nullptr;
    int rc = parent->group->self(&group);
    if (rc != QV_SUCCESS) return rc;
    // Create the hardware pool.
    qvi_hwpool_s *hwpool = nullptr;
    rc = qvi_new(&hwpool);
    if (rc != QV_SUCCESS) {
        qvi_delete(&group);
        return rc;
    }
    // Get the appropriate cpuset based on the caller's request.
    hwloc_cpuset_t cpuset = nullptr;
    rc = qvi_rmi_get_cpuset_for_nobjs(
        parent->group->task()->rmi(),
        parent->hwpool->cpuset().cdata(),
        type, nobjs, &cpuset
    );
    if (rc != QV_SUCCESS) {
        qvi_delete(&group);
        qvi_delete(&hwpool);
        return rc;
    }
    // Now that we have the desired cpuset,
    // initialize the new hardware pool.
    rc = hwpool->initialize(parent->group->hwloc(), cpuset);
    if (rc != QV_SUCCESS) {
        qvi_delete(&group);
        qvi_delete(&hwpool);
        return rc;
    }
    // No longer needed.
    qvi_hwloc_bitmap_delete(&cpuset);
    // Create and initialize the new scope.
    qv_scope_t *ichild = nullptr;
    rc = scope_new(group, hwpool, &ichild);
    if (rc != QV_SUCCESS) {
        qvi_scope_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

void
qvi_scope_delete(
    qv_scope_t **scope
) {
    qvi_delete(scope);
}

void
qvi_scope_thdelete(
    qv_scope_t ***kscopes,
    uint_t k
) {
    if (!kscopes) return;
    qv_scope_t **ikscopes = *kscopes;
    for (uint_t i = 0; i < k; ++i) {
        qvi_scope_delete(&ikscopes[i]);
    }
    delete[] ikscopes;
    *kscopes = nullptr;
}

qvi_group_t *
qvi_scope_group(
    qv_scope_t *scope
) {
    assert(scope);
    return scope->group;
}

int
qvi_scope_group_size(
    qv_scope_t *scope,
    int *ntasks
) {
    assert(scope);
    *ntasks = scope->group->size();
    return QV_SUCCESS;
}

int
qvi_scope_group_rank(
    qv_scope_t *scope,
    int *taskid
) {
    assert(scope);
    *taskid = scope->group->rank();
    return QV_SUCCESS;
}

int
qvi_scope_nobjects(
    qv_scope_t *scope,
    qv_hw_obj_type_t obj,
    int *result
) {
    return scope->hwpool->nobjects(
        scope->group->hwloc(), obj, result
    );
}

int
qvi_scope_device_id(
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_type,
    int dev_index,
    qv_device_id_type_t format,
    char **result
) {
    *result = nullptr;
    // Look for the requested device.
    int id = 0;
    qvi_hwpool_dev_s *finfo = nullptr;
    for (const auto &dinfo : scope->hwpool->devices()) {
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
qvi_scope_barrier(
    qv_scope_t *scope
) {
    assert(scope);
    return scope->group->barrier();
}

int
qvi_scope_bind_push(
    qv_scope_t *scope
) {
    return scope->group->task()->bind_push(
        scope->hwpool->cpuset().cdata()
    );
}

int
qvi_scope_bind_pop(
    qv_scope_t *scope
) {
    return scope->group->task()->bind_pop();
}

int
qvi_scope_bind_string(
    qv_scope_t *scope,
    qv_bind_string_format_t format,
    char **result
) {
    hwloc_cpuset_t bitmap = nullptr;
    int rc = scope->group->task()->bind_top(&bitmap);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = qvi_hwloc_bitmap_string(bitmap, format, result);
    qvi_hwloc_bitmap_delete(&bitmap);
    return rc;
}

int
qvi_scope_split(
    qv_scope_t *parent,
    int npieces,
    int color,
    qv_hw_obj_type_t maybe_obj_type,
    qv_scope_t **child
) {
    int rc = QV_SUCCESS, colorp = 0;
    qvi_hwpool_s *hwpool = nullptr;
    qvi_group_t *group = nullptr;
    qv_scope_t *ichild = nullptr;
    // Split the hardware resources based on the provided split parameters.
    qvi_coll_hwsplit_s chwsplit(
        parent, npieces, color, maybe_obj_type
    );
    rc = chwsplit.split(&colorp, &hwpool);
    if (rc != QV_SUCCESS) goto out;
    // Split underlying group. Notice the use of colorp here.
    rc = parent->group->split(
        colorp, parent->group->rank(), &group
    );
    if (rc != QV_SUCCESS) goto out;
    // Create and initialize the new scope.
    rc = scope_new(group, hwpool, &ichild);
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&hwpool);
        qvi_delete(&group);
        qvi_scope_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_scope_thsplit(
    qv_scope_t *parent,
    uint_t npieces,
    int *kcolors,
    uint_t k,
    qv_hw_obj_type_t maybe_obj_type,
    qv_scope_t ***thchildren
) {
    *thchildren = nullptr;

    qvi_group_t *const pgroup = parent->group;
    const uint_t group_size = k;
    // Construct the hardware split.
    qvi_hwsplit_s hwsplit(parent, group_size, npieces, maybe_obj_type);
    // Eagerly make room for the group member information.
    hwsplit.reserve();
    // Since this is called by a single task, get its ID and associated
    // hardware affinity here, and replicate them in the following loop
    // that populates splitagg.
    //No point in doing this in a loop.
    const pid_t taskid = qvi_task_t::mytid();
    hwloc_cpuset_t task_affinity = nullptr;
    // Get the task's current affinity.
    int rc = pgroup->task()->bind_top(&task_affinity);
    if (rc != QV_SUCCESS) return rc;
    for (uint_t i = 0; i < group_size; ++i) {
        // Store requested colors in aggregate.
        hwsplit.m_colors[i] = kcolors[i];
        // Since the parent hardware pool is the resource we are splitting and
        // agg_split_* calls expect |group_size| elements, replicate by dups.
        rc = qvi_dup(*parent->hwpool, &hwsplit.m_hwpools[i]);
        if (rc != QV_SUCCESS) break;
        // Since this is called by a single task, replicate its task ID, too.
        hwsplit.m_taskids[i] = taskid;
        // Same goes for the task's affinity.
        hwsplit.m_affinities[i].set(task_affinity);
    }
    // Cleanup: we don't need task_affinity anymore.
    qvi_hwloc_bitmap_delete(&task_affinity);
    if (rc != QV_SUCCESS) return rc;
    // Split the hardware resources based on the provided split parameters.
    rc = hwsplit.split();
    if (rc != QV_SUCCESS) return rc;
    // Split off from our parent group. This call is called from a context in
    // which a process is splitting its resources across threads, so create a
    // new thread group for each child.
    qvi_group_t *thgroup = nullptr;
    rc = pgroup->thsplit(group_size, &thgroup);
    if (rc != QV_SUCCESS) return rc;
    // Now create and populate the children.
    qv_scope_t **ithchildren = new qv_scope_t *[group_size];
    for (uint_t i = 0; i < group_size; ++i) {
        // Copy out, since the hardware pools in splitagg will get freed.
        qvi_hwpool_s *hwpool = nullptr;
        rc = qvi_dup(*hwsplit.m_hwpools[i], &hwpool);
        if (rc != QV_SUCCESS) break;
        // Create and initialize the new scope.
        qv_scope_t *child = nullptr;
        rc = scope_new(thgroup, hwpool, &child);
        if (rc != QV_SUCCESS) break;
        thgroup->retain();
        ithchildren[i] = child;
    }
    if (rc != QV_SUCCESS) {
        qvi_scope_thdelete(&ithchildren, k);
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
qvi_scope_thsplit_at(
    qv_scope_t *parent,
    qv_hw_obj_type_t type,
    int *kgroup_ids,
    uint_t k,
    qv_scope_t ***kchildren
) {
    int nobj = 0;
    const int rc = qvi_scope_nobjects(parent, type, &nobj);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return qvi_scope_thsplit(parent, nobj, kgroup_ids, k, type, kchildren);
}

int
qvi_scope_split_at(
    qv_scope_t *parent,
    qv_hw_obj_type_t type,
    int color,
    qv_scope_t **child
) {
    int nobj = 0;
    const int rc = qvi_scope_nobjects(parent, type, &nobj);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return qvi_scope_split(parent, nobj, color, type, child);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
