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
#include <string>

// Verbose output max length.
static constexpr size_t vmaxl = qvi_maxolen;

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
    bool wrote_entry = false;
    for (const auto &[src, dests] : assignments) {
        oss << "  {" << src << ", {";
        bool first = true;
        for (size_t dest : dests) {
            if (!first) oss << ", ";
            oss << dest;
            first = false;
        }
        oss << "}},\n";
        wrote_entry = true;
    }
    // Move the put pointer back by 2 characters relative to the end
    // to remove the unneeded ",\n" for the last item in the map.
    if (wrote_entry) {
        oss.seekp(-2, std::ios_base::end);
    }
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
            // Destination is not mapped, so map it. Note that our intent
            // is to not allow multiple destinations per source to be mapped.
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
        qvi_log_info(qvi_spadtolen("Color Mapping Started ", "=", vmaxl));
    }
    auto &src_colors = config.src_colors;
    const size_t n = src_colors.size();
    const size_t m = config.ndst;
    // These should be clamped: 0 to m-1, so verify that.
    assert(
        std::ranges::all_of(src_colors, [m](int val) {
            return val >= 0 && val < static_cast<int>(m);
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
        qvi_log_info(qvi_spadtolen("Color Mapping Done ", "=", vmaxl));
    }
    return QV_SUCCESS;
}

int
qvi_map_packed(
    const qvi_map_config &config,
    qvi_map_t &map
) {
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(qvi_spadtolen("Packed Mapping Started ", "=", vmaxl));
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
        qvi_log_info(qvi_spadtolen("Packed Mapping Done ", "=", vmaxl));
    }
    return QV_SUCCESS;
}

int
qvi_map_spread(
    const qvi_map_config &config,
    qvi_map_t &map
) {
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(qvi_spadtolen("Spread Mapping Started ", "=", vmaxl));
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
        qvi_log_info(qvi_spadtolen("Spread Mapping Done ", "=", vmaxl));
    }
    return QV_SUCCESS;
}

class stable_marriage_solver {
private:
    struct slot {
        size_t destination;
        size_t slot_index;

        bool
        operator<(
            const slot &other
        ) const {
            if (destination != other.destination) {
                return destination < other.destination;
            }
            return slot_index < other.slot_index;
        }

        bool
        operator==(
            const slot &other
        ) const {
            return destination == other.destination &&
                   slot_index == other.slot_index;
        }
    };

    bool m_be_verbose;
    size_t m_nsrcs;
    size_t m_ndsts;
    size_t m_slots_per_dst;
    // Preference lists.
    std::map<size_t, std::vector<slot>> m_src_prefs;
    std::map<slot, std::vector<size_t>> m_slot_prefs;
    // Current matching state.
    std::map<size_t, slot> m_src_to_slot;
    std::map<slot, size_t> m_slot_to_src;
    // Source ID to next proposal ID.
    std::map<size_t, size_t> m_src_next_proposal;

    /**
     * Calculate contention score for each destination.
     * Higher score = more sources want this destination.
     */
    std::map<size_t, size_t>
    m_calculate_dst_contention(
        const qvi_map_t &affinities
    ) {
        std::map<size_t, size_t> contention;
        for (size_t dst = 0; dst < m_ndsts; ++dst) {
            contention[dst] = 0;
        }

        for (const auto &[src, dsts] : affinities) {
            if (dsts.empty()) {
                // Empty affinity counts as wanting all destinations.
                for (size_t dst = 0; dst < m_ndsts; ++dst) {
                    contention[dst]++;
                }
            }
            else {
                for (size_t dst : dsts) {
                    contention[dst]++;
                }
            }
        }
        return contention;
    }

