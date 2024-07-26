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
#include "qvi-map.h"
#include "qvi-utils.h"

/** Maintains a mapping between IDs to device information. */
using id_devinfo_multimap_t = std::multimap<int, const qvi_hwpool_dev_s *>;

struct qv_scope_s {
    /** Task group associated with this scope instance. */
    qvi_group_t *group = nullptr;
    /** Hardware resource pool. */
    qvi_hwpool_s *hwpool = nullptr;
    /** Constructor */
    qv_scope_s(void) = delete;
    /** Constructor */
    qv_scope_s(
        qvi_group_t *group_a,
        qvi_hwpool_s *hwpool_a
    ) : group(group_a)
      , hwpool(hwpool_a) { }
    /** Destructor */
    ~qv_scope_s(void)
    {
        qvi_delete(&hwpool);
        group->release();
    }
};

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
    int rc = txpool->packto(&txbuff);
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

            rc = pools[i]->packto(txbuffs[i]);
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
 * Split aggregation: a collection of data relevant to split operations
 * requiring aggregated (e.g., global) knowledge to perform a split.
 *
 * NOTE: since splitting and mapping operations are performed by a single
 * process, this structure does not support collective operations that require
 * coordination between cooperating tasks. The structure for that is
 * qvi_scope_coll_data_s. Typically, collective operations will fill in a
 * qvi_scope_split_agg_s, but that isn't a requirement.
 */
struct qvi_scope_split_agg_s {
    /**
     * A pointer to my RMI.
     */
    qvi_rmi_client_t *rmi = nullptr;
    /**
     * The base hardware pool we are splitting.
     */
    qvi_hwpool_s *base_hwpool = nullptr;
    /**
     * The number of members that are part of the split.
     */
    uint_t group_size = 0;
    /**
     * The number of pieces in the split.
     */
    uint_t split_size = 0;
    /**
     * The potential hardware resource that we are splitting at. QV_HW_OBJ_LAST
     * indicates that we are called from a split() context. Any other hardware
     * resource type indicates that we are splitting at that type: called from a
     * split_at() context.
     */
    qv_hw_obj_type_t split_at_type{};
    /**
     * Vector of task IDs, one for each member of the group. Note that the
     * number of task IDs will always match the group size and that their array
     * index corresponds to a task ID. It is handy to have the task IDs for
     * splitting so we can query task characteristics during a splitting.
     */
    std::vector<pid_t> taskids{};
    /**
     * Vector of hardware pools, one for each member of the group. Note that the
     * number of hardware pools will always match the group size and that their
     * array index corresponds to a task ID: 0 ... group_size - 1.
     */
    std::vector<qvi_hwpool_s *> hwpools{};
    /**
     * Vector of colors, one for each member of the group. Note that the number
     * of colors will always match the group size and that their array index
     * corresponds to a task ID.
     */
    std::vector<int> colors{};
    /**
     * Vector of task affinities.
     */
    qvi_hwloc_cpusets_t affinities{};
    /**
     * Constructor.
     */
    qvi_scope_split_agg_s(void) = default;
    /**
     * Destructor
     */
    ~qvi_scope_split_agg_s(void)
    {
        for (auto &hwpool : hwpools) {
            qvi_delete(&hwpool);
        }
    }
    /**
     * Minimally initializes instance.
     */
    int
    init(
        qvi_rmi_client_t *rmi_a,
        qvi_hwpool_s *base_hwpool_a,
        uint_t group_size_a,
        uint_t split_size_a,
        qv_hw_obj_type_t split_at_type_a
    ) {
        rmi = rmi_a;
        base_hwpool = base_hwpool_a;
        group_size = group_size_a;
        split_size = split_size_a;
        split_at_type = split_at_type_a;
        // To save memory we don't eagerly resize our vectors to group_size
        // since most processes will not use the storage. For example, in the
        // collective case the root ID process will be the only one needing
        // group_size elements in our vectors. We'll let the call paths enforce
        // appropriate vector sizing.
        return QV_SUCCESS;
    }
};

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
    /**
     * Points to the parent scope that we are splitting.
     */
    qv_scope_t *parent_scope = nullptr;
    /**
     * My color.
     */
    int mycolor = 0;
    /**
     * Stores group-global split information brought together by collective
     * operations across the members in parent_scope.
     */
    qvi_scope_split_agg_s gsplit{};
    /**
     * Initializes instance.
     */
    int
    init(
        qv_scope_t *parent_scope_a,
        uint_t split_size_a,
        int mycolor_a,
        qv_hw_obj_type_t split_at_type_a
    ) {
        const int myid = parent_scope_a->group->rank();
        parent_scope = parent_scope_a;
        mycolor = mycolor_a;
        if (myid == qvi_scope_split_coll_s::rootid) {
            const uint_t group_size = parent_scope->group->size();
            gsplit.init(
                parent_scope->group->task()->rmi(), parent_scope->hwpool,
                group_size, split_size_a, split_at_type_a
            );
        }
        return QV_SUCCESS;
    }
};

