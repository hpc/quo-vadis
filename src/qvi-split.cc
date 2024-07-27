/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-split.cc
 */

#include "qvi-split.h"
#include "qvi-bbuff.h"
#include "qvi-rmi.h"
#include "qvi-bbuff-rmi.h"
#include "qvi-task.h" // IWYU pragma: keep
#include "qvi-scope.h" // IWYU pragma: keep

/** Maintains a mapping between IDs to device information. */
using id2devs_t = std::multimap<int, const qvi_hwpool_dev_s *>;

qvi_hwsplit_s::qvi_hwsplit_s(
    qv_scope_t *parent,
    uint_t group_size,
    uint_t split_size,
    qv_hw_obj_type_t split_at_type
) : m_rmi(parent->m_group->task()->rmi())
  , m_hwpool(parent->m_hwpool)
  , m_group_size(group_size)
  , m_split_size(split_size)
  , m_split_at_type(split_at_type)
{
    // To save memory we don't eagerly resize our vectors to group_size
    // since most processes will not use the storage. For example, in the
    // collective case the root ID process will be the only one needing
    // group_size elements in our vectors. We'll let the call paths enforce
    // appropriate vector sizing.
}

qvi_hwsplit_s::~qvi_hwsplit_s(void)
{
    for (auto &hwpool : m_hwpools) {
        qvi_delete(&hwpool);
    }
}

void
qvi_hwsplit_s::reserve(void)
{
    m_taskids.resize(m_group_size);
    m_hwpools.resize(m_group_size);
    m_colors.resize(m_group_size);
    m_affinities.resize(m_group_size);
}

qvi_hwloc_bitmap_s
qvi_hwsplit_s::cpuset(void) const
{
    // This shouldn't happen.
    assert(m_hwpools.size() != 0);
    return m_hwpools[0]->cpuset();
}

int
qvi_hwsplit_s::split_cpuset(
    qvi_hwloc_cpusets_t &result
) const {
    // The cpuset that we are going to split.
    const qvi_hwloc_bitmap_s base_cpuset = cpuset();
    // Pointer to my hwloc instance.
    qvi_hwloc_t *const hwloc = qvi_rmi_client_hwloc(m_rmi);
    // Holds the host's split cpusets.
    result.resize(m_split_size);
    // Notice that we do not go through the RMI for this because this is just an
    // local, temporary splitting that is ultimately fed to another splitting
    // algorithm.
    int rc = QV_SUCCESS;
    for (uint_t chunkid = 0; chunkid < m_split_size; ++chunkid) {
        rc = qvi_hwloc_split_cpuset_by_chunk_id(
            hwloc, base_cpuset.cdata(), m_split_size,
            chunkid, result[chunkid].data()
        );
        if (rc != QV_SUCCESS) break;
    }
    return rc;
}

int
qvi_hwsplit_s::osdev_cpusets(
    qvi_hwloc_cpusets_t &result
) const {
    // Get the number of devices we have available in the provided scope.
    int nobj = 0;
    int rc = m_hwpool->nobjects(
        qvi_rmi_client_hwloc(m_rmi), m_split_at_type, &nobj
    );
    if (rc != QV_SUCCESS) return rc;
    // Holds the device affinities used for the split.
    result.resize(nobj);
    uint_t affi = 0;
    for (const auto &dinfo : m_hwpool->devices()) {
        // Not the type we are looking to split.
        if (m_split_at_type != dinfo.first) continue;
        // Copy the device's affinity to our list of device affinities.
        result[affi++] = dinfo.second->affinity();
    }
    return rc;
}

int
qvi_hwsplit_s::primary_cpusets(
    qvi_hwloc_cpusets_t &result
) const {
    // We were provided a real host resource type that we have to split. Or
    // QV_HW_OBJ_LAST is instead provided to indicate that we were called from a
    // split() context, which uses the host's cpuset to split the resources.
    // TODO(skg) This looks suspicious to me. Make sure we want to do this.
    // What about getting called from a split context for devices?
    if (qvi_hwloc_obj_type_is_host_resource(m_split_at_type) ||
        m_split_at_type == QV_HW_OBJ_LAST) {
        return split_cpuset(result);
    }
    // An OS device.
    else {
        return osdev_cpusets(result);
    }
}

