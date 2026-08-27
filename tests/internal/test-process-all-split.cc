/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-process-all-split.cc
 *
 * Process-scope driver for the shared qv_split() verification battery. All of
 * the actual checking lives in the scope-kind-agnostic driver
 * (ctu_split::run_all_split_tests, in common-split-utils.cc); this file only
 * supplies the process backend, which is the single scope-kind-specific piece.
 *
 * A process context has a group size of exactly one, so every split call is
 * made by the single calling task and yields one subscope. The backend below
 * therefore performs one qv_split() and returns a one-element result vector.
 * The driver's contiguous/arbitrary/undefined/automatic coverage is identical
 * to the MPI and thread suites; see common-split-utils.h for the design.
 */

// IWYU pragma: begin_keep
#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-scope.h"
// IWYU pragma: end_keep

#include "quo-vadis.h"
#include "common-test-utils.h"
#include "common-split-utils.h"

#include <string>
#include <vector>

using namespace ctu_split;

namespace {

/**
 * Process backend: the calling group is a single task, so each split is one
 * qv_split() call producing one subscope.
 */
struct process_backend : split_backend {
    qv_scope_t *m_base = nullptr;
    hwloc_topology_t m_topo = nullptr;
    qvi_hwloc_bitmap m_base_cpuset;

    explicit process_backend(qv_scope_t *base)
        : m_base(base)
        , m_topo(base->group().hwloc().topology_get())
        , m_base_cpuset(scope_cpuset(base)) { }

    int group_size(void) const override { return 1; }

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
            colors.size() == 1,
            "%s: process split expects 1 color, got %zu",
            label.c_str(), colors.size()
        );
        qv_scope_t *sub = m_base; // non-null sentinel to confirm clearing.
        check(
            qv_split(m_base, npieces, colors[0], &sub),
            label + ": qv_split() failed"
        );

        task_result r;
        if (sub == nullptr) {
            r.opted_out = true;
        }
        else {
            r.opted_out = false;
            r.cpuset = scope_cpuset(sub);
            check(qv_group_rank(sub, &r.piece_rank),
                  label + ": qv_group_rank(sub) failed");
            check(qv_group_size(sub, &r.piece_size),
                  label + ": qv_group_size(sub) failed");
            check(qv_free(sub), label + ": qv_free(sub) failed");
        }
        return {r};
    }

    void log(const std::string &msg) override {
        ctu_logf("%s", msg.c_str());
    }
};

} // namespace

int
main(void)
{
    // A base scope over all resources allowed to this process; the group is
    // just this one task.
    qv_scope_t *base_scope = nullptr;
    check(
        qv_process_scope(QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &base_scope),
        "qv_process_scope(QV_SCOPE_USER) failed"
    );

    ctu_emit_scope_report(
        base_scope, CTU_SCOPE_KIND_PROCESS, "     base_scope"
    );

    int base_gsize = -1;
    check(qv_group_size(base_scope, &base_gsize), "qv_group_size() failed");
    ctu_assert(
        base_gsize == 1,
        "unexpected base group size (expected 1, got %d)", base_gsize
    );

    process_backend backend(base_scope);
    run_all_split_tests(backend);

    check(qv_free(base_scope), "qv_free(base_scope) failed");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
