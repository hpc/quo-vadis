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

qvi_map_t
qvi_map_invert(
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

qvi_map_t
qvi_map_colors(
    const qvi_map_config &config
) {
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(qvi_spadtolen("Color Mapping Started ", "=", vmaxl));
    }
    auto &src_colors = config.src_colors;
    const size_t n = src_colors.size();
    // Sanity check: this mapper accepts non-negative colors and the special
    // QV_SPLIT_UNDEFINED sentinel. QV_SPLIT_UNDEFINED sources are excluded from
    // the split, so they are intentionally left unmapped here.
    assert(
        std::ranges::all_of(config.src_colors, [](int val) {
            return val >= 0 || val == QV_SPLIT_UNDEFINED;
        })
    );
    qvi_map_t map;
    // Assign each source to the piece indicated by its color. Sources colored
    // QV_SPLIT_UNDEFINED are skipped so they receive no resources (an empty
    // scope), per the documented semantics in quo-vadis.h.
    for (size_t i = 0; i < n; ++i) {
        if (src_colors[i] == QV_SPLIT_UNDEFINED) continue;
        map[i].insert(src_colors[i]);
    }
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info("Color Mapping done with N={}", n);
        qvi_map_emit("Color Mapping", map);
        qvi_log_info(qvi_spadtolen("Color Mapping Done ", "=", vmaxl));
    }
    return map;
}

qvi_map_t
qvi_map_packed(
    const qvi_map_config &config
) {
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(qvi_spadtolen("Packed Mapping Started ", "=", vmaxl));
    }
    const size_t n = config.nsrc;
    const size_t m = config.ndst;
    qvi_map_t map;
    // Nothing to do.
    if (n == 0 || m == 0) {
        return map;
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
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(
            "Packed Mapping done with N={}, M={}", n, m
        );
        qvi_map_emit("Packed", map);
        qvi_log_info(qvi_spadtolen("Packed Mapping Done ", "=", vmaxl));
    }
    return map;
}

qvi_map_t
qvi_map_spread(
    const qvi_map_config &config
) {
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(qvi_spadtolen("Spread Mapping Started ", "=", vmaxl));
    }
    const size_t n = config.nsrc;
    const size_t m = config.ndst;
    qvi_map_t map;
    // Nothing to do.
    if (n == 0 || m == 0) {
        return map;
    }
    if (n < m) {
        // Spread sources out across destinations.
        const size_t stride = m / n;
        for (size_t srci = 0, dsti = 0; srci < n; ++srci) {
            map[srci].insert(dsti);
            dsti += stride;
        }
    }
    else {
        // When n >= m, spread acts like a cyclic distribution.
        for (size_t srci = 0, dsti = 0; srci < n; ++srci) {
            // Mod to loop around destination IDs.
            map[srci].insert((dsti++) % m);
        }
    }
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(
            "Spread Mapping done with N={}, M={}", n, m
        );
        qvi_map_emit("Spread", map);
        qvi_log_info(qvi_spadtolen("Spread Mapping Done ", "=", vmaxl));
    }
    return map;
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

qvi_map_t
qvi_map_close(
    const qvi_map_config &config
) {
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(qvi_spadtolen("Close Mapping Started ", "=", vmaxl));
    }
    // Cache relevant input data.
    const auto &src = config.src_affinities;
    const auto &dst = config.dst_affinities;
    const size_t nsrc = src.size();
    const size_t ndst = dst.size();
    // Determine the affinities shared between sources and destinations.
    const auto affinities = qvi_map_calc_affinities(src, dst);
    if (qvi_unlikely(config.be_verbose)) {
        qvi_map_emit("Close Affinities", affinities);
    }
    // Solve the mapping problem.
    // Note: our algorithm doesn't require that nsrc >= ndst.
    const auto map = solve_ap_mapping(
        nsrc, ndst, affinities, config.be_verbose
    );
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info("Close done with N={}, M={}", nsrc, ndst);
        qvi_map_emit("Close", map);
        qvi_log_info(qvi_spadtolen("Close Mapping Done ", "=", vmaxl));
    }
    return map;
}

