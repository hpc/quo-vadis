/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
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
    const qvi_map_shaffinity_t &smap,
    std::set<uint_t> &result
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
 * is, for consumers with shared affinity we remove sharing by assigning a
 * previously shared ID to a single resource round robin; unshared IDs remain in
 * place.
 */
static int
make_shared_affinity_map_disjoint(
    qvi_map_shaffinity_t &samap,
    const std::set<uint_t> &interids
) {
    // Number of intersecting consumer IDs.
    const uint_t ninter = interids.size();
    // Number of resources.
    const uint_t nres = samap.size();
    // Max intersecting consumer IDs per resource.
    const uint_t maxcpr = qvi_map_maxiperk(ninter, nres);

    qvi_map_shaffinity_t dmap;
    // First remove all IDs that intersect from the provided set map.
    for (const auto &mi: samap) {
        const uint_t rid = mi.first;
        std::set_difference(
            mi.second.cbegin(),
            mi.second.cend(),
            interids.cbegin(),
            interids.cend(),
            std::inserter(dmap[rid], dmap[rid].begin())
        );
    }
    // Copy IDs into a set we can modify.
    std::set<uint_t> coii(interids);
    // Assign disjoint IDs to relevant resources.
    for (const auto &mi: samap) {
        const uint_t rid = mi.first;
        uint_t nids = 0;
        for (const auto &cid : mi.second) {
            if (coii.find(cid) == coii.end()) continue;
            dmap[rid].insert(cid);
            coii.erase(cid);
            if (++nids == maxcpr || coii.empty()) break;
        }
    }
    // Update the provided set map.
    samap = dmap;
    return QV_SUCCESS;
}