    /**
     * Build preference list for a source with STRONG affinity preference.
     *
     * Ordering strategy:
     * 1. Affinity destinations first (with lowest contention preferred).
     * 2. Within affinity: prefer less contended destinations (better chance).
     * 3. Within destination: all slots in order.
     * 4. Non-affinity destinations LAST (only as fallback).
     */
    std::vector<slot>
    m_build_src_prefs(
        const std::set<size_t> &affinities,
        bool has_empty_affinity,
        const std::map<size_t, size_t> &contention
    ) {
        auto preference_sorter = [&contention](size_t a, size_t b) {
            const size_t cont_a = contention.at(a);
            const size_t cont_b = contention.at(b);
            // Less contention first.
            if (cont_a != cont_b) return cont_a < cont_b;
            // Tie-breaker: lower destination number.
            return a < b;
        };
        std::vector<slot> preferences;
        // === Phase 1: affinity destinations (highest priority). ===
        std::vector<size_t> preferred_dests(
            affinities.begin(), affinities.end()
        );
        // Sort by contention (less contended first = better chance of success).
        // Break ties by destination number (lower is better).
        std::sort(
            preferred_dests.begin(),
            preferred_dests.end(),
            preference_sorter
        );
        // Add all slots of affinity destinations
        for (size_t dst : preferred_dests) {
            for (size_t slot = 0; slot < m_slots_per_dst; ++slot) {
                preferences.push_back({dst, slot});
            }
        }
        // === Phase 2: non-affinity destinations (fallback only). ===
        // Only add if source has specific affinities (not empty affinity).
        // These are MUCH less preferred.
        if (!has_empty_affinity && affinities.size() < m_ndsts) {
            std::vector<size_t> non_preferred;
            for (size_t dst = 0; dst < m_ndsts; ++dst) {
                if (affinities.count(dst) == 0) {
                    non_preferred.push_back(dst);
                }
            }
            // Sort non-preferred by contention:
            // (less contended first for better fallback).
            std::sort(
                non_preferred.begin(),
                non_preferred.end(),
                preference_sorter
            );

            for (size_t dst : non_preferred) {
                for (size_t slot = 0; slot < m_slots_per_dst; ++slot) {
                    preferences.push_back({dst, slot});
                }
            }
        }
        return preferences;
    }

    /**
     * Build preference list for a destination slot with STRONG affinity preference.
     *
     * Ordering strategy:
     * 1. Sources WITH affinity for this destination (much higher priority).
     *    - Ordered by source number (lower first for stability).
     * 2. Sources WITHOUT affinity (much lower priority).
     *    - Only accepted as last resort.
     */
    std::vector<size_t>
    m_build_slot_preferences(
        const slot &slot,
        const qvi_map_t &affinities
    ) {
        std::vector<size_t> with_affinity;
        std::vector<size_t> without_affinity;

        for (size_t src = 0; src < m_nsrcs; ++src) {
            auto it = affinities.find(src);
            bool has_affinity = false;

            if (it != affinities.end()) {
                // Empty affinity (can go anywhere). Treat as having affinity.
                if (it->second.empty()) has_affinity = true;
                else has_affinity = it->second.count(slot.destination) > 0;
            }
            if (has_affinity) with_affinity.push_back(src);
            else without_affinity.push_back(src);
        }
        // Sort by source number within each group.
        std::sort(with_affinity.begin(), with_affinity.end());
        std::sort(without_affinity.begin(), without_affinity.end());
        // CRITICAL: Affinity sources come first (much higher priority).
        std::vector<size_t> preferences;
        preferences.reserve(m_nsrcs);
        preferences.insert(
            preferences.end(),
            with_affinity.begin(),
            with_affinity.end()
        );
        preferences.insert(
            preferences.end(),
            without_affinity.begin(),
            without_affinity.end()
        );
        return preferences;
    }

