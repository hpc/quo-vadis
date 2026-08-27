/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file common-split-utils.cc
 *
 * Implementation of the shared qv_split() verification helpers. See
 * common-split-utils.h for the rationale and the mapping semantics being
 * verified.
 */

#include "common-split-utils.h"
#include "common-test-utils.h"

#include <algorithm>
#include <map>
#include <set>

namespace ctu_split {

void
check(
    int rc,
    const std::string &what
) {
    if (rc != QV_SUCCESS) {
        ctu_panic("%s (rc=%s)", what.c_str(), qv_strerr(rc));
    }
}

qvi_hwloc_bitmap
scope_cpuset(
    qv_scope_t *scope
) {
    return qvi_hwloc_bitmap(scope->hwpool().cpuset().cdata());
}

std::string
cpuset_str(
    const qvi_hwloc_bitmap &bitmap
) {
    return qvi_hwloc::bitmap_list_string(bitmap.cdata());
}

std::vector<qvi_hwloc_bitmap>
expected_chunks(
    hwloc_topology_t topo,
    const qvi_hwloc_bitmap &base,
    int npieces
) {
    // Enumerate the base PUs in hwloc iteration order.
    std::vector<hwloc_obj_t> pus;
    hwloc_obj_t pu = nullptr;
    while ((pu = hwloc_get_next_obj_inside_cpuset_by_type(
                topo, base.cdata(), HWLOC_OBJ_PU, pu)) != nullptr) {
        pus.push_back(pu);
    }
    const int n = static_cast<int>(pus.size());
    const int base_chunk = n / npieces;
    const int remainder = n % npieces;

    std::vector<qvi_hwloc_bitmap> chunks(npieces);
    int pos = 0;
    for (int i = 0; i < npieces; ++i) {
        const int sz = base_chunk + (i < remainder ? 1 : 0);
        for (int k = 0; k < sz; ++k) {
            const int orrc = hwloc_bitmap_or(
                chunks[i].data(), chunks[i].cdata(), pus[pos++]->cpuset
            );
            ctu_assert(orrc == 0, "hwloc_bitmap_or() failed");
        }
    }
    ctu_assert(pos == n, "chunking consumed %d of %d PUs", pos, n);
    return chunks;
}

void
assert_same_cpuset(
    const qvi_hwloc_bitmap &got,
    const qvi_hwloc_bitmap &want,
    const std::string &label
) {
    ctu_assert(
        got == want,
        "%s: cpuset mismatch\n  got : %s\n  want: %s",
        label.c_str(), cpuset_str(got).c_str(), cpuset_str(want).c_str()
    );
}

int
require_aligned_chunk(
    const qvi_hwloc_bitmap &got,
    const std::vector<qvi_hwloc_bitmap> &chunks,
    const std::string &label
) {
    for (size_t i = 0; i < chunks.size(); ++i) {
        if (got == chunks[i]) return static_cast<int>(i);
    }
    ctu_panic(
        "%s: cpuset does not match any chunk of the %zu-way split\n"
        "  got: %s",
        label.c_str(), chunks.size(), cpuset_str(got).c_str()
    );
    return -1; // Unreachable.
}

void
verify_clamp_invariant(
    const std::vector<int> &colors,
    const std::string &label
) {
    const std::vector<int> clamped = qvi_map_clamp_colors(colors);
    ctu_assert(
        clamped.size() == colors.size(),
        "%s: clamp changed the number of colors (%zu -> %zu)",
        label.c_str(), colors.size(), clamped.size()
    );

    // Rank each distinct *non-undefined* input color among the sorted distinct
    // colors. QV_SPLIT_UNDEFINED entries opt out of the split and are excluded
    // from the ranking; they pass through the clamp unchanged.
    std::set<int> distinct(colors.begin(), colors.end());
    distinct.erase(QV_SPLIT_UNDEFINED);
    std::map<int, int> expected_rank;
    int rank = 0;
    for (const int c : distinct) expected_rank[c] = rank++;
    const int ndistinct = static_cast<int>(distinct.size());

    // The distinct clamped values (excluding the passed-through opt-outs) must
    // be exactly [0, ndistinct).
    std::set<int> clamped_distinct(clamped.begin(), clamped.end());
    clamped_distinct.erase(QV_SPLIT_UNDEFINED);
    ctu_assert(
        static_cast<int>(clamped_distinct.size()) == ndistinct,
        "%s: distinct clamped count (%zu) != distinct input count (%d)",
        label.c_str(), clamped_distinct.size(), ndistinct
    );
    int expect = 0;
    for (const int cv : clamped_distinct) {
        ctu_assert(
            cv == expect,
            "%s: clamped values are not a dense [0, %d) range "
            "(saw %d, expected %d)",
            label.c_str(), ndistinct, cv, expect
        );
        ++expect;
    }

    // Every element must map to its color's rank (or pass through unchanged if
    // it opted out), preserving order and equality.
    for (size_t i = 0; i < colors.size(); ++i) {
        const int want = (colors[i] == QV_SPLIT_UNDEFINED)
            ? QV_SPLIT_UNDEFINED
            : expected_rank[colors[i]];
        ctu_assert(
            clamped[i] == want,
            "%s: color %d clamped to %d, expected %d",
            label.c_str(), colors[i], clamped[i], want
        );
    }
}

int
predict_auto_chunk(
    int split_kind,
    int npieces,
    int group_size,
    int taskid
) {
    // The split size the library actually uses. QV_SPLIT_AUTO clamps the split
    // size down to the group size (see determine_actual_split_size); the others
    // honor the requested piece count exactly.
    const size_t split_size = (split_kind == QV_SPLIT_AUTO)
        ? std::min(static_cast<size_t>(group_size),
                   static_cast<size_t>(npieces))
        : static_cast<size_t>(npieces);

    // Reuse the library's own mapping primitives so we predict the exact
    // destination piece, not merely a plausible one. PACKED and AUTO both use
    // qvi_map_packed; SPREAD uses qvi_map_spread.
    const qvi_map_config config(
        static_cast<size_t>(group_size), split_size
    );
    const qvi_map_t map = (split_kind == QV_SPLIT_SPREAD)
        ? qvi_map_spread(config)
        : qvi_map_packed(config);

    const auto it = map.find(static_cast<size_t>(taskid));
    ctu_assert(
        it != map.end(),
        "predict_auto_chunk: task %d has no mapping (group_size=%d)",
        taskid, group_size
    );
    ctu_assert(
        it->second.size() == 1,
        "predict_auto_chunk: task %d maps to %zu pieces (expected 1)",
        taskid, it->second.size()
    );
    return static_cast<int>(*it->second.begin());
}

// ---------------------------------------------------------------------------
// Shared, scope-kind-agnostic driver.
// ---------------------------------------------------------------------------

namespace {

int
npus(
    const qvi_hwloc_bitmap &cpuset
) {
    return hwloc_bitmap_weight(cpuset.cdata());
}

// The piece counts every suite exercises, clipped to the base PU count (the
// library cannot carve more pieces than there are PUs).
std::vector<int>
piece_counts_for(
    int base_npus
) {
    std::vector<int> result;
    for (const int p : {1, 2, 3, 4}) {
        if (p <= base_npus) result.push_back(p);
    }
    return result;
}

// Verifies each participating task landed in exactly the chunk its (clamped)
// color selects, that opted-out tasks got no subscope, and that the union of
// the occupied pieces matches the coverage implied by |clamped|. |chunks| is
// the npieces-way chunking the colors index into.
void
verify_explicit_placement(
    split_backend &backend,
    const std::vector<int> &colors,
    const std::vector<int> &clamped,
    const std::vector<qvi_hwloc_bitmap> &chunks,
    const std::vector<task_result> &results,
    const std::string &label
) {
    const int gsize = backend.group_size();
    ctu_assert(
        static_cast<int>(results.size()) == gsize,
        "%s: backend returned %zu results for %d tasks",
        label.c_str(), results.size(), gsize
    );

    qvi_hwloc_bitmap occupied_union;
    qvi_hwloc_bitmap expected_union;
    for (int i = 0; i < gsize; ++i) {
        const task_result &r = results[i];
        if (colors[i] == QV_SPLIT_UNDEFINED) {
            // Opted-out tasks receive no subscope and occupy no piece.
            ctu_assert(
                r.opted_out,
                "%s: task %d opted out but received a subscope", label.c_str(), i
            );
            continue;
        }
        ctu_assert(
            !r.opted_out,
            "%s: task %d expected a subscope but opted out", label.c_str(), i
        );
        const int want_idx = clamped[i];
        ctu_assert(
            r.cpuset == chunks[want_idx],
            "%s: task %d color %d clamped to %d -> %s, expected chunk %d = %s",
            label.c_str(), i, colors[i], want_idx,
            cpuset_str(r.cpuset).c_str(), want_idx,
            cpuset_str(chunks[want_idx]).c_str()
        );
        // Every resource a child receives must come from the parent.
        ctu_assert(
            hwloc_bitmap_isincluded(
                r.cpuset.cdata(), backend.base_cpuset().cdata()
            ),
            "%s: task %d cpuset '%s' is not a subset of base '%s'",
            label.c_str(), i, cpuset_str(r.cpuset).c_str(),
            cpuset_str(backend.base_cpuset()).c_str()
        );
        const int orrc = hwloc_bitmap_or(
            occupied_union.data(), occupied_union.cdata(), r.cpuset.cdata()
        );
        ctu_assert(orrc == 0, "hwloc_bitmap_or() failed");
    }
    // The occupied pieces are exactly the chunks named by the distinct clamped
    // colors; recompute that expected coverage and compare.
    std::set<int> occupied_idx;
    for (int i = 0; i < gsize; ++i) {
        if (colors[i] != QV_SPLIT_UNDEFINED) occupied_idx.insert(clamped[i]);
    }
    for (const int idx : occupied_idx) {
        const int orrc = hwloc_bitmap_or(
            expected_union.data(), expected_union.cdata(), chunks[idx].cdata()
        );
        ctu_assert(orrc == 0, "hwloc_bitmap_or() failed");
    }
    ctu_assert(
        occupied_union == expected_union,
        "%s: occupied union %s does not match expected coverage %s",
        label.c_str(), cpuset_str(occupied_union).c_str(),
        cpuset_str(expected_union).c_str()
    );
}

// Section 1: explicit, contiguous coloring. Task i requests color i % npieces
// and must land in exactly that chunk.
void
test_explicit_contiguous(
    split_backend &backend,
    int npieces,
    const std::vector<qvi_hwloc_bitmap> &chunks
) {
    const int gsize = backend.group_size();
    const std::string label =
        "explicit-contiguous[" + std::to_string(npieces) + "]";

    std::vector<int> colors(gsize);
    for (int i = 0; i < gsize; ++i) colors[i] = i % npieces;

    verify_clamp_invariant(colors, label + "/clamp");
    const std::vector<int> clamped = qvi_map_clamp_colors(colors);

    const auto results = backend.do_split(npieces, colors, label);
    verify_explicit_placement(
        backend, colors, clamped, chunks, results, label
    );
    backend.log(label + ": OK\n");
}

// Section 2: explicit, arbitrary positive coloring. Task i draws an arbitrary
// color from a palette; the clamp folds those into [0, ndistinct) preserving
// order and each task must land in the chunk the clamp selects.
void
test_explicit_arbitrary(
    split_backend &backend,
    int npieces,
    const std::vector<qvi_hwloc_bitmap> &chunks
) {
    const int gsize = backend.group_size();
    const std::string label =
        "explicit-arbitrary[" + std::to_string(npieces) + "]";

    // Arbitrary, unsorted palette. Using at most npieces distinct values keeps
    // ndistinct <= npieces (a library requirement). Reuse entries beyond
    // npieces so some colors repeat, exercising the dedup path too.
    const std::vector<int> palette = {73, 5, 1000, 42, 8, 900};
    std::vector<int> colors(gsize);
    for (int i = 0; i < gsize; ++i) {
        colors[i] = palette[(i % npieces) % static_cast<int>(palette.size())];
    }

    verify_clamp_invariant(colors, label + "/clamp");
    const std::vector<int> clamped = qvi_map_clamp_colors(colors);

    const auto results = backend.do_split(npieces, colors, label);
    verify_explicit_placement(
        backend, colors, clamped, chunks, results, label
    );
    backend.log(label + ": OK\n");
}

// Section 2b: QV_SPLIT_UNDEFINED mixed in with valid colors. Even tasks opt
// out; odd tasks provide arbitrary valid colors. Opted-out tasks must receive
// no subscope and occupy no piece; the rest land in their clamped chunk.
// Requires at least two tasks so both an opt-out and a participant exist.
void
test_mixed_undefined(
    split_backend &backend,
    int npieces,
    const std::vector<qvi_hwloc_bitmap> &chunks
) {
    const int gsize = backend.group_size();
    if (gsize < 2) return;

    const std::string label =
        "mixed-undefined[" + std::to_string(npieces) + "]";

    const std::vector<int> palette = {73, 5, 1000, 42, 8, 900};
    std::vector<int> colors(gsize);
    int nvalid = 0;
    for (int i = 0; i < gsize; ++i) {
        if (i % 2 == 0) {
            colors[i] = QV_SPLIT_UNDEFINED; // task 0 always opts out.
        }
        else {
            colors[i] =
                palette[(nvalid++ % npieces) % static_cast<int>(palette.size())];
        }
    }

    verify_clamp_invariant(colors, label + "/clamp");
    const std::vector<int> clamped = qvi_map_clamp_colors(colors);

    const auto results = backend.do_split(npieces, colors, label);
    verify_explicit_placement(
        backend, colors, clamped, chunks, results, label
    );
    backend.log(label + ": OK\n");
}

// Section 3: QV_SPLIT_UNDEFINED for every task. All tasks opt out and receive
// no subscope.
void
test_all_undefined(
    split_backend &backend,
    int npieces
) {
    const int gsize = backend.group_size();
    const std::string label =
        "undefined[" + std::to_string(npieces) + "]";

    const std::vector<int> colors(gsize, QV_SPLIT_UNDEFINED);
    const auto results = backend.do_split(npieces, colors, label);
    ctu_assert(
        static_cast<int>(results.size()) == gsize,
        "%s: backend returned %zu results for %d tasks",
        label.c_str(), results.size(), gsize
    );
    for (int i = 0; i < gsize; ++i) {
        ctu_assert(
            results[i].opted_out,
            "%s: task %d expected no subscope for QV_SPLIT_UNDEFINED",
            label.c_str(), i
        );
    }
    backend.log(label + ": OK\n");
}

// Section 4: automatic groupings. PACKED/SPREAD/AUTO are deterministic, so the
// exact destination chunk of every task is predicted with predict_auto_chunk
// and asserted. CLOSE is affinity-driven, so it is only required to place each
// task in a valid, chunk-aligned piece whose union is a non-empty subset of the
// base.
void
test_automatic(
    split_backend &backend,
    int npieces,
    const std::vector<qvi_hwloc_bitmap> &chunks
) {
    const int gsize = backend.group_size();
    const int base_npus = npus(backend.base_cpuset());
    hwloc_topology_t topo = backend.topology();
    const qvi_hwloc_bitmap &base = backend.base_cpuset();

    const struct { int kind; const char *name; } auto_kinds[] = {
        {QV_SPLIT_PACKED, "QV_SPLIT_PACKED"},
        {QV_SPLIT_SPREAD, "QV_SPLIT_SPREAD"},
        {QV_SPLIT_AUTO,   "QV_SPLIT_AUTO"},
    };

    for (const auto &ak : auto_kinds) {
        const std::string label =
            std::string("auto/") + ak.name + "[" + std::to_string(npieces) + "]";
        // AUTO clamps the effective split size to min(group_size, npieces); the
        // chunk vector must be recomputed at that clamped size.
        const int eff_pieces = (ak.kind == QV_SPLIT_AUTO)
            ? std::min(gsize, npieces) : npieces;
        const std::vector<qvi_hwloc_bitmap> eff_chunks =
            (eff_pieces == npieces)
                ? chunks
                : expected_chunks(topo, base, eff_pieces);

        const std::vector<int> colors(gsize, ak.kind);
        const auto results = backend.do_split(npieces, colors, label);
        ctu_assert(
            static_cast<int>(results.size()) == gsize,
            "%s: backend returned %zu results for %d tasks",
            label.c_str(), results.size(), gsize
        );
        for (int i = 0; i < gsize; ++i) {
            const int want = predict_auto_chunk(ak.kind, npieces, gsize, i);
            ctu_assert(
                !results[i].opted_out,
                "%s: task %d unexpectedly opted out", label.c_str(), i
            );
            ctu_assert(
                results[i].cpuset == eff_chunks[want],
                "%s: task %d landed in %s, expected chunk %d = %s",
                label.c_str(), i, cpuset_str(results[i].cpuset).c_str(),
                want, cpuset_str(eff_chunks[want]).c_str()
            );
        }
        backend.log(label + ": OK\n");
    }

    // CLOSE: affinity-driven, so we do not pin tasks to specific pieces nor
    // assume every piece is occupied (affinity may pile several tasks onto one
    // piece and leave others empty). We require each task to land in a valid,
    // chunk-aligned piece and the union of occupied pieces to be a non-empty
    // subset of the base.
    {
        const std::string label =
            "auto/QV_SPLIT_CLOSE[" + std::to_string(npieces) + "]";
        const std::vector<int> colors(gsize, QV_SPLIT_CLOSE);
        const auto results = backend.do_split(npieces, colors, label);
        qvi_hwloc_bitmap occupied_union;
        for (int i = 0; i < gsize; ++i) {
            ctu_assert(
                !results[i].opted_out,
                "%s: task %d unexpectedly opted out", label.c_str(), i
            );
            (void)require_aligned_chunk(results[i].cpuset, chunks, label);
            const int orrc = hwloc_bitmap_or(
                occupied_union.data(), occupied_union.cdata(),
                results[i].cpuset.cdata()
            );
            ctu_assert(orrc == 0, "hwloc_bitmap_or() failed");
        }
        const int union_npus = npus(occupied_union);
        ctu_assert(
            union_npus > 0 && union_npus <= base_npus,
            "%s: CLOSE union has %d PUs, expected in (0, %d]",
            label.c_str(), union_npus, base_npus
        );
        backend.log(label + ": OK\n");
    }
}

} // namespace

void
run_all_split_tests(
    split_backend &backend
) {
    hwloc_topology_t topo = backend.topology();
    const qvi_hwloc_bitmap &base = backend.base_cpuset();
    const int base_npus = npus(base);
    ctu_assert(base_npus > 0, "base scope reported no PUs (%d)", base_npus);

    backend.log(
        "group_size=" + std::to_string(backend.group_size()) +
        " base_npus=" + std::to_string(base_npus) +
        " base_cpuset=" + cpuset_str(base) + "\n"
    );

    for (const int npieces : piece_counts_for(base_npus)) {
        const std::vector<qvi_hwloc_bitmap> chunks =
            expected_chunks(topo, base, npieces);

        test_explicit_contiguous(backend, npieces, chunks);
        test_explicit_arbitrary(backend, npieces, chunks);
        test_mixed_undefined(backend, npieces, chunks);
        test_all_undefined(backend, npieces);
        test_automatic(backend, npieces, chunks);
    }

    backend.log("All qv_split combinations verified successfully.\n");
}

} // namespace ctu_split

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
