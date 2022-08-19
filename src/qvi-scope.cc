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

/** Maps colors (keys) to other IDs. */
using qvi_scope_color_map_t = std::multimap<int, int>;
/** Maps colors to device information. */
using qvi_scope_c2d_map_t = std::multimap<
    int, const qvi_devinfo_t *
>;

// Type definition
struct qv_scope_s {
    /** Pointer to initialized RMI infrastructure. */
    qvi_rmi_client_t *rmi = nullptr;
    /** Task group associated with this scope instance. */
    qvi_group_t *group = nullptr;
    /** Hardware resource pool. */
    qvi_hwpool_t *hwpool = nullptr;
};

int
qvi_scope_new(
    qv_scope_t **scope
) {
    int rc = QV_SUCCESS;

    qv_scope_t *iscope = qvi_new qv_scope_t();
    if (!scope) rc = QV_ERR_OOR;
    // hwpool and group will be initialized in scope_init().
    if (rc != QV_SUCCESS) qvi_scope_free(&iscope);
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
    assert(rmi && group && hwpool);
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
    *taskid = scope->group->id();
    return QV_SUCCESS;
}

int
qvi_scope_ntasks(
    qv_scope_t *scope,
    int *ntasks
) {
    *ntasks = scope->group->size();
    return QV_SUCCESS;
}

