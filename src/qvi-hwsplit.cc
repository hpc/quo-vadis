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
#include "qvi-task.h"
#include "qvi-rmi.h"
#include "qvi-coll.h"
#include "qvi-scope.h"

// TODOs
// * Resource reference counting.
// * Need to deal with resource unavailability.
// * Split and attach devices properly.
// * Have bitmap scratch pad that is initialized once, then destroyed? This
//   approach may be a nice allocation optimization, but in heavily threaded
//   code may be a bottleneck.
// * Add RMI to acquire/release resources.

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

static std::string
format_coloring(
    const std::vector<int> &colors
) {
    const size_t ncolors = colors.size();
    std::ostringstream oss;
    oss << "  Key: (tid, color)\n";
    oss << "{\n";
    for (size_t i = 0; i < ncolors; ++i) {
        oss << "  (" << i << ", " << colors.at(i) << ")";
        oss << ((i == ncolors - 1 ) ? "\n" : ",\n");
    }
    oss << "}\n";
    return oss.str();
}

qvi_hwsplit::qvi_hwsplit(
    qv_scope *parent,
    size_t group_size,
    size_t split_size,
    qv_hw_obj_type_t split_at_type
) : m_my_rmi(parent->group().task().rmi())
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

qvi_hwloc_bitmap
qvi_hwsplit::m_primary_cpuset_for_split(
    qv_hw_obj_type_t requested_type
) const {
    const auto res_class = qvi_hwloc::obj_res_class(requested_type);
    switch (res_class) {
        // Were we provided a real resource type that we have to split? Or was
        // QV_HW_OBJ_LAST instead provided to indicate that we were called from
        // a split() context.
        case QVI_HWLOC_RES_CLASS_LAST:
        case QVI_HWLOC_RES_CLASS_HOST:
            return m_base_hwpool.cpuset();
        case QVI_HWLOC_RES_CLASS_DEV: {
            // The cpuset will be the union over the devices affinities.
            qvi_hwloc_bitmap result;
            for (const auto &dev : m_base_hwpool.devices(requested_type)) {
                result = result | dev.get()->affinity();
            }
            return result;
        }
        [[unlikely]] default:
            throw qvi_runtime_error(QV_ERR_INTERNAL);
    }
}

std::vector<qvi_hwloc_bitmap>
qvi_hwsplit::m_split_base_cpuset(void)
{
    // Determine the cpuset that we are splitting over.
    const auto pri_cpuset = m_primary_cpuset_for_split(m_split_at_type);
    return m_my_rmi.hwloc().bitmap_split(pri_cpuset, m_split_size);
}

/**
 * Determine which kind of coloring we are dealing with and validate it.
 *
 * There are exactly two mutually exclusive kinds of coloring:
 *
 * 1. User-defined coloring: every value is either a non-negative,
 * caller-provided color, or the QV_SPLIT_UNDEFINED sentinel. Non-negative
 * colors select the piece a member joins; QV_SPLIT_UNDEFINED members opt out of
 * the split and receive no subscope (the scope layer returns a NULL scope to
 * such callers). A coloring consisting entirely of QV_SPLIT_UNDEFINED is
 * unusual (every member opts out) but valid.
 *
 * 2. Automatic grouping: every value is the *same* automatic grouping constant
 * (QV_SPLIT_CLOSE, QV_SPLIT_PACKED, QV_SPLIT_SPREAD, or QV_SPLIT_AUTO).
 * QV_SPLIT_UNDEFINED is NOT an automatic grouping constant and may not appear
 * in an automatic split.
 *
 * Any other combination (e.g. mixing a real color or QV_SPLIT_UNDEFINED with an
 * automatic constant, or mixing different automatic constants) is invalid.
 *
 * QV_SPLIT_UNDEFINED is the discriminator: it is only ever legal alongside
 * non-negative user-defined colors, never with the automatic constants.
 */
