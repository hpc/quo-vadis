/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-process-all-split.cc
 *
 * Exercises and verifies the major combinations of calls to qv_split() using
 * both automatic and explicit (user-defined) groupings.
 *
 * The public qv_split() entry point is the code under test. Verification,
 * however, is done against the resulting scopes' *internal* cpusets
 * (scope->hwpool().cpuset()) using hwloc bitmap operations. This is far more
 * robust than parsing bind-string output: it compares the actual resource
 * bitmaps directly for identity, subset, disjointness, and union.
 *
 * This test runs in a process context, so the calling group comprises a single
 * task (group size 1). That is intentional: it lets us reason about the split
 * results deterministically without the noise of inter-task coloring, while
 * still covering every accepted form of the group_id argument.
 *
 * The library carves a scope's resources by splitting the base scope's PU list,
 * enumerated in hwloc order, into |npieces| contiguous, ordered chunks (see
 * qvi_hwloc::bitmap_split): chunk i receives floor(npus/npieces) PUs, and the
 * first (npus % npieces) chunks receive one extra PU. This test recomputes
 * those expected chunk cpusets independently (by enumerating PUs directly from
 * the hwloc topology) and asserts the observed subscopes match.
 *
 * The accepted forms of the group_id argument are:
 *
 *   1. Explicit, contiguous coloring in the range [0, npieces): the caller
 *      lands in exactly the piece named by its color, i.e. the color-th chunk.
 *      Sweeping every color yields a disjoint partition whose union is exactly
 *      the base scope.
 *
 *   2. Explicit, arbitrary positive coloring: the caller may provide any
 *      non-negative color value. The library clamps the distinct colors into a
 *      usable [0, ndistinct) range, preserving order (smallest distinct color
 *      -> piece 0, and so on), with the invariant ndistinct <= npieces. We
 *      verify the clamping function directly over multi-color vectors that
 *      exercise every chunk index (not just the first), then confirm the
 *      end-to-end single-task split lands in the chunk the clamp selects.
 *
 *   3. QV_SPLIT_UNDEFINED: part of the explicit, user-defined coloring (not an
 *      automatic grouping constant). The caller opts out and receives no
 *      subscope: the split succeeds but the returned scope pointer is NULL.
 *      Passing it as the sole color here is an all-opted-out user-defined
 *      split: unusual but valid.
 *
 *   4. Automatic grouping constants QV_SPLIT_CLOSE, QV_SPLIT_PACKED,
 *      QV_SPLIT_SPREAD, and QV_SPLIT_AUTO. With a single task the destination
 *      piece for PACKED/SPREAD/CLOSE is an internal mapping/affinity decision,
 *      so each must land in *exactly one* valid chunk of the |npieces|-way
 *      split. AUTO clamps the split size down to the group size (1), so the
 *      task receives the entire base scope.
 */

// IWYU pragma: begin_keep
#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-map.h"
#include "qvi-scope.h"
#include "qvi-utils.h"
// IWYU pragma: end_keep

#include "quo-vadis.h"
#include "common-test-utils.h"

#include <set>
#include <string>
#include <vector>

