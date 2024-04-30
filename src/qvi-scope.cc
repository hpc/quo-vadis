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
#include "qvi-hwpool.h"
#include "qvi-map.h"

/** Maintains a mapping between IDs to device information. */
using id_devinfo_multimap_t = std::multimap<int, const qvi_hwpool_devinfo_s *>;

/** Scope type definition. */
struct qv_scope_s {
    int qvim_rc = QV_SUCCESS;
    /** Pointer to initialized RMI infrastructure. */
    qvi_rmi_client_t *rmi = nullptr;
    /** Task group associated with this scope instance. */
    qvi_group_t *group = nullptr;
    /** Hardware resource pool. */
    qvi_hwpool_t *hwpool = nullptr;
    /** Constructor */
    qv_scope_s(void) = default;
    /** Destructor */
    ~qv_scope_s(void)
    {
        rmi = nullptr;
        qvi_hwpool_free(&hwpool);
        qvi_group_free(&group);
    }
};

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
    qvi_hwpool_t *base_hwpool = nullptr;
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
    std::vector<qvi_task_id_t> taskids{};
    /**
     * Vector of hardware pools, one for each member of the group. Note that the
     * number of hardware pools will always match the group size and that their
     * array index corresponds to a task ID: 0 ... group_size - 1.
     */
    std::vector<qvi_hwpool_t *> hwpools{};
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
            qvi_hwpool_free(&hwpool);
        }
        hwpools.clear();
    }
    /**
     * Minimally initializes instance.
     */
    int
    init(
        qvi_rmi_client_t *rmi_a,
        qvi_hwpool_t *base_hwpool_a,
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
        const int myid = parent_scope_a->group->id();
        parent_scope = parent_scope_a;
        mycolor = mycolor_a;
        if (myid == qvi_scope_split_coll_s::rootid) {
            const uint_t group_size = parent_scope->group->size();
            gsplit.init(
                parent_scope->rmi, parent_scope->hwpool,
                group_size, split_size_a, split_at_type_a
            );
        }
        return QV_SUCCESS;
    }
};

