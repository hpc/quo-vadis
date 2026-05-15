/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwsplit.cc
 */

#include "qvi-hwsplit.h"
#include "qvi-rmi.h"
#include "qvi-task.h" // IWYU pragma: keep
#include "qvi-coll.h"
#include "qvi-scope.h" // IWYU pragma: keep

#if 0
/**
 *
 */
static int
cpus_available(
    hwloc_const_cpuset_t which,
    hwloc_const_cpuset_t from,
    bool *avail
) {
    // TODO(skg) Cache storage for calculation?
    hwloc_cpuset_t tcpus = nullptr;
    int rc = qvi_hwloc::bitmap_calloc(&tcpus);
    if (rc != QV_SUCCESS) return rc;

    int hrc = hwloc_bitmap_and(tcpus, which, from);
    if (hrc != 0) {
        rc = QV_ERR_HWLOC;
    }
    if (rc == QV_SUCCESS) {
        *avail = cpusets_equal(tcpus, which);
    }
    else {
        *avail = false;
    }
    qvi_hwloc_bitmap_free(&tcpus);
    return rc;
}
#endif

/**
 * Example:
 * obcpuset  0110 0101
 * request   1000 1010
 * obcpuset' 1110 1111
 */
#if 0
static int
pool_obtain_cpus_by_cpuset(
    qvi_hwpool *pool,
    hwloc_const_cpuset_t request
) {
#if 0
    int hwrc = hwloc_bitmap_or(
        pool->obcpuset,
        pool->obcpuset,
        request
    );
    return (hwrc == 0 ? QV_SUCCESS : QV_ERR_HWLOC);
#endif
    qvi_unused(pool);
    qvi_unused(request);
    return QV_SUCCESS;
}
#endif

/**
 * Example:
 * obcpuset  0110 0101
 * release   0100 0100
 * obcpuset' 0010 0001
 */
#if 0
static int
pool_release_cpus_by_cpuset(
    qvi_hwpool *pool,
    hwloc_const_cpuset_t release
) {
    int hwrc = hwloc_bitmap_andnot(
        pool->obcpuset,
        pool->obcpuset,
        release
    );
    return (hwrc == 0 ? QV_SUCCESS : QV_ERR_HWLOC);
}
#endif

// TODOs
// * Resource reference counting.
// * Need to deal with resource unavailability.
// * Split and attach devices properly.
// * Have bitmap scratch pad that is initialized once, then destroyed? This
//   approach may be an nice allocation optimization, but in heavily threaded
//   code may be a bottleneck.
// TODO(skg) Use distance API for device affinity.
// TODO(skg) Add RMI to acquire/release resources.

// Notes:
// * Does it make sense attempting resource exclusivity? Why not just let the
// users get what they ask for and hope that the abstractions that we provide do
// a good enough job most of the time. Making the user deal with resource
// exhaustion and retries (which will eventually be the case with
// QV_RES_UNAVAILABLE) is error prone and often frustrating.
//
// * Reference Counting: we should probably still implement a rudimentary
// reference counting system, but perhaps not for enforcing resource
// exclusivity. Rather we could use this information to guide a collection of
// resource allocators that would use resource availability for their pool
// management strategies.

// A Straightforward Reference Counting Approach: Maintain an array of integers
// with length number of cpuset bits. As each resource (bitmap) is obtained,
// increment the internal counter of each corresponding position. When a
// resource is released, decrement in a similar way. If a location in the array
// is zero, then the resource is not in use. For devices, we can take a similar
// approach using the device IDs instead of the bit positions.

/**
 * Takes a vector of colors and clamps their values to [0, ndc)
 * in place, where ndc is the number of distinct numbers found in values.
 */
