/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
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
    int rc = QV_SUCCESS;
    const uint_t group_size = group->size();
    int shared = 0;
    qvi_bbuff_t *txbuff = nullptr;
    qvi_bbuff_t **bbuffs = nullptr;

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
    return rc;
}

static int
gather_hwpools(
    qvi_group_t *group,
    int root,
    qvi_hwpool_t *txpool,
    std::vector<qvi_hwpool_t *> &rxpools
) {
    int rc = QV_SUCCESS;
    const uint_t group_size = group->size();
    int shared = 0;
    qvi_bbuff_t *txbuff = nullptr;
    qvi_bbuff_t **bbuffs = nullptr;

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
    int rc = QV_SUCCESS;
    std::vector<qvi_bbuff_t *> txbuffs;
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
    std::vector<TYPE> values = {};

    if (root == group->id()) {
        values.resize(group->size());
        std::fill(values.begin(), values.end(), *value);
    }
    return scatter_values(group, root, values, value);
}

/**
 * Base class for global data that require collective communication.
 */
struct qvi_global_data_t {
    /**
     * The root task ID used for collective operations.
     * Note: We use 0 as the root because 0 will always exist.
     */
    static const int rootid = 0;
    /**
     * The group instance used for group communication.
     */
    qvi_group_t *group = nullptr;
    /**
     * Size of the underlying group.
     */
    uint_t group_size = 0;
    /**
     * My group ID.
     */
    int myid = 0;

    /**
     * Default constructor.
     */
    qvi_global_data_t(void) = delete;

    /**
     * Constructor.
     */
    qvi_global_data_t(
        qvi_group_t *group
    ) : group(group)
      , group_size(group->size())
      , myid(group->id())
    {
        // Nothing to do here.
    }

    /**
     * Destructor.
     */
    ~qvi_global_data_t(void) = default;
};

/**
 * The collection of data relevant to scope operations requiring global
 * knowledge.
 */
struct qvi_global_scope_data_t : public qvi_global_data_t {
    /**
     * A pointer to the scope whose data we are agglomerating.
     */
    qv_scope_t *scope = nullptr;
    /**
     * Convenience pointer to the underlying scope's RMI.
     */
    qvi_rmi_client_t *rmi = nullptr;
    /**
     * Convenience pointer to the underlying scope's hwloc.
     */
    qvi_hwloc_t *hwloc = nullptr;
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
     * Vector of queried task affinities.
     */
    std::vector<hwloc_cpuset_t> task_affinities = {};

private:
    /**
     * Gathers current task affinities by querying RMI.
     */
    int
    gather_task_affinities(void)
    {
        // Nothing to do if we aren't the root.
        if (myid != rootid) return QV_SUCCESS;

        int rc = QV_SUCCESS;
        for (uint_t tid = 0; tid < group_size; ++tid) {
            rc = qvi_rmi_task_get_cpubind(
                scope->rmi,
                taskids[tid],
                &task_affinities[tid]
            );
            if (rc != QV_SUCCESS) break;
        }
        return rc;
    }

public:
    /**
     * Default constructor.
     */
    qvi_global_scope_data_t(void) = delete;

    /**
     * Constructor.
     */
    qvi_global_scope_data_t(
        qv_scope_t *scope
    ) : qvi_global_data_t(scope->group)
      , scope(scope)
      , rmi(scope->rmi)
      , hwloc(qvi_rmi_client_hwloc_get(scope->rmi))
    {
        if (myid == rootid) {
            taskids.resize(group_size);
            task_affinities.resize(group_size);
            hwpools.resize(group_size);
        }
    }

    /**
     * Destructor.
     */
    ~qvi_global_scope_data_t(void)
    {
        for (auto &hwpool : hwpools) {
            qvi_hwpool_free(&hwpool);
        }
        for (auto &cpuset : task_affinities) {
            qvi_hwloc_bitmap_free(&cpuset);
        }
    }

    /**
     * Returns the base cpuset associated with the underlying scope. Note that
     * the cpuset will be shared among the group members, but other resources
     * may be distributed differently. For example, some hardware pools may have
     * GPUs, while others may not.
     */
    hwloc_const_cpuset_t
    base_cpuset(void)
    {
        return qvi_hwpool_cpuset_get(scope->hwpool);
    }

