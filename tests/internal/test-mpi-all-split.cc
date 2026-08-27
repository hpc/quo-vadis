/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-all-split.cc
 *
 * MPI-scope driver for the shared qv_split() verification battery. All of the
 * actual checking lives in the scope-kind-agnostic driver
 * (ctu_split::run_all_split_tests, in common-split-utils.cc); this file only
 * supplies the MPI backend, which is the single scope-kind-specific piece.
 *
 * An MPI context's calling group is the set of ranks sharing a job scope. Each
 * rank calls qv_split() with its own color and receives one subscope; the
 * backend then gathers every rank's result (opt-out flag + cpuset) to all ranks
 * so the shared driver -- which runs identically on every rank -- sees the full
 * per-task result vector and can verify both per-task placement and group-wide
 * partition invariants. Because the same driver runs on all ranks against the
 * same gathered data, its assertions agree across ranks with no extra collective
 * coordination.
 *
 * This is what exercises the multi-task mapping semantics the single-task
 * process suite cannot: inter-task coloring, the packed/spread distribution of
 * many tasks across pieces, and QV_SPLIT_UNDEFINED mixed with valid colors.
 */

// IWYU pragma: begin_keep
#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-scope.h"
// IWYU pragma: end_keep

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"
#include "common-split-utils.h"

#include <string>
#include <vector>

using namespace ctu_split;

namespace {

/** Panics on a non-success MPI return code. */
void
mpi_check(
    int rc,
    const std::string &what
) {
    if (rc != MPI_SUCCESS) {
        ctu_panic("%s (rc=%d)", what.c_str(), rc);
    }
}

// Max serialized cpuset string length gathered between ranks. hwloc taskset
// strings are compact hex; this is comfortably larger than any real topology.
constexpr int CPUSET_STR_MAX = 512;

/**
 * MPI backend: one task per rank in the job scope. do_split() has every rank
 * split with its own color, then Allgathers the per-rank results so all ranks
 * observe the same full task-result vector.
 */
struct mpi_backend : split_backend {
    MPI_Comm m_comm = MPI_COMM_NULL;
    qv_scope_t *m_base = nullptr;
    hwloc_topology_t m_topo = nullptr;
    qvi_hwloc_bitmap m_base_cpuset;
    int m_gsize = -1;
    int m_grank = -1;

    mpi_backend(MPI_Comm comm, qv_scope_t *base)
        : m_comm(comm)
        , m_base(base)
        , m_topo(base->group().hwloc().topology_get())
        , m_base_cpuset(scope_cpuset(base)) {
        check(qv_group_size(base, &m_gsize), "qv_group_size() failed");
        check(qv_group_rank(base, &m_grank), "qv_group_rank() failed");
        ctu_assert(m_gsize >= 1, "unexpected group size (%d)", m_gsize);
    }

    int group_size(void) const override { return m_gsize; }

    int my_task(void) const override { return m_grank; }

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
            static_cast<int>(colors.size()) == m_gsize,
            "%s: MPI split expects %d colors, got %zu",
            label.c_str(), m_gsize, colors.size()
        );
        // Every rank splits with its own color. qv_split() is collective over
        // the base group, so all ranks must call it here.
        qv_scope_t *sub = m_base; // non-null sentinel.
        check(
            qv_split(m_base, npieces, colors[m_grank], &sub),
            label + ": qv_split() failed"
        );

        // Local result: opt-out flag, cpuset string, and piece rank/size.
        int local_optout = 0;
        int local_prank = -1, local_psize = -1;
        std::string local_cpuset_str;
        if (sub == nullptr) {
            local_optout = 1;
        }
        else {
            const qvi_hwloc_bitmap cs = scope_cpuset(sub);
            local_cpuset_str = qvi_hwloc::bitmap_string(cs);
            ctu_assert(
                static_cast<int>(local_cpuset_str.size()) < CPUSET_STR_MAX,
                "%s: serialized cpuset too long (%zu)",
                label.c_str(), local_cpuset_str.size()
            );
            check(qv_group_rank(sub, &local_prank),
                  label + ": qv_group_rank(sub) failed");
            check(qv_group_size(sub, &local_psize),
                  label + ": qv_group_size(sub) failed");
            check(qv_free(sub), label + ": qv_free(sub) failed");
        }

        // Gather opt-out flags, piece ranks/sizes, and cpuset strings so every
        // rank can reconstruct the full task-result vector.
        std::vector<int> all_optout(m_gsize, 0);
        std::vector<int> all_prank(m_gsize, -1);
        std::vector<int> all_psize(m_gsize, -1);
        mpi_check(
            MPI_Allgather(&local_optout, 1, MPI_INT,
                          all_optout.data(), 1, MPI_INT, m_comm),
            label + ": MPI_Allgather(optout) failed"
        );
        mpi_check(
            MPI_Allgather(&local_prank, 1, MPI_INT,
                          all_prank.data(), 1, MPI_INT, m_comm),
            label + ": MPI_Allgather(prank) failed"
        );
        mpi_check(
            MPI_Allgather(&local_psize, 1, MPI_INT,
                          all_psize.data(), 1, MPI_INT, m_comm),
            label + ": MPI_Allgather(psize) failed"
        );

        std::vector<char> sendbuf(CPUSET_STR_MAX, '\0');
        std::snprintf(
            sendbuf.data(), CPUSET_STR_MAX, "%s", local_cpuset_str.c_str()
        );
        std::vector<char> recvbuf(
            static_cast<size_t>(CPUSET_STR_MAX) * m_gsize, '\0'
        );
        mpi_check(
            MPI_Allgather(sendbuf.data(), CPUSET_STR_MAX, MPI_CHAR,
                          recvbuf.data(), CPUSET_STR_MAX, MPI_CHAR, m_comm),
            label + ": MPI_Allgather(cpuset) failed"
        );

        std::vector<task_result> results(m_gsize);
        for (int i = 0; i < m_gsize; ++i) {
            task_result &r = results[i];
            if (all_optout[i]) {
                r.opted_out = true;
                continue;
            }
            r.opted_out = false;
            r.piece_rank = all_prank[i];
            r.piece_size = all_psize[i];
            char *str = &recvbuf[static_cast<size_t>(i) * CPUSET_STR_MAX];
            const int rc = qvi_hwloc::bitmap_sscanf(r.cpuset.data(), str);
            ctu_assert(
                rc == QV_SUCCESS,
                "%s: bitmap_sscanf('%s') failed for task %d",
                label.c_str(), str, i
            );
        }
        return results;
    }

    void log(const std::string &msg) override {
        // Route through the MPI-aware reporter so only rank 0 prints, ordered.
        ctu_emit(
            m_base, CTU_SCOPE_KIND_MPI, "%s", (m_grank == 0) ? msg.c_str() : ""
        );
    }
};

} // namespace

int
main(
    int argc,
    char **argv
) {
    MPI_Comm comm = MPI_COMM_WORLD;
    mpi_check(MPI_Init(&argc, &argv), "MPI_Init() failed");

    // A shared job scope: every rank sees the same base resources and hence the
    // same npieces-way chunking.
    qv_scope_t *base_scope = nullptr;
    check(
        qv_mpi_scope(comm, QV_SCOPE_JOB, QV_SCOPE_FLAG_NONE, &base_scope),
        "qv_mpi_scope(QV_SCOPE_JOB) failed"
    );

    ctu_emit_scope_report(base_scope, CTU_SCOPE_KIND_MPI, "     base_scope");

    mpi_backend backend(comm, base_scope);
    run_all_split_tests(backend);

    check(qv_free(base_scope), "qv_free(base_scope) failed");

    MPI_Finalize();
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