qvi_map_t
qvi_map_afpacked(
    const qvi_map_config &config
) {
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info(qvi_spadtolen("Afpacked Mapping Started ", "=", vmaxl));
    }
    // Cache relevant input data. Sources are the source affinities; destinations
    // are the affinities of the places where the sources will be mapped.
    const auto &src = config.src_affinities;
    const auto &dst = config.dst_affinities;
    const size_t nsrc = src.size();
    const size_t ndst = dst.size();
    qvi_map_t map;
    // Nothing to do.
    if (nsrc == 0 || ndst == 0) {
        return map;
    }
    // Tracks whether a given source has already been mapped. Sources must be
    // mapped exactly once.
    std::vector<bool> placed(nsrc, false);
    // Soft per-destination capacity: ceil(nsrc / ndst). Sources are packed onto
    // destinations up to this capacity. The capacity is a soft target: if all
    // of a source's affinity destinations are already full, the source still
    // overflows into the first destination it has affinity to.
    const size_t cap = qvi_maxiperk(nsrc, ndst);
    // Tracks how many sources have been assigned to each destination.
    std::vector<size_t> count(ndst, 0);
    // Affinity predicates.
    const auto included = [&](size_t srci, size_t dsti) {
        return hwloc_bitmap_isincluded(
            src.at(srci).cdata(), dst.at(dsti).cdata()
        ) != 0;
    };
    const auto intersects = [&](size_t srci, size_t dsti) {
        return hwloc_bitmap_intersects(
            src.at(srci).cdata(), dst.at(dsti).cdata()
        ) != 0;
    };
    // Returns the first (lowest-index) destination satisfying the predicate for
    // source srci that is also under capacity, or ndst if none qualifies.
    const auto first_fit = [&](size_t srci, const auto &predicate) -> size_t {
        for (size_t dsti = 0; dsti < ndst; ++dsti) {
            if (predicate(srci, dsti) && count[dsti] < cap) return dsti;
        }
        return ndst;
    };
    // Returns the first (lowest-index) destination satisfying the predicate for
    // source srci, ignoring capacity, or ndst if none qualifies.
    const auto first_affinity = [&](size_t srci, const auto &predicate) -> size_t {
        for (size_t dsti = 0; dsti < ndst; ++dsti) {
            if (predicate(srci, dsti)) return dsti;
        }
        return ndst;
    };
    // Pack each source onto a destination in source order.
    //
    // Priority per source:
    //   1. First destination that fully contains the source's affinity and is
    //      under capacity (containment is preferred).
    //   2. First destination that shares any affinity and is under capacity.
    //   3. Soft-cap overflow: first destination the source has affinity to,
    //      preferring containment, then intersection, ignoring capacity.
    for (size_t srci = 0; srci < nsrc; ++srci) {
        size_t dsti = first_fit(srci, included);
        if (dsti == ndst) dsti = first_fit(srci, intersects);
        // All affinity destinations are at capacity: overflow into the first
        // destination the source has affinity to (containment first).
        if (dsti == ndst) dsti = first_affinity(srci, included);
        if (dsti == ndst) dsti = first_affinity(srci, intersects);
        if (dsti != ndst) {
            map[srci].insert(dsti);
            placed[srci] = true;
            count[dsti]++;
        }
    }
    // Every source is expected to have affinity to at least one destination and
    // therefore must have been mapped exactly once. If any source was left
    // unplaced, the input has no feasible mapping.
    for (size_t srci = 0; srci < nsrc; ++srci) {
        if (!placed[srci]) {
            qvi_log_error(
                "Source {} has no affinity to any destination: "
                "no feasible mapping!", srci
            );
            throw qvi_runtime_error(QV_ERR_NOT_SUPPORTED);
        }
    }
    if (qvi_unlikely(config.be_verbose)) {
        qvi_log_info("Afpacked done with N={}, M={}", nsrc, ndst);
        qvi_map_emit("Afpacked", map);
        qvi_log_info(qvi_spadtolen("Afpacked Mapping Done ", "=", vmaxl));
    }
    return map;
}

void
qvi_map_emit(
    const std::string &name,
    const qvi_map_t &map
) {
    qvi_log_info("{} assignments:\n{}", name, format_assignments(map));
}

std::vector<int>
qvi_map_clamp_colors(
    const std::vector<int> &colors
) {
    // Recall: sets are ordered. QV_SPLIT_UNDEFINED marks a member that opts out
    // of the split, so it is excluded from the distinct-color ranking and
    // passed through unchanged rather than folded into a real color.
    std::set<int> colorset(colors.begin(), colors.end());
    colorset.erase(QV_SPLIT_UNDEFINED);
    // Maps the input vector colors to their clamped values.
    std::map<int, int> color2clamped;
    // color': the clamped color.
    int colorp = 0;
    for (const auto val : colorset) {
        color2clamped.insert({val, colorp++});
    }
    std::vector<int> result(colors.size());
    for (size_t i = 0; i < colors.size(); ++i) {
        result[i] = colors[i] == QV_SPLIT_UNDEFINED
            ? QV_SPLIT_UNDEFINED
            : color2clamped[colors[i]];
    }
    return result;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