    /**
     * Check if slot prefers new_source over current_source.
     */
    bool
    m_slot_prefers(
        const slot &slot,
        size_t new_source,
        size_t cur_source
    ) const {
        const auto &prefs = m_slot_prefs.at(slot);
        const size_t nprefs = prefs.size();
        // Initialize with sentinel values.
        size_t new_rank = nprefs;
        size_t cur_rank = nprefs;

        for (size_t i = 0; i < nprefs; ++i) {
            if (prefs[i] == new_source) new_rank = i;
            if (prefs[i] == cur_source) cur_rank = i;
            // Early termination check.
            if (new_rank != nprefs && cur_rank != nprefs) break;
        }
        // Lower in list, so higher in preference?
        return new_rank < cur_rank;
    }

    std::string
    m_format_contention_scores(
        const std::map<size_t, size_t> &contention
    ) {
            std::ostringstream oss;
            oss << "\nGS Destination contention scores:\n";
            oss << "  Key: {dst, score}\n";
            oss << "{\n";
            for (size_t dst = 0; dst < m_ndsts; ++dst) {
                size_t score = 0;
                if (contention.contains(dst)) {
                    score = contention.at(dst);
                }
                oss << "  {" << dst << ", " << score << "},\n";
            }
            if (m_ndsts > 0) {
                oss.seekp(-2, std::ios_base::end);
            }
            oss << "\n}\n";
            return oss.str();
    }

public:
    stable_marriage_solver(
        size_t n,
        size_t m,
        const qvi_map_t &affinities,
        bool be_verbose
    ) : m_be_verbose(be_verbose)
      , m_nsrcs(n)
      , m_ndsts(m)
      , m_slots_per_dst(qvi_maxiperk(n, m))
    {
        // Calculate contention for smart ordering.
        const auto contention = m_calculate_dst_contention(affinities);

        if (qvi_unlikely(m_be_verbose)) {
            qvi_log_info("{}", m_format_contention_scores(contention));
        }
        // Build preference lists for sources.
        for (size_t src = 0; src < m_nsrcs; ++src) {
            const auto it = affinities.find(src);
            std::set<size_t> src_affinities;
            bool has_empty_affinity = false;

            if (it != affinities.end()) {
                src_affinities = it->second;
                has_empty_affinity = src_affinities.empty();

                if (has_empty_affinity) {
                    for (size_t dst = 0; dst < m; ++dst) {
                        src_affinities.insert(dst);
                    }
                }
            }
            m_src_prefs[src] = m_build_src_prefs(
                src_affinities,
                has_empty_affinity,
                contention
            );
            m_src_next_proposal[src] = 0;
        }
        // Build preference lists for slots.
        for (size_t dst = 0; dst < m; ++dst) {
            for (size_t slot_id = 0; slot_id < m_slots_per_dst; ++slot_id) {
                const slot slot{dst, slot_id};
                m_slot_prefs[slot] = m_build_slot_preferences(slot, affinities);
                m_slot_to_src[slot] = SIZE_MAX;
            }
        }
    }

    /**
     * Run the Gale-Shapley algorithm to find a stable matching.
     */
    void
    solve(void)
    {
        std::queue<size_t> free_sources;
        for (size_t src = 0; src < m_nsrcs; ++src) {
            free_sources.push(src);
        }

        while (!free_sources.empty()) {
            const size_t source = free_sources.front();
            free_sources.pop();

            if (m_src_next_proposal[source] >= m_src_prefs[source].size()) {
                const auto ers = "Source " + std::to_string(source) +
                    " exhausted all preferences: no feasible solution!";
                qvi_log_error(ers);
                throw qvi_runtime_error(QV_ERR_NOT_SUPPORTED);
            }

            slot slot = m_src_prefs[source][m_src_next_proposal[source]];
            m_src_next_proposal[source]++;

            const size_t current_match = m_slot_to_src[slot];
            // Slot is free.
            if (current_match == SIZE_MAX) {
                m_src_to_slot[source] = slot;
                m_slot_to_src[slot] = source;
            }
            // Slot is occupied and prefers new source.
            else if (m_slot_prefers(slot, source, current_match)) {
                // Slot prefers new source.
                m_src_to_slot[source] = slot;
                m_slot_to_src[slot] = source;
                m_src_to_slot.erase(current_match);
                free_sources.push(current_match);
            }
            // Slot is occupied and rejects new source.
            else {
                free_sources.push(source);
            }
        }
    }

