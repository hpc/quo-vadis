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
    // Move the put pointer back by 2 characters relative to the end
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
            // Make note that we have seen that destination.
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
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info("Color Mapping Started =================================");
    }
    auto &src_colors = config.src_colors;
    const size_t n = src_colors.size();
    const size_t m = config.ndst;
    // These should be clamped: 0 to n-1, so verify that.
    assert(
        std::ranges::all_of(src_colors, [n](int val) {
            return val >= 0 && val < static_cast<int>(n);
        })
    );
    // Map the color index to its color value,
    // which corresponds to the destination index.
    for (size_t i = 0; i < n; ++i) {
        map[i].insert(src_colors[i]);
    }
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(
            "Color Mapping done with N={}, M={}", n, m
        );
        qvi_map_emit("Color Mapping", map);
        qvi_log_info("Color Mapping Done ====================================");
    }
    return QV_SUCCESS;
}

int
qvi_map_packed(
    const qvi_map_config &config,
    qvi_map_t &map
) {
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info("Packed Mapping Started ================================");
    }
    // Did we invert the sources and destinations?
    bool inverted = false;
    size_t n = config.nsrc;
    size_t m = config.ndst;
    // More sources than destinations?
    if (n < m) {
        std::swap(n, m);
        inverted = true;
    }
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
    // If we did an inverted solve, invert the
    // result to match the original n and m.
    if (inverted) {
        map = invert_map(map);
    }
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(
            "Packed Mapping done with N={}, M={} (inverted solve={})",
            n, m, inverted
        );
        qvi_map_emit("Packed", map);
        qvi_log_info("Packed Mapping Done ===================================");
    }
    return QV_SUCCESS;
}

int
qvi_map_spread(
    const qvi_map_config &config,
    qvi_map_t &map
) {
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info("Spread Mapping Started ================================");
    }

    // Did we invert the sources and destinations?
    bool inverted = false;
    size_t n = config.nsrc;
    size_t m = config.ndst;
    // More sources than destinations?
    if (n < m) {
        std::swap(n, m);
        inverted = true;
    }
    for (size_t srci = 0, dsti = 0; srci < n; ++srci) {
        // Mod to loop around destination IDs.
        map[srci].insert((dsti++) % m);
    }
    // If we did an inverted solve, invert the
    // result to match the original n and m.
    if (inverted) {
        map = invert_map(map);
    }
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(
            "Spread Mapping done with N={}, M={} (inverted solve={})",
            n, m, inverted
        );
        qvi_map_emit("Spread", map);
        qvi_log_info("Spread Mapping Done ===================================");
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
    const size_t slots_per_dest = qvi_maxiperk(n, m);
    // Node layout:
    // 0: Super source
    // 1...n: Source nodes
    // n+1...n+(m*slots_per_dest): Destination slot nodes
    // last: Super sink
    const size_t total_slot_nodes = m * slots_per_dest;
    const int super_source = 0;
    const int super_sink = static_cast<int>(1 + n + total_slot_nodes);
    const int total_nodes = super_sink + 1;
    // Create the network.
    qvi_min_cost_max_flow mcmf(total_nodes);
    // Edge 1: Super source-->source nodes (capacity 1, cost 0).
    for (size_t i = 0; i < n; ++i) {
        mcmf.add_edge(super_source, 1 + i, 1, 0);
    }
    // Penalty for using later slots.
    const int64_t slot_penalty = 100000LL;
    // High penalty for no-affinity (zero affinity) destinations.
    const int64_t zaff_penalty = 10000000LL;
    // Edge 2: Source nodes-->destination slot nodes
    //         (capacity 1, progressive cost).
    // Cost structure:
    // 1. Progressive penalty based on slot number:
    //    strongly prefer first slots.
    // 2. Small tie-breaker based on destination number:
    //    prefer lower-numbered destinations.
    // 3. Huge penalty for non-affinity destinations:
    //    but still possible as fallback.
    for (size_t src = 0; src < n; ++src) {
        const auto it = affinities.find(src);
        std::set<size_t> src_affinities;
        if (it != affinities.end()) {
            src_affinities = it->second;
        }
        // If affinity set is empty, treat it as "no preference":
        // allow any destination with low cost.
        if (src_affinities.empty()) {
            for (size_t dst = 0; dst < m; ++dst) {
                src_affinities.insert(dst);
            }
        }
        // Connect to ALL destinations, but use different costs.
        for (size_t dst = 0; dst < m; ++dst) {
            const bool is_preferred = src_affinities.count(dst) > 0;
            // Connect source to all slots of this destination.
            for (size_t slot = 0; slot < slots_per_dest; ++slot) {
                const int slot_node = 1 + n + dst * slots_per_dest + slot;
                const int64_t prog_penalty = slot_penalty * slot;
                const int64_t dest_tiebrkr = dst;
                const int64_t aff_penalty = is_preferred ? 0 : zaff_penalty;
                const int64_t cost = aff_penalty + prog_penalty + dest_tiebrkr;
                mcmf.add_edge(1 + src, slot_node, 1, cost);
            }
        }
    }
    // Edge 3: Destination slot nodes-->super sink (capacity 1, cost 0).
    for (size_t dst = 0; dst < m; ++dst) {
        for (size_t slot = 0; slot < slots_per_dest; ++slot) {
            const int slot_node = 1 + n + dst * slots_per_dest + slot;
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
                const size_t dst = slot_idx / slots_per_dest;
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
    aff_type_t rsrc = config.src_affinities;
    aff_type_t rdst = config.dst_affinities;
    // Our algorithm requires that nsrc >= ndst. If that
    // isn't the case, invert them and note that we did so.
    if (rsrc.size() < rdst.size()) {
        std::swap(rsrc, rdst);
        inverted = true;
    }
    const size_t n = rsrc.size();
    const size_t m = rdst.size();
    assert(n >= m);
    // Determine the affinities shared between sources and destinations.
    const auto affinities = qvi_map_calc_affinities(rsrc, rdst);
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
        qvi_map_emit("APM", map);
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

int
qvi_map_clamp_colors(
    std::vector<int> &colors
) {
    // Recall: sets are ordered.
    const std::set<int> colorset(colors.begin(), colors.end());
    // Maps the input vector colors to their clamped values.
    std::map<int, int> color2clamped;
    // color': the clamped color.
    int colorp = 0;
    for (const auto val : colorset) {
        color2clamped.insert({val, colorp++});
    }
    for (size_t i = 0; i < colors.size(); ++i) {
        colors[i] = color2clamped[colors[i]];
    }
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