static std::vector<int>
normalize_colors(
    const std::vector<int> &colors,
    size_t split_size
) {
    const bool have_undefined = std::ranges::any_of(
        colors, [](int val) { return val == QV_SPLIT_UNDEFINED; }
    );
    const bool have_nonneg = std::ranges::any_of(
        colors, [](int val) { return val >= 0; }
    );
    // User-defined coloring: any non-negative color is present, or the caller
    // opted out via QV_SPLIT_UNDEFINED. Every value must then be either
    // non-negative or QV_SPLIT_UNDEFINED; an automatic constant here is a
    // programming error (automatic grouping cannot be mixed with an explicit
    // coloring or with opting out).
    if (have_nonneg || have_undefined) {
        const bool only_userdefined = std::ranges::all_of(
            colors, [](int val) {
                return val >= 0 || val == QV_SPLIT_UNDEFINED;
            }
        );
        if (!only_userdefined) {
            // A user-defined coloring was mixed with an automatic grouping
            // constant (or some other reserved negative value).
            throw qvi_runtime_error(QV_ERR_INVLD_ARG);
        }
        // Clamp the caller-provided colors to a usable [0, ndistinct) range for
        // internal consumption, unless they already sit in [0, split_size).
        // qvi_map_clamp_colors leaves QV_SPLIT_UNDEFINED members unchanged, so
        // they remain excluded from the split rather than folded into a color.
        const bool all_in_range = std::ranges::all_of(
            colors, [split_size](int val) {
                return val == QV_SPLIT_UNDEFINED ||
                       (val >= 0 && val < static_cast<int>(split_size));
            }
        );
        const auto result = all_in_range
            ? colors
            : qvi_map_clamp_colors(colors);
        // Validate the coloring. QV_SPLIT_UNDEFINED members do not occupy a
        // piece, so they do not count as distinct destinations.
        std::set<int> color_set(result.begin(), result.end());
        color_set.erase(QV_SPLIT_UNDEFINED);
        if (color_set.size() > split_size) {
            throw qvi_runtime_error(QV_ERR_SPLIT);
        }
        return result;
    }
    // Automatic grouping: no non-negative colors and no QV_SPLIT_UNDEFINED.
    // Every value must be the same automatic grouping constant.
    else {
        std::vector<int> tcolors(colors);
        std::sort(tcolors.begin(), tcolors.end());
        if (tcolors.front() != tcolors.back()) {
            throw qvi_runtime_error(QV_ERR_INVLD_ARG);
        }
        return colors;
    }
}

static size_t
determine_actual_split_size(
    const std::vector<int> &colors,
    size_t group_size,
    size_t requested_split_size
) {
    switch (colors.front()) {
        case QV_SPLIT_AUTO:
            // QV_SPLIT_AUTO has different semantics than the other split
            // options: it automatically determines a reasonable grouping and
            // splitting based on the resources being split, the requested split
            // size, and the group size. If the group size is smaller than the
            // requested split size, return the group size so that the split
            // does not leave a subset of the parent's resources unused.
            return std::min(group_size, requested_split_size);
        default:
            return requested_split_size;
    }
}

qvi_map_config
qvi_hwsplit::m_get_map_config(void)
{
    switch (m_colors.front()) {
        case QV_SPLIT_CLOSE:
            return qvi_map_config(
                m_task_affinities,
                m_split_base_cpuset(),
                qvi_map_close
            );
        case QV_SPLIT_PACKED:
            return qvi_map_config(
                m_group_size,
                m_split_size,
                qvi_map_packed
            );
        case QV_SPLIT_SPREAD:
            return qvi_map_config(
                m_group_size,
                m_split_size,
                qvi_map_spread
            );
        case QV_SPLIT_AUTO:
            return qvi_map_config(
                m_group_size,
                m_split_size,
                qvi_map_packed
            );
        default: // User-defined splitting.
            return qvi_map_config(
                m_colors,
                qvi_map_colors
            );
    }
}

/**
 * Splits the base hardware pool and returns a vector
 * of qvi_hwpool with |m_split_size| elements.
 */