static int
clamp_colors(
    std::vector<int> &colors
) {
    // Recall: sets are ordered.
    const std::set<int> colorset(colors.begin(), colors.end());
    // Maps the input vector colors to their clamped values.
    std::map<int, int> colors2clamped;
    // color': the clamped color.
    int colorp = 0;
    for (const auto val : colorset) {
        colors2clamped.insert({val, colorp++});
    }
    for (size_t i = 0; i < colors.size(); ++i) {
        colors[i] = colors2clamped[colors[i]];
    }
    return QV_SUCCESS;
}

qvi_hwsplit::qvi_hwsplit(
    qv_scope *parent,
    size_t group_size,
    size_t split_size,
    qv_hw_obj_type_t split_at_type
) : m_rmi(parent->group().task().rmi())
  , m_my_hwpool(parent->hwpool())
  , m_group_size(group_size)
  , m_split_size(split_size)
  , m_split_at_type(split_at_type)
{
    const int rc = parent->group().task().bind_top(m_my_cpu_affinity);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);

    // To save memory we don't eagerly resize our vectors to group_size
    // since most processes will not use the storage. For example, in the
    // collective case the root ID process will be the only one needing
    // group_size elements in our vectors. We'll let the call paths enforce
    // appropriate vector sizing.
}

void
qvi_hwsplit::m_reserve(void)
{
    m_group_tids.resize(m_group_size);
    m_hwpools.resize(m_group_size);
    m_colors.resize(m_group_size);
    m_task_affinities.resize(m_group_size);
}

int
qvi_hwsplit::m_finalize_mapping(
    const qvi_map_t &map
) {
    for (const auto &[taski, cpusetis] : map) {
        for (const auto &c : cpusetis) {
            // Just use the only value that should be there.
            m_hwpools.at(taski) = qvi_hwpool(m_split_cpusets.at(c));
        }
    }

    //m_colors = qvi_map_flatten_to_colors(map);

    std::vector<qvi_hwloc_bitmap> hwpool_aff;
    for (const auto &hwpool : m_hwpools) {
        hwpool_aff.emplace_back(hwpool.cpuset());
    }
    // Iterate over supported device types and add devices based on affinity.
    for (const auto devt : qvi_hwloc::supported_devices()) {
        const auto devs = m_base_hwpool.devices(devt);
        if (devs.empty()) continue;
        // If we have devices, then get their affinities.
        const auto dev_affinities = m_base_hwpool.device_affinities(devt);
        // Map devices to cpusets, trying to maintain good affinity.
        qvi_map_config devs2hres_config;
        devs2hres_config.src_affinities = dev_affinities;
        devs2hres_config.dst_affinities = hwpool_aff;
        qvi_map_t devs2hres_map;
        int rc = qvi_map_affinity_preserving(
            devs2hres_config, devs2hres_map
        );
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        // Now that we have the mapping, assign
        // devices to the associated hardware pools.
        for (const auto &[devi, poolis] : devs2hres_map) {
            for (const auto &pooli : poolis) {
                //qvi_log_debug("adding dev {} to hwpool {}", devi, pooli);
                rc = m_hwpools[pooli].add_device(devs[devi]);
                if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
            }
        }
    }
    return QV_SUCCESS;
}

std::pair<qv_hw_obj_type_t, qvi_hwloc_bitmap>
qvi_hwsplit::m_primary_cpuset_for_split(
    qv_hw_obj_type_t requested_type
) const {
    qv_hw_obj_type_t real_type = requested_type;
    // Were we provided a real resource type that we have to split? Or was
    // QV_HW_OBJ_LAST instead provided to indicate that we were called from a
    // split() context.
    if (requested_type == QV_HW_OBJ_LAST) {
        // Pick PUs as the host resource, since this is
        // the atomic unit at which host resources are split.
        real_type = QV_HW_OBJ_PU;
    }

    if (qvi_hwloc::obj_is_host_resource(real_type)) {
        return {real_type, m_base_hwpool.cpuset()};
    }
    // Else, an OS device. The cpuset will be
    // the union over the devices affinities.
    qvi_hwloc_bitmap result;
    for (const auto &dev : m_base_hwpool.devices(real_type)) {
        result = result | dev.affinity();
    }
    return {real_type, result};
}

