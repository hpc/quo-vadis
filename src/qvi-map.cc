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
#include "qvi-utils.h"

/**
 * Takes a map and assigns its values as keys and keys as values.
 */
static qvi_map_t
invert_map(
    const qvi_map_t &original
) {
    qvi_map_t inverted;
    for (const auto &[key, values] : original) {
        for (size_t value : values) {
            inverted[value].insert(key);
        }
    }
    return inverted;
}

template<typename T>
std::set<T>
k_set_intersection(
    const std::vector<std::set<T>> &sets
) {
    if (sets.empty()) return {};
    if (sets.size() == 1) return sets[0];
    // Start with the first set
    std::set<T> result = sets[0];
    // Intersect with each subsequent set.
    for (size_t i = 1; i < sets.size(); ++i) {
        std::set<T> tmp;
        std::ranges::set_intersection(
            result, sets[i], std::inserter(tmp, tmp.begin())
        );
        result = std::move(tmp);
        // Early exit if result is empty.
        if (result.empty()) break;
    }
    return result;
}

#if QVI_DEBUG_MODE != 0
static std::string
format_assignments(
    const qvi_map_t &assignments
) {
    std::ostringstream oss;
    oss << "  Key: {src, {dst}}\n";
    oss << "{\n";
    for (const auto &[src, dests] : assignments) {
        oss << "  {" << src << ", {";
        bool first = true;
        for (size_t dest : dests) {
            if (!first) oss << ", ";
            oss << dest;
            first = false;
        }
        oss << "}},\n";
    }
    oss << "}";
    return oss.str();
}
#endif

