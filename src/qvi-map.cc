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
    // Start with the first set.
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

static std::string
format_assignments(
    const qvi_map_t &assignments
) {
    std::ostringstream oss;
    oss << "  Key: {src, {dsts}}\n";
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
    // Move the put pointer back by 2 character relative to the end
    // to remove the unneeded ",\n" for the last item in the map.
    oss.seekp(-2, std::ios_base::end);
    oss << "\n}";
    return oss.str();
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
    return result;
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
    // Distribute sources to destinations.
    size_t source_id = 0;
    for (size_t dest_id = 0; dest_id < m; ++dest_id) {
        // First extra destinations get one additional source.
        const size_t count = base_count + (dest_id < extra ? 1 : 0);
        for (size_t i = 0; i < count; ++i) {
            map[source_id++].insert(dest_id);
        }
    }
    return QV_SUCCESS;
}

int
qvi_map_spread(
    const qvi_map_config &config,
    qvi_map_t &map
) {
    for (size_t srci = 0, dsti = 0; srci < config.nsrc; ++srci) {
        // Mod to loop around destination IDs.
        map[srci].insert((dsti++) % config.ndst);
    }
    return QV_SUCCESS;
}

/**
 * Solves the affinity-preserving source-to-destination mapping problem using
 * Min-Cost Max-Flow with progressive cost model and destination ordering
 * tie-breaker.
 */
static qvi_map_t
solve_ap_mapping(
    size_t n,
    size_t m,
    const qvi_map_t &affinities
) {
    // Calculate max slots per destination.
    const size_t max_slots_per_dest = qvi_maxiperk(n, m);
    // Node layout:
    // 0: Super source
    // 1...n: Source nodes
    // n+1...n+(m*max_slots_per_dest): Destination slot nodes
    // last: Super sink
    const size_t total_slot_nodes = m * max_slots_per_dest;
    const int super_source = 0;
    const int super_sink = static_cast<int>(1 + n + total_slot_nodes);
    const int total_nodes = super_sink + 1;
    // Create the network.
    qvi_min_cost_max_flow mcmf(total_nodes);
    // Edge 1: Super source-->source nodes (capacity 1, cost 0).
    for (size_t i = 0; i < n; ++i) {
        mcmf.add_edge(super_source, 1 + i, 1, 0);
    }
    // Edge 2: Source nodes-->destination slot nodes
    //         (capacity 1, progressive cost).
    // Cost structure:
    // 1. Progressive penalty based on slot number
    //    (strongly prefer first slots).
    // 2. Small tie-breaker based on destination number
    //    (prefer lower-numbered destinations).
    for (size_t src = 0; src < n; ++src) {
        const auto it = affinities.find(src);
        std::set<size_t> src_affinities;
        if (it != affinities.end()) {
            src_affinities = it->second;
        }

        for (size_t dst : src_affinities) {
            // Connect source to all slots of this destination.
            for (size_t slot = 0; slot < max_slots_per_dest; ++slot) {
                const int slot_node = 1 + n + dst * max_slots_per_dest + slot;
                // Progressive cost model:
                // - Base cost: 100000 per slot:
                //   strongly penalize using later slots.
                // - Tie-breaker: +dst:
                //   prefer lower-numbered destinations when slots are equal.
                const int64_t progressive_penalty = 100000LL * slot;
                // Small preference for lower destination numbers.
                const int64_t dst_tiebreaker = dst;
                const int64_t cost = progressive_penalty + dst_tiebreaker;

                mcmf.add_edge(1 + src, slot_node, 1, cost);
            }
        }
    }
    // Edge 3: Destination slot nodes-->super sink (capacity 1, cost 0).
    for (size_t dst = 0; dst < m; ++dst) {
        for (size_t slot = 0; slot < max_slots_per_dest; ++slot) {
            const int slot_node = 1 + n + dst * max_slots_per_dest + slot;
            mcmf.add_edge(slot_node, super_sink, 1, 0);
        }
    }
    // Run min-cost max-flow.
    const auto [flow, cost] = mcmf.min_cost_flow(super_source, super_sink, n);
    // Verify that all sources were assigned.
    if (flow != static_cast<int64_t>(n)) {
        // Failed to assign all sources: no feasible solution exists.
        throw qvi_runtime_error(QV_ERR_NOT_SUPPORTED);
    }
    // Extract assignment from the flow network
    qvi_map_t result;
    const auto &graph = mcmf.get_graph();

    for (size_t src = 0; src < n; ++src) {
        int src_node = 1 + src;
        for (const auto &edge : graph[src_node]) {
            const int slot_node = edge.to;
            if (slot_node >= static_cast<int>(1 + n) &&
                slot_node < static_cast<int>(1 + n + total_slot_nodes) &&
                edge.flow > 0) {
                const size_t slot_idx = slot_node - (1 + n);
                const size_t dst = slot_idx / max_slots_per_dest;
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
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info("APM Mapping Started ===================================");
    }
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
    if (qvi_unlikely(config.be_verbose)) {
        qvi_map_emit("APM Affinities", affinities);
    }
    // Extract a view into just the values in the map.
    const auto avv = affinities | std::views::values;
    const auto affinity_intersection = k_set_intersection(
        std::vector<std::set<size_t>>(avv.begin(), avv.end())
    );
    // If the destination affinities overlap completely, then just map simply.
    if (affinity_intersection.size() == m) {
        if (qvi_unlikely(config.be_verbose)) {
            qvi_log_info("APM Detected simple mapping for n={}, m={}", n, m);
        }
        const int rc = qvi_map_packed(qvi_map_config(n, m), map);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    }
    else {
        // Solve the more complex mapping problem.
        map = solve_ap_mapping(n, m, affinities);
    }
    // If we did an inverted solve, invert the
    // result to match the original n and m.
    if (inverted) {
        map = invert_map(map);
    }
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(
            "APM done with N={}, M={} (inverted solve={})", n, m, inverted
        );
        qvi_map_emit("APM" , map);
        qvi_log_info("APM Mapping Done ======================================");
    }
    return QV_SUCCESS;
}

void
qvi_map_emit(
    const std::string &name,
    const qvi_map_t &map
) {
    qvi_log_info("{} assignments:\n{}", name, format_assignments(map));
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