static int
scope_split_coll_gather(
    qvi_scope_split_coll_s &splitcoll
) {
    qv_scope_t *const parent = splitcoll.parent_scope;

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
            rc = qvi_rmi_cpubind(
                parent->group->task()->rmi(),
                splitcoll.gsplit.taskids[tid], &cpuset
            );
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
        splitcoll.parent_scope->group,
        qvi_scope_split_coll_s::rootid,
        splitcoll.gsplit.colors,
        colorp
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return scatter_hwpools(
        splitcoll.parent_scope->group,
        qvi_scope_split_coll_s::rootid,
        splitcoll.gsplit.hwpools,
        result
    );
}

/**
 * Returns a copy of the aggregate cpuset. Note that the cpuset will be shared
 * among the aggregate, but other resources may be distributed differently.
 * For example, some hardware pools may have GPUs, while others may not.
 */
static int
qvi_scope_split_agg_cpuset_dup(
    const qvi_scope_split_agg_s &splitagg,
    qvi_hwloc_bitmap_s &result
) {
    // This shouldn't happen.
    assert(splitagg.hwpools.size() != 0);

    result = splitagg.hwpools[0]->cpuset();
    return QV_SUCCESS;
}

static int
apply_cpuset_mapping(
    qvi_hwloc_t *hwloc,
    const qvi_map_t &map,
    const qvi_hwloc_cpusets_t cpusets,
    std::vector<qvi_hwpool_s *> &hwpools,
    std::vector<int> &colors
) {
    int rc = QV_SUCCESS;

    const uint_t npools = hwpools.size();
    for (uint_t pid = 0; pid < npools; ++pid) {
        rc = hwpools.at(pid)->initialize(
            hwloc, qvi_map_cpuset_at(map, cpusets, pid)
        );
        if (rc != QV_SUCCESS) break;
    }
    if (rc != QV_SUCCESS) {
        // Invalidate colors
        colors.clear();
    }
    else {
        colors = qvi_map_flatten_to_colors(map);
    }
    return rc;
}

static qvi_map_fn_t
split_agg_get_affinity_preserving_policy(
    const qvi_scope_split_agg_s &splitagg
) {
    switch (splitagg.split_at_type) {
        // For split()
        case QV_HW_OBJ_LAST:
            return qvi_map_packed;
        // For split_at()
        default:
            return qvi_map_spread;
    }
}

/**
 * Releases all devices contained in the provided split aggregate.
 */
static int
agg_split_release_devices(
    qvi_scope_split_agg_s &splitagg
) {
    int rc = QV_SUCCESS;
    for (auto &hwpool : splitagg.hwpools) {
        rc = hwpool->release_devices();
        if (rc != QV_SUCCESS) return rc;
    }
    return rc;
}

/**
 * Straightforward user-defined device splitting.
 */
