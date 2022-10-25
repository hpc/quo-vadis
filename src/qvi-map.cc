/*
 * Copyright (c) 2022      Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-map.cc
 */

#include "qvi-map.h"

/**
 * Performs a k-set intersection of the sets included in the provided set map.
 */
static int
k_set_intersection(
    const qvi_map_id_to_uids_t &smap,
    std::set<int> &result
) {
    result.clear();
    // Nothing to do.
    if (smap.size() <= 1) {
        return QV_SUCCESS;
    }
    // Remember that this is an ordered map.
    auto setai = smap.cbegin();
    auto setbi = std::next(setai);
    while (setbi != smap.cend()) {
        std::set_intersection(
            setai->second.cbegin(),
            setai->second.cend(),
            setbi->second.cbegin(),
            setbi->second.cend(),
            std::inserter(result, result.begin())
        );
        setbi = std::next(setbi);
    }
    return QV_SUCCESS;
}

/**
 * Makes the provided shared affinity map disjoint with regard to affinity. That
 * is, for colors with shared affinity we remove sharing by assigning a
 * previously shared ID to a single color round robin; unshared IDs remain in
 * place.
 */
static int
make_shared_affinity_map_disjoint(
    qvi_map_id_to_uids_t &color_affinity_map,
    const std::set<int> &interids
) {
    const uint_t ninter = interids.size();
    const uint_t ncolor = color_affinity_map.size();
    // Max intersecting IDs per color.
    const uint_t maxipc = qvi_map_maxiperk(ninter, ncolor);

    qvi_map_id_to_uids_t dmap;
    // First remove all IDs that intersect from the provided set map.
    for (const auto &mi: color_affinity_map) {
        const int color = mi.first;
        std::set_difference(
            mi.second.cbegin(),
            mi.second.cend(),
            interids.cbegin(),
            interids.cend(),
            std::inserter(dmap[color], dmap[color].begin())
        );
    }
    // Copy IDs into a set we can modify.
    std::set<int> coii(interids);
    // Assign disjoint IDs to relevant colors.
    for (const auto &mi: color_affinity_map) {
        const int color = mi.first;
        uint_t nids = 0;
        for (const auto &id : mi.second) {
            if (coii.find(id) == coii.end()) continue;
            dmap[color].insert(id);
            coii.erase(id);
            if (++nids == maxipc || coii.empty()) break;
        }
    }
    // Update the provided set map.
    color_affinity_map = dmap;
    return QV_SUCCESS;
}

uint_t
qvi_map_maxfit(
    uint_t space_left,
    uint_t max_chunk
) {
    uint_t result = max_chunk;
    while (result > space_left) {
        result--;
    }
    return result;
}

uint_t
qvi_map_maxiperk(
    uint_t i,
    uint_t k
) {
    return std::ceil(i / float(k));
}

int
qvi_map_packed(
    qvi_map_t &map
) {
    int rc = QV_SUCCESS;
    const uint_t group_size = map.nids();
    const uint_t split_size = map.ncolors();
    // Max tasks per color.
    const uint_t maxtpc = qvi_map_maxiperk(group_size, split_size);
    // Keeps track of the next ID to map.
    uint_t id = 0;
    // Number of entities that have already been mapped to a resource.
    uint_t nmapped = map.nmapped();
    for (uint_t color = 0; color < split_size; ++color) {
        // Number of entities to map.
        const uint_t nmap = qvi_map_maxfit(group_size - nmapped, maxtpc);
        for (uint_t i = 0; i < nmap; ++i, ++id, ++nmapped) {
            // Already mapped (potentially by some other mapper).
            if (map.id_mapped(id)) continue;

            rc = map.map_id_to_color(id, color);
            if (rc != QV_SUCCESS) return rc;
        }
    }
    return rc;
}

int
qvi_map_disjoint_affinity(
    qvi_map_t &map,
    const qvi_map_id_to_uids_t &disjoint_affinity_map
) {
    int rc = QV_SUCCESS;

    for (uint_t color = 0; color < map.ncolors(); ++color) {
        // We are done.
        if (map.complete()) break;

        for (const auto &id : disjoint_affinity_map.at(color)) {
            // Already mapped (potentially by some other mapper).
            if (map.id_mapped(id)) continue;
            // Map the ID to its color.
            rc = map.map_id_to_color(id, color);
            if (rc != QV_SUCCESS) return rc;
        }
    }
    return rc;
}

int
qvi_map_affinity_preserving(
    qvi_map_t &map,
    const std::vector<hwloc_cpuset_t> &affinities
) {
    int rc = QV_SUCCESS;
    // Maps cpuset IDs (colors) to IDs with shared affinity.
    qvi_map_id_to_uids_t color_affinity_map;
    // Stores the IDs that all share affinity with a split resource.
    std::set<int> affinity_intersection;

    // Determine the IDs that have shared affinity within each cpuset.
    for (uint_t color = 0; color < map.ncolors(); ++color) {
        for (uint_t id = 0; id < map.nids(); ++id) {
            const int intersects = hwloc_bitmap_intersects(
                affinities.at(id), map.cpusetat(color)
            );
            if (intersects) {
                color_affinity_map[color].insert(id);
            }
        }
    }
    // Calculate k-set intersection.
    rc = k_set_intersection(color_affinity_map, affinity_intersection);
    if (rc != QV_SUCCESS) goto out;
    // Now make a mapping decision based on the intersection size.
    // Completely disjoint sets.
    if (affinity_intersection.size() == 0) {
        rc = qvi_map_disjoint_affinity(map, color_affinity_map);
        if (rc != QV_SUCCESS) goto out;
    }
    // All entities overlap. Really no hope of doing anything fancy.
    // Note that we typically see this in the *no task is bound case*.
    else if (affinity_intersection.size() == map.nids()) {
        rc = qvi_map_packed(map);
        if (rc != QV_SUCCESS) goto out;
    }
    // Only a strict subset of tasks share a resource. First favor mapping
    // tasks with affinity to a particular resource, then map the rest.
    else {
        rc = make_shared_affinity_map_disjoint(
            color_affinity_map, affinity_intersection
        );
        if (rc != QV_SUCCESS) goto out;

        rc = qvi_map_disjoint_affinity(map, color_affinity_map);
        if (rc != QV_SUCCESS) goto out;

        rc = qvi_map_packed(map);
        if (rc != QV_SUCCESS) goto out;
    }
out:
    if (rc != QV_SUCCESS) {
        // Invalidate the map.
        map.clear();
    }
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