int
qvi_scope_barrier(
    qv_scope_t *scope
) {
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

template <typename TYPE>
static int
gather_values(
    qvi_group_t *group,
    int root,
    TYPE invalue,
    TYPE **outvals
) {
    const int group_size = group->size();
    int shared = 0;
    int rc = QV_SUCCESS;
    qvi_bbuff_t *txbuff = nullptr;
    qvi_bbuff_t **bbuffs = nullptr;
    TYPE *ioutvals = nullptr;

    rc = qvi_bbuff_new(&txbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_bbuff_append(
        txbuff, &invalue, sizeof(TYPE)
    );
    if (rc != QV_SUCCESS) goto out;

    rc = group->gather(txbuff, root, &bbuffs, &shared);
    if (rc != QV_SUCCESS) goto out;

    if (group->id() == root) {
        ioutvals = qvi_new TYPE[group_size]();
        if (!ioutvals) {
            rc = QV_ERR_OOR;
            goto out;
        }
        // Unpack the values.
        for (int i = 0; i < group_size; ++i) {
            ioutvals[i] = *(TYPE *)qvi_bbuff_data(bbuffs[i]);
        }
    }
out:
    if ( !shared || (shared && (group->id() == root)))
      if (bbuffs) {
        for (int i = 0; i < group_size; ++i) {
            qvi_bbuff_free(&bbuffs[i]);
        }
        delete[] bbuffs;
      }
    qvi_bbuff_free(&txbuff);
    if (rc != QV_SUCCESS) {
        delete[] ioutvals;
        ioutvals = nullptr;
    }
    *outvals = ioutvals;
    return rc;
}

static int
gather_hwpools(
    qvi_group_t *group,
    int root,
    qvi_hwpool_t *txpool,
    qvi_hwpool_t ***rxpools
) {
    const int group_size = group->size();

    int shared = 0;
    int rc = QV_SUCCESS;
    qvi_bbuff_t *txbuff = nullptr;
    qvi_bbuff_t **bbuffs = nullptr;
    qvi_hwpool_t **hwpools = nullptr;

    rc = qvi_bbuff_new(&txbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_pack(txpool, txbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = group->gather(txbuff, root, &bbuffs, &shared);
    if (rc != QV_SUCCESS) goto out;

    if (group->id() == root) {
        // Notice the use of zero initialization here: the '()'.
        hwpools = qvi_new qvi_hwpool_t*[group_size]();
        if (!hwpools) {
            rc = QV_ERR_OOR;
            goto out;
        }
        // Unpack the hwpools.
        for (int i = 0; i < group_size; ++i) {
            rc = qvi_hwpool_unpack(
                qvi_bbuff_data(bbuffs[i]), &hwpools[i]
            );
            if (rc != QV_SUCCESS) break;
        }
    }
out:
    if ( !shared || (shared && (group->id() == root)))
      if (bbuffs) {
        for (int i = 0; i < group_size; ++i) {
            qvi_bbuff_free(&bbuffs[i]);
        }
        delete[] bbuffs;
      }
    qvi_bbuff_free(&txbuff);
    if (rc != QV_SUCCESS) {
        if (hwpools) {
            for (int i = 0; i < group_size; ++i) {
                qvi_hwpool_free(&hwpools[i]);
            }
            delete[] hwpools;
            hwpools = nullptr;
        }
    }
    *rxpools = hwpools;
    return rc;
}

template <typename TYPE>
static int
scatter_values(
    qvi_group_t *group,
    int root,
    TYPE *values,
    TYPE *value
) {
    const int group_size = group->size();

    int rc = QV_SUCCESS;
    qvi_bbuff_t **txbuffs = nullptr;
    qvi_bbuff_t *rxbuff = nullptr;

    if (root == group->id()) {
        // Notice the use of zero initialization here: the '()'.
        txbuffs = qvi_new qvi_bbuff_t *[group_size]();
        if (!txbuffs) {
            rc = QV_ERR_OOR;
            goto out;
        }
        // Pack the values.
        for (int i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_new(&txbuffs[i]);
            if (rc != QV_SUCCESS) break;
            rc = qvi_bbuff_append(
                txbuffs[i], &values[i], sizeof(TYPE)
            );
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) goto out;
    }

    rc = group->scatter(txbuffs, root, &rxbuff);
    if (rc != QV_SUCCESS) goto out;

    *value = *(TYPE *)qvi_bbuff_data(rxbuff);
out:
    if (txbuffs) {
        for (int i = 0; i < group_size; ++i) {
            qvi_bbuff_free(&txbuffs[i]);
        }
        delete[] txbuffs;
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
    qvi_hwpool_t **pools,
    qvi_hwpool_t **pool
) {
    const int group_size = group->size();

    int rc = QV_SUCCESS;
    qvi_bbuff_t **txbuffs = nullptr;
    qvi_bbuff_t *rxbuff = nullptr;

    if (root == group->id()) {
        // Notice the use of zero initialization here: the '()'.
        txbuffs = qvi_new qvi_bbuff_t *[group_size]();
        if (!txbuffs) {
            rc = QV_ERR_OOR;
            goto out;
        }
        // Pack the hwpools.
        for (int i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_new(&txbuffs[i]);
            if (rc != QV_SUCCESS) break;
            rc = qvi_hwpool_pack(pools[i], txbuffs[i]);
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) goto out;
    }

    rc = group->scatter(txbuffs, root, &rxbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_unpack(qvi_bbuff_data(rxbuff), pool);
out:
    if (txbuffs) {
        for (int i = 0; i < group_size; ++i) {
            qvi_bbuff_free(&txbuffs[i]);
        }
        delete[] txbuffs;
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
    const int group_size = group->size();

    int rc = QV_SUCCESS;
    TYPE *values = nullptr;

    if (root == group->id()) {
        values = qvi_new TYPE[group_size]();
        if (!values) {
            rc = QV_ERR_OOR;
            goto out;
        }
        for (int i = 0; i < group_size; ++i) {
            values[i] = *value;
        }
    }
    rc = scatter_values(group, root, values, value);
out:
    delete[] values;
    return rc;
}

/**
 * Straightforward device splitting.
 */
static int
split_devices_basic(
    qv_scope_t *parent,
    int ncolors,
    int *colors,
    qvi_hwpool_t **hwpools
) {
    const int group_size = parent->group->size();
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    int rc = QV_SUCCESS;
    // Determine mapping of colors to task IDs. The array index i of colors is
    // the color requested by task i. Also determine the number of distinct
    // colors provided in the colors array.
    qvi_scope_color_map_t cmap;
    std::set<int> color_set;
    for (int i = 0; i < group_size; ++i) {
        cmap.insert(std::make_pair(colors[i], i));
        color_set.insert(colors[i]);
    }
    // Adjust the color set so that the distinct colors provided fall within the
    // range of the number of splits requested.
    std::set<int> color_setp;
    int ncolors_chosen = 0;
    for (const auto &c : color_set) {
        if (ncolors_chosen >= ncolors) break;
        color_setp.insert(c);
        ncolors_chosen++;
    }
    // Release devices from the hardware pools because they will be
    // redistributed in the next step.
    for (int i = 0; i < group_size; ++i) {
        rc = qvi_hwpool_release_devices(hwpools[i]);
        if (rc != QV_SUCCESS) return rc;
    }
    // Iterate over the supported device types and split them up round-robin.
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        // The current device type.
        const qv_hw_obj_type_t devt = devts[i];
        // All device infos associated with the parent hardware pool.
        auto dinfos = qvi_hwpool_devinfos_get(parent->hwpool);
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
        //const int maxdpc = std::ceil(ndevs / float(color_setp.size()));
        // Maps colors to device information.
        qvi_scope_c2d_map_t devmap;
        int devi = 0;
        while (devi < ndevs) {
            for (const auto &c : color_setp) {
                if (devi >= ndevs) break;
                devmap.insert(std::make_pair(c, devs[devi++]));
            }
        }
        // Now that we have the mapping of colors to devices, assign devices to
        // the associated hardware pools.
        for (int i = 0; i < group_size; ++i) {
            const int color = colors[i];
            for (const auto &c2d : devmap) {
                if (c2d.first != color) continue;
                rc = qvi_hwpool_add_device(
                    hwpools[i],
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
static int
split_user_defined(
    qv_scope_t *parent,
    int ncolors,
    int *colors,
    qvi_hwpool_t **hwpools
) {
    const int group_size = parent->group->size();

    int rc = QV_SUCCESS;
    hwloc_bitmap_t *cpusets = nullptr;

    cpusets = qvi_new hwloc_bitmap_t[group_size]();
    if (!cpusets) {
        rc = QV_ERR_OOR;
        goto out;
    }

    for (int i = 0; i < group_size; ++i) {
        rc = qvi_rmi_split_cpuset_by_color(
            parent->rmi,
            qvi_hwpool_cpuset_get(parent->hwpool),
            ncolors, colors[i], &cpusets[i]
        );
        if (rc != QV_SUCCESS) break;
        // Reinitialize the hwpool with the new cpuset.
        rc = qvi_hwpool_init(hwpools[i], cpusets[i]);
        if (rc != QV_SUCCESS) break;
    }
    if (rc != QV_SUCCESS) {
       goto out;
    }
    // Use a straightforward device splitting algorithm.
    rc = split_devices_basic(
        parent, ncolors, colors, hwpools
    );
out:
    for (int i = 0; i < group_size; ++i) {
        qvi_hwloc_bitmap_free(&cpusets[i]);
    }
    delete[] cpusets;
    return rc;
}

/**
 *
 */
static int
split_dispatch(
    qv_scope_t *parent,
    int ncolors,
    int *colors,
    qvi_hwpool_t **hwpools
) {
    const int group_size = parent->group->size();

    int rc = QV_SUCCESS;
    bool auto_split = false;
    // Make sure that the supplied colors are consistent and determine the type
    // of coloring we are using. Positive values denote an explicit coloring
    // provided by the caller. Negative values are reserved for automatic
    // coloring algorithms and should be constants defined in quo-vadis.h.
    std::vector<int> tcolors(colors, colors + group_size);
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
        return split_user_defined(
            parent, ncolors, colors, hwpools
        );
    }
    // Automatic splitting.
    switch (colors[0]) {
        case QV_SCOPE_SPLIT_AFFINITY_PRESERVING:
            rc = QV_ERR_NOT_SUPPORTED;
            break;
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
    // Always use 0 as the root because 0 will always exist.
    const int rootid = 0, myid = parent->group->id();
    // The number of hardware pools will always match the group size.
    qvi_hwpool_t **hwpools = nullptr;
    // Array of colors used for splitting.
    int *colors = nullptr;
    // First consolidate the provided information, as this is likely coming from
    // an SPMD-like context (e.g., splitting a resource shared by MPI
    // processes).  In most cases it is easiest to have a single process
    // calculate the split based on global knowledge and then redistribute the
    // result.
    rc = gather_values(
        parent->group, rootid, color, &colors
    );
    if (rc != QV_SUCCESS) goto out;
    // Note that the result hwpools are copies, so we can modify them freely.
    rc = gather_hwpools(
        parent->group, rootid, parent->hwpool, &hwpools
    );
    if (rc != QV_SUCCESS) goto out;
    // The root does this calculation.
    if (myid == rootid) {
        rc2 = split_dispatch(parent, ncolors, colors, hwpools);
    }
    // To avoid hangs in split error paths, share the split rc with everyone.
    rc = bcast_value(parent->group, rootid, &rc2);
    if (rc != QV_SUCCESS) goto out;
    // If the split failed, return the error to all callers.
    if (rc2 != QV_SUCCESS) {
        rc = rc2;
        goto out;
    }
    // Scatter split results. Notice that we use colorp here because it could
    // have changed based on decisions made in the split algorithm.
    rc = scatter_values(
        parent->group, rootid, colors, colorp
    );
    if (rc != QV_SUCCESS) goto out;

    rc = scatter_hwpools(
        parent->group, rootid, hwpools, result
    );
out:
    if (hwpools) {
        for (int i = 0; i < parent->group->size(); ++i) {
            qvi_hwpool_free(&hwpools[i]);
        }
        delete[] hwpools;
    }
    delete[] colors;
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
    qvi_group_t *group = nullptr;
    qvi_hwpool_t *hwpool = nullptr;
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
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Now that we have the desired cpuset,
    // create a corresponding hardware pool.
    rc = qvi_hwpool_new(&hwpool);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    rc = qvi_hwpool_init(hwpool, cpuset);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Create underlying group. Notice the use of self here.
    rc = parent->group->self(&group);
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