// TODO(skg) Plenty of opportunity for optimization.
// TODO(skg) Move lots of logic to map
static int
agg_split_devices_user_defined(
    qvi_scope_split_agg_s &splitagg
) {
    const uint_t group_size = splitagg.group_size;
    // Release devices from the hardware pools because
    // they will be redistributed in the next step.
    int rc = agg_split_release_devices(splitagg);
    if (rc != QV_SUCCESS) return rc;
    // Determine mapping of colors to task IDs. The array index i of colors is
    // the color requested by task i. Also determine the number of distinct
    // colors provided in the colors array.
    std::set<int> color_set(splitagg.colors.begin(), splitagg.colors.end());
    // Adjust the color set so that the distinct colors provided
    // fall within the range of the number of splits requested.
    std::set<int> color_setp;
    uint_t ncolors_chosen = 0;
    for (const auto &c : color_set) {
        if (ncolors_chosen >= splitagg.split_size) break;
        color_setp.insert(c);
        ncolors_chosen++;
    }
    // Cache all device infos associated with the parent hardware pool.
    auto dinfos = splitagg.base_hwpool->devices();
    // Iterate over the supported device types and split them up round-robin.
    // TODO(skg) Should this be a mapping operation in qvi-map?
    for (const auto devt : qvi_hwloc_supported_devices()) {
        // Get the number of devices.
        const uint_t ndevs = dinfos.count(devt);
        // Store device infos.
        std::vector<const qvi_hwpool_dev_s *> devs;
        for (const auto &dinfo : dinfos) {
            // Not the type we are currently dealing with.
            if (devt != dinfo.first) continue;
            devs.push_back(dinfo.second.get());
        }
        // Maps colors to device information.
        id_devinfo_multimap_t devmap;
        uint_t devi = 0;
        while (devi < ndevs) {
            for (const auto &c : color_setp) {
                if (devi >= ndevs) break;
                devmap.insert(std::make_pair(c, devs[devi++]));
            }
        }
        // Now that we have the mapping of colors to devices, assign devices to
        // the associated hardware pools.
        for (uint_t i = 0; i < group_size; ++i) {
            const int color = splitagg.colors[i];
            for (const auto &c2d : devmap) {
                if (c2d.first != color) continue;
                rc = splitagg.hwpools[i]->add_device(*c2d.second);
                if (rc != QV_SUCCESS) break;
            }
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) break;
    }
    return rc;
}

/**
 * Affinity preserving device splitting.
 */
static int
agg_split_devices_affinity_preserving(
    qvi_scope_split_agg_s &splitagg
) {
    // Release devices from the hardware pools because
    // they will be redistributed in the next step.
    int rc = agg_split_release_devices(splitagg);
    if (rc != QV_SUCCESS) return rc;
    // Get a pointer to device infos associated with the parent hardware pool.
    auto dinfos = splitagg.base_hwpool->devices();
    // Iterate over the supported device types and split them up.
    for (const auto devt : qvi_hwloc_supported_devices()) {
        // Store device infos.
        std::vector<const qvi_hwpool_dev_s *> devs;
        for (const auto &dinfo : dinfos) {
            // Not the type we are currently dealing with.
            if (devt != dinfo.first) continue;
            devs.push_back(dinfo.second.get());
        }
        // Store device affinities.
        qvi_hwloc_cpusets_t devaffs;
        for (auto &dev : devs) {
            devaffs.push_back(dev->affinity);
        }

        qvi_map_t map;
        const auto policy = split_agg_get_affinity_preserving_policy(splitagg);
        rc = qvi_map_affinity_preserving(
            map, policy, devaffs, splitagg.affinities
        );
        if (rc != QV_SUCCESS) return rc;
#if 0
        qvi_map_debug_dump(map);
#endif
        // Now that we have the mapping, assign
        // devices to the associated hardware pools.
        for (const auto &mi : map) {
            const uint_t devid = mi.first;
            const uint_t pooli = mi.second;
            rc = splitagg.hwpools[pooli]->add_device(*devs[devid]);
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) break;
    }
    return rc;
}

/**
 * Performs a straightforward splitting of the provided cpuset: split the
 * provided base cpuset into splitagg.split_size distinct pieces.
 */