    /**
     * Returns all the device infos associated with the underlying scope.
     */
    const qvi_hwpool_devinfos_t *
    devinfos(void)
    {
        return qvi_hwpool_devinfos_get(scope->hwpool);
    }

    /**
     * Gathers global data to root.
     */
    int
    gather(void)
    {
        int rc = gather_values(
            scope->group,
            rootid,
            scope->group->task_id(),
            taskids
        );
        if (rc != QV_SUCCESS) return rc;

        // Note that the result hwpools are copies, so we can modify them freely.
        rc = gather_hwpools(
            scope->group,
            rootid,
            scope->hwpool,
            hwpools
        );
        if (rc != QV_SUCCESS) return rc;
        // Finally the root queries for and caches current
        // affinities for all tasks in the initializing scope.
        return gather_task_affinities();
    }
};

struct qvi_global_color_data_t : public qvi_global_data_t {
    /**
     * The number of pieces in the coloring (split).
     */
    uint_t ncolors;
    /**
     * My color.
     */
    int mycolor;
    /**
     * Vector of colors, one for each member of the group. Note that the number
     * of colors will always match the group size and that their array index
     * corresponds to a task ID.
     */
    std::vector<int> colors = {};

    /**
     * Default constructor.
     */
    qvi_global_color_data_t(void) = delete;

    /**
     * Constructor.
     */
    qvi_global_color_data_t(
        qvi_group_t *group,
        int ncolors,
        int mycolor
    ) : qvi_global_data_t(group)
      , ncolors(ncolors)
      , mycolor(mycolor)
    {
        if (myid == rootid) {
            colors.resize(group_size);
        }
    }

    /**
     * Destructor.
     */
    ~qvi_global_color_data_t(void) = default;

    /**
     * Gathers global data to root.
     */
    int
    gather(void)
    {
        return gather_values(group, rootid, mycolor, colors);
    }

    int
    scatter(
        int *colorp
    ) {
        return scatter_values(group, rootid, colors, colorp);
    }
};

int
qvi_scope_new(
    qv_scope_t **scope
) {
    int rc = QV_SUCCESS;

    qv_scope_t *iscope = qvi_new qv_scope_t();
    if (!scope) rc = QV_ERR_OOR;
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
    if (!scope) return nullptr;
    return qvi_hwpool_cpuset_get(scope->hwpool);
}

const qvi_hwpool_t *
qvi_scope_hwpool_get(
    qv_scope_t *scope
) {
    if (!scope) return nullptr;
    return scope->hwpool;
}

int
qvi_scope_taskid(
    qv_scope_t *scope,
    int *taskid
) {
    if (!scope) return QV_ERR_INTERNAL;
    *taskid = scope->group->id();
    return QV_SUCCESS;
}

int
qvi_scope_ntasks(
    qv_scope_t *scope,
    int *ntasks
) {
    if (!scope) return QV_ERR_INTERNAL;
    *ntasks = scope->group->size();
    return QV_SUCCESS;
}

int
qvi_scope_barrier(
    qv_scope_t *scope
) {
    if (!scope) return QV_ERR_INTERNAL;
    return scope->group->barrier();
}