uint_t
qvi_map_maxfit(
    uint_t max_chunk,
    uint_t space_left
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

uint_t
qvi_map_nfids_mapped(
    const qvi_map_t &map
) {
    return map.size();
}

bool
qvi_map_fid_mapped(
    const qvi_map_t &map,
    uint_t cid
) {
    return map.find(cid) != map.end();
}

int
qvi_map_colors(
    qvi_map_t &map,
    const std::vector<int> &fcolors,
    const qvi_hwloc_cpusets_t &tres
) {
    // Note: the array index i of fcolors is the color requested by task i.
    // Determine the number of distinct colors provided in the colors array.
    std::set<int> color_set(fcolors.begin(), fcolors.end());
    const uint_t nfrom = color_set.size();
    // For convenience, we convert the set to a vector for later use.
    std::vector<int> color_vec(color_set.begin(), color_set.end());
    // Maps a given color to its corresponding set index.
    // For example, given colors = {3, 5, 3, 4}, we get the following:
    // color_set = {3, 4, 5}, color_vec = {3, 4, 5}
    // color_set_index (csi) = {0, 1, 2}, since we have three distinct colors.
    // color2csi = {3:0, 4:1, 5:2}.
    std::map<int, uint_t> color2csi;
    for (uint_t i = 0; i < color_vec.size(); ++i) {
        color2csi.insert({color_vec[i], i});
    }
    // Create a mapping of color_set indices to cpuset indices.
    qvi_map_t csi2cpui;
    // We map packed here because we are assuming that like or near colors
    // should be mapped close together.
    int rc = qvi_map_packed(csi2cpui, nfrom, tres);
    if (rc != QV_SUCCESS) return rc;
    // Now map the task colors to their respective cpusets.
    for (uint_t fid = 0; fid < fcolors.size(); ++fid) {
        // Already mapped (potentially by some other mapper).
        if (qvi_map_fid_mapped(map, fid)) continue;
        const uint_t csi = color2csi.at(fcolors[fid]);
        const uint_t tid = csi2cpui.at(csi);
        map.insert({fid, tid});
    }
    return rc;
}

int
qvi_map_packed(
    qvi_map_t &map,
    uint_t nfids,
    const qvi_hwloc_cpusets_t &tres
) {
    const uint_t ntres = tres.size();
    // Max consumers per resource.
    const uint_t maxcpr = qvi_map_maxiperk(nfids, ntres);
    // Keeps track of the next consumer ID to map.
    uint_t fid = 0;
    // Number of consumers mapped to a resource.
    uint_t nmapped = qvi_map_nfids_mapped(map);
    for (uint_t tid = 0; tid < ntres; ++tid) {
        // Number of consumer IDs to map.
        const uint_t nmap = qvi_map_maxfit(maxcpr, nfids - nmapped);
        for (uint_t i = 0; i < nmap; ++i, ++fid) {
            // Already mapped (potentially by some other mapper).
            if (qvi_map_fid_mapped(map, fid)) continue;
            // Else map the consumer to the resource ID.
            map.insert({fid, tid});
            nmapped++;
        }
    }
    return QV_SUCCESS;
}

/**
 * Maps round-robin over the given resources.
 */
int
qvi_map_spread(
    qvi_map_t &map,
    uint_t nfids,
    const qvi_hwloc_cpusets_t &tres
) {
    const uint_t ntres = tres.size();
    for (uint_t fid = 0, tid = 0; fid < nfids; ++fid) {
        // Already mapped (potentially by some other mapper).
        if (qvi_map_fid_mapped(map, fid)) continue;
        // Mod to loop around 'to resource' IDs.
        map.insert({fid, (tid++) % ntres});
    }
    return QV_SUCCESS;
}

int
qvi_map_disjoint_affinity(
    qvi_map_t &map,
    const qvi_map_shaffinity_t &damap
) {
    for (auto &tidfids : damap) {
        const int tid = tidfids.first;
        for (const auto fid : damap.at(tid)) {
            // Already mapped (potentially by some other mapper).
            if (qvi_map_fid_mapped(map, fid)) continue;
            // Map the consumer ID to its resource ID.
            map.insert({fid, tid});
        }
    }
    return QV_SUCCESS;
}

int
qvi_map_calc_shaffinity(
    const qvi_hwloc_cpusets_t &faffs,
    const qvi_hwloc_cpusets_t &tores,
    qvi_map_shaffinity_t &res_affinity_map
) {
    // Number of consumers.
    const uint_t ncon = faffs.size();
    // Number of resource we are mapping to.
    const uint_t nres = tores.size();

    for (uint_t cid = 0; cid < ncon; ++cid) {
        for (uint_t rid = 0; rid < nres; ++rid) {
            const int intersects = hwloc_bitmap_intersects(
                faffs.at(cid).cdata(), tores.at(rid).cdata()
            );
            if (intersects) {
                res_affinity_map[rid].insert(cid);
            }
        }
    }
    return QV_SUCCESS;
}

int
qvi_map_affinity_preserving(
    qvi_map_t &map,
    const qvi_map_fn_t map_rest_fn,
    const qvi_hwloc_cpusets_t &faffs,
    const qvi_hwloc_cpusets_t &tores
) {
    int rc = QV_SUCCESS;
    // Number of consumers.
    const uint_t ncon = faffs.size();
    // Maps resource IDs to consumer IDs with shared affinity.
    qvi_map_shaffinity_t res_affinity_map;
    // Stores the consumer IDs that all share affinity with a split resource.
    std::set<uint_t> affinity_intersection;
    // Determine the consumer IDs that have shared affinity with the resources.
    rc = qvi_map_calc_shaffinity(faffs, tores, res_affinity_map);
    if (rc != QV_SUCCESS) goto out;
    // Calculate k-set intersection.
    rc = k_set_intersection(res_affinity_map, affinity_intersection);
    if (rc != QV_SUCCESS) goto out;
    // Now make a mapping decision based on the intersection size.
    // Completely disjoint sets.
    if (affinity_intersection.size() == 0) {
        rc = qvi_map_disjoint_affinity(map, res_affinity_map);
        if (rc != QV_SUCCESS) goto out;
    }
    // Only a subset of consumers share a resource. First favor mapping
    // consumers with affinity to a particular resource, then map the rest.
    // Note that the subset is not strict, so this branch also covers the
    // 'all consumers share a resource' case, too.
    else {
        rc = make_shared_affinity_map_disjoint(
            res_affinity_map, affinity_intersection
        );
        if (rc != QV_SUCCESS) goto out;

        rc = qvi_map_disjoint_affinity(map, res_affinity_map);
        if (rc != QV_SUCCESS) goto out;

        rc = map_rest_fn(map, ncon, tores);
        if (rc != QV_SUCCESS) goto out;
    }
out:
    if (rc != QV_SUCCESS) {
        // Invalidate the map.
        map.clear();
    }
    return rc;
}

hwloc_const_cpuset_t
qvi_map_cpuset_at(
    const qvi_map_t &map,
    const qvi_hwloc_cpusets_t &cpusets,
    uint_t fid
) {
    return cpusets.at(map.at(fid)).cdata();
}

std::vector<uint_t>
qvi_map_flatten(
    const qvi_map_t &map
) {
    std::vector<uint_t> result(map.size());
    for (const auto &mi : map) {
        result[mi.first] = mi.second;
    }
    return result;
}

std::vector<int>
qvi_map_flatten_to_colors(
    const qvi_map_t &map
) {
    std::vector<int> result(map.size());
    for (const auto &mi : map) {
        result[mi.first] = (int)mi.second;
    }
    return result;
}

void
qvi_map_debug_dump(
    const qvi_map_t &map
) {
#if QVI_DEBUG_MODE == 0
    qvi_unused(map);
#else
    qvi_log_debug(" # nfids_mapped={}", qvi_map_nfids_mapped(map));
    for (const auto &mi : map) {
        qvi_log_debug(" # fid={} mapped to tid={}", mi.first, mi.second);
    }
#endif
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
