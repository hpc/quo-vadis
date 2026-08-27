/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-thread-all-split.cc
 *
 * Thread-scope driver for the shared qv_split() verification battery. All of
 * the actual checking lives in the scope-kind-agnostic driver
 * (ctu_split::run_all_split_tests, in common-split-utils.cc); this file only
 * supplies the thread backend, which is the single scope-kind-specific piece.
 *
 * Unlike the process and MPI suites -- where each task calls qv_split()
 * individually and receives one subscope -- the thread API (qv_thread_split)
 * produces *all* k subscopes from a single call by the host thread, given a
 * k-length color array. That maps naturally onto the driver's "one result per
 * task, all gathered" model: the backend runs the split once and returns the k
 * results directly, no cross-instance communication needed.
 *
 * Two semantic notes handled by the backend so the driver stays uniform:
 *   - qv_thread_split takes a real int[] color array; the automatic groupings
 *     (PACKED/SPREAD/AUTO/CLOSE) and QV_SPLIT_UNDEFINED are expressed as the
 *     corresponding QV_SPLIT_* values in that array, which the shared internal
 *     machinery interprets identically to the process/MPI paths.
 *   - An opted-out task (QV_SPLIT_UNDEFINED) yields, in the thread API, a
 *     subscope with an *empty* cpuset rather than the NULL subscope the
 *     process/MPI split returns. The backend detects the empty cpuset and
 *     reports it as opted_out, so the driver sees identical results everywhere.
 */

// IWYU pragma: begin_keep
#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-scope.h"
// IWYU pragma: end_keep

#include "quo-vadis.h"
#include "quo-vadis-thread.h"
#include "common-test-utils.h"
#include "common-split-utils.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace ctu_split;

namespace {

/**
 * Thread backend: a single host thread splits its process scope into |m_k|
 * thread subscopes in one qv_thread_split() call. The k subscopes are the
 * "tasks" the driver reasons about.
 */
struct thread_backend : split_backend {
    qv_scope_t *m_base = nullptr;
    hwloc_topology_t m_topo = nullptr;
    qvi_hwloc_bitmap m_base_cpuset;
    int m_k = 0;

    thread_backend(qv_scope_t *base, int k)
        : m_base(base)
        , m_topo(base->group().hwloc().topology_get())
        , m_base_cpuset(scope_cpuset(base))
        , m_k(k) {
        ctu_assert(m_k >= 1, "thread backend needs k >= 1 (got %d)", m_k);
    }

    int group_size(void) const override { return m_k; }

    int my_task(void) const override { return 0; }

    hwloc_topology_t topology(void) const override { return m_topo; }

    const qvi_hwloc_bitmap &base_cpuset(void) const override {
        return m_base_cpuset;
    }

    std::vector<task_result>
    do_split(
        int npieces,
        const std::vector<int> &colors,
        const std::string &label
    ) override {
        ctu_assert(
            static_cast<int>(colors.size()) == m_k,
            "%s: thread split expects %d colors, got %zu",
            label.c_str(), m_k, colors.size()
        );
        // qv_thread_split takes a mutable int[] color array of length k and
        // produces k subscopes. The QV_SPLIT_* values in |colors| (explicit
        // colors, QV_SPLIT_UNDEFINED, or the automatic-grouping constants) flow
        // straight through to the same internal split machinery.
        std::vector<int> kcolors(colors.begin(), colors.end());
        qv_scope_t **subs = nullptr;
        check(
            qv_thread_split(m_base, npieces, kcolors.data(), m_k, &subs),
            label + ": qv_thread_split() failed"
        );
        ctu_assert(subs != nullptr, "%s: null subscope array", label.c_str());

        std::vector<task_result> results(m_k);
        for (int i = 0; i < m_k; ++i) {
            ctu_assert(
                subs[i] != nullptr,
                "%s: task %d subscope is null", label.c_str(), i
            );
            const qvi_hwloc_bitmap cs = scope_cpuset(subs[i]);
            task_result &r = results[i];
            if (hwloc_bitmap_iszero(cs.cdata())) {
                // An opted-out (QV_SPLIT_UNDEFINED) task gets an empty subscope
                // in the thread API; normalize it to the driver's opt-out form.
                r.opted_out = true;
            }
            else {
                r.opted_out = false;
                r.cpuset = cs;
                check(qv_group_rank(subs[i], &r.piece_rank),
                      label + ": qv_group_rank(sub) failed");
                check(qv_group_size(subs[i], &r.piece_size),
                      label + ": qv_group_size(sub) failed");
            }
        }
        check(qv_thread_free(m_k, subs), label + ": qv_thread_free() failed");
        return results;
    }

    void log(const std::string &msg) override {
        ctu_logf("%s", msg.c_str());
    }
};

} // namespace

int
main(void)
{
    // A base process scope to split across threads.
    qv_scope_t *base_scope = nullptr;
    check(
        qv_process_scope(QV_SCOPE_PROCESS, QV_SCOPE_FLAG_NONE, &base_scope),
        "qv_process_scope(QV_SCOPE_PROCESS) failed"
    );

    ctu_emit_scope_report(
        base_scope, CTU_SCOPE_KIND_PROCESS, "     base_scope"
    );

    // Choose the number of thread tasks. Use the core count so the split has
    // several tasks to distribute (exercising the multi-task mapping the
    // process suite cannot), but cap it so colorings stay small and fast.
    int ncores = 0;
    check(
        qv_hw_obj_count(base_scope, QV_HW_OBJ_CORE, &ncores),
        "qv_hw_obj_count(QV_HW_OBJ_CORE) failed"
    );
    const int k = std::max(1, std::min(ncores, 8));

    thread_backend backend(base_scope, k);
    run_all_split_tests(backend);

    check(qv_free(base_scope), "qv_free(base_scope) failed");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