std::vector<qvi_hwpool>
qvi_hwsplit::m_split_base_hwpool(void)
{
    // Split the base resource cpuset.
    const auto split_cpusets = m_split_base_cpuset();
    // These are the pools that are created from host resource split.
    std::vector<qvi_hwpool> result;
    for (const auto &cpuset : split_cpusets) {
        result.emplace_back(qvi_hwpool(cpuset));
    }
    // Now iterate over supported device types and add
    // devices based on affinity to the split cpusets.
    for (const auto devt : qvi_hwloc::supported_devices()) {
        const auto &devs = m_base_hwpool.devices(devt);
        if (devs.empty()) continue;
        // If we have devices, then get their affinities.
        const auto dev_affinities = m_base_hwpool.device_affinities(devt);
        // Map devices to cpusets, trying to maintain good affinity.
        const qvi_map_config devs2hres_config = {
            dev_affinities,
            split_cpusets
        };
        const auto devs2hres_map = qvi_map_close(devs2hres_config);

        if (qvi_unlikely(devs2hres_config.be_verbose)) {
            const auto label = "Final device (devt="
                             + std::to_string(devt)
                             + ") to hardware pool";
            qvi_map_emit(label, devs2hres_map);
        }
        // Now that we have the mapping, assign
        // devices to the associated hardware pools.
        for (const auto &[devi, poolis] : devs2hres_map) {
            for (const auto &pooli : poolis) {
                result[pooli].add_device(*devs[devi].get());
            }
        }
    }
    return result;
}

// This is the main split function called by the splitting process.
int
qvi_hwsplit::m_split(void)
{
    // First update instance state to reflect important split characteristics.
    // Update m_colors: verify and normalize input colors.
    m_colors = normalize_colors(m_colors, m_split_size);
    // Update m_split_size: reflect the requirements of the upcoming split.
    m_split_size = determine_actual_split_size(
        m_colors, m_group_size, m_split_size
    );
    // Now that those characteristics are updated, proceed with the split.
    // Determine the mapping configuration based on the user's request.
    const auto map_config = m_get_map_config();
    // Split the base hardware pool based on that request.
    const auto split_hwpools = m_split_base_hwpool();
    // Calculate the task to hardware pool resource mapping.
    const auto hwpool_map = map_config.map_fn(map_config);

    if (qvi_unlikely(map_config.be_verbose)) {
        qvi_map_emit("\nTask ID to Host Hardware Pool", hwpool_map);
    }
    // Perform final task to hardware pool assignments
    // and task coloring based on determined mapping.
    for (const auto &[taski, hwpoolis] : hwpool_map) {
        for (const auto &hwpooli : hwpoolis) {
            m_hwpools.at(taski) = split_hwpools.at(hwpooli);
            m_colors.at(taski) = static_cast<int>(hwpooli);
        }
    }
    // Members that opted out via QV_SPLIT_UNDEFINED are not present in the
    // mapping, so their hardware pool remains empty (a default-constructed
    // qvi_hwpool has an empty cpuset). Give each such member its own unique
    // group color beyond the real piece range so that it forms a valid,
    // singleton group holding no resources. The scope layer detects the opt-out
    // and returns a NULL scope to the caller rather than wrapping this pool.
    int excluded_color = static_cast<int>(m_split_size);
    for (size_t taski = 0; taski < m_colors.size(); ++taski) {
        if (m_colors.at(taski) == QV_SPLIT_UNDEFINED) {
            m_colors.at(taski) = excluded_color++;
        }
    }
    if (qvi_unlikely(map_config.be_verbose)) {
        qvi_log_info("\nColor assignments:\n{}", format_coloring(m_colors));
    }
    return QV_SUCCESS;
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
        hwsplit.m_base_hwpool = qvi_hwpool::set_union(hwsplit.m_hwpools);
        // The temporary hardware pools are no longer needed.
        hwsplit.m_hwpools.clear();
        // Create room for the real hardware pools.
        hwsplit.m_hwpools.resize(group.size());
        // And colors.
        hwsplit.m_colors.resize(group.size());
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
    // No point in doing this in a loop.
    const pid_t task_id = qvi_task::mytid();
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
        hwsplit.m_group_tids.at(i) = task_id;
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
