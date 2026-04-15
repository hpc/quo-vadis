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

/** Maintains a mapping between IDs to device information. */
using id2devs_t = std::multimap<int, const qvi_hwpool_dev *>;

qvi_hwsplit::qvi_hwsplit(
    qv_scope *parent,
    uint_t group_size,
    uint_t split_size,
    qv_hw_obj_type_t split_at_type
) : m_rmi(parent->group().task().rmi())
  , m_my_hwpool(parent->hwpool())
  , m_group_size(group_size)
  , m_split_size(split_size)
  , m_split_at_type(split_at_type)
{
    qvi_hwloc_bitmap task_affinity;
    int rc = parent->group().task().bind_top(task_affinity);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);

    rc = m_cpu_affinity.set(task_affinity.cdata());
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
    m_cpu_affinities.resize(m_group_size);
}

const qvi_hwloc_bitmap &
qvi_hwsplit::m_cpuset(void) const
{
    return m_base_hwpool.cpuset();
}

int
qvi_hwsplit::m_split_cpuset(
    std::vector<qvi_hwloc_bitmap> &result
) const {
    return m_rmi.hwloc().bitmap_split(
        m_base_hwpool.cpuset(), m_split_size, result
    );
}

int
qvi_hwsplit::m_osdev_cpusets(
    std::vector<qvi_hwloc_bitmap> &result
) const {
    auto [first, last] = m_base_hwpool.devices().equal_range(m_split_at_type);
    auto view = std::ranges::subrange(first, last);
    for (const auto &[dev_type, dev] : view) {
        result.emplace_back(dev.affinity());
    }
    return QV_SUCCESS;
}

// TODO(skg) Merge with hwpool things like primary_cpuset_for_split() or delete.
int
qvi_hwsplit::m_primary_cpusets(
    std::vector<qvi_hwloc_bitmap> &result
) const {
    // We were provided a real host resource type that we have to split. Or
    // QV_HW_OBJ_LAST is instead provided to indicate that we were called from a
    // split() context, which uses the host's cpuset to split the resources.
    if (qvi_hwloc::obj_is_host_resource(m_split_at_type) ||
        m_split_at_type == QV_HW_OBJ_LAST) {
        return m_split_cpuset(result);
    }
    // An OS device.
    else {
        return m_osdev_cpusets(result);
    }
}

qvi_map_fn_t
qvi_hwsplit::affinity_preserving_policy(void) const
{
    switch (m_split_at_type) {
        // For split()
        case QV_HW_OBJ_LAST:
            return qvi_map_packed;
        // For split_at()
        default:
            return qvi_map_spread;
    }
}

int
qvi_hwsplit::m_release_devices(void)
{
    int rc = QV_SUCCESS;
    for (auto &hwpool : m_hwpools) {
        rc = hwpool.release_devices();
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    }
    return rc;
}

/**
 * Straightforward user-defined device splitting.
 */
