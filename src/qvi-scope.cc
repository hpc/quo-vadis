/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2023 Triad National Security, LLC
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

#include "qvi-common.h"

#include "qvi-scope.h"
#include "qvi-rmi.h"
#include "qvi-hwpool.h"
#include "qvi-map.h"

/** Maintains a mapping between IDs to device information. */
using id_devinfo_multimap_t = std::multimap<int, const qvi_devinfo_t *>;

/** Scope type definition. */
struct qv_scope_s {
    /** Pointer to initialized RMI infrastructure. */
    qvi_rmi_client_t *rmi = nullptr;
    /** Task group associated with this scope instance. */
    qvi_group_t *group = nullptr;
    /** Hardware resource pool. */
    qvi_hwpool_t *hwpool = nullptr;
};

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
    std::vector<qvi_bbuff_t *> txbuffs = {};
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
    std::vector<qvi_bbuff_t *> txbuffs = {};
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

    std::vector<TYPE> values = {};

    if (root == group->id()) {
        values.resize(group->size());
        std::fill(values.begin(), values.end(), *value);
    }
    return scatter_values(group, root, values, value);
}

/**
 * The collection of data relevant to split
 * operations requiring global knowledge.
 */
struct qvi_global_split_t {
    /**
     * The root task ID used for collective operations.
     * Note: We use 0 as the root because 0 will always exist.
     */
    static const int rootid = 0;
    /**
     * Points to the parent scope that we are splitting.
     */
    qv_scope_t *parent_scope = nullptr;
    /**
     * The number of pieces in the coloring (split).
     */
    uint_t size = 0;
    /**
     * My color.
     */
    int mycolor = 0;
    /**
     * The potential hardware resource that we are splitting at. QV_HW_OBJ_LAST
     * indicates that we are called from a split() context. Any other hardware
     * resource type indicates that we are splitting at that type: called from a
     * split_at() context.
     */
    qv_hw_obj_type_t split_at_type = {};
    /**
     * Vector of task IDs, one for each member of the group. Note that the
     * number of task IDs will always match the group size and that their array
     * index corresponds to a task ID. It is handy to have the task IDs for
     * splitting so we can query task characteristics during a splitting.
     */
    std::vector<qvi_task_id_t> taskids = {};
    /**
     * Vector of hardware pools, one for each member of the group. Note that the
     * number of hardware pools will always match the group size and that their
     * array index corresponds to a task ID: 0 ... group_size - 1.
     */
    std::vector<qvi_hwpool_t *> hwpools = {};
    /**
     * Vector of colors, one for each member of the group. Note that the number
     * of colors will always match the group size and that their array index
     * corresponds to a task ID.
     */
    std::vector<int> colors = {};
    /**
     * Vector of task affinities.
     */
    qvi_map_cpusets_t affinities = {};
};

static int
qvi_global_split_create(
    qvi_global_split_t &gsplit,
    qv_scope_t *parent_scope,
    uint_t split_size,
    int mycolor,
    qv_hw_obj_type_t maybe_obj_type
) {
    gsplit.parent_scope = parent_scope;
    gsplit.mycolor = mycolor;
    if (parent_scope->group->id() == qvi_global_split_t::rootid) {
        const uint_t group_size = parent_scope->group->size();
        gsplit.size = split_size;
        gsplit.split_at_type = maybe_obj_type;
        gsplit.taskids.resize(group_size);
        gsplit.hwpools.resize(group_size);
        gsplit.affinities.resize(group_size);
    }
    return QV_SUCCESS;
}

static int
qvi_global_split_free(
    qvi_global_split_t &gsplit
) {
    for (auto &hwpool : gsplit.hwpools) {
        qvi_hwpool_free(&hwpool);
    }
    gsplit.hwpools.clear();

    for (auto &cpuset : gsplit.affinities) {
        qvi_hwloc_bitmap_free(&cpuset);
    }
    gsplit.affinities.clear();

    return QV_SUCCESS;
}