namespace {

/**
 * Convenience wrapper that panics on a non-success return code.
 */
void
check(
    int rc,
    const std::string &what
) {
    if (rc != QV_SUCCESS) {
        ctu_panic("%s (rc=%s)", what.c_str(), qv_strerr(rc));
    }
}

/**
 * Returns a copy of the scope's underlying resource cpuset. This reaches into
 * the internal scope representation, which is exactly why this test lives under
 * tests/internal: it can verify the true mapping directly rather than parsing
 * bind-string output.
 */
qvi_hwloc_bitmap
scope_cpuset(
    qv_scope_t *scope
) {
    return qvi_hwloc_bitmap(scope->hwpool().cpuset().cdata());
}

/**
 * Renders a cpuset as an hwloc "list" string for diagnostics.
 */
std::string
cpuset_str(
    const qvi_hwloc_bitmap &bitmap
) {
    return qvi_hwloc::bitmap_list_string(bitmap.cdata());
}

/**
 * Verifies a freshly split subscope's group properties. In a process context
 * every subscope's group is a single task, so its size must be 1 and the
 * caller's rank within it must be 0.
 */
void
verify_group_invariants(
    qv_scope_t *subscope,
    const std::string &label
) {
    int gsize = -1;
    check(qv_group_size(subscope, &gsize), label + ": qv_group_size() failed");
    ctu_assert(
        gsize == 1,
        "%s: unexpected group size (expected 1, got %d)",
        label.c_str(), gsize
    );

    int grank = -1;
    check(qv_group_rank(subscope, &grank), label + ": qv_group_rank() failed");
    ctu_assert(
        grank == 0,
        "%s: unexpected group rank (expected 0, got %d)",
        label.c_str(), grank
    );
}

/**
 * Independently recomputes the expected contiguous chunks of an |npieces|-way
 * split of |base|, mirroring the specification of qvi_hwloc::bitmap_split. PUs
 * are enumerated in hwloc order (the order the library consumes them), then
 * partitioned into |npieces| contiguous chunks: chunk i holds
 * floor(N/npieces) PUs, and the first N%npieces chunks hold one extra PU.
 * Returns |npieces| cpusets.
 */
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

/**
 * Splits |base| into |npieces| using |group_id|, verifies the resulting
 * subscope's group invariants, and returns its cpuset. The returned cpuset is
 * also confirmed to be a subset of |base_cpuset|.
 */
qvi_hwloc_bitmap
split_and_get_cpuset(
    qv_scope_t *base,
    int npieces,
    int group_id,
    const qvi_hwloc_bitmap &base_cpuset,
    const std::string &label
) {
    qv_scope_t *subscope = nullptr;
    check(
        qv_split(base, npieces, group_id, &subscope),
        label + ": qv_split() failed"
    );
    verify_group_invariants(subscope, label);

    const qvi_hwloc_bitmap cpuset = scope_cpuset(subscope);
    // Every resource a child receives must come from the parent.
    ctu_assert(
        hwloc_bitmap_isincluded(cpuset.cdata(), base_cpuset.cdata()),
        "%s: subscope cpuset '%s' is not a subset of the base cpuset '%s'",
        label.c_str(), cpuset_str(cpuset).c_str(),
        cpuset_str(base_cpuset).c_str()
    );

    ctu_logf(
        "%-40s npieces=%d group_id=%d -> cpuset=%s\n",
        label.c_str(), npieces, group_id, cpuset_str(cpuset).c_str()
    );

    check(qv_free(subscope), label + ": qv_free(subscope) failed");
    return cpuset;
}

/**
 * Asserts two cpusets are identical, emitting a helpful diagnostic otherwise.
 */
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

/**
 * Verifies the color-clamping invariant used by explicit, arbitrary-positive
 * colorings against qvi_map_clamp_colors (the routine qv_split() relies on to
 * fold caller-provided colors into a usable [0, ndistinct) range).
 *
 * The invariant is that, given a vector of arbitrary non-negative colors:
 *   - The set of distinct clamped values is exactly [0, ndistinct), where
 *     ndistinct is the number of distinct input colors (the "n-distinct colors"
 *     invariant).
 *   - Order is preserved: the clamped value of a color equals that color's rank
 *     among the sorted distinct input colors (rank 0 for the smallest, etc.).
 *   - Equal input colors always map to equal clamped values, and distinct input
 *     colors always map to distinct clamped values.
 *
 * This checks *all* resulting chunk indices, not just the first, so a mapping
 * that happened to place one color correctly while mishandling the rest would
 * still be caught.
 */
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

    // Rank each distinct input color among the sorted distinct colors.
    const std::set<int> distinct(colors.begin(), colors.end());
    std::map<int, int> expected_rank;
    int rank = 0;
    for (const int c : distinct) expected_rank[c] = rank++;
    const int ndistinct = static_cast<int>(distinct.size());

    // The distinct clamped values must be exactly [0, ndistinct).
    const std::set<int> clamped_distinct(clamped.begin(), clamped.end());
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

