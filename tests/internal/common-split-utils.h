/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file common-split-utils.h
 *
 * Shared verification harness for the qv_split() family of tests. These
 * routines reach into the *internal* scope representation
 * (scope->hwpool().cpuset()) and reuse the library's own mapping primitives
 * (qvi_map_*), which is exactly why they live under tests/internal: they can
 * verify the true resource mapping directly rather than parsing bind-string
 * output.
 *
 * The design mirrors tests/benchmarks: the overwhelming majority of the split
 * verification is scope-kind-agnostic, so it is written *once* in the shared
 * driver (run_all_split_tests). The only thing that differs between the
 * process, thread, and MPI suites is *how* a split is carried out and how each
 * task's result is obtained -- captured by the split_backend abstraction. Each
 * suite supplies a small backend and calls the shared driver; the actual
 * checks (exact chunk placement, clamp semantics, opt-out handling, partition
 * unions, automatic-grouping prediction) are identical across all three.
 *
 * A "task" here is one member of the split's calling group:
 *   - process: exactly one task (the calling process),
 *   - MPI:     one task per rank in the job scope,
 *   - thread:  one task per requested subscope (qv_thread_split produces k of
 *              them from a single call).
 * The underlying library machinery (qvi_hwsplit) is shared by all three, so a
 * color vector maps to pieces identically regardless of context; that is what
 * makes a single uniform driver correct for every suite.
 */

#ifndef COMMON_SPLIT_UTILS_H
#define COMMON_SPLIT_UTILS_H

// IWYU pragma: begin_keep
#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-map.h"
#include "qvi-scope.h"
#include "qvi-utils.h"
// IWYU pragma: end_keep

#include "quo-vadis.h"

#include <string>
#include <vector>

namespace ctu_split {

/**
 * Convenience wrapper that panics on a non-success return code.
 */
void
check(
    int rc,
    const std::string &what
);

/**
 * Returns a copy of the scope's underlying resource cpuset. This reaches into
 * the internal scope representation to verify the true mapping directly.
 */
qvi_hwloc_bitmap
scope_cpuset(
    qv_scope_t *scope
);

/**
 * Renders a cpuset as an hwloc "list" string for diagnostics.
 */
std::string
cpuset_str(
    const qvi_hwloc_bitmap &bitmap
);

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
);

/**
 * Asserts two cpusets are identical, emitting a helpful diagnostic otherwise.
 */
void
assert_same_cpuset(
    const qvi_hwloc_bitmap &got,
    const qvi_hwloc_bitmap &want,
    const std::string &label
);

/**
 * Asserts |got| equals exactly one chunk of the |chunks| split and returns the
 * matched chunk index. A cpuset that merely has the right PU *count* but is not
 * aligned to a real chunk boundary is rejected.
 */
int
require_aligned_chunk(
    const qvi_hwloc_bitmap &got,
    const std::vector<qvi_hwloc_bitmap> &chunks,
    const std::string &label
);

/**
 * Verifies the color-clamping invariant used by explicit colorings against
 * qvi_map_clamp_colors (the routine qv_split() relies on to fold
 * caller-provided colors into a usable [0, ndistinct) range).
 *
 * The invariant, given a vector of colors that are each either a non-negative
 * value or the QV_SPLIT_UNDEFINED opt-out sentinel:
 *   - QV_SPLIT_UNDEFINED entries are excluded from the distinct-color ranking
 *     and pass through the clamp unchanged (they occupy no piece).
 *   - The set of distinct clamped values among the non-undefined entries is
 *     exactly [0, ndistinct), where ndistinct is the number of distinct
 *     non-undefined input colors.
 *   - Order is preserved: the clamped value of a color equals that color's rank
 *     among the sorted distinct non-undefined input colors.
 *   - Equal input colors always map to equal clamped values, and distinct input
 *     colors always map to distinct clamped values.
 *
 * This checks *all* resulting chunk indices, not just the first, so a mapping
 * that happened to place one color correctly while mishandling the rest would
 * still be caught. Handles both pure-valid-color vectors and vectors that mix
 * QV_SPLIT_UNDEFINED in with valid colors.
 */