static int
agg_split_cpuset(
    const qvi_scope_split_agg_s &splitagg,
    qvi_hwloc_cpusets_t &result
) {
    int rc = QV_SUCCESS;
    // The cpuset that we are going to split.
    qvi_hwloc_bitmap_s base_cpuset;
    rc = qvi_scope_split_agg_cpuset_dup(splitagg, base_cpuset);
    if (rc != QV_SUCCESS) return rc;
    // Pointer to my hwloc instance.
    qvi_hwloc_t *const hwloc = qvi_rmi_client_hwloc(splitagg.rmi);
    // Holds the host's split cpusets.
    result.resize(splitagg.split_size);
    // Notice that we do not go through the RMI for this because this is just an
    // local, temporary splitting that is ultimately fed to another splitting
    // algorithm.
    for (uint_t chunkid = 0; chunkid < splitagg.split_size; ++chunkid) {
        rc = qvi_hwloc_split_cpuset_by_chunk_id(
            hwloc, base_cpuset.cdata(), splitagg.split_size,
            chunkid, result[chunkid].data()
        );
        if (rc != QV_SUCCESS) break;
    }
    return rc;
}

/**
 * User-defined split.
 */
static int
agg_split_user_defined(
    qvi_scope_split_agg_s &splitagg
) {
    const uint_t split_size = splitagg.split_size;
    // Split the base cpuset into the requested number of pieces.
    qvi_hwloc_cpusets_t cpusets;
    int rc = agg_split_cpuset(splitagg, cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Developer sanity check.
    if (cpusets.size() != split_size) {
        qvi_abort();
    }
    // Maintains the mapping between task (consumer) IDs and resource IDs.
    qvi_map_t map{};
    rc = qvi_map_colors(map, splitagg.colors, cpusets);
    if (rc != QV_SUCCESS) return rc;
    qvi_hwloc_t *const hwloc = qvi_rmi_client_hwloc(splitagg.rmi);
    // Update the hardware pools and colors to reflect the new mapping.
    rc = apply_cpuset_mapping(
        hwloc, map, cpusets, splitagg.hwpools, splitagg.colors
    );
    if (rc != QV_SUCCESS) return rc;
    // Use a straightforward device splitting algorithm based on user's request.
    return agg_split_devices_user_defined(splitagg);
}

static int
agg_split_get_new_osdev_cpusets(
    const qvi_scope_split_agg_s &splitagg,
    qvi_hwloc_cpusets_t &result
) {
    // The target object type.
    const qv_hw_obj_type_t obj_type = splitagg.split_at_type;
    // Get the number of devices we have available in the provided scope.
    int nobj = 0;
    int rc = splitagg.base_hwpool->nobjects(
        qvi_rmi_client_hwloc(splitagg.rmi), obj_type, &nobj
    );
    if (rc != QV_SUCCESS) return rc;
    // Holds the device affinities used for the split.
    result.resize(nobj);
    // Get a pointer to device infos associated with the base hardware pool.
    auto dinfos = splitagg.base_hwpool->devices();
    uint_t affi = 0;
    for (const auto &dinfo : dinfos) {
        // Not the type we are looking to split.
        if (obj_type != dinfo.first) continue;
        // Copy the device's affinity to our list of device affinities.
        result[affi++] = dinfo.second->affinity;
    }
    return rc;
}

static int
agg_split_get_primary_cpusets(
    const qvi_scope_split_agg_s &splitagg,
    qvi_hwloc_cpusets_t &result
) {
    const qv_hw_obj_type_t obj_type = splitagg.split_at_type;
    // We were provided a real host resource type that we have to split. Or
    // QV_HW_OBJ_LAST is instead provided to indicate that we were called from a
    // split() context, which uses the host's cpuset to split the resources.
    if (qvi_hwloc_obj_type_is_host_resource(obj_type) ||
        obj_type == QV_HW_OBJ_LAST) {
        return agg_split_cpuset(splitagg, result);
    }
    // An OS device.
    else {
        return agg_split_get_new_osdev_cpusets(splitagg, result);
    }
}

static int
agg_split_affinity_preserving_pass1(
    qvi_scope_split_agg_s &splitagg
) {
    const uint_t group_size = splitagg.group_size;
    // cpusets used for first mapping pass.
    qvi_hwloc_cpusets_t cpusets{};
    // Get the primary cpusets used for the first pass of mapping.
    int rc = agg_split_get_primary_cpusets(splitagg, cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Maintains the mapping between task (consumer) IDs and resource IDs.
    qvi_map_t map{};
    // Map tasks based on their affinity to resources encoded by the cpusets.
    const auto policy = split_agg_get_affinity_preserving_policy(splitagg);
    rc = qvi_map_affinity_preserving(
        map, policy, splitagg.affinities, cpusets
    );
    if (rc != QV_SUCCESS) return rc;
    // Make sure that we mapped all the tasks. If not, this is a bug.
    if (qvi_map_nfids_mapped(map) != group_size) {
        qvi_abort();
    }
    qvi_hwloc_t *const hwloc = qvi_rmi_client_hwloc(splitagg.rmi);
    // Update the hardware pools and colors to reflect the new mapping.
    return apply_cpuset_mapping(
        hwloc, map, cpusets, splitagg.hwpools, splitagg.colors
    );
}

/**
 * Affinity preserving split.
 */
// TODO(skg) This needs more work.
static int
agg_split_affinity_preserving(
    qvi_scope_split_agg_s &splitagg
) {
    int rc = agg_split_affinity_preserving_pass1(splitagg);
    if (rc != QV_SUCCESS) return rc;
    // Finally, split the devices.
    return agg_split_devices_affinity_preserving(splitagg);
}

/**
 * Takes a vector of colors and clamps their values to [0, ndc)
 * in place, where ndc is the number of distinct numbers found in values.
 */
static int
clamp_colors(
    std::vector<int> &values
) {
    // Recall: sets are ordered.
    std::set<int> valset(values.begin(), values.end());
    // Maps the input vector colors to their clamped values.
    std::map<int, int> colors2clamped;
    // color': the clamped color.
    int colorp = 0;
    for (auto val : valset) {
        colors2clamped.insert({val, colorp++});
    }
    for (uint_t i = 0; i < values.size(); ++i) {
        values[i] = colors2clamped[values[i]];
    }
    return QV_SUCCESS;
}

/**
 * Splits aggregate scope data.
 */
static int
agg_split(
    qvi_scope_split_agg_s &splitagg
) {
    int rc = QV_SUCCESS;
    bool auto_split = false;
    // Make sure that the supplied colors are consistent and determine the type
    // of coloring we are using. Positive values denote an explicit coloring
    // provided by the caller. Negative values are reserved for internal
    // use and shall be constants defined in quo-vadis.h. Note we don't sort the
    // splitagg's colors directly because they are ordered by task ID.
    std::vector<int> tcolors(splitagg.colors);
    std::sort(tcolors.begin(), tcolors.end());
    // We have a few possibilities here:
    // * The values are all positive: user-defined split, but we have to clamp
    //   their values to a usable range for internal consumption.
    // * The values are negative and equal:
    //   - All the same, valid auto split constant: auto split
    //   - All the same, undefined constant: user-defined split, but this is a
    //     strange case since all participants will get empty sets.
    // * A mix if positive and negative values:
    //   - A strict subset is QV_SCOPE_SPLIT_UNDEFINED: user-defined split
    //   - A strict subset is not QV_SCOPE_SPLIT_UNDEFINED: return error code.

    // All colors are positive.
    if (tcolors.front() >= 0) {
        rc = clamp_colors(splitagg.colors);
        if (rc != QV_SUCCESS) return rc;
    }
    // Some values are negative.
    else {
        // TODO(skg) Implement the rest.
        if (tcolors.front() != tcolors.back()) {
            return QV_ERR_INVLD_ARG;
        }
        auto_split = true;
    }
    // User-defined splitting.
    if (!auto_split) {
        return agg_split_user_defined(splitagg);
    }
    // Automatic splitting.
    switch (splitagg.colors[0]) {
        case QV_SCOPE_SPLIT_AFFINITY_PRESERVING:
            return agg_split_affinity_preserving(splitagg);
        default:
            rc = QV_ERR_INVLD_ARG;
            break;
    }
    return rc;
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
    int rc = QV_SUCCESS, rc2 = QV_SUCCESS;
    const int rootid = qvi_scope_split_coll_s::rootid, myid = parent->group->rank();
    // Information relevant to hardware resource splitting. Note that
    // aggregated data are only valid for the task whose id is equal to
    // qvi_global_split_t::rootid after gather has completed.
    qvi_scope_split_coll_s splitcoll{};
    rc = splitcoll.init(parent, npieces, color, maybe_obj_type);
    if (rc != QV_SUCCESS) goto out;
    // First consolidate the provided information, as this is coming from a
    // SPMD-like context (e.g., splitting a resource shared by MPI processes).
    // In most cases it is easiest to have a single task calculate the split
    // based on global knowledge and later redistribute the calculated result to
    // its group members.
    rc = scope_split_coll_gather(splitcoll);
    if (rc != QV_SUCCESS) goto out;
    // The root does this calculation.
    if (myid == rootid) {
        rc2 = agg_split(splitcoll.gsplit);
    }
    // Wait for the split information. Explicitly barrier here in case the
    // underlying broadcast implementation polls heavily for completion.
    rc = splitcoll.parent_scope->group->barrier();
    if (rc != QV_SUCCESS) goto out;
    // To avoid hangs in split error paths, share the split rc with everyone.
    rc = bcast_value(splitcoll.parent_scope->group, rootid, &rc2);
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

    const uint_t group_size = k;
    qvi_scope_split_agg_s splitagg{};
    int rc = splitagg.init(
        parent->group->task()->rmi(), parent->hwpool,
        group_size, npieces, maybe_obj_type
    );
    if (rc != QV_SUCCESS) return rc;
    // Since this is called by a single task, get its ID and associated hardware
    // affinity here, and replicate them in the following loop that populates
    // splitagg.
    //No point in doing this in a loop.
    const pid_t taskid = qvi_task_t::mytid();
    hwloc_cpuset_t task_affinity = nullptr;
    rc = qvi_rmi_cpubind(
        parent->group->task()->rmi(),
        taskid, &task_affinity
    );
    if (rc != QV_SUCCESS) return rc;
    // Now populate the relevant data before attempting a split.
    splitagg.colors.resize(group_size);
    splitagg.hwpools.resize(group_size);
    splitagg.taskids.resize(group_size);
    splitagg.affinities.resize(group_size);
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
    if (rc != QV_SUCCESS) return rc;
    // Cleanup: we don't need task_affinity anymore.
    qvi_hwloc_bitmap_delete(&task_affinity);
    if (rc != QV_SUCCESS) return rc;
    // Split the hardware resources based on the provided split parameters.
    rc = agg_split(splitagg);
    if (rc != QV_SUCCESS) return rc;
    // Split off from our parent group. This call is called from a context in
    // which a process is splitting its resources across threads, so create a
    // new thread group for each child.
    qvi_group_t *thgroup = nullptr;
    rc = parent->group->thsplit(group_size, &thgroup);
    if (rc != QV_SUCCESS) return rc;
    // Now create and populate the children.
    qv_scope_t **ithchildren = new qv_scope_t *[group_size];
    for (uint_t i = 0; i < group_size; ++i) {
        // Copy out, since the hardware pools in splitagg will get freed.
        qvi_hwpool_s *hwpool = nullptr;
        rc = qvi_dup(*splitagg.hwpools[i], &hwpool);
        if (rc != QV_SUCCESS) {
            qvi_delete(&thgroup);
            break;
        }
        // Create and initialize the new scope.
        qv_scope_t *child = nullptr;
        rc = scope_new(thgroup, hwpool, &child);
        if (rc != QV_SUCCESS) {
            qvi_delete(&thgroup);
            break;
        }
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