static int
qvi_global_split_gather(
    qvi_global_split_t &gsplit
) {
    qv_scope_t *const parent_scope = gsplit.parent_scope;
    const uint_t group_size = parent_scope->group->size();

    int rc = gather_values(
        parent_scope->group,
        qvi_global_split_t::rootid,
        parent_scope->group->task_id(),
        gsplit.taskids
    );
    if (rc != QV_SUCCESS) goto out;
    // Note that the result hwpools are copies, so we can modify them freely.
    rc = gather_hwpools(
        parent_scope->group,
        qvi_global_split_t::rootid,
        parent_scope->hwpool,
        gsplit.hwpools
    );
    if (rc != QV_SUCCESS) goto out;

    rc = gather_values(
        parent_scope->group,
        qvi_global_split_t::rootid,
        gsplit.mycolor,
        gsplit.colors
    );
    if (rc != QV_SUCCESS) goto out;

    if (parent_scope->group->id() == qvi_global_split_t::rootid) {
        for (uint_t tid = 0; tid < group_size; ++tid) {
            rc = qvi_rmi_task_get_cpubind(
                parent_scope->rmi,
                gsplit.taskids[tid],
                &gsplit.affinities[tid]
            );
            if (rc != QV_SUCCESS) goto out;
        }
    }
out:
    if (rc != QV_SUCCESS) {
        (void)qvi_global_split_free(gsplit);
    }
    return rc;
}

static int
qvi_global_split_scatter(
    const qvi_global_split_t &gsplit,
    int *colorp,
    qvi_hwpool_t **result
) {
    int rc = scatter_values(
        gsplit.parent_scope->group,
        qvi_global_split_t::rootid,
        gsplit.colors,
        colorp
    );
    if (rc != QV_SUCCESS) return rc;

    rc = scatter_hwpools(
        gsplit.parent_scope->group,
        qvi_global_split_t::rootid,
        gsplit.hwpools,
        result
    );
    return rc;
}

/**
 * Returns a copy of the global cpuset. Note that the cpuset will be shared
 * among the group members, but other resources may be distributed differently.
 * For example, some hardware pools may have GPUs, while others may not.
 */
static int
qvi_global_split_get_cpuset(
    const qvi_global_split_t &gsplit,
    hwloc_cpuset_t *result
) {
    // This shouldn't happen.
    if (gsplit.hwpools.size() == 0) {
        abort();
    }

    int rc = qvi_hwloc_bitmap_calloc(result);
    if (rc != QV_SUCCESS) return rc;

    return qvi_hwloc_bitmap_copy(
        qvi_hwpool_cpuset_get(gsplit.hwpools[0]), *result
    );
}

int
qvi_scope_new(
    qv_scope_t **scope
) {
    int rc = QV_SUCCESS;

    qv_scope_t *iscope = qvi_new qv_scope_t();
    if (!iscope) rc = QV_ERR_OOR;
    // hwpool and group will be initialized in scope_init().
    if (rc != QV_SUCCESS) {
        qvi_scope_free(&iscope);
    }
    *scope = iscope;
    return rc;
}

// TODO(skg) Add RMI to release resources.
void
qvi_scope_free(
    qv_scope_t **scope
) {
    if (!scope) return;
    qv_scope_t *iscope = *scope;
    if (!iscope) goto out;
    qvi_hwpool_free(&iscope->hwpool);
    qvi_group_free(&iscope->group);
    delete iscope;
out:
    *scope = nullptr;
}

static int
scope_init(
    qv_scope_t *scope,
    qvi_rmi_client_t *rmi,
    qvi_group_t *group,
    qvi_hwpool_t *hwpool
) {
    if (!rmi || !hwpool || !scope) return QV_ERR_INTERNAL;
    scope->rmi = rmi;
    scope->group = group;
    scope->hwpool = hwpool;
    return QV_SUCCESS;
}