int
qvi_hwsplit::m_setup_map_config(void)
{
    int rc = QV_SUCCESS;
    // Make sure that the supplied colors are consistent and determine the type
    // of coloring we are using. Positive values denote an explicit coloring
    // provided by the caller. Negative values are reserved for internal use and
    // shall be constants defined in quo-vadis.h. Note we don't sort m_colors
    // directly because they are ordered by task ID.
    std::vector<int> tcolors(m_colors);
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
    bool auto_split = false;
    // All colors are positive.
    if (tcolors.front() >= 0) {
        rc = clamp_colors(m_colors);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    }
    // Some values are negative.
    else {
        // TODO(skg) Implement the rest.
        if (tcolors.front() != tcolors.back()) {
            return QV_ERR_INVLD_ARG;
        }
        auto_split = true;
    }
    // Determine the primary type and cpuset that we are splitting over.
    const auto [pri_type, pri_cpuset] = m_primary_cpuset_for_split(
        m_split_at_type
    );
    // TODO(skg) Audit this.
    const size_t real_split_size = std::min(m_split_size, m_group_size);
    // Split the primary cpuset into the requested split size pieces.
    rc = m_rmi.hwloc().bitmap_split(
        pri_cpuset, real_split_size, m_split_cpusets
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // User-defined splitting. Map colors to cpusets.
    if (!auto_split) {
        m_map_config.src_colors = m_colors;
        m_map_config.ndst = m_split_cpusets.size();
        m_map_config.map_fn = qvi_map_colors;
        return QV_SUCCESS;
    }
    // Automatic splitting.
    switch (m_colors[0]) {
        case QV_SCOPE_SPLIT_AFFINITY_PRESERVING: {
            m_map_config.src_affinities = m_task_affinities;
            m_map_config.dst_affinities = m_split_cpusets;
            m_map_config.map_fn = qvi_map_affinity_preserving;
            return QV_SUCCESS;
        }
        case QV_SCOPE_SPLIT_PACKED: {
            m_map_config.nsrc = m_group_size;
            m_map_config.ndst = m_split_cpusets.size();
            m_map_config.map_fn = qvi_map_packed;
            return QV_SUCCESS;
        }
        case QV_SCOPE_SPLIT_SPREAD: {
            m_map_config.nsrc = m_group_size;
            m_map_config.ndst = m_split_cpusets.size();
            m_map_config.map_fn = qvi_map_spread;
            return QV_SUCCESS;
        }
        default:
            return QV_ERR_INVLD_ARG;
    }
    // Shouldn't get here.
    return QV_ERR;
}

/**
 * Splits aggregate scope data.
 */
int
qvi_hwsplit::m_split(void)
{
    // Perform the split and map on the host
    // resources based on the configuration.
    int rc = m_setup_map_config();
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    qvi_map_t hostres_map;
    rc = m_map_config.map_fn(m_map_config, hostres_map);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return m_finalize_mapping(hostres_map);
}

int
qvi_hwsplit::m_gather_split_data(
    const qvi_group &group,
    int rootid,
    qvi_hwsplit &hwsplit,
    int color
) {
    int rc = qvi_coll::gather(
        group, rootid, qvi_task::mytid(), hwsplit.m_group_tids
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = qvi_coll::gather(
        group, rootid, hwsplit.m_my_cpu_affinity, hwsplit.m_task_affinities
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = qvi_coll::gather(
        group, rootid, color, hwsplit.m_colors
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // The root creates new hardware pools so it can modify them freely. Use
    // m_hwpools as a temporary buffer to store all the base hardware pools.
    rc = qvi_coll::gather(
        group, rootid, hwsplit.m_my_hwpool, hwsplit.m_hwpools
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // The root creates the base hardware pool that it will later split.
    if (group.rank() == rootid) {
        // The base hardware pool is the union of all provided hardware pools.
        hwsplit.m_base_hwpool  = qvi_hwpool::set_union(hwsplit.m_hwpools);
        // The temporary hardware pools are no longer needed.
        hwsplit.m_hwpools.clear();
        // Create room for the real hardware pools.
        hwsplit.m_hwpools.resize(group.size());
    }
    return QV_SUCCESS;
}

int
qvi_hwsplit::m_scatter_split_results(
    const qvi_group &group,
    int rootid,
    const qvi_hwsplit &hwsplit,
    int *colorp,
    qvi_hwpool &result
) {
    const int rc = qvi_coll::scatter(group, rootid, hwsplit.m_colors, *colorp);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return qvi_coll::scatter(group, rootid, hwsplit.m_hwpools, result);
}

int
qvi_hwsplit::split(
    qv_scope_t *parent,
    size_t npieces,
    int color,
    qv_hw_obj_type_t maybe_obj_type,
    int *colorp,
    qvi_hwpool &result
) {
    const qvi_group &pgroup = parent->group();
    // Everyone create a hardware split object.
    qvi_hwsplit hwsplit(
        parent, pgroup.size(), npieces, maybe_obj_type
    );
    // First consolidate the provided information, as this is coming from a
    // SPMD-like context (e.g., splitting a resource shared by MPI processes).
    // In most cases it is easiest to have a single task calculate the split
    // based on global knowledge and later redistribute the calculated result to
    // its group members. Note that aggregated data are only valid for the task
    // whose id is equal to qvi_hwsplit::s_root after gather has completed.
    int rc = m_gather_split_data(pgroup, s_root, hwsplit, color);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // The root does this calculation.
    int rc2 = QV_SUCCESS;
    if (pgroup.rank() == s_root) {
        rc2 = hwsplit.m_split();
    }
    // Wait for the split information. Explicitly barrier here in case the
    // underlying collective operations poll heavily for completion.
    rc = pgroup.barrier();
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // To avoid hangs in split error paths, share the split rc with everyone.
    rc = qvi_coll::bcast(pgroup, s_root, rc2);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // If the split failed, return the error to all participants.
    if (qvi_unlikely(rc2 != QV_SUCCESS)) return rc2;
    // Scatter the results.
    return m_scatter_split_results(pgroup, s_root, hwsplit, colorp, result);
}

int
qvi_hwsplit::thread_split(
    qv_scope_t *parent,
    size_t npieces,
    int *kcolors,
    size_t k,
    qv_hw_obj_type_t maybe_obj_type,
    std::vector<int> &kcolorps,
    std::vector<qvi_hwpool> &khwpools
) {
    const size_t group_size = k;
    // Construct the hardware split.
    qvi_hwsplit hwsplit(parent, group_size, npieces, maybe_obj_type);
    // Eagerly make room for the group member information.
    hwsplit.m_reserve();
    // Since this is called by a single task, get its ID and associated
    // hardware affinity here, and replicate them in the following loop
    // that populates hwsplit.
    //No point in doing this in a loop.
    const pid_t taskid = qvi_task::mytid();
    // Set the base hardware pool. Since the parent has it, just copy it over.
    hwsplit.m_base_hwpool = hwsplit.m_my_hwpool;
    // Get the task's current affinity.
    qvi_hwloc_bitmap task_affinity;
    int rc = parent->group().task().bind_top(task_affinity);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Prepare the hwsplit with our parent's information.
    for (size_t i = 0; i < group_size; ++i) {
        // Store requested colors in aggregate.
        hwsplit.m_colors.at(i) = kcolors[i];
        // Since this is called by a single task, replicate its task ID, too.
        hwsplit.m_group_tids.at(i) = taskid;
        // Same goes for the task's affinity.
        hwsplit.m_task_affinities.at(i) = task_affinity;
    }
    // Split the hardware resources based on the provided split parameters.
    rc = hwsplit.m_split();
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Now populate the hardware pools as the result.
    khwpools = hwsplit.m_hwpools;
    kcolorps = hwsplit.m_colors;
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
