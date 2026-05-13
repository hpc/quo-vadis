/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2026 Triad National Security, LLC
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
 * Makes the provided shared affinity map disjoint with regard to affinity. That
 * is, for consumers with shared affinity we remove sharing by assigning a
 * previously shared ID to a single resource round robin; unshared IDs remain in
 * place.
 */
static qvi_map_t
get_disjoint_affinity(
    const qvi_map_t &samap,
    const std::set<size_t> &interids
) {
    // Number of source IDs.
    const size_t nres = samap.size();
    // Number of intersecting consumer IDs.
    const size_t ninter = interids.size();
    // Max intersecting consumer IDs per resource.
    const size_t maxcpr = qvi_map_maxiperk(ninter, nres);

    qvi_map_t dmap;
    // First remove all IDs that intersect from the provided set map.
    for (const auto &mi: samap) {
        const size_t rid = mi.first;
        std::set_difference(
            mi.second.cbegin(),
            mi.second.cend(),
            interids.cbegin(),
            interids.cend(),
            std::inserter(dmap[rid], dmap[rid].begin())
        );
    }
    // Copy IDs into a set we can modify.
    std::set<size_t> coii(interids);
    // Assign disjoint IDs to relevant resources.
    for (const auto &mi: samap) {
        const size_t rid = mi.first;
        size_t nids = 0;
        for (const auto &cid : mi.second) {
            if (coii.find(cid) == coii.end()) continue;
            dmap[rid].insert(cid);
            coii.erase(cid);
            if (++nids == maxcpr || coii.empty()) break;
        }
    }
    return dmap;
}

size_t
qvi_map_maxfit(
    size_t max_chunk,
    size_t space_left
) {
    return std::min(max_chunk, space_left);
}

size_t
qvi_map_maxiperk(
    size_t i,
    size_t k
) {
    // Guard against division by zero.
    if (k == 0) return 0;
    return (i + k - 1) / k;
}

size_t
qvi_map_nsids_mapped(
    const qvi_map_t &map
) {
    return map.size();
}

bool
qvi_map_srcid_mapped(
    const qvi_map_t &map,
    size_t srcid
) {
    return map.find(srcid) != map.end();
}

int
qvi_map_colors(
    const qvi_map_config &config,
    qvi_map_t &map
) {
    // Note: the array index i of fcolors is the color requested by task i.
    // Determine the number of distinct colors provided in the colors array.
    std::set<int> color_set(config.src_colors.begin(), config.src_colors.end());
    // For convenience, we convert the set to a vector for later use.
    std::vector<int> color_vec(color_set.begin(), color_set.end());
    // Maps a given color to its corresponding set index.
    // For example, given colors = {3, 5, 3, 4}, we get the following:
    // color_set = {3, 4, 5}, color_vec = {3, 4, 5}
    // color_set_index (csi) = {0, 1, 2}, since we have three distinct colors.
    // color2csi = {3:0, 4:1, 5:2}.
    std::map<int, size_t> color2csi;
    for (size_t i = 0; i < color_vec.size(); ++i) {
        color2csi.insert({color_vec[i], i});
    }
    // Create a mapping of color_set indices to cpuset indices.
    qvi_map_t csi2cpui;
    // We map packed here because we are assuming that like or near colors
    // should be mapped close together.
    qvi_map_config packed_config;
    packed_config.nsrc = color_set.size();
    packed_config.ndst = config.ndst;
    int rc = qvi_map_packed(packed_config, csi2cpui);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Now map the task colors to their respective cpusets.
    for (const auto src : config.src_colors) {
        // Already mapped (potentially by some other mapper).
        if (qvi_map_srcid_mapped(map, src)) continue;
        const size_t csis = color2csi.at(config.src_colors[src]);
        for (const auto dst : csi2cpui.at(csis)) {
            map[src].insert(dst);
        }
    }
    return QV_SUCCESS;
}