// TODO(skg) Plenty of opportunity for optimization.
// TODO(skg) Move lots of logic to map
int
qvi_hwsplit::split_devices_user_defined(void)
{
    // Release devices from the hardware pools because
    // they will be redistributed in the next step.
    int rc = m_release_devices();
    if (rc != QV_SUCCESS) return rc;
    // Determine mapping of colors to task IDs. The array index i of colors is
    // the color requested by task i. Also determine the number of distinct
    // colors provided in the colors array.
    std::set<int> color_set(m_colors.begin(), m_colors.end());
    // Adjust the color set so that the distinct colors provided
    // fall within the range of the number of splits requested.
    std::set<int> color_setp;
    uint_t ncolors_chosen = 0;
    for (const auto &c : color_set) {
        if (ncolors_chosen >= m_split_size) break;
        color_setp.insert(c);
        ncolors_chosen++;
    }
    // Cache all device infos associated with the parent hardware pool.
    auto dinfos = m_base_hwpool.devices();
    // Iterate over the supported device types and split them up round-robin.
    // TODO(skg) Should this be a mapping operation in qvi-map?
    for (const auto devt : qvi_hwloc::supported_devices()) {
        // Get the number of devices.
        const uint_t ndevs = dinfos.count(devt);
        // Store device infos.
        std::vector<const qvi_hwpool_dev *> devs;
        for (const auto &dinfo : dinfos) {
            // Not the type we are currently dealing with.
            if (devt != dinfo.first) continue;
            devs.push_back(&dinfo.second);
        }
        // Maps colors to device information.
        id2devs_t devmap;
        uint_t devi = 0;
        while (devi < ndevs) {
            for (const auto &c : color_setp) {
                if (devi >= ndevs) break;
                devmap.insert(std::make_pair(c, devs[devi++]));
            }
        }
        // Now that we have the mapping of colors to devices, assign devices to
        // the associated hardware pools.
        for (uint_t i = 0; i < m_group_size; ++i) {
            const int color = m_colors[i];
            for (const auto &c2d : devmap) {
                if (c2d.first != color) continue;
                rc = m_hwpools[i].add_device(*c2d.second);
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
int
qvi_hwsplit::split_devices_affinity_preserving(void)
{
    // Release devices from the hardware pools because
    // they will be redistributed in the next step.
    int rc = m_release_devices();
    if (rc != QV_SUCCESS) return rc;
    // Get a pointer to device infos associated with the parent hardware pool.
    auto dinfos = m_base_hwpool.devices();
    // Iterate over the supported device types and split them up.
    for (const auto devt : qvi_hwloc::supported_devices()) {
        // Store device infos.
        std::vector<const qvi_hwpool_dev *> devs;
        for (const auto &dinfo : dinfos) {
            // Not the type we are currently dealing with.
            if (devt != dinfo.first) continue;
            devs.push_back(&dinfo.second);
        }
        // Store device affinities.
        std::vector<qvi_hwloc_bitmap> devaffs;
        for (const auto &dev : devs) {
            devaffs.push_back(dev->affinity());
        }

        qvi_map_t map;
        const auto policy = affinity_preserving_policy();
        rc = qvi_map_affinity_preserving(
            map, policy, devaffs, m_cpu_affinities
        );
        if (rc != QV_SUCCESS) return rc;
        //qvi_map_debug_dump(map);
        // Now that we have the mapping, assign
        // devices to the associated hardware pools.
        for (const auto &mi : map) {
            const uint_t devid = mi.first;
            const uint_t pooli = mi.second;
            rc = m_hwpools[pooli].add_device(*devs[devid]);
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) break;
    }
    return rc;
}

static int
apply_cpuset_mapping(
    qvi_hwloc &hwloc,
    const qvi_map_t &map,
    const std::vector<qvi_hwloc_bitmap> &cpusets,
    std::vector<qvi_hwpool> &hwpools,
    std::vector<int> &colors
) {
    int rc = QV_SUCCESS;

    const uint_t npools = hwpools.size();
    for (uint_t pid = 0; pid < npools; ++pid) {
        rc = hwpools.at(pid).initialize(
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

/**
 * User-defined split.
 */
#if 1
int
qvi_hwsplit::split_user_defined(void)
{
    // Split the base cpuset into the requested number of pieces.
    std::vector<qvi_hwloc_bitmap> cpusets;
    int rc = m_split_cpuset(cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Developer sanity check.
    assert(cpusets.size() == m_split_size);
    // Maintains the mapping between task (consumer) IDs and resource IDs.
    qvi_map_t map{};
    rc = qvi_map_colors(map, m_colors, cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Update the hardware pools and colors to reflect the new mapping.
    rc = apply_cpuset_mapping(
        m_rmi.hwloc(), map, cpusets, m_hwpools, m_colors
    );
    if (rc != QV_SUCCESS) return rc;
    // Use a straightforward device splitting algorithm based on user's request.
    return split_devices_user_defined();
}
#else
int
qvi_hwsplit::split_user_defined(void)
{

}
#endif

int
qvi_hwsplit::split_affinity_preserving_pass1(void)
{
    // cpusets used for first mapping pass.
    std::vector<qvi_hwloc_bitmap> cpusets;
    // Get the primary cpusets used for the first pass of mapping.
    int rc = m_primary_cpusets(cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Maintains the mapping between task (consumer) IDs and resource IDs.
    qvi_map_t map;
    // Map tasks based on their affinity to resources encoded by the cpusets.
    const auto policy = affinity_preserving_policy();
    rc = qvi_map_affinity_preserving(
        map, policy, m_cpu_affinities, cpusets
    );
    if (rc != QV_SUCCESS) return rc;
    // Make sure that we mapped all the tasks. If not, this is a bug.
    if (qvi_map_nfids_mapped(map) != m_group_size) {
        qvi_abort();
    }
    // Update the hardware pools and colors to reflect the new mapping.
    return apply_cpuset_mapping(
        m_rmi.hwloc(), map, cpusets, m_hwpools, m_colors
    );
}

/**
 * Affinity preserving split.
 */
// TODO(skg) This needs more work.
int
qvi_hwsplit::split_affinity_preserving(void)
{
    int rc = split_affinity_preserving_pass1();
    if (rc != QV_SUCCESS) return rc;
    // Finally, split the devices.
    return split_devices_affinity_preserving();
}

// TODO(skg) Add device splitting.
int
qvi_hwsplit::split_packed(void)
{
    // cpusets used for mapping.
    std::vector<qvi_hwloc_bitmap> cpusets;
    // Get the primary cpusets for the mapping.
    int rc = m_primary_cpusets(cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Maintains the mapping between task (consumer) IDs and resource IDs.
    qvi_map_t map;
    rc = qvi_map_packed(map, m_group_size, cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Make sure that we mapped all the tasks. If not, this is a bug.
    if (qvi_map_nfids_mapped(map) != m_group_size) {
        qvi_abort();
    }
    // Update the hardware pools and colors to reflect the new mapping.
    return apply_cpuset_mapping(
        m_rmi.hwloc(), map, cpusets, m_hwpools, m_colors
    );
}

// TODO(skg) Add device splitting.
int
qvi_hwsplit::split_spread(void)
{
    // cpusets used for mapping.
    std::vector<qvi_hwloc_bitmap> cpusets;
    // Get the primary cpusets for the mapping.
    int rc = m_primary_cpusets(cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Maintains the mapping between task (consumer) IDs and resource IDs.
    qvi_map_t map;
    rc = qvi_map_spread(map, m_group_size, cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Make sure that we mapped all the tasks. If not, this is a bug.
    if (qvi_map_nfids_mapped(map) != m_group_size) {
        qvi_abort();
    }
    // Update the hardware pools and colors to reflect the new mapping.
    return apply_cpuset_mapping(
        m_rmi.hwloc(), map, cpusets, m_hwpools, m_colors
    );
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
int
qvi_hwsplit::m_split(void)
{
    int rc = QV_SUCCESS;
    bool auto_split = false;
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

    // All colors are positive.
    if (tcolors.front() >= 0) {
        rc = clamp_colors(m_colors);
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

    // TODO(skg) One split, then multiple maps?

    // TODO(skg) Favor using function pointers?
    // User-defined splitting.
    if (!auto_split) {
        return split_user_defined();
    }
    // Automatic splitting.
    switch (m_colors[0]) {
        case QV_SCOPE_SPLIT_AFFINITY_PRESERVING:
            return split_affinity_preserving();
        case QV_SCOPE_SPLIT_PACKED:
            return split_packed();
        case QV_SCOPE_SPLIT_SPREAD:
            return split_spread();
        default:
            rc = QV_ERR_INVLD_ARG;
            break;
    }
    return rc;
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
        group, rootid, hwsplit.m_cpu_affinity, hwsplit.m_cpu_affinities
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
    uint_t npieces,
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
    uint_t npieces,
    int *kcolors,
    uint_t k,
    qv_hw_obj_type_t maybe_obj_type,
    std::vector<int> &kcolorps,
    std::vector<qvi_hwpool> &khwpools
) {
    const uint_t group_size = k;
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
    for (uint_t i = 0; i < group_size; ++i) {
        // Store requested colors in aggregate.
        hwsplit.m_colors.at(i) = kcolors[i];
        // Since this is called by a single task, replicate its task ID, too.
        hwsplit.m_group_tids.at(i) = taskid;
        // Same goes for the task's affinity.
        hwsplit.m_cpu_affinities.at(i).set(task_affinity.cdata());
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