qvi_map_fn_t
qvi_hwsplit_s::affinity_preserving_policy(void) const
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
/** Releases all devices contained in the provided split aggregate. */
int
qvi_hwsplit_s::release_devices(void)
{
    int rc = QV_SUCCESS;
    for (auto &hwpool : m_hwpools) {
        rc = hwpool->release_devices();
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
qvi_hwsplit_s::split_devices_user_defined(void)
{
    // Release devices from the hardware pools because
    // they will be redistributed in the next step.
    int rc = release_devices();
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
    auto dinfos = m_hwpool->devices();
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
                rc = m_hwpools[i]->add_device(*c2d.second);
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
qvi_hwsplit_s::split_devices_affinity_preserving(void)
{
    // Release devices from the hardware pools because
    // they will be redistributed in the next step.
    int rc = release_devices();
    if (rc != QV_SUCCESS) return rc;
    // Get a pointer to device infos associated with the parent hardware pool.
    auto dinfos = m_hwpool->devices();
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
            devaffs.push_back(dev->affinity());
        }

        qvi_map_t map;
        const auto policy = affinity_preserving_policy();
        rc = qvi_map_affinity_preserving(
            map, policy, devaffs, m_affinities
        );
        if (rc != QV_SUCCESS) return rc;
        //qvi_map_debug_dump(map);
        // Now that we have the mapping, assign
        // devices to the associated hardware pools.
        for (const auto &mi : map) {
            const uint_t devid = mi.first;
            const uint_t pooli = mi.second;
            rc = m_hwpools[pooli]->add_device(*devs[devid]);
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) break;
    }
    return rc;
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
/**
 * User-defined split.
 */
int
qvi_hwsplit_s::split_user_defined(void)
{
    // Split the base cpuset into the requested number of pieces.
    qvi_hwloc_cpusets_t cpusets;
    int rc = split_cpuset(cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Developer sanity check.
    assert(cpusets.size() == m_split_size);
    // Maintains the mapping between task (consumer) IDs and resource IDs.
    qvi_map_t map{};
    rc = qvi_map_colors(map, m_colors, cpusets);
    if (rc != QV_SUCCESS) return rc;
    qvi_hwloc_t *const hwloc = qvi_rmi_client_hwloc(m_rmi);
    // Update the hardware pools and colors to reflect the new mapping.
    rc = apply_cpuset_mapping(
        hwloc, map, cpusets, m_hwpools, m_colors
    );
    if (rc != QV_SUCCESS) return rc;
    // Use a straightforward device splitting algorithm based on user's request.
    return split_devices_user_defined();
}

int
qvi_hwsplit_s::split_affinity_preserving_pass1(void)
{
    // cpusets used for first mapping pass.
    qvi_hwloc_cpusets_t cpusets{};
    // Get the primary cpusets used for the first pass of mapping.
    int rc = primary_cpusets(cpusets);
    if (rc != QV_SUCCESS) return rc;
    // Maintains the mapping between task (consumer) IDs and resource IDs.
    qvi_map_t map{};
    // Map tasks based on their affinity to resources encoded by the cpusets.
    const auto policy = affinity_preserving_policy();
    rc = qvi_map_affinity_preserving(
        map, policy, m_affinities, cpusets
    );
    if (rc != QV_SUCCESS) return rc;
    // Make sure that we mapped all the tasks. If not, this is a bug.
    if (qvi_map_nfids_mapped(map) != m_group_size) {
        qvi_abort();
    }
    qvi_hwloc_t *const hwloc = qvi_rmi_client_hwloc(m_rmi);
    // Update the hardware pools and colors to reflect the new mapping.
    return apply_cpuset_mapping(
        hwloc, map, cpusets, m_hwpools, m_colors
    );
}

/**
 * Affinity preserving split.
 */
// TODO(skg) This needs more work.
int
qvi_hwsplit_s::split_affinity_preserving(void)
{
    int rc = split_affinity_preserving_pass1();
    if (rc != QV_SUCCESS) return rc;
    // Finally, split the devices.
    return split_devices_affinity_preserving();
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
qvi_hwsplit_s::split(void)
{
    int rc = QV_SUCCESS;
    bool auto_split = false;
    // Make sure that the supplied colors are consistent and determine the type
    // of coloring we are using. Positive values denote an explicit coloring
    // provided by the caller. Negative values are reserved for internal
    // use and shall be constants defined in quo-vadis.h. Note we don't sort the
    // splitagg's colors directly because they are ordered by task ID.
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
    // User-defined splitting.
    if (!auto_split) {
        return split_user_defined();
    }
    // Automatic splitting.
    switch (m_colors[0]) {
        case QV_SCOPE_SPLIT_AFFINITY_PRESERVING:
            return split_affinity_preserving();
        default:
            rc = QV_ERR_INVLD_ARG;
            break;
    }
    return rc;
}

qvi_coll_hwsplit_s::qvi_coll_hwsplit_s(
    qv_scope_t *parent,
    uint_t npieces,
    int color,
    qv_hw_obj_type_t split_at_type
) : m_parent(parent)
  , m_color(color)
{
    const qvi_group_t *const pgroup = m_parent->m_group;
    if (pgroup->rank() == qvi_coll_hwsplit_s::s_rootid) {
        m_hwsplit = qvi_hwsplit_s(
            m_parent, pgroup->size(), npieces, split_at_type
        );
    }
}

template <typename TYPE>
int
qvi_coll_hwsplit_s::scatter_values(
    const std::vector<TYPE> &values,
    TYPE *value
) {
    static_assert(std::is_trivially_copyable<TYPE>::value, "");

    int rc = QV_SUCCESS;
    qvi_bbuff_t *rxbuff = nullptr;

    qvi_group_t *const group = m_parent->m_group;
    std::vector<qvi_bbuff_t *> txbuffs(0);
    if (group->rank() == s_rootid) {
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

    rc = group->scatter(txbuffs.data(), s_rootid, &rxbuff);
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

template <typename TYPE>
int
qvi_coll_hwsplit_s::bcast_value(
    TYPE *value
) {
    static_assert(std::is_trivially_copyable<TYPE>::value, "");
    qvi_group_t *const group = m_parent->m_group;

    std::vector<TYPE> values;
    if (group->rank() == s_rootid) {
        values.resize(group->size());
        std::fill(values.begin(), values.end(), *value);
    }
    return scatter_values(values, value);
}

template <typename TYPE>
int
qvi_coll_hwsplit_s::gather_values(
    TYPE invalue,
    std::vector<TYPE> &outvals
) {
    static_assert(std::is_trivially_copyable<TYPE>::value, "");
    qvi_group_t *const group = m_parent->m_group;
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
    rc = group->gather(txbuff, s_rootid, &shared, &bbuffs);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // The root fills in the output.
    if (group->rank() == s_rootid) {
        outvals.resize(group_size);
        // Unpack the values.
        for (uint_t i = 0; i < group_size; ++i) {
            outvals[i] = *(TYPE *)bbuffs[i]->data();
        }
    }
out:
    if (!shared || (shared && (group->rank() == s_rootid))) {
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

int
qvi_coll_hwsplit_s::gather_hwpools(
    qvi_hwpool_s *txpool,
    std::vector<qvi_hwpool_s *> &rxpools
) {
    qvi_group_t *const group = m_parent->m_group;
    const uint_t group_size = group->size();
    // Pack the hardware pool into a buffer.
    qvi_bbuff_t txbuff;
    int rc = txpool->packinto(&txbuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Gather the values to the root.
    bool shared = false;
    qvi_bbuff_t **bbuffs = nullptr;
    rc = group->gather(&txbuff, s_rootid, &shared, &bbuffs);
    if (rc != QV_SUCCESS) goto out;

    if (group->rank() == s_rootid) {
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
    if (!shared || (shared && (group->rank() == s_rootid))) {
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

int
qvi_coll_hwsplit_s::gather(void)
{
    int rc = gather_values(qvi_task_t::mytid(), m_hwsplit.m_taskids);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Note that the result hwpools are copies, so we can modify them freely.
    rc = gather_hwpools(m_parent->m_hwpool, m_hwsplit.m_hwpools);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = gather_values(m_color, m_hwsplit.m_colors);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    const int myid = m_parent->m_group->rank();
    const uint_t group_size = m_parent->m_group->size();
    if (myid == qvi_coll_hwsplit_s::s_rootid) {
        m_hwsplit.m_affinities.resize(group_size);
        for (uint_t tid = 0; tid < group_size; ++tid) {
            hwloc_cpuset_t cpuset = nullptr;
            rc = m_parent->m_group->task()->bind_top(&cpuset);
            if (qvi_unlikely(rc != QV_SUCCESS)) break;
            //
            rc = m_hwsplit.m_affinities[tid].set(cpuset);
            // Clean up.
            qvi_hwloc_bitmap_delete(&cpuset);
            if (qvi_unlikely(rc != QV_SUCCESS)) break;
        }
    }
    return rc;
}

int
qvi_coll_hwsplit_s::scatter_hwpools(
    const std::vector<qvi_hwpool_s *> &pools,
    qvi_hwpool_s **pool
) {
    int rc = QV_SUCCESS;
    std::vector<qvi_bbuff_t *> txbuffs(0);
    qvi_bbuff_t *rxbuff = nullptr;

    qvi_group_t *const group = m_parent->m_group;

    if (group->rank() == s_rootid) {
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

    rc = group->scatter(txbuffs.data(), s_rootid, &rxbuff);
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

int
qvi_coll_hwsplit_s::scatter(
    int *colorp,
    qvi_hwpool_s **result
) {
    const int rc = scatter_values(m_hwsplit.m_colors, colorp);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    return scatter_hwpools(m_hwsplit.m_hwpools, result);
}

int
qvi_coll_hwsplit_s::barrier(void)
{
    return m_parent->m_group->barrier();
}

int
qvi_coll_hwsplit_s::split(
    int *colorp,
    qvi_hwpool_s **result
) {
    // First consolidate the provided information, as this is coming from a
    // SPMD-like context (e.g., splitting a resource shared by MPI processes).
    // In most cases it is easiest to have a single task calculate the split
    // based on global knowledge and later redistribute the calculated result to
    // its group members. Note that aggregated data are only valid for the task
    // whose id is equal to qvi_global_split_t::rootid after gather has
    // completed.
    int rc = gather();
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // The root does this calculation.
    int rc2 = QV_SUCCESS;
    if (m_parent->m_group->rank() == s_rootid) {
        rc2 = m_hwsplit.split();
    }
    // Wait for the split information. Explicitly barrier here in case the
    // underlying broadcast implementation polls heavily for completion.
    rc = barrier();
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // To avoid hangs in split error paths, share the split rc with everyone.
    rc = bcast_value(&rc2);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // If the split failed, return the error to all participants.
    if (qvi_unlikely(rc2 != QV_SUCCESS)) return rc2;
    // Scatter the results.
    return scatter(colorp, result);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