int
qvi_scope_get(
    qvi_zgroup_t *zgroup,
    qvi_rmi_client_t *rmi,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    int rc = QV_SUCCESS;
    qvi_group_t *group = nullptr;
    qvi_hwpool_t *hwpool = nullptr;
    // Get the requested intrinsic group.
    rc = zgroup->group_create_intrinsic(
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
    if (!scope) return nullptr;
    return scope->group;
}

/**
 * Straightforward user-defined device splitting.
 */
// TODO(skg) Plenty of opportunity for optimization.
// TODO(skg) Move lots of logic to map
static int
split_devices_user_defined(
    qvi_global_scope_data_t &gsd,
    qvi_global_color_data_t &gcd
) {
    int rc = QV_SUCCESS;
    const uint_t group_size = gsd.group_size;
    // Determine mapping of colors to task IDs. The array index i of colors is
    // the color requested by task i. Also determine the number of distinct
    // colors provided in the colors array.
    std::set<int> color_set;
    for (uint_t i = 0; i < group_size; ++i) {
        color_set.insert(gcd.colors[i]);
    }
    // Adjust the color set so that the distinct colors provided fall within the
    // range of the number of splits requested.
    std::set<int> color_setp;
    uint_t ncolors_chosen = 0;
    for (const auto &c : color_set) {
        if (ncolors_chosen >= gcd.ncolors) break;
        color_setp.insert(c);
        ncolors_chosen++;
    }
    // Release devices from the hardware pools because they will be
    // redistributed in the next step.
    for (uint_t i = 0; i < group_size; ++i) {
        rc = qvi_hwpool_release_devices(gsd.hwpools[i]);
        if (rc != QV_SUCCESS) return rc;
    }
    // Iterate over the supported device types and split them up round-robin.
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        // The current device type.
        const qv_hw_obj_type_t devt = devts[i];
        // All device infos associated with the parent hardware pool.
        auto dinfos = gsd.devinfos();
        // Get the number of devices.
        const int ndevs = dinfos->count(devt);
        // Store device infos.
        std::vector<const qvi_devinfo_t *> devs;
        for (const auto &dinfo : *dinfos) {
            // Not the type we are currently dealing with.
            if (devt != dinfo.first) continue;
            devs.push_back(dinfo.second.get());
        }
        // Max devices per color.
        // We will likely use this when we update the mapping algorithm to
        // include device affinity.
        //const uint_t maxdpc = maxiperk(ndevs, color_setp.size());
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
            const int color = gcd.colors[i];
            for (const auto &c2d : devmap) {
                if (c2d.first != color) continue;
                rc = qvi_hwpool_add_device(
                    gsd.hwpools[i],
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
// TODO(skg) Move lots of logic to map
static int
split_devices_affinity_preserving(
    qvi_global_scope_data_t &gsd,
    qvi_global_color_data_t &gcd
) {
    int rc = QV_SUCCESS;
    const uint_t group_size = gsd.group_size;
    // Determine mapping of colors to task IDs. The array index i of colors is
    // the color requested by task i. Also determine the number of distinct
    // colors provided in the colors array.
    std::set<int> color_set;
    for (uint_t i = 0; i < group_size; ++i) {
        color_set.insert(gcd.colors[i]);
    }
    // Adjust the color set so that the distinct colors provided fall within the
    // range of the number of splits requested.
    std::set<int> color_setp;
    uint_t ncolors_chosen = 0;
    for (const auto &c : color_set) {
        if (ncolors_chosen >= gcd.ncolors) break;
        color_setp.insert(c);
        ncolors_chosen++;
    }
    // Release devices from the hardware pools because they will be
    // redistributed in the next step.
    for (uint_t i = 0; i < group_size; ++i) {
        rc = qvi_hwpool_release_devices(gsd.hwpools[i]);
        if (rc != QV_SUCCESS) return rc;
    }
    // Iterate over the supported device types and split them up round-robin.
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        // The current device type.
        const qv_hw_obj_type_t devt = devts[i];
        // All device infos associated with the parent hardware pool.
        auto dinfos = gsd.devinfos();
        // Get the number of devices.
        const uint_t ndevs = dinfos->count(devt);
        // Store device infos.
        std::vector<const qvi_devinfo_t *> devs;
        for (const auto &dinfo : *dinfos) {
            // Not the type we are currently dealing with.
            if (devt != dinfo.first) continue;
            devs.push_back(dinfo.second.get());
        }
        // Store device affinities.
        std::vector<hwloc_cpuset_t> devaffs;
        for (auto &dev : devs) {
            devaffs.push_back(dev->affinity);
        }
        qvi_map_t map;
        map.initialize(gcd.colors, devaffs);

        qvi_map_affinity_preserving(map, gsd.task_affinities);
#if 0
        qvi_log_debug("N Dev nmapped {}", map.nmapped());
        for (uint_t i = 0; i < ndevs; ++i) {
            if (map.id_mapped(i)) {
                qvi_log_debug("dev {} mapped to task {}", i, map.mapped_color(i));
            }
            else {
                qvi_log_debug("id not mapped {}", i);
            }
        }
#endif
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
            const int color = gcd.colors[i];
            for (const auto &c2d : devmap) {
                if (c2d.first != color) continue;
                rc = qvi_hwpool_add_device(
                    gsd.hwpools[i],
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
 * User-defined split.
 */
// TODO(skg) Move lots of logic to map.
static int
split_user_defined(
    qvi_global_scope_data_t &gsd,
    qvi_global_color_data_t &gcd
) {
    int rc = QV_SUCCESS;
    hwloc_const_cpuset_t base_cpuset = gsd.base_cpuset();
    std::vector<hwloc_bitmap_t> cpusets(gsd.group_size);

    for (uint_t i = 0; i < gsd.group_size; ++i) {
        rc = qvi_hwloc_split_cpuset_by_color(
            gsd.hwloc,
            base_cpuset,
            gcd.ncolors,
            gcd.colors[i],
            &cpusets[i]
        );
        if (rc != QV_SUCCESS) break;
        // Reinitialize the hwpool with the new cpuset.
        rc = qvi_hwpool_init(gsd.hwpools[i], cpusets[i]);
        if (rc != QV_SUCCESS) break;
    }
    if (rc != QV_SUCCESS) goto out;
    // Use a straightforward device splitting algorithm based on user's request.
    rc = split_devices_user_defined(gsd, gcd);
out:
    for (auto &cpuset : cpusets) {
        qvi_hwloc_bitmap_free(&cpuset);
    }
    return rc;
}

/**
 * Affinity preserving split.
 */
static int
split_affinity_preserving(
    qvi_global_scope_data_t &gsd,
    qvi_global_color_data_t &gcd
) {
    int rc = QV_SUCCESS;
    // The group size: number of members.
    const uint_t group_size = gsd.group_size;
    // The cpuset that we are going to split.
    hwloc_const_cpuset_t base_cpuset = gsd.base_cpuset();
    // cpusets with straightforward splitting: one for each color.
    std::vector<hwloc_bitmap_t> cpusets(gcd.ncolors);
    // The ID to resource map.
    qvi_map_t map;

    // Perform a straightforward splitting of the provided cpuset. Notice that
    // we do not go through the RMI for this because this is just an local,
    // temporary splitting that is ultimately fed to another splitting
    // algorithm.
    // TODO(skg) Should we keep track of split resources? What useful
    // information does this provide us going through the RMI?
    for (uint_t color = 0; color < gcd.ncolors; ++color) {
        rc = qvi_hwloc_split_cpuset_by_color(
            gsd.hwloc, base_cpuset, gcd.ncolors, color, &cpusets[color]
        );
        if (rc != QV_SUCCESS) goto out;
    }

    // Initialize the map.
    rc = map.initialize(gcd.colors, cpusets);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_map_affinity_preserving(map, gsd.task_affinities);
    if (rc != QV_SUCCESS) goto out;

    // Make sure that we mapped all the tasks. If not, this is a bug.
    if (map.nmapped() != group_size) {
        rc = QV_ERR_INTERNAL;
        goto out;
    }
    // TODO(skg) Add function to reinitialize the hwpool with the new cpuset.
    for (uint_t tid = 0; tid < group_size; ++tid) {
        rc = qvi_hwpool_init(gsd.hwpools[tid], map.mapped_cpuset(tid));
        if (rc != QV_SUCCESS) goto out;
        // TODO(skg) Improve
        gcd.colors[tid] = map.mapped_color(tid);
    }

    rc = split_devices_affinity_preserving(gsd, gcd);
out:
    for (auto &cpuset : cpusets) {
        qvi_hwloc_bitmap_free(&cpuset);
    }
    return rc;
}

/**
 * Splits global scope data.
 */
static int
split_global_scope_data(
    qvi_global_scope_data_t &gsd,
    qvi_global_color_data_t &gcd
) {
    int rc = QV_SUCCESS;
    bool auto_split = false;
    // Make sure that the supplied colors are consistent and determine the type
    // of coloring we are using. Positive values denote an explicit coloring
    // provided by the caller. Negative values are reserved for automatic
    // coloring algorithms and should be constants defined in quo-vadis.h.
    std::vector<int> tcolors(gcd.colors);
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
        return split_user_defined(gsd, gcd);
    }
    // Automatic splitting.
    switch (gcd.colors[0]) {
        case QV_SCOPE_SPLIT_AFFINITY_PRESERVING:
            return split_affinity_preserving(gsd, gcd);
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
 * - colorp: color' is potentially a new color assignment determined by one
 *   of our coloring algorithms. This value can be used to influence the
 *   group splitting that occurs after this call completes.
 */
static int
split_hardware_resources(
    qv_scope_t *parent,
    int ncolors,
    int color,
    int *colorp,
    qvi_hwpool_t **result
) {
    int rc = QV_SUCCESS, rc2 = QV_SUCCESS;
    const int rootid = qvi_global_data_t::rootid, myid = parent->group->id();
    // Information relevant to hardware resource splitting. Note that
    // agglomerated data are only valid for the task whose id is equal to
    // qvi_global_scope_data_t::rootid after gather() has completed.
    qvi_global_scope_data_t gsd(parent);
    qvi_global_color_data_t gcd(parent->group, ncolors, color);

    // First consolidate the provided information, as this is likely coming from
    // an SPMD-like context (e.g., splitting a resource shared by MPI
    // processes).  In most cases it is easiest to have a single task
    // calculate the split based on global knowledge and later redistribute the
    // calculated result to its group members.
    rc = gcd.gather();
    if (rc != QV_SUCCESS) goto out;

    rc = gsd.gather();
    if (rc != QV_SUCCESS) goto out;
    // The root does this calculation.
    if (myid == rootid) {
        rc2 = split_global_scope_data(gsd, gcd);
    }
    // Wait for the split information. Explicitly barrier here in case the
    // underlying broadcast implementation polls heavily for completion.
    rc = parent->group->barrier();
    if (rc != QV_SUCCESS) goto out;
    // To avoid hangs in split error paths, share the split rc with everyone.
    rc = bcast_value(parent->group, rootid, &rc2);
    if (rc != QV_SUCCESS) goto out;
    // If the split failed, return the error to all callers.
    if (rc2 != QV_SUCCESS) {
        rc = rc2;
        goto out;
    }

    rc = gcd.scatter(colorp);
    if (rc != QV_SUCCESS) goto out;

    rc = scatter_hwpools(gsd.group, rootid, gsd.hwpools, result);

    // TODO(skg) Lots of cleanup required here ^^^.
out:
    return rc;
}

int
qvi_scope_split(
    qv_scope_t *parent,
    int ncolors,
    int color,
    qv_scope_t **child
) {
    int rc = QV_SUCCESS, colorp = 0;
    qvi_hwpool_t *hwpool = nullptr;
    qvi_group_t *group = nullptr;
    qv_scope_t *ichild = nullptr;
    // Split the hardware resources based on the provided split parameters.
    rc = split_hardware_resources(
        parent, ncolors, color, &colorp, &hwpool
    );
    if (rc != QV_SUCCESS) goto out;
    // Split underlying group. Notice the use of colorp here.
    rc = parent->group->split(
        colorp, parent->group->id(), &group
    );
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

    rc = qvi_scope_split(parent, nobj, group_id, &ichild);
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
    hwloc_bitmap_t cpuset = nullptr;
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
    if (obj == QV_HW_OBJ_GPU) {
        *n = qvi_hwpool_devinfos_get(scope->hwpool)->count(obj);
        return QV_SUCCESS;
    }
    return qvi_rmi_get_nobjs_in_cpuset(
        scope->rmi, obj, qvi_hwpool_cpuset_get(scope->hwpool), n
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
    // Device infos.
    auto dinfos = qvi_hwpool_devinfos_get(scope->hwpool);

    int rc = QV_SUCCESS, id = 0, nw = 0;
    qvi_devinfo_t *finfo = nullptr;
    for (const auto &dinfo : *dinfos) {
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