void
verify_clamp_invariant(
    const std::vector<int> &colors,
    const std::string &label
);

/**
 * Predicts, using the library's own mapping primitives, the chunk index that
 * task |taskid| (of |group_size| tasks) lands in for an |npieces|-way split
 * requested with the automatic grouping constant |split_kind| (one of
 * QV_SPLIT_PACKED, QV_SPLIT_SPREAD, or QV_SPLIT_AUTO). Reusing qvi_map_packed /
 * qvi_map_spread here means the test checks the *exact* destination piece each
 * task receives, not merely that it landed in some valid chunk.
 *
 * QV_SPLIT_CLOSE is intentionally excluded: it is affinity-driven and thus not
 * deterministically predictable from task/piece counts alone.
 */
int
predict_auto_chunk(
    int split_kind,
    int npieces,
    int group_size,
    int taskid
);

/**
 * The outcome of a single task's participation in a split.
 *
 * A split of a group of G tasks yields one of these per task. Every backend
 * gathers the full G-length vector so the shared driver can verify per-task
 * placement *and* group-wide invariants (disjoint partition, coverage) without
 * knowing anything about the underlying scope kind.
 */
struct task_result {
    /** True if the task opted out (QV_SPLIT_UNDEFINED) and got no subscope. */
    bool opted_out = false;
    /** The task's resulting cpuset. Valid only when !opted_out. */
    qvi_hwloc_bitmap cpuset;
    /** The task's rank within its resulting subscope, or -1 if opted out. */
    int piece_rank = -1;
    /** The size of the task's resulting subscope's group, or -1 if opted out. */
    int piece_size = -1;
};

/**
 * Scope-kind-specific backend for the shared split-test driver.
 *
 * A backend knows how to run a single npieces-way split over a base scope using
 * a caller-provided per-task color vector, and to report every task's result
 * (gathered on all participants). Everything else -- the actual verification --
 * is shared. This is the sole extension point that differs between the process,
 * thread, and MPI suites, exactly analogous to qvb_backend_t in the benchmarks.
 */
struct split_backend {
    virtual ~split_backend(void) = default;
    /** Number of tasks in the calling group (1 for process). */
    virtual int group_size(void) const = 0;
    /**
     * This participant's own task id in [0, group_size()). For process/thread
     * there is a single participant driving the split, so this is 0; for MPI it
     * is the caller's rank. Used only to label per-participant diagnostics.
     */
    virtual int my_task(void) const = 0;
    /** The shared hwloc topology backing the base scope. */
    virtual hwloc_topology_t topology(void) const = 0;
    /** The base scope's cpuset (identical for every task in the group). */
    virtual const qvi_hwloc_bitmap &base_cpuset(void) const = 0;
    /**
     * Performs an npieces-way split of the base scope with the given full,
     * group_size()-length color vector, and returns the gathered result for
     * *every* task in the group. |colors[i]| is the color task i requested
     * (possibly QV_SPLIT_UNDEFINED or an automatic-grouping constant). The
     * returned vector has exactly group_size() entries.
     *
     * Implementations must ensure every participant returns identical results
     * (e.g. MPI backends gather across ranks) so the shared driver's collective
     * assertions agree on all participants.
     */
    virtual std::vector<task_result>
    do_split(
        int npieces,
        const std::vector<int> &colors,
        const std::string &label
    ) = 0;
    /** Emits a diagnostic line from a single designated participant only. */
    virtual void
    log(
        const std::string &msg
    ) = 0;
};

/**
 * The shared, scope-kind-agnostic split-test driver. Runs the full battery of
 * qv_split() verifications (explicit contiguous coloring, explicit arbitrary
 * coloring, QV_SPLIT_UNDEFINED alone and mixed with valid colors, and the
 * automatic groupings PACKED/SPREAD/AUTO/CLOSE) against whatever backend is
 * supplied. Identical for the process, thread, and MPI suites.
 */
void
run_all_split_tests(
    split_backend &backend
);

} // namespace ctu_split

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