    // Every element must map to its color's rank, preserving order and equality.
    for (size_t i = 0; i < colors.size(); ++i) {
        ctu_assert(
            clamped[i] == expected_rank[colors[i]],
            "%s: color %d clamped to %d, expected rank %d",
            label.c_str(), colors[i], clamped[i], expected_rank[colors[i]]
        );
    }
}

} // namespace

int
main(void)
{
    // Obtain a base scope over all resources allowed to this process. In a
    // process context the group is just this one task.
    qv_scope_t *base_scope = nullptr;
    check(
        qv_process_scope(QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &base_scope),
        "qv_process_scope(QV_SCOPE_USER) failed"
    );

    ctu_emit_scope_report(
        base_scope, CTU_SCOPE_KIND_PROCESS, "     base_scope"
    );

    // A process context must have a group size of exactly one.
    int base_gsize = -1;
    check(qv_group_size(base_scope, &base_gsize), "qv_group_size() failed");
    ctu_assert(
        base_gsize == 1,
        "unexpected base group size (expected 1, got %d)", base_gsize
    );

    // Capture the base scope's exact cpuset and the hwloc topology. All child
    // scopes are verified against this ground truth.
    hwloc_topology_t topo = base_scope->group().hwloc().topology_get();
    const qvi_hwloc_bitmap base_cpuset = scope_cpuset(base_scope);
    const int base_npus = hwloc_bitmap_weight(base_cpuset.cdata());
    ctu_assert(
        base_npus > 0, "base scope reported no PUs (got %d)", base_npus
    );
    ctu_logf(
        "base_scope has %d PUs: %s\n",
        base_npus, cpuset_str(base_cpuset).c_str()
    );

    // Piece counts we exercise. Skip any that exceed the number of PUs, since
    // the library cannot carve more pieces than there are PUs.
    std::vector<int> piece_counts;
    for (int p : {1, 2, 3, 4}) {
        if (p <= base_npus) piece_counts.push_back(p);
    }

    ////////////////////////////////////////////////////////////////////////
    // 1. Explicit, contiguous coloring: group_id in [0, npieces).
    //
    //    For each npieces we sweep every color 0..npieces-1 and require that
    //    color i yields *exactly* the i-th contiguous chunk of the base's PU
    //    list. Collectively the pieces must form a disjoint partition whose
    //    union is exactly the base scope. This verifies both identity and
    //    relative location of every PU, not just counts.
    ////////////////////////////////////////////////////////////////////////
    for (const int npieces : piece_counts) {
        const std::vector<qvi_hwloc_bitmap> want =
            expected_chunks(topo, base_cpuset, npieces);

        qvi_hwloc_bitmap union_of_pieces;
        for (int color = 0; color < npieces; ++color) {
            const std::string label =
                "explicit-contiguous[" + std::to_string(npieces) +
                "]/color=" + std::to_string(color);
            const qvi_hwloc_bitmap got = split_and_get_cpuset(
                base_scope, npieces, color, base_cpuset, label
            );
            // Color i must land in exactly the i-th chunk.
            assert_same_cpuset(got, want[color], label);
            // Pieces must be pairwise disjoint.
            ctu_assert(
                !hwloc_bitmap_intersects(union_of_pieces.cdata(), got.cdata()),
                "%s: piece overlaps a previously seen piece", label.c_str()
            );
            const int orrc = hwloc_bitmap_or(
                union_of_pieces.data(), union_of_pieces.cdata(), got.cdata()
            );
            ctu_assert(orrc == 0, "hwloc_bitmap_or() failed");
        }
        // The union of all pieces must be exactly the base scope.
        assert_same_cpuset(
            union_of_pieces, base_cpuset,
            "explicit-contiguous[" + std::to_string(npieces) + "]/union"
        );
    }

    ////////////////////////////////////////////////////////////////////////
    // 2. Explicit, arbitrary positive coloring.
    //
    //    The library clamps arbitrary non-negative colors into a usable
    //    [0, ndistinct) range via qvi_map_clamp_colors, preserving order:
    //    the smallest distinct color becomes piece 0, the next piece 1, and so
    //    on, with the invariant that ndistinct <= npieces.
    //
    //    We verify this in two complementary ways:
    //
    //    (a) The clamping function itself, over representative multi-color
    //        vectors that exercise *every* chunk index (not just the first):
    //        the number of distinct clamped colors must equal the number of
    //        distinct inputs, the clamped values must densely fill [0, ndistinct)
    //        and preserve the input color ordering.
    //
    //    (b) The end-to-end single-task split: a process context presents one
    //        color per split call, which necessarily clamps to piece 0, so the
    //        caller must receive exactly the chunk selected by the clamped color.
    ////////////////////////////////////////////////////////////////////////
    for (const int npieces : piece_counts) {
        const std::vector<qvi_hwloc_bitmap> chunks =
            expected_chunks(topo, base_cpuset, npieces);

        // (a) Verify the clamping invariant directly, using color vectors whose
        //     distinct-color count spans 1..npieces so every chunk index is
        //     exercised. Colors are deliberately arbitrary (large, unsorted,
        //     with repeats) to stress the sort/rank/dedup logic.
        for (int ndistinct = 1; ndistinct <= npieces; ++ndistinct) {
            // Build a vector of |npieces| entries drawing from |ndistinct|
            // arbitrary color values (repeating to fill), then verify that the
            // clamp maps them onto exactly the chunk indices [0, ndistinct).
            const std::vector<int> palette = {73, 5, 1000, 42, 8, 900};
            std::vector<int> colors;
            for (int i = 0; i < npieces; ++i) {
                colors.push_back(palette[i % ndistinct]);
            }
            const std::string label =
                "explicit-arbitrary[" + std::to_string(npieces) +
                "]/clamp/ndistinct=" + std::to_string(ndistinct);
            verify_clamp_invariant(colors, label);

            // The clamped colors index into the chunks; confirm each element
            // selects a real, in-range chunk and that the mapping is a proper
            // surjection onto [0, ndistinct).
            const std::vector<int> clamped = qvi_map_clamp_colors(colors);
            std::set<int> hit;
            for (const int cv : clamped) {
                ctu_assert(
                    cv >= 0 && cv < static_cast<int>(chunks.size()),
                    "%s: clamped color %d out of chunk range [0,%zu)",
                    label.c_str(), cv, chunks.size()
                );
                hit.insert(cv);
            }
            ctu_assert(
                static_cast<int>(hit.size()) == ndistinct,
                "%s: clamped coloring covered %zu chunks, expected %d",
                label.c_str(), hit.size(), ndistinct
            );
        }

        // (b) End-to-end: each single arbitrary color clamps to piece 0, so the
        //     received cpuset must be exactly the chunk the clamp selects (0).
        for (const int color : {7, 42, 1000, npieces * 100}) {
            const std::vector<int> clamped = qvi_map_clamp_colors({color});
            const qvi_hwloc_bitmap &want = chunks[clamped.front()];
            const std::string label =
                "explicit-arbitrary[" + std::to_string(npieces) +
                "]/color=" + std::to_string(color);
            const qvi_hwloc_bitmap got = split_and_get_cpuset(
                base_scope, npieces, color, base_cpuset, label
            );
            assert_same_cpuset(got, want, label);
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // 3. QV_SPLIT_UNDEFINED: caller opts out and receives no subscope. The
    //    split succeeds but *subscope is set to NULL to signal the opt-out.
    //    This is part of the explicit, user-defined coloring, not an automatic
    //    mode.
    ////////////////////////////////////////////////////////////////////////
    for (const int npieces : piece_counts) {
        const std::string label =
            "undefined[" + std::to_string(npieces) + "]";
        // Prime with a non-null sentinel to confirm the call actively clears it.
        qv_scope_t *subscope = base_scope;
        check(
            qv_split(base_scope, npieces, QV_SPLIT_UNDEFINED, &subscope),
            label + ": qv_split() failed"
        );
        ctu_assert(
            subscope == nullptr,
            "%s: expected a NULL subscope for QV_SPLIT_UNDEFINED, got %p",
            label.c_str(), static_cast<const void *>(subscope)
        );
        ctu_logf(
            "%-40s npieces=%d group_id=%d -> subscope=(null)\n",
            label.c_str(), npieces, QV_SPLIT_UNDEFINED
        );
    }

    ////////////////////////////////////////////////////////////////////////
    // 4. Automatic grouping: QV_SPLIT_CLOSE, QV_SPLIT_PACKED, QV_SPLIT_SPREAD,
    //    and QV_SPLIT_AUTO.
    //
    //    For PACKED/SPREAD/CLOSE the exact destination piece a single task
    //    lands in is an internal mapping/affinity decision, so we do not pin it
    //    to a fixed index. We do, however, require the received cpuset to be
    //    *exactly one* of the valid npieces-way chunks: correct resources and
    //    aligned to a real chunk boundary (not an arbitrary cpuset that merely
    //    has the right PU count). AUTO is fully deterministic here: it clamps
    //    the split size to the group size (1), so the task must receive the
    //    entire base scope.
    ////////////////////////////////////////////////////////////////////////
    for (const int npieces : piece_counts) {
        const std::vector<qvi_hwloc_bitmap> chunks =
            expected_chunks(topo, base_cpuset, npieces);

        // Requires |got| to equal exactly one chunk of the npieces-way split,
        // returning the matched chunk index for reporting.
        auto require_aligned_chunk = [&](
            const qvi_hwloc_bitmap &got, const std::string &label
        ) -> int {
            for (size_t i = 0; i < chunks.size(); ++i) {
                if (got == chunks[i]) return static_cast<int>(i);
            }
            ctu_panic(
                "%s: cpuset does not match any chunk of the %d-way split\n"
                "  got: %s",
                label.c_str(), npieces, cpuset_str(got).c_str()
            );
            return -1; // Unreachable.
        };

        // PACKED: single task lands in one valid, chunk-aligned piece.
        {
            const std::string label =
                "auto/QV_SPLIT_PACKED[" + std::to_string(npieces) + "]";
            const qvi_hwloc_bitmap got = split_and_get_cpuset(
                base_scope, npieces, QV_SPLIT_PACKED, base_cpuset, label
            );
            const int idx = require_aligned_chunk(got, label);
            ctu_logf("%-40s -> chunk %d/%d\n", label.c_str(), idx, npieces);
        }

        // SPREAD: single task lands in one valid, chunk-aligned piece.
        {
            const std::string label =
                "auto/QV_SPLIT_SPREAD[" + std::to_string(npieces) + "]";
            const qvi_hwloc_bitmap got = split_and_get_cpuset(
                base_scope, npieces, QV_SPLIT_SPREAD, base_cpuset, label
            );
            const int idx = require_aligned_chunk(got, label);
            ctu_logf("%-40s -> chunk %d/%d\n", label.c_str(), idx, npieces);
        }

        // AUTO: clamps the split size down to the group size (1), so the task
        // receives the *entire* base scope regardless of npieces.
        {
            const std::string label =
                "auto/QV_SPLIT_AUTO[" + std::to_string(npieces) + "]";
            const qvi_hwloc_bitmap got = split_and_get_cpuset(
                base_scope, npieces, QV_SPLIT_AUTO, base_cpuset, label
            );
            assert_same_cpuset(got, base_cpuset, label);
        }

        // CLOSE: affinity-driven, so we do not pin it to a specific index; it
        // must still land in exactly one valid, chunk-aligned piece.
        {
            const std::string label =
                "auto/QV_SPLIT_CLOSE[" + std::to_string(npieces) + "]";
            const qvi_hwloc_bitmap got = split_and_get_cpuset(
                base_scope, npieces, QV_SPLIT_CLOSE, base_cpuset, label
            );
            const int idx = require_aligned_chunk(got, label);
            ctu_logf("%-40s -> chunk %d/%d\n", label.c_str(), idx, npieces);
        }
    }

    check(qv_free(base_scope), "qv_free(base_scope) failed");

    ctu_logf("All qv_split combinations verified successfully.\n");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
