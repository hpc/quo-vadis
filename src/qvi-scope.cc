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
#include "qvi-task.h"
#include "qvi-bbuff.h"
#include "qvi-rmi.h"
#include "qvi-bbuff-rmi.h"
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

template <typename TYPE>
static int
gather_values(
    qvi_group_t *group,
    int root,
    TYPE invalue,
    std::vector<TYPE> &outvals
) {
    static_assert(std::is_trivially_copyable<TYPE>::value, "");
    const uint_t group_size = group->size();

    qvi_bbuff_t *txbuff = nullptr;
    int rc = qvi_bbuff_new(&txbuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = txbuff->append(&invalue, sizeof(TYPE));
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_bbuff_delete(&txbuff);
        return rc;
    }
    // Gather the values to the root.
    bool shared = false;
    qvi_bbuff_t **bbuffs = nullptr;
    rc = group->gather(txbuff, root, &shared, &bbuffs);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // The root fills in the output.
    if (group->rank() == root) {
        outvals.resize(group_size);
        // Unpack the values.
        for (uint_t i = 0; i < group_size; ++i) {
            outvals[i] = *(TYPE *)bbuffs[i]->data();
        }
    }
out:
    if (!shared || (shared && (group->rank() == root))) {
        if (bbuffs) {
            for (uint_t i = 0; i < group_size; ++i) {
                qvi_bbuff_delete(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
    }
    qvi_bbuff_delete(&txbuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        // If something went wrong, just zero-initialize the values.
        outvals = {};
    }
    return rc;
}

static int
gather_hwpools(
    qvi_group_t *group,
    int root,
    qvi_hwpool_s *txpool,
    std::vector<qvi_hwpool_s *> &rxpools
) {
    const uint_t group_size = group->size();
    // Pack the hardware pool into a buffer.
    qvi_bbuff_t txbuff;
    int rc = txpool->packinto(&txbuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Gather the values to the root.
    bool shared = false;
    qvi_bbuff_t **bbuffs = nullptr;
    rc = group->gather(&txbuff, root, &shared, &bbuffs);
    if (rc != QV_SUCCESS) goto out;

    if (group->rank() == root) {
        rxpools.resize(group_size);
        // Unpack the hwpools.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_rmi_unpack(
                bbuffs[i]->data(), &rxpools[i]
            );
            if (qvi_unlikely(rc != QV_SUCCESS)) break;
        }
    }
out:
    if (!shared || (shared && (group->rank() == root))) {
        if (bbuffs) {
            for (uint_t i = 0; i < group_size; ++i) {
                qvi_bbuff_delete(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
    }
    if (rc != QV_SUCCESS) {
        // If something went wrong, just zero-initialize the pools.
        rxpools = {};
    }
    return rc;
}

template <typename TYPE>
static int
scatter_values(
    qvi_group_t *group,
    int root,
    const std::vector<TYPE> &values,
    TYPE *value
) {
    static_assert(std::is_trivially_copyable<TYPE>::value, "");

    int rc = QV_SUCCESS;
    qvi_bbuff_t *rxbuff = nullptr;

    std::vector<qvi_bbuff_t *> txbuffs(0);
    if (root == group->rank()) {
        const uint_t group_size = group->size();
        txbuffs.resize(group_size);
        // Pack the values.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_new(&txbuffs[i]);
            if (qvi_unlikely(rc != QV_SUCCESS)) break;

            rc = txbuffs[i]->append(&values[i], sizeof(TYPE));
            if (qvi_unlikely(rc != QV_SUCCESS)) break;
        }
        if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    }

    rc = group->scatter(txbuffs.data(), root, &rxbuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;

    *value = *(TYPE *)rxbuff->data();
out:
    for (auto &buff : txbuffs) {
        qvi_bbuff_delete(&buff);
    }
    qvi_bbuff_delete(&rxbuff);
    if (rc != QV_SUCCESS) {
        // If something went wrong, just zero-initialize the value.
        *value = {};
    }
    return rc;
}

static int
scatter_hwpools(
    qvi_group_t *group,
    int root,
    const std::vector<qvi_hwpool_s *> &pools,
    qvi_hwpool_s **pool
) {
    int rc = QV_SUCCESS;
    std::vector<qvi_bbuff_t *> txbuffs(0);
    qvi_bbuff_t *rxbuff = nullptr;

    if (root == group->rank()) {
        const uint_t group_size = group->size();
        txbuffs.resize(group_size);
        // Pack the hwpools.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_new(&txbuffs[i]);
            if (rc != QV_SUCCESS) break;

            rc = pools[i]->packinto(txbuffs[i]);
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) goto out;
    }

    rc = group->scatter(txbuffs.data(), root, &rxbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_bbuff_rmi_unpack(rxbuff->data(), pool);
out:
    for (auto &buff : txbuffs) {
        qvi_bbuff_delete(&buff);
    }
    qvi_bbuff_delete(&rxbuff);
    if (rc != QV_SUCCESS) {
        qvi_delete(pool);
    }
    return rc;
}

template <typename TYPE>
static int
bcast_value(
    qvi_group_t *group,
    int root,
    TYPE *value
) {
    static_assert(std::is_trivially_copyable<TYPE>::value, "");

    std::vector<TYPE> values;
    if (root == group->rank()) {
        values.resize(group->size());
        std::fill(values.begin(), values.end(), *value);
    }
    return scatter_values(group, root, values, value);
}

/**
 * Collective split structure: a collection of data relevant to split operations
 * requiring aggregated resource knowledge AND coordination between tasks in the
 * parent scope to perform a split.
 */
struct qvi_scope_split_coll_s {
    /**
     * The root task ID used for collective operations.
     * Note: We use 0 as the root because 0 will always exist.
     */
    static constexpr int rootid = 0;
    /** Points to the parent scope that we are splitting. */
    qv_scope_t *parent = nullptr;
    /** My color. */
    int mycolor = 0;
    /**
     * Stores group-global split information brought together by collective
     * operations across the members in parent_scope.
     */
    qvi_hwsplit_s gsplit;
    /** Constructor. */
    qvi_scope_split_coll_s(void) = delete;
    /** Constructor. */
    qvi_scope_split_coll_s(
        qv_scope_t *parent_a,
        uint_t split_size_a,
        int mycolor_a,
        qv_hw_obj_type_t split_at_type_a
    ) : parent(parent_a)
      , mycolor(mycolor_a)
    {
        const qvi_group_t *const pgroup = parent->group;
        if (pgroup->rank() == qvi_scope_split_coll_s::rootid) {
            gsplit = qvi_hwsplit_s(
                parent, pgroup->size(), split_size_a, split_at_type_a
            );
        }
    }
};

static int
scope_split_coll_gather(
    qvi_scope_split_coll_s &splitcoll
) {
    qv_scope_t *const parent = splitcoll.parent;

    int rc = gather_values(
        parent->group,
        qvi_scope_split_coll_s::rootid,
        qvi_task_t::mytid(),
        splitcoll.gsplit.taskids
    );
    if (rc != QV_SUCCESS) return rc;
    // Note that the result hwpools are copies, so we can modify them freely.
    rc = gather_hwpools(
        parent->group,
        qvi_scope_split_coll_s::rootid,
        parent->hwpool,
        splitcoll.gsplit.hwpools
    );
    if (rc != QV_SUCCESS) return rc;

    rc = gather_values(
        parent->group,
        qvi_scope_split_coll_s::rootid,
        splitcoll.mycolor,
        splitcoll.gsplit.colors
    );
    if (rc != QV_SUCCESS) return rc;

    const int myid = parent->group->rank();
    const uint_t group_size = parent->group->size();
    if (myid == qvi_scope_split_coll_s::rootid) {
        splitcoll.gsplit.affinities.resize(group_size);
        for (uint_t tid = 0; tid < group_size; ++tid) {
            hwloc_cpuset_t cpuset = nullptr;
            rc = parent->group->task()->bind_top(&cpuset);
            if (rc != QV_SUCCESS) break;

            rc = splitcoll.gsplit.affinities[tid].set(cpuset);
            // Clean up.
            qvi_hwloc_bitmap_delete(&cpuset);
            if (rc != QV_SUCCESS) break;
        }
    }
    return rc;
}

static int
scope_split_coll_scatter(
    const qvi_scope_split_coll_s &splitcoll,
    int *colorp,
    qvi_hwpool_s **result
) {
    const int rc = scatter_values(
        splitcoll.parent->group,
        qvi_scope_split_coll_s::rootid,
        splitcoll.gsplit.colors,
        colorp
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return scatter_hwpools(
        splitcoll.parent->group,
        qvi_scope_split_coll_s::rootid,
        splitcoll.gsplit.hwpools,
        result
    );
}

/**
 * Split the hardware resources based on the provided split parameters:
 * - npieces: The number of splits requested.
 * - color: Either user-supplied (explicitly set) or a value that requests
 *   us to do the coloring for the callers.
 *   maybe_obj_type: Potentially the object type that we are splitting at. This
 *   value influences how the splitting algorithms perform their mapping.
 * - colorp: color' is potentially a new color assignment determined by one
 *   of our coloring algorithms. This value can be used to influence the
 *   group splitting that occurs after this call completes.
 */
static int
coll_split_hardware_resources(
    qv_scope_t *parent,
    int npieces,
    int color,
    qv_hw_obj_type_t maybe_obj_type,
    int *colorp,
    qvi_hwpool_s **result
) {
    int rc2 = QV_SUCCESS;
    const int rootid = qvi_scope_split_coll_s::rootid, myid = parent->group->rank();
    // Information relevant to hardware resource splitting. Note that
    // aggregated data are only valid for the task whose id is equal to
    // qvi_global_split_t::rootid after gather has completed.
    qvi_scope_split_coll_s splitcoll(
        parent, npieces, color, maybe_obj_type
    );
    // First consolidate the provided information, as this is coming from a
    // SPMD-like context (e.g., splitting a resource shared by MPI processes).
    // In most cases it is easiest to have a single task calculate the split
    // based on global knowledge and later redistribute the calculated result to
    // its group members.
    int rc = scope_split_coll_gather(splitcoll);
    if (rc != QV_SUCCESS) goto out;
    // The root does this calculation.
    if (myid == rootid) {
        rc2 = splitcoll.gsplit.split();
    }
    // Wait for the split information. Explicitly barrier here in case the
    // underlying broadcast implementation polls heavily for completion.
    rc = splitcoll.parent->group->barrier();
    if (rc != QV_SUCCESS) goto out;
    // To avoid hangs in split error paths, share the split rc with everyone.
    rc = bcast_value(splitcoll.parent->group, rootid, &rc2);
    if (rc != QV_SUCCESS) goto out;
    // If the split failed, return the error to all callers.
    if (rc2 != QV_SUCCESS) {
        rc = rc2;
        goto out;
    }
    // Scatter the results.
    rc = scope_split_coll_scatter(splitcoll, colorp, result);
    if (rc != QV_SUCCESS) goto out;
out:
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
    rc = coll_split_hardware_resources(
        parent, npieces, color, maybe_obj_type, &colorp, &hwpool
    );
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

    qvi_hwsplit_s splitagg(
        parent, group_size, npieces, maybe_obj_type
    );
    // Eagerly make room for the group member information.
    splitagg.reserve();
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
        splitagg.colors[i] = kcolors[i];
        // Since the parent hardware pool is the resource we are splitting and
        // agg_split_* calls expect |group_size| elements, replicate by dups.
        rc = qvi_dup(*parent->hwpool, &splitagg.hwpools[i]);
        if (rc != QV_SUCCESS) break;
        // Since this is called by a single task, replicate its task ID, too.
        splitagg.taskids[i] = taskid;
        // Same goes for the task's affinity.
        splitagg.affinities[i].set(task_affinity);
    }
    // Cleanup: we don't need task_affinity anymore.
    qvi_hwloc_bitmap_delete(&task_affinity);
    if (rc != QV_SUCCESS) return rc;
    // Split the hardware resources based on the provided split parameters.
    rc = splitagg.split();
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
        rc = qvi_dup(*splitagg.hwpools[i], &hwpool);
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