int
qvi_map_packed(
    const qvi_map_config &config,
    qvi_map_t &map
) {
    // Max consumers per resource.
    const size_t maxcpr = qvi_map_maxiperk(config.nsrc, config.ndst);
    // Keeps track of the next source ID to map.
    size_t srci = 0;
    // Number of sources mapped to a destination.
    size_t nmapped = qvi_map_nsids_mapped(map);
    for (size_t dsti = 0; dsti < config.ndst; ++dsti) {
        // Number of consumer IDs to map.
        const size_t nmap = qvi_map_maxfit(maxcpr, config.nsrc - nmapped);
        for (size_t i = 0; i < nmap; ++i, ++srci) {
            // Already mapped (potentially by some other mapper).
            if (qvi_map_srcid_mapped(map, srci)) continue;
            // Else map the consumer to the resource ID.
            map[srci].insert(dsti);
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
    const qvi_map_config &config,
    qvi_map_t &map
) {
    for (size_t srci = 0, dsti = 0; srci < config.nsrc; ++srci) {
        // Already mapped (potentially by some other mapper).
        if (qvi_map_srcid_mapped(map, srci)) continue;
        // Mod to loop around destination IDs.
        map[srci].insert((dsti++) % config.ndst);
    }
    return QV_SUCCESS;
}

qvi_map_t
qvi_map_calc_affinities(
    const std::vector<qvi_hwloc_bitmap> &src,
    const std::vector<qvi_hwloc_bitmap> &dst
) {
    qvi_map_t result;
    // Number of sources.
    const size_t nsrc = src.size();
    // Number of destinations we are mapping to.
    const size_t ndst = dst.size();

    for (size_t srci = 0; srci < nsrc; ++srci) {
        for (size_t dsti = 0; dsti < ndst; ++dsti) {
            const int intersects = hwloc_bitmap_intersects(
                src.at(srci).cdata(), dst.at(dsti).cdata()
            );
            if (intersects) {
                result[srci].insert(dsti);
            }
        }
    }
#if 0
    for (const auto &[srci, dstis] : result) {
        qvi_log_debug("# src {} has shared affinity with:", srci);
        for (const auto &dsti : dstis) {
            qvi_log_debug("# -- {}", dsti);
        }
    }
#endif
    return result;
}

/**
 * A straightforward Disjoin Set Union implementation.
 */
struct qvi_map_dsu {
private:
    std::vector<size_t> m_parent;
public:
    qvi_map_dsu(
        size_t n
    ) : m_parent(n) {
        std::iota(m_parent.begin(), m_parent.end(), 0);
    }

    size_t
    find(
        size_t i
    ) {
        if (m_parent[i] == i) return i;
        return m_parent[i] = find(m_parent[i]);
    }

    void
    unite(
        size_t i,
        size_t j
    ) {
        const size_t root_i = find(i);
        const size_t root_j = find(j);
        if (root_i != root_j) m_parent[root_i] = root_j;
    }
};

static std::vector<std::set<size_t>>
group_intersecting_indices(
    const std::vector<std::set<size_t>> &vec_of_sets
) {
    const size_t n = vec_of_sets.size();
    qvi_map_dsu dsu(n);

    std::unordered_map<size_t, size_t> element_to_first_index;
    for (size_t i = 0; i < n; ++i) {
        for (size_t val : vec_of_sets[i]) {
            if (element_to_first_index.contains(val)) {
                dsu.unite(i, element_to_first_index[val]);
            }
            else {
                element_to_first_index[val] = i;
            }
        }
    }

    qvi_map_t groups;
    for (size_t i = 0; i < n; ++i) {
        groups[dsu.find(i)].insert(i);
    }

    std::vector<std::set<size_t>> result;
    for (auto& [root, indices] : groups) {
        result.push_back(std::move(indices));
    }
    return result;
}

int
qvi_map_affinity_preserving(
    const qvi_map_config &config,
    qvi_map_t &map
) {
    auto affinity_map = qvi_map_calc_affinities(
        config.src_affinities, config.dst_affinities
    );
    do {
        bool something_mapped = false;
        const auto destids = affinity_map | std::views::values;
        const std::vector<std::set<size_t>> k_destids(
            destids.begin(), destids.end()
        );
        const auto intersecting_indis = group_intersecting_indices(k_destids);

        for (auto &ii : intersecting_indis) {
            const auto dmap = get_disjoint_affinity(affinity_map, ii);
            std::set<size_t> mapped_srcs;
            for (auto &[src, dsts] : dmap) {
                for (auto dst : dsts) {
                    if (mapped_srcs.contains(src)) continue;
                    map[src].insert(dst);
                    mapped_srcs.insert(src);
                    affinity_map.erase(src);
                    something_mapped = true;
                }
            }
        }
        if (!something_mapped) break;
    } while (true);

    return QV_SUCCESS;
}

std::vector<int>
qvi_map_flatten_to_colors(
    const qvi_map_t &map
) {
    std::vector<int> result(map.size());
    for (const auto &[srci, dstis] : map) {
        for (const auto &[dsti, dstis] : map) {
            result.at(srci) = (int)dsti;
        }
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
    qvi_log_debug(" # nsids_mapped={}", qvi_map_nsids_mapped(map));
    for (const auto &[srci, dstis] : map) {
        qvi_log_debug(" # srci {} mapped to:", srci);
        for (const auto &dsti : dstis) {
            qvi_log_debug(" # -- {}", dsti);
        }
    }
#endif
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