size_t
qvi_map_nsrcids_mapped(
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
    qvi_map_config packed_config = {
        color_set.size(),
        config.ndst
    };
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
    const size_t n = config.nsrc;
    const size_t m = config.ndst;
    assert(n >= m && "n must be >= m");
    // Nothing to do.
    if (n == 0 || m == 0) {
        map.clear();
        return QV_SUCCESS;
    }
    // Calculate base number of sources per destination and remainder.
    const size_t base_count = n / m;
    const size_t extra = n % m;

    size_t source_id = 0;
    // Distribute sources to destinations.
    for (size_t dest_id = 0; dest_id < m; ++dest_id) {
        // First 'extra' destinations get one additional source
        const size_t count = base_count + (dest_id < extra ? 1 : 0);

        for (size_t i = 0; i < count; ++i) {
            map[source_id++].insert(dest_id);
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
    // number of sources.
    const size_t nsrc = src.size();
    // number of destinations we are mapping to.
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
    return result;
}

/**
 * Solves the AP source-to-destination mapping problem using Min-Cost Max-Flow.
 *
 * Approach:
 * 1. Model as a flow network with source S, sink T, source nodes, and destination nodes
 * 2. Each source must be assigned to exactly one destination (flow = 1)
 * 3. Destinations have flexible capacity (ceil(n/m)) to handle affinity constraints
 * 4. Edge costs encourage balanced distribution while respecting affinities
 * 5. Cost function: base_cost + (assignment_number)^2 for progressive balancing
 */
static qvi_map_t
solve_ap_mapping(
    size_t n,
    size_t m,
    const qvi_map_t &affinities
) {
    if (qvi_unlikely(n == 0 || m == 0)) {
        throw qvi_runtime_error(QV_ERR_INTERNAL);
    }
    // Node layout:
    // 0: super source S
    // 1...n: source nodes (s0, s1, ..., s(n-1))
    // n+1...n+m: destination nodes (d0, d1, ..., d(m-1))
    // n+m+1: super sink T
    const int super_source = 0;
    const int super_sink = static_cast<int>(n + m + 1);
    const int total_nodes = static_cast<int>(n + m + 2);
    // Create the mcmf.
    qvi_min_cost_max_flow mcmf(total_nodes);
    // Edge 1: Super source --> source nodes (capacity 1, cost 0).
    // Each source must be assigned exactly once
    for (size_t i = 0; i < n; ++i) {
        mcmf.add_edge(super_source, 1 + i, 1, 0);
    }
    // Edge 2: Source nodes --> dest nodes (capacity 1, progressive cost).
    // Cost structure encourages balanced distribution:
    // - For each destination, costs increase with the number of assignments.
    // - This creates a natural load balancing effect.
    for (size_t src = 0; src < n; ++src) {
        auto it = affinities.find(src);
        std::set<size_t> src_affinities;
        if (it != affinities.end()) {
            src_affinities = it->second;
        }

        for (size_t dst = 0; dst < m; ++dst) {
            if (src_affinities.count(dst) > 0) {
                // Progressive cost: encourages filling destinations up to the
                // ideal level, then increases cost for overfilling.
                // Use a quadratic cost based on how far from ideal.
                const int64_t base_cost = 1000;
                // Add cost to encourage balance:
                // destinations closer to ideal get lower cost.
                int64_t cost = base_cost + static_cast<int64_t>(dst * 100);
                mcmf.add_edge(1 + src, 1 + n + dst, 1, cost);
            }
        }
    }
    // Edge 3: Destination nodes --> super sink (flexible capacity, cost 0).
    // Use maxiperk(n, m) capacity per destination to ensure flexibility. This
    // allows the flow algorithm to find feasible solutions even with uneven
    // affinity distributions.
    const size_t max_capacity_per_dest = qvi_maxiperk(n, m);

    for (size_t i = 0; i < m; ++i) {
        // Each destination can take up to maxiperk(n, m) sources.
        // This ensures enough total capacity: m * maxiperk(n, m) >= n.
        mcmf.add_edge(1 + n + i, super_sink, max_capacity_per_dest, 0);
    }
    // Run min-cost max-flow.
    const auto [flow, cost] = mcmf.min_cost_flow(super_source, super_sink, n);
    // Verify that all sources were assigned.
    if (qvi_unlikely(flow != static_cast<int64_t>(n))) {
        // Failed to assign all sources: no feasible solution exists.
        // TODO(skg) We should probably just use some other mapper here.
        throw qvi_runtime_error(QV_ERR_INTERNAL);
    }
    // Extract assignment from the flow network.
    qvi_map_t result;
    const auto &graph = mcmf.get_graph();

    for (size_t src = 0; src < n; ++src) {
        int src_node = 1 + src;
        for (const auto &edge : graph[src_node]) {
            // Check if this edge goes to a destination node and has flow.
            const int dst_node = edge.to;
            if (dst_node >= static_cast<int>(1 + n) &&
                dst_node < static_cast<int>(1 + n + m) &&
                edge.flow > 0
            ) {
                const size_t dst = dst_node - (1 + n);
                result[src].insert(dst);
            }
        }
    }
    return result;
}

int
qvi_map_affinity_preserving(
    const qvi_map_config &config,
    qvi_map_t &map
) {
    using aff_type_t = decltype(config.src_affinities);
    // Did we invert the sources and destinations?
    bool inverted = false;
    // The real sources and destinations.
    auto rsrc = const_cast<aff_type_t *>(&config.src_affinities);
    auto rdst = const_cast<aff_type_t *>(&config.dst_affinities);
    // Our algorithm requires that nsrc >= ndst. If that
    // isn't the case, invert them and note that we did so.
    if (rsrc->size() < rdst->size()) {
        std::swap(rsrc, rdst);
        inverted = true;
    }
    const size_t n = rsrc->size();
    const size_t m = rdst->size();
    assert(n >= m);
    // Determine the affinities shared between sources and destinations.
    const auto affinities = qvi_map_calc_affinities(*rsrc, *rdst);
    // Extract a view into just the values in the map.
    const auto avv = affinities | std::views::values;
    const auto affinity_intersection = k_set_intersection(
        std::vector<std::set<size_t>>(avv.begin(), avv.end())
    );
    // If the destination affinities overlap completely, then just map simply.
    if (affinity_intersection.size() == m) {
        qvi_log_debug("AP Detected simple mapping for n={}, m={}", n, m);
        const qvi_map_config config = {
            n,
            m
        };
        const int rc = qvi_map_packed(config, map);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        if (inverted) {
            map = invert_map(map);
        }
        return QV_SUCCESS;
    }
    // Solve the more complex mapping problem.
    if (inverted) {
        map = invert_map(solve_ap_mapping(n, m, affinities));
    }
    else {
        map = solve_ap_mapping(n, m, affinities);
    }
#if 0 // TODO(skg) Add an environment variable to expose this to users.
    qvi_log_debug("N={}, M={}", n, m);
    qvi_map_debug_dump("Affinity Preserved" , map);
#endif
    return QV_SUCCESS;
}

std::vector<int>
qvi_map_flatten_to_colors(
    const qvi_map_t &map
) {
    std::vector<int> result(map.size());
    for (const auto &[srci, dstis] : map) {
        // In this application, there shall only be one destination per source.
        if (qvi_unlikely(dstis.size() != 1)) {
            throw qvi_runtime_error(QV_ERR_INTERNAL);
        }
        // Just use the only value that should be there.
        result.at(srci) = static_cast<int>(*dstis.begin());
    }
    return result;
}

qvi_map_t
qvi_map_uniq(
    const qvi_map_t &map
) {
    qvi_map_t result;
    std::set<size_t> seen;
    for (const auto &[src, dsts] : map) {
        for (const auto &dst : dsts) {
            // Destination is mapped, skip.
            if (seen.contains(dst)) continue;
            // Destination is not mapped, so map it.
            result.insert({src, {dst}});
            // Make note that we have see that destination.
            seen.insert(dst);
            // Done with this src.
            break;
        }
    }
    return result;
}

void
qvi_map_debug_dump(
    const std::string &name,
    const qvi_map_t &map
) {
#if QVI_DEBUG_MODE == 0
    qvi_unused(name);
    qvi_unused(map);
#else
    qvi_log_debug("{} assignments:\n{}", name, format_assignments(map));
#endif
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