    /**
     * Returns the final matching.
     */
    qvi_map_t
    get_matching(void) const {
        qvi_map_t result;
        for (const auto &[source, slot] : m_src_to_slot) {
            result[source].insert(slot.destination);
        }
        return result;
    }

    /**
     * Verify stability and analyze affinity satisfaction.
     */
    bool
    stable(
        const qvi_map_t &affinities
    ) const {
        size_t affinity_satisfied = 0;
        size_t total_with_affinity = 0;
        // Check each source.
        for (size_t src = 0; src < m_nsrcs; ++src) {
            auto it = m_src_to_slot.find(src);
            if (it == m_src_to_slot.end()) {
                qvi_log_error("Source {} is unmatched", src);
                return false;
            }

            const slot &current_slot = it->second;
            size_t assigned_dst = current_slot.destination;
            // Check affinity satisfaction.
            auto aff_it = affinities.find(src);
            if (aff_it != affinities.end() && !aff_it->second.empty()) {
                total_with_affinity++;
                if (aff_it->second.count(assigned_dst) > 0) {
                    affinity_satisfied++;
                }
            }
            // Check for blocking pairs.
            for (const auto &preferred_slot : m_src_prefs.at(src)) {
                if (preferred_slot == current_slot) break;

                auto slot_match_it = m_slot_to_src.find(preferred_slot);
                if (slot_match_it == m_slot_to_src.end()) continue;

                size_t other_source = slot_match_it->second;

                if (other_source == SIZE_MAX) {
                    qvi_log_error(
                        "Blocking pair: source {} prefers free slot!", src
                    );
                    return false;
                }

                if (m_slot_prefers(preferred_slot, src, other_source)) {
                    qvi_log_error("Blocking pair found!");
                    return false;
                }
            }
        }
        if (qvi_unlikely(m_be_verbose)) {
            qvi_log_info(
                "GS Affinity satisfaction: {}/{}\n",
                affinity_satisfied,
                total_with_affinity
            );
        }
        return true;
    }
};

/**
 * Solves the mapping problem with STRONG affinity preference.
 */
qvi_map_t
solve_ap_mapping(
    size_t n,
    size_t m,
    const qvi_map_t &affinities,
    bool be_verbose
) {
    stable_marriage_solver solver(n, m, affinities, be_verbose);
    solver.solve();

    if (!solver.stable(affinities)) {
        qvi_log_error("Solution is not stable!");
        throw qvi_runtime_error(QV_ERR_NOT_SUPPORTED);
    }
    return solver.get_matching();
}

int
qvi_map_affinity_preserving(
    const qvi_map_config &config,
    qvi_map_t &map
) {
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(qvi_spadtolen("APM Mapping Started ", "=", vmaxl));
    }
    // Cache relevant input data.
    const auto &src = config.src_affinities;
    const auto &dst = config.dst_affinities;
    const size_t nsrc = src.size();
    const size_t ndst = dst.size();
    // Determine the affinities shared between sources and destinations.
    const auto affinities = qvi_map_calc_affinities(src, dst);
    if (qvi_unlikely(config.be_verbose)) {
        qvi_map_emit("APM Affinities", affinities);
    }
    // Solve the mapping problem.
    // Note: our algorithm doesn't require that nsrc >= ndst.
    map = solve_ap_mapping(nsrc, ndst, affinities, config.be_verbose);
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info("APM done with N={}, M={}", nsrc, ndst);
        qvi_map_emit("APM", map);
        qvi_log_info(qvi_spadtolen("APM Mapping Done ", "=", vmaxl));
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