hwloc_const_cpuset_t
qvi_scope_cpuset_get(
    qv_scope_t *scope
) {
    if (!scope) abort();
    return qvi_hwpool_cpuset_get(scope->hwpool);
}

const qvi_hwpool_t *
qvi_scope_hwpool_get(
    qv_scope_t *scope
) {
    if (!scope) abort();
    return scope->hwpool;
}

int
qvi_scope_taskid(
    qv_scope_t *scope,
    int *taskid
) {
    if (!scope) abort();
    *taskid = scope->group->id();
    return QV_SUCCESS;
}

int
qvi_scope_ntasks(
    qv_scope_t *scope,
    int *ntasks
) {
    if (!scope) abort();
    *ntasks = scope->group->size();
    return QV_SUCCESS;
}

int
qvi_scope_barrier(
    qv_scope_t *scope
) {
    if (!scope) abort();
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
    if (!scope) abort();
    return scope->group;
}

static int
apply_cpuset_mapping(
    const qvi_map_t &map,
    const qvi_map_cpusets_t cpusets,
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
        if (rc != QV_SUCCESS) goto out;
    }

    colors = qvi_map_flatten(map);
out:
    if (rc != QV_SUCCESS) {
        // Invalidate colors
        colors.clear();
    }
    return rc;
}

static qvi_map_affinity_preserving_policy_t
global_split_get_affinity_preserving_policy(
    const qvi_global_split_t &gsplit
) {
    switch (gsplit.split_at_type) {
        case QV_HW_OBJ_LAST:
            return QVI_MAP_AFFINITY_PRESERVING_PACKED;
        default:
            return QVI_MAP_AFFINITY_PRESERVING_SPREAD;
    }
}

/**
 * Straightforward user-defined device splitting.
 */
