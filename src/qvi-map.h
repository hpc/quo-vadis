/*
 * Copyright (c) 2022      Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-map.h
 */

#ifndef QVI_MAP_H
#define QVI_MAP_H

#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-devinfo.h"

#ifdef __cplusplus

/** Maintains a mapping between IDs to a set of other identifiers. */
// TODO(skg) Rename
using qvi_map_id_to_uids_t = std::map<int, std::set<int>>;

struct qvi_map_t {
private:
    /**
     * The initial mapping between IDs and their respective colors.
     */
    std::vector<int> colors = {};
    /**
     * The cpusets we are mapping to. This structure also encodes a mapping
     * between colors (indices) and cpusets. This implies that the number of
     * cpusets is the number of colors one has available to map on to.
     */
    std::vector<hwloc_bitmap_t> cpusets = {};
    /**
     * The mapping between IDs and their respective colors.
     */
    std::vector<int> colorps = {};
    /**
     * The mapping between IDs and cpuset indices in cpusets above.
     */
    std::map<int, int> idmap = {};

public:
    /**
     * Constructor.
     */
    qvi_map_t(void) = default;

    /**
     * Destructor.
     */
    ~qvi_map_t(void)
    {
        clear();
    }

    /**
     * Clears the map.
     */
    void
    clear(void)
    {
        colors.clear();
        for (auto &cpuset : cpusets) {
            qvi_hwloc_bitmap_free(&cpuset);
        }
        colorps.clear();
        idmap.clear();
    }

    /**
     * Initializes the map.
     */
    int
    initialize(
        const std::vector<int> &icolors,
        const std::vector<hwloc_bitmap_t> &icpusets
    ) {
        // First make sure that we clear out any old data.
        clear();

        const uint_t ncolors = icpusets.size();
        colors = icolors;
        cpusets.resize(ncolors);
        colorps.resize(colors.size());

        int rc = QV_SUCCESS;
        for (uint_t i = 0; i < ncolors; ++i) {
            rc = qvi_hwloc_bitmap_calloc(&cpusets[i]);
            if (rc != QV_SUCCESS) break;
            // Copy the provided cpusets.
            rc = qvi_hwloc_bitmap_copy(icpusets[i], cpusets[i]);
            if (rc != QV_SUCCESS) break;
        }

        return rc;
    }

    /**
     * Returns the total number of IDs we are attempting to map.
     */
    uint_t
    nids(void)
    {
        return colors.size();
    }

    /**
     * Returns the number of colors we are mapping on to.
     */
    uint_t
    ncolors(void)
    {
        return cpusets.size();
    }

    /**
     * Returns a reference to the cpuset at a given ID.
     */
    hwloc_const_cpuset_t
    cpusetat(
        int index
    ) {
        return cpusets.at(index);
    }

    /**
     * Returns the number of IDs that have already been mapped.
     */
    uint_t
    nmapped(void)
    {
        return idmap.size();
    }

    /**
     * Returns whether or not the provided ID is already mapped.
     */
    bool
    id_mapped(int id)
    {
        return idmap.find(id) != idmap.end();
    }

    /**
     * Returns whether or not all the IDs have been mapped.
     */
    bool
    complete(void)
    {
        return idmap.size() == colors.size();
    }

    /**
     * Maps the given ID to the provided color.
     */
    int
    map_id_to_color(
        int id,
        int color
    ) {
        colorps[id] = color;
        idmap.insert({id, color});

        return QV_SUCCESS;
    }

    /**
     * Returns the given ID's mapped cpuset.
     */
    hwloc_const_cpuset_t
    mapped_cpuset(
        int id
    ) {
        const uint_t color = idmap.at(id);
        return cpusets.at(color);
    }

    /**
     * Returns the given ID's mapped color.
     */
    int
    mapped_color(
        int id
    ) {
        return colorps.at(id);
    }
};

/**
 * Maps IDs to colors by associating contiguous task IDs with each color.
 */
int
qvi_map_packed(
    qvi_map_t &map
);

int
qvi_map_disjoint_affinity(
    qvi_map_t &map,
    const qvi_map_id_to_uids_t &disjoint_affinity_map
);

/**
 * Maps IDs to colors by associating contiguous task IDs with each color.
 */
int
qvi_map_affinity_preserving(
    qvi_map_t &map,
    const std::vector<hwloc_cpuset_t> &affinities
);

#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the largest number that will fit in the space available.
 */
uint_t
qvi_map_maxfit(
    uint_t space_left,
    uint_t max_chunk
);

/**
 * Returns the max i per k.
 */
uint_t
qvi_map_maxiperk(
    uint_t i,
    uint_t k
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