// TODO(skg) This should live in RMI.
static int
get_nobjs_in_hwpool(
    qvi_rmi_client_t *rmi,
    qvi_hwpool_t *hwpool,
    qv_hw_obj_type_t obj,
    int *n
) {
    if (qvi_hwloc_obj_type_is_host_resource(obj)) {
        return qvi_rmi_get_nobjs_in_cpuset(
            rmi, obj, qvi_hwpool_cpuset_get(hwpool), n
        );
    }
    // TODO(skg) This should go through the RMI.
    *n = qvi_hwpool_devinfos_get(hwpool)->count(obj);
    return QV_SUCCESS;
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

    int rc = QV_SUCCESS, shared = 0;
    const uint_t group_size = group->size();
    qvi_bbuff_t *txbuff = nullptr, **bbuffs = nullptr;

    rc = qvi_bbuff_new(&txbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_bbuff_append(
        txbuff, &invalue, sizeof(TYPE)
    );
    if (rc != QV_SUCCESS) goto out;

    rc = group->gather(txbuff, root, &bbuffs, &shared);
    if (rc != QV_SUCCESS) goto out;

    if (group->id() == root) {
        outvals.resize(group_size);
        // Unpack the values.
        for (uint_t i = 0; i < group_size; ++i) {
            outvals[i] = *(TYPE *)qvi_bbuff_data(bbuffs[i]);
        }
    }
out:
    if (!shared || (shared && (group->id() == root))) {
        if (bbuffs) {
            for (uint_t i = 0; i < group_size; ++i) {
                qvi_bbuff_free(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
    }
    qvi_bbuff_free(&txbuff);
    if (rc != QV_SUCCESS) {
        // If something went wrong, just zero-initialize the values.
        outvals = {};
    }
    return rc;
}

static int
gather_hwpools(
    qvi_group_t *group,
    int root,
    qvi_hwpool_t *txpool,
    std::vector<qvi_hwpool_t *> &rxpools
) {
    int rc = QV_SUCCESS, shared = 0;
    const uint_t group_size = group->size();
    qvi_bbuff_t *txbuff = nullptr, **bbuffs = nullptr;

    rc = qvi_bbuff_new(&txbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_pack(txpool, txbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = group->gather(txbuff, root, &bbuffs, &shared);
    if (rc != QV_SUCCESS) goto out;

    if (group->id() == root) {
        rxpools.resize(group_size);
        // Unpack the hwpools.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = qvi_hwpool_unpack(
                qvi_bbuff_data(bbuffs[i]), &rxpools[i]
            );
            if (rc != QV_SUCCESS) break;
        }
    }
out:
    if (!shared || (shared && (group->id() == root))) {
        if (bbuffs) {
            for (uint_t i = 0; i < group_size; ++i) {
                qvi_bbuff_free(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
    }
    qvi_bbuff_free(&txbuff);
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
    std::vector<qvi_bbuff_t *> txbuffs{};
    qvi_bbuff_t *rxbuff = nullptr;

    if (root == group->id()) {
        const uint_t group_size = group->size();
        txbuffs.resize(group_size);
        // Pack the values.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_new(&txbuffs[i]);
            if (rc != QV_SUCCESS) break;

            rc = qvi_bbuff_append(
                txbuffs[i], &values[i], sizeof(TYPE)
            );
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) goto out;
    }

    rc = group->scatter(txbuffs.data(), root, &rxbuff);
    if (rc != QV_SUCCESS) goto out;

    *value = *(TYPE *)qvi_bbuff_data(rxbuff);
out:
    for (auto &buff : txbuffs) {
        qvi_bbuff_free(&buff);
    }
    qvi_bbuff_free(&rxbuff);
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
    const std::vector<qvi_hwpool_t *> &pools,
    qvi_hwpool_t **pool
) {
    int rc = QV_SUCCESS;
    std::vector<qvi_bbuff_t *> txbuffs{};
    qvi_bbuff_t *rxbuff = nullptr;

    if (root == group->id()) {
        const uint_t group_size = group->size();
        txbuffs.resize(group_size);
        // Pack the hwpools.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_new(&txbuffs[i]);
            if (rc != QV_SUCCESS) break;

            rc = qvi_hwpool_pack(pools[i], txbuffs[i]);
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) goto out;
    }

    rc = group->scatter(txbuffs.data(), root, &rxbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_unpack(qvi_bbuff_data(rxbuff), pool);
out:
    for (auto &buff : txbuffs) {
        qvi_bbuff_free(&buff);
    }
    qvi_bbuff_free(&rxbuff);
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(pool);
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

    std::vector<TYPE> values{};

    if (root == group->id()) {
        values.resize(group->size());
        std::fill(values.begin(), values.end(), *value);
    }
    return scatter_values(group, root, values, value);
}

static int
scope_split_coll_gather(
    qvi_scope_split_coll_s &splitcoll
) {
    qv_scope_t *const parent = splitcoll.parent_scope;

    int rc = gather_values(
        parent->group,
        qvi_scope_split_coll_s::rootid,
        parent->group->task_id(),
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

    const int myid = parent->group->id();
    const uint_t group_size = parent->group->size();
    if (myid == qvi_scope_split_coll_s::rootid) {
        splitcoll.gsplit.affinities.resize(group_size);
        for (uint_t tid = 0; tid < group_size; ++tid) {
            hwloc_cpuset_t cpuset = nullptr;
            rc = qvi_rmi_task_get_cpubind(
                parent->rmi, splitcoll.gsplit.taskids[tid], &cpuset
            );
            if (rc != QV_SUCCESS) break;

            rc = splitcoll.gsplit.affinities[tid].set(cpuset);
            // Clean up.
            qvi_hwloc_bitmap_free(&cpuset);
            if (rc != QV_SUCCESS) break;
        }
    }
    return rc;
}

static int
scope_split_coll_scatter(
    const qvi_scope_split_coll_s &splitcoll,
    int *colorp,
    qvi_hwpool_t **result
) {
    int rc = scatter_values(
        splitcoll.parent_scope->group,
        qvi_scope_split_coll_s::rootid,
        splitcoll.gsplit.colors,
        colorp
    );
    if (rc != QV_SUCCESS) return rc;

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
    hwloc_cpuset_t *result
) {
    // This shouldn't happen.
    assert(splitagg.hwpools.size() != 0);

    return qvi_hwloc_bitmap_dup(
        qvi_hwpool_cpuset_get(splitagg.hwpools[0]), result
    );
}

int
qvi_scope_new(
    qv_scope_t **scope
) {
    return qvi_new_rc(scope);
}

void
qvi_scope_free(
    qv_scope_t **scope
) {
    qvi_delete(scope);
}

void
qvi_scope_kfree(
    qv_scope_t ***kscopes,
    uint_t k
) {
    if (!kscopes) return;

    qv_scope_t **ikscopes = *kscopes;
    for (uint_t i = 0; i < k; ++i) {
        qvi_scope_free(&ikscopes[i]);
    }
    delete[] ikscopes;
    *kscopes = nullptr;
}

static int
scope_init(
    qv_scope_t *scope,
    qvi_rmi_client_t *rmi,
    qvi_group_t *group,
    qvi_hwpool_t *hwpool
) {
    assert(rmi && hwpool && scope);

    scope->rmi = rmi;
    scope->group = group;
    scope->hwpool = hwpool;
    return QV_SUCCESS;
}

hwloc_const_cpuset_t
qvi_scope_cpuset_get(
    qv_scope_t *scope
) {
    assert(scope);
    return qvi_hwpool_cpuset_get(scope->hwpool);
}

const qvi_hwpool_t *
qvi_scope_hwpool_get(
    qv_scope_t *scope
) {
    assert(scope);
    return scope->hwpool;
}

int
qvi_scope_taskid(
    qv_scope_t *scope,
    int *taskid
) {
    assert(scope);
    *taskid = scope->group->id();
    return QV_SUCCESS;
}

int
qvi_scope_ntasks(
    qv_scope_t *scope,
    int *ntasks
) {
    assert(scope);
    *ntasks = scope->group->size();
    return QV_SUCCESS;
}

int
qvi_scope_barrier(
    qv_scope_t *scope
) {
    assert(scope);
    return scope->group->barrier();
}

int
qvi_scope_get(
    qvi_zgroup_t *zgroup,
    qvi_rmi_client_t *rmi,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    qvi_group_t *group = nullptr;
    qvi_hwpool_t *hwpool = nullptr;
    // Get the requested intrinsic group.
    int rc = zgroup->group_create_intrinsic(
        iscope, &group
    );
    if (rc != QV_SUCCESS) goto out;
    // Get the requested intrinsic hardware pool.
    rc = qvi_rmi_scope_get_intrinsic_hwpool(
        rmi,
        qvi_task_task_id(zgroup->task()),
        iscope,
        &hwpool
    );
    if (rc != QV_SUCCESS) goto out;
    // Create the scope.
    rc = qvi_scope_new(scope);
    if (rc != QV_SUCCESS) goto out;
    // Initialize the scope.
    rc = scope_init(*scope, rmi, group, hwpool);
out:
    if (rc != QV_SUCCESS) {
        qvi_scope_free(scope);
        qvi_hwpool_free(&hwpool);
        qvi_group_free(&group);
    }
    return rc;
}

qvi_group_t *
qvi_scope_group_get(
    qv_scope_t *scope
) {
    assert(scope);
    return scope->group;
}

static int
apply_cpuset_mapping(
    const qvi_map_t &map,
    const qvi_hwloc_cpusets_t cpusets,
    std::vector<qvi_hwpool_t *> &hwpools,
    std::vector<int> &colors
) {
    int rc = QV_SUCCESS;

    const uint_t npools = hwpools.size();
    for (uint_t pid = 0; pid < npools; ++pid) {
        rc = qvi_hwpool_init(
            hwpools.at(pid),
            qvi_map_cpuset_at(map, cpusets, pid)
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
        rc = qvi_hwpool_release_devices(hwpool);
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
    auto dinfos = qvi_hwpool_devinfos_get(splitagg.base_hwpool);
    // Iterate over the supported device types and split them up round-robin.
    // TODO(skg) Should this be a mapping operation in qvi-map?
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        // The current device type.
        const qv_hw_obj_type_t devt = devts[i];
        // Get the number of devices.
        const uint_t ndevs = dinfos->count(devt);
        // Store device infos.
        std::vector<const qvi_hwpool_devinfo_s *> devs;
        for (const auto &dinfo : *dinfos) {
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
                rc = qvi_hwpool_add_device(
                    splitagg.hwpools[i],
                    c2d.second->type,
                    c2d.second->id,
                    c2d.second->pci_bus_id.c_str(),
                    c2d.second->uuid.c_str(),
                    c2d.second->affinity.data
                );
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
    auto dinfos = qvi_hwpool_devinfos_get(splitagg.base_hwpool);
    // Iterate over the supported device types and split them up.
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        // The current device type.
        const qv_hw_obj_type_t devt = devts[i];
        // Store device infos.
        std::vector<const qvi_hwpool_devinfo_s *> devs;
        for (const auto &dinfo : *dinfos) {
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
            rc = qvi_hwpool_add_device(
                splitagg.hwpools[pooli],
                devs[devid]->type,
                devs[devid]->id,
                devs[devid]->pci_bus_id.c_str(),
                devs[devid]->uuid.c_str(),
                devs[devid]->affinity.data
            );
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
    hwloc_cpuset_t base_cpuset = nullptr;
    rc = qvi_scope_split_agg_cpuset_dup(splitagg, &base_cpuset);
    if (rc != QV_SUCCESS) return rc;
    // Pointer to my hwloc instance.
    qvi_hwloc_t *const hwloc = qvi_rmi_client_hwloc_get(splitagg.rmi);
    // Holds the host's split cpusets.
    result.resize(splitagg.split_size);
    // Notice that we do not go through the RMI for this because this is just an
    // local, temporary splitting that is ultimately fed to another splitting
    // algorithm.
    for (uint_t chunkid = 0; chunkid < splitagg.split_size; ++chunkid) {
        rc = qvi_hwloc_split_cpuset_by_chunk_id(
            hwloc, base_cpuset, splitagg.split_size,
            chunkid, result[chunkid].data
        );
        if (rc != QV_SUCCESS) break;
    }
    qvi_hwloc_bitmap_free(&base_cpuset);
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
        return QV_ERR_INTERNAL;
    }
    // Maintains the mapping between task (consumer) IDs and resource IDs.
    qvi_map_t map{};
    rc = qvi_map_colors(map, splitagg.colors, cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Update the hardware pools and colors to reflect the new mapping.
    rc = apply_cpuset_mapping(
        map, cpusets, splitagg.hwpools, splitagg.colors
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
    int rc = get_nobjs_in_hwpool(
        splitagg.rmi, splitagg.base_hwpool, obj_type, &nobj
    );
    if (rc != QV_SUCCESS) return rc;
    // Holds the device affinities used for the split.
    result.resize(nobj);
    // Get a pointer to device infos associated with the base hardware pool.
    auto dinfos = qvi_hwpool_devinfos_get(splitagg.base_hwpool);
    uint_t affi = 0;
    for (const auto &dinfo : *dinfos) {
        // Not the type we are looking to split.
        if (obj_type != dinfo.first) continue;
        // Copy the device's affinity to our list of device affinities.
        rc = qvi_hwloc_bitmap_copy(
            dinfo.second->affinity.data, result[affi++].data
        );
        if (rc != QV_SUCCESS) break;
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
        return QV_ERR_INTERNAL;
    }
    // Update the hardware pools and colors to reflect the new mapping.
    return apply_cpuset_mapping(
        map, cpusets, splitagg.hwpools, splitagg.colors
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
    qvi_hwpool_t **result
) {
    int rc = QV_SUCCESS, rc2 = QV_SUCCESS;
    const int rootid = qvi_scope_split_coll_s::rootid, myid = parent->group->id();
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
    qvi_hwpool_t *hwpool = nullptr;
    qvi_group_t *group = nullptr;
    qv_scope_t *ichild = nullptr;
    // Split the hardware resources based on the provided split parameters.
    rc = coll_split_hardware_resources(
        parent, npieces, color, maybe_obj_type, &colorp, &hwpool
    );
    if (rc != QV_SUCCESS) goto out;
    // Split underlying group. Notice the use of colorp here.
    rc = parent->group->split(
        colorp, parent->group->id(), &group
    );
    if (rc != QV_SUCCESS) goto out;
    // Create and initialize the new scope.
    rc = qvi_scope_new(&ichild);
    if (rc != QV_SUCCESS) goto out;

    rc = scope_init(ichild, parent->rmi, group, hwpool);
out:
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(&hwpool);
        qvi_group_free(&group);
        qvi_scope_free(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_scope_ksplit(
    qv_scope_t *parent,
    uint_t npieces,
    int *kcolors,
    uint_t k,
    qv_hw_obj_type_t maybe_obj_type,
    qv_scope_t ***kchildren
) {
    if (k == 0 || !kchildren) return QV_ERR_INVLD_ARG;
    *kchildren = nullptr;

    const uint_t group_size = k;
    qvi_scope_split_agg_s splitagg{};
    int rc = splitagg.init(
        parent->rmi, parent->hwpool, group_size, npieces, maybe_obj_type
    );
    if (rc != QV_SUCCESS) return rc;
    // Since this is called by a single task, get its ID and associated hardware
    // affinity here, and replicate them in the following loop that populates
    // splitagg. No point in doing this in a loop.
    const qvi_task_id_t taskid = parent->group->task_id();
    hwloc_cpuset_t task_affinity = nullptr;
    rc = qvi_rmi_task_get_cpubind(
        parent->rmi, taskid, &task_affinity

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
        rc = qvi_hwpool_dup(parent->hwpool, &splitagg.hwpools[i]);
        if (rc != QV_SUCCESS) break;
        // Since this is called by a single task, replicate its task ID, too.
        splitagg.taskids[i] = taskid;
        // Same goes for the task's affinity.
        splitagg.affinities[i].set(task_affinity);
    }
    // Cleanup: we don't need task_affinity anymore.
    qvi_hwloc_bitmap_free(&task_affinity);
    if (rc != QV_SUCCESS) return rc;
    // Split the hardware resources based on the provided split parameters.
    rc = agg_split(splitagg);
    if (rc != QV_SUCCESS) return rc;

    // Now populate the children.
    qv_scope_t **ikchildren = qvi_new qv_scope_t*[group_size];
    if (!ikchildren) return QV_ERR_OOR;

    for (uint_t i = 0; i < group_size; ++i) {
        // Split off from our parent group. This call is usually called from a
        // context in which a process is splitting its resources across its
        // threads, so create a new self group for each child.
        qvi_group_t *group = nullptr;
        rc = parent->group->self(&group);
        if (rc != QV_SUCCESS) break;
        // Create and initialize the new scope.
        qv_scope_t *child = nullptr;
        rc = qvi_scope_new(&child);
        if (rc != QV_SUCCESS) {
            qvi_group_free(&group);
            break;
        }
        // Copy out, since the hardware pools in splitagg will get freed.
        qvi_hwpool_t *hwpool = nullptr;
        rc = qvi_hwpool_dup(splitagg.hwpools[i], &hwpool);
        if (rc != QV_SUCCESS) {
            qvi_group_free(&group);
            qvi_scope_free(&child);
            break;
        }
        rc = scope_init(child, parent->rmi, group, hwpool);
        if (rc != QV_SUCCESS) {
            qvi_hwpool_free(&hwpool);
            qvi_group_free(&group);
            qvi_scope_free(&child);
            break;
        }
        ikchildren[i] = child;
    }
    if (rc != QV_SUCCESS) {
        qvi_scope_kfree(&ikchildren, k);
    }
    *kchildren = ikchildren;
    return rc;
}

int
qvi_scope_split_at(
    qv_scope_t *parent,
    qv_hw_obj_type_t type,
    int color,
    qv_scope_t **child
) {
    int nobj = 0;
    int rc = qvi_scope_nobjs(parent, type, &nobj);
    if (rc != QV_SUCCESS) return rc;

    return qvi_scope_split(parent, nobj, color, type, child);
}

int
qvi_scope_ksplit_at(
    qv_scope_t *parent,
    qv_hw_obj_type_t type,
    int *kgroup_ids,
    uint_t k,
    qv_scope_t ***kchildren
) {
    int nobj = 0;
    int rc = qvi_scope_nobjs(parent, type, &nobj);
    if (rc != QV_SUCCESS) return rc;

    return qvi_scope_ksplit(parent, nobj, kgroup_ids, k, type, kchildren);
}

int
qvi_scope_create(
    qv_scope_t *parent,
    qv_hw_obj_type_t type,
    int nobjs,
    qv_scope_create_hint_t hint,
    qv_scope_t **child
) {
    // TODO(skg) Implement use of hints.
    QVI_UNUSED(hint);

    qvi_group_t *group = nullptr;
    qvi_hwpool_t *hwpool = nullptr;
    qv_scope_t *ichild = nullptr;
    hwloc_cpuset_t cpuset = nullptr;
    // TODO(skg) We need to acquire these resources.
    int rc = qvi_rmi_get_cpuset_for_nobjs(
        parent->rmi,
        qvi_hwpool_cpuset_get(parent->hwpool),
        type, nobjs, &cpuset
    );
    if (rc != QV_SUCCESS) goto out;
    // Now that we have the desired cpuset,
    // create a corresponding hardware pool.
    rc = qvi_hwpool_new(&hwpool);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_init(hwpool, cpuset);
    if (rc != QV_SUCCESS) goto out;
    // Create underlying group. Notice the use of self here.
    rc = parent->group->self(&group);
    if (rc != QV_SUCCESS) goto out;
    // Create and initialize the new scope.
    rc = qvi_scope_new(&ichild);
    if (rc != QV_SUCCESS) goto out;

    rc = scope_init(ichild, parent->rmi, group, hwpool);
out:
    qvi_hwloc_bitmap_free(&cpuset);
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(&hwpool);
        qvi_scope_free(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_scope_nobjs(
    qv_scope_t *scope,
    qv_hw_obj_type_t obj,
    int *n
) {
    return get_nobjs_in_hwpool(
        scope->rmi, scope->hwpool, obj, n
    );
}

int
qvi_scope_get_device_id(
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int i,
    qv_device_id_type_t id_type,
    char **dev_id
) {
    int rc = QV_SUCCESS, id = 0, nw = 0;

    qvi_hwpool_devinfo_s *finfo = nullptr;
    for (const auto &dinfo : *qvi_hwpool_devinfos_get(scope->hwpool)) {
        if (dev_obj != dinfo.first) continue;
        if (id++ == i) {
            finfo = dinfo.second.get();
            break;
        }
    }
    if (!finfo) {
        rc = QV_ERR_NOT_FOUND;
        goto out;
    }

    switch (id_type) {
        case (QV_DEVICE_ID_UUID):
            nw = asprintf(dev_id, "%s", finfo->uuid.c_str());
            break;
        case (QV_DEVICE_ID_PCI_BUS_ID):
            nw = asprintf(dev_id, "%s", finfo->pci_bus_id.c_str());
            break;
        case (QV_DEVICE_ID_ORDINAL):
            nw = asprintf(dev_id, "%d", finfo->id);
            break;
        default:
            rc = QV_ERR_INVLD_ARG;
            goto out;
    }
    if (nw == -1) rc = QV_ERR_OOR;
out:
    if (rc != QV_SUCCESS) {
        *dev_id = nullptr;
    }
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