// TODO(skg) Plenty of opportunity for optimization.
// TODO(skg) Move lots of logic to map
static int
global_split_devices_user_defined(
    qvi_global_split_t &gsplit
) {
    int rc = QV_SUCCESS;
    qv_scope_t *const parent_scope = gsplit.parent_scope;
    const uint_t group_size = parent_scope->group->size();
    // Release devices from the hardware pools because they will be
    // redistributed in the next step.
    for (auto &hwpool : gsplit.hwpools) {
        rc = qvi_hwpool_release_devices(hwpool);
        if (rc != QV_SUCCESS) return rc;
    }
    // Determine mapping of colors to task IDs. The array index i of colors is
    // the color requested by task i. Also determine the number of distinct
    // colors provided in the colors array.
    std::set<int> color_set;
    for (const auto &color : gsplit.colors) {
        color_set.insert(color);
    }
    // Adjust the color set so that the distinct colors provided
    // fall within the range of the number of splits requested.
    std::set<int> color_setp;
    uint_t ncolors_chosen = 0;
    for (const auto &c : color_set) {
        if (ncolors_chosen >= gsplit.size) break;
        color_setp.insert(c);
        ncolors_chosen++;
    }
    // Cache all device infos associated with the parent hardware pool.
    auto dinfos = qvi_hwpool_devinfos_get(parent_scope->hwpool);
    // Iterate over the supported device types and split them up round-robin.
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        // The current device type.
        const qv_hw_obj_type_t devt = devts[i];
        // Get the number of devices.
        const int ndevs = dinfos->count(devt);
        // Store device infos.
        std::vector<const qvi_devinfo_t *> devs;
        for (const auto &dinfo : *dinfos) {
            // Not the type we are currently dealing with.
            if (devt != dinfo.first) continue;
            devs.push_back(dinfo.second.get());
        }
        // Maps colors to device information.
        id_devinfo_multimap_t devmap;
        int devi = 0;
        while (devi < ndevs) {
            for (const auto &c : color_setp) {
                if (devi >= ndevs) break;
                devmap.insert(std::make_pair(c, devs[devi++]));
            }
        }
        // Now that we have the mapping of colors to devices, assign devices to
        // the associated hardware pools.
        for (uint_t i = 0; i < group_size; ++i) {
            const int color = gsplit.colors[i];
            for (const auto &c2d : devmap) {
                if (c2d.first != color) continue;
                rc = qvi_hwpool_add_device(
                    gsplit.hwpools[i],
                    c2d.second->type,
                    c2d.second->id,
                    c2d.second->pci_bus_id,
                    c2d.second->uuid,
                    c2d.second->affinity
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
qvi_global_split_devices_affinity_preserving(
    qvi_global_split_t &gsplit
) {
    int rc = QV_SUCCESS;
    // Release devices from the hardware pools because they will be
    // redistributed in the next step.
    for (auto &hwpool : gsplit.hwpools) {
        rc = qvi_hwpool_release_devices(hwpool);
        if (rc != QV_SUCCESS) return rc;
    }
    // Get a pointer to device infos associated with the parent hardware pool.
    auto dinfos = qvi_hwpool_devinfos_get(gsplit.parent_scope->hwpool);
    // Iterate over the supported device types and split them up.
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        // The current device type.
        const qv_hw_obj_type_t devt = devts[i];
        // Store device infos.
        std::vector<const qvi_devinfo_t *> devs;
        for (const auto &dinfo : *dinfos) {
            // Not the type we are currently dealing with.
            if (devt != dinfo.first) continue;
            devs.push_back(dinfo.second.get());
        }
        // Store device affinities.
        qvi_map_cpusets_t devaffs;
        for (auto &dev : devs) {
            devaffs.push_back(dev->affinity);
        }

        qvi_map_t map;
        const auto policy = global_split_get_affinity_preserving_policy(gsplit);
        rc = qvi_map_affinity_preserving(
            map, policy, devaffs, gsplit.affinities
        );
        if (rc != QV_SUCCESS) return rc;
#if 0
        qvi_map_debug_dump(map);
#endif
        // Now that we have the mapping, assign
        // devices to the associated hardware pools.
        for (const auto &mi : map) {
            const int devid = mi.first;
            const int pooli = mi.second;
            rc = qvi_hwpool_add_device(
                gsplit.hwpools[pooli],
                devs[devid]->type,
                devs[devid]->id,
                devs[devid]->pci_bus_id,
                devs[devid]->uuid,
                devs[devid]->affinity
            );
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) break;
    }
    return rc;
}

/**
 * User-defined split.
 */
// TODO(skg) We may have to make some modifications here to make device and
// cpuset splitting consistent with the automatic splitting algorithms.
static int
global_split_user_defined(
    qvi_global_split_t &gsplit
) {
    int rc = QV_SUCCESS;
    qv_scope_t *const parent_scope = gsplit.parent_scope;
    const uint_t group_size = parent_scope->group->size();
    qvi_hwloc_t *const hwloc = qvi_rmi_client_hwloc_get(parent_scope->rmi);
    qvi_map_cpusets_t cpusets(group_size);
    // Get the base cpuset.
    hwloc_cpuset_t base_cpuset = nullptr;
    rc = qvi_global_split_get_cpuset(gsplit, &base_cpuset);
    if (rc != QV_SUCCESS) goto out;

    for (uint_t i = 0; i < group_size; ++i) {
        rc = qvi_hwloc_split_cpuset_by_color(
            hwloc,
            base_cpuset,
            gsplit.size,
            gsplit.colors[i],
            &cpusets[i]
        );
        if (rc != QV_SUCCESS) break;
        // Reinitialize the hwpool with the new cpuset.
        rc = qvi_hwpool_init(gsplit.hwpools[i], cpusets[i]);
        if (rc != QV_SUCCESS) break;
    }
    if (rc != QV_SUCCESS) goto out;
    // Use a straightforward device splitting algorithm based on user's request.
    rc = global_split_devices_user_defined(gsplit);
out:
    for (auto &cpuset : cpusets) {
        qvi_hwloc_bitmap_free(&cpuset);
    }
    qvi_hwloc_bitmap_free(&base_cpuset);
    return rc;
}

static int
global_split_get_new_host_cpusets(
    const qvi_global_split_t &gsplit,
    qvi_map_cpusets_t &result
) {
    int rc = QV_SUCCESS;
    qv_scope_t *const parent_scope = gsplit.parent_scope;
    // Pointer to my hwloc instance.
    qvi_hwloc_t *const hwloc = qvi_rmi_client_hwloc_get(parent_scope->rmi);
    // Holds the host's split cpusets.
    result.resize(gsplit.size);
    // The cpuset that we are going to split.
    hwloc_cpuset_t base_cpuset = nullptr;
    rc = qvi_global_split_get_cpuset(gsplit, &base_cpuset);
    if (rc != QV_SUCCESS) goto out;
    // Perform a straightforward splitting of the provided cpuset: split the
    // provided base cpuset into gsplit.size distinct pieces. Notice that we do
    // not go through the RMI for this because this is just an local, temporary
    // splitting that is ultimately fed to another splitting algorithm.
    for (uint_t color = 0; color < gsplit.size; ++color) {
        rc = qvi_hwloc_split_cpuset_by_color(
            hwloc, base_cpuset, gsplit.size, color, &result[color]
        );
        if (rc != QV_SUCCESS) goto out;
    }
out:
    if (rc != QV_SUCCESS) {
        for (auto &cpuset : result) {
            qvi_hwloc_bitmap_free(&cpuset);
        }
        result.clear();
    }
    qvi_hwloc_bitmap_free(&base_cpuset);
    return rc;
}

static int
global_split_get_new_osdev_cpusets(
    const qvi_global_split_t &gsplit,
    qvi_map_cpusets_t &result
) {
    int rc = QV_SUCCESS, nobj = 0;
    qv_scope_t *const parent_scope = gsplit.parent_scope;
    // The target object type.
    const qv_hw_obj_type_t obj_type = gsplit.split_at_type;
    // Get the number of devices we have available in the provided scope.
    rc = qvi_scope_nobjs(parent_scope, obj_type, &nobj);
    if (rc != QV_SUCCESS) return rc;
    // Holds the device affinities used for the split.
    result.resize(nobj);
    // Get a pointer to device infos associated with the parent hardware pool.
    auto dinfos = qvi_hwpool_devinfos_get(parent_scope->hwpool);
    uint_t affi = 0;
    for (const auto &dinfo : *dinfos) {
        // Not the type we are looking to split.
        if (obj_type != dinfo.first) continue;

        rc = qvi_hwloc_bitmap_calloc(&result[affi]);
        if (rc != QV_SUCCESS) goto out;
        // Copy the device's affinity to our list of device affinities.
        rc = qvi_hwloc_bitmap_copy(dinfo.second->affinity, result[affi++]);
        if (rc != QV_SUCCESS) goto out;
    }
out:
    if (rc != QV_SUCCESS) {
        for (auto &cpuset : result) {
            qvi_hwloc_bitmap_free(&cpuset);
        }
        result.clear();
    }
    return rc;
}

static int
global_split_get_primary_cpusets(
    const qvi_global_split_t &gsplit,
    qvi_map_cpusets_t &result
) {
    const qv_hw_obj_type_t obj_type = gsplit.split_at_type;
    // We were provided a real host resource type that we have to split. Or
    // QV_HW_OBJ_LAST is instead provided to indicate that we were called from a
    // split() context, which uses the host's cpuset to split the resources.
    if (qvi_hwloc_obj_type_is_host_resource(obj_type) ||
        obj_type == QV_HW_OBJ_LAST) {
        return global_split_get_new_host_cpusets(gsplit, result);
    }
    // An OS device.
    else {
        return global_split_get_new_osdev_cpusets(gsplit, result);
    }
}

static int
global_split_affinity_preserving_pass1(
    qvi_global_split_t &gsplit
) {
    qv_scope_t *const parent_scope = gsplit.parent_scope;
    // The group size: number of members.
    const uint_t group_size = parent_scope->group->size();
    // cpusets used for first mapping pass.
    qvi_map_cpusets_t cpusets = {};
    // Get the primary cpusets used for the first pass of mapping.
    int rc = global_split_get_primary_cpusets(gsplit, cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Maintains the mapping between task (consumer) IDs and resource IDs.
    qvi_map_t map = {};
    // Map tasks based on their affinity to resources encoded by the cpusets.
    const auto policy = global_split_get_affinity_preserving_policy(gsplit);
    rc = qvi_map_affinity_preserving(
        map, policy, gsplit.affinities, cpusets
    );
    if (rc != QV_SUCCESS) goto out;
    // Make sure that we mapped all the tasks. If not, this is a bug.
    if (qvi_map_nfids_mapped(map) != group_size) {
        rc = QV_ERR_INTERNAL;
        goto out;
    }
    // Update the hardware pools to reflect the new mapping.
    rc = apply_cpuset_mapping(
        map, cpusets, gsplit.hwpools, gsplit.colors
    );
out:
    for (auto &cpuset : cpusets) {
        qvi_hwloc_bitmap_free(&cpuset);
    }
    return rc;
}

/**
 * Affinity preserving split.
 */
// TODO(skg) This needs more work.
static int
global_split_affinity_preserving(
    qvi_global_split_t &gsplit
) {
    int rc = global_split_affinity_preserving_pass1(gsplit);
    if (rc != QV_SUCCESS) return rc;
    // Finally, split the devices.
    return qvi_global_split_devices_affinity_preserving(gsplit);
}

/**
 * Splits global scope data.
 */
static int
qvi_global_split(
    qvi_global_split_t &gsplit
) {
    int rc = QV_SUCCESS;
    bool auto_split = false;
    // Make sure that the supplied colors are consistent and determine the type
    // of coloring we are using. Positive values denote an explicit coloring
    // provided by the caller. Negative values are reserved for automatic
    // coloring algorithms and should be constants defined in quo-vadis.h.
    std::vector<int> tcolors(gsplit.colors);
    std::sort(tcolors.begin(), tcolors.end());
    // If all the values are negative and equal, then auto split. If not, then
    // we were called with an invalid request. Else the values are all positive
    // and we are going to split based on the input from the caller.
    if (tcolors.front() < 0) {
        if (tcolors.front() != tcolors.back()) {
            return QV_ERR_INVLD_ARG;
        }
        auto_split = true;
    }
    // User-defined splitting.
    if (!auto_split) {
        return global_split_user_defined(gsplit);
    }
    // Automatic splitting.
    switch (gsplit.colors[0]) {
        case QV_SCOPE_SPLIT_AFFINITY_PRESERVING:
            return global_split_affinity_preserving(gsplit);
        default:
            rc = QV_ERR_INVLD_ARG;
            break;
    }
    return rc;
}

/**
 * Split the hardware resources based on the provided split parameters:
 * - ncolors: The number of splits requested.
 * - color: Either user-supplied (explicitly set) or a value that requests
 *   us to do the coloring for the callers.
 *   maybe_obj_type: Potentially the object type that we are splitting at. This
 *   value influences how the splitting algorithms perform their mapping.
 * - colorp: color' is potentially a new color assignment determined by one
 *   of our coloring algorithms. This value can be used to influence the
 *   group splitting that occurs after this call completes.
 */
static int
split_hardware_resources(
    qv_scope_t *parent,
    int ncolors,
    int color,
    qv_hw_obj_type_t maybe_obj_type,
    int *colorp,
    qvi_hwpool_t **result
) {
    int rc = QV_SUCCESS, rc2 = QV_SUCCESS;
    const int rootid = qvi_global_split_t::rootid, myid = parent->group->id();
    // Information relevant to hardware resource splitting. Note that
    // agglomerated data are only valid for the task whose id is equal to
    // qvi_global_split_t::rootid after gather has completed.
    qvi_global_split_t gsplit = {};
    rc = qvi_global_split_create(
        gsplit, parent, ncolors, color, maybe_obj_type
    );
    if (rc != QV_SUCCESS) goto out;
    // First consolidate the provided information, as this is likely coming from
    // a SPMD-like context (e.g., splitting a resource shared by MPI processes).
    // In most cases it is easiest to have a single task calculate the split
    // based on global knowledge and later redistribute the calculated result to
    // its group members.
    rc = qvi_global_split_gather(gsplit);
    if (rc != QV_SUCCESS) goto out;
    // The root does this calculation.
    if (myid == rootid) {
        rc2 = qvi_global_split(gsplit);
    }
    // Wait for the split information. Explicitly barrier here in case the
    // underlying broadcast implementation polls heavily for completion.
    rc = gsplit.parent_scope->group->barrier();
    if (rc != QV_SUCCESS) goto out;
    // To avoid hangs in split error paths, share the split rc with everyone.
    rc = bcast_value(gsplit.parent_scope->group, rootid, &rc2);
    if (rc != QV_SUCCESS) goto out;
    // If the split failed, return the error to all callers.
    if (rc2 != QV_SUCCESS) {
        rc = rc2;
        goto out;
    }
    // Scatter the results.
    rc = qvi_global_split_scatter(gsplit, colorp, result);
    if (rc != QV_SUCCESS) goto out;
    // Done, so free split.
    rc = qvi_global_split_free(gsplit);
out:
    return rc;
}

int
qvi_scope_split(
    qv_scope_t *parent,
    int ncolors,
    int color,
    qv_hw_obj_type_t maybe_obj_type,
    qv_scope_t **child
) {
    int rc = QV_SUCCESS, colorp = 0;
    qvi_hwpool_t *hwpool = nullptr;
    qvi_group_t *group = nullptr;
    qv_scope_t *ichild = nullptr;
    // Split the hardware resources based on the provided split parameters.
    rc = split_hardware_resources(
        parent, ncolors, color, maybe_obj_type, &colorp, &hwpool
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
        qvi_scope_free(&ichild);
        qvi_group_free(&group);
        qvi_hwpool_free(&hwpool);
    }
    *child = ichild;
    return rc;
}

int
qvi_scope_split_at(
    qv_scope_t *parent,
    qv_hw_obj_type_t type,
    int group_id,
    qv_scope_t **child
) {
    int rc = QV_SUCCESS, nobj = 0;
    qv_scope_t *ichild = nullptr;

    rc = qvi_scope_nobjs(parent, type, &nobj);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_scope_split(parent, nobj, group_id, type, &ichild);
out:
    if (rc != QV_SUCCESS) {
        qvi_scope_free(&ichild);
    }
    *child = ichild;
    return rc;
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
    if (qvi_hwloc_obj_type_is_host_resource(obj)) {
        return qvi_rmi_get_nobjs_in_cpuset(
            scope->rmi, obj, qvi_hwpool_cpuset_get(scope->hwpool), n
        );
    }
    // TODO(skg) Should this go through the RMI?
    *n = qvi_hwpool_devinfos_get(scope->hwpool)->count(obj);
    return QV_SUCCESS;
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

    qvi_devinfo_t *finfo = nullptr;
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
            nw = asprintf(dev_id, "%s", finfo->uuid);
            break;
        case (QV_DEVICE_ID_PCI_BUS_ID):
            nw = asprintf(dev_id, "%s", finfo->pci_bus_id);
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
