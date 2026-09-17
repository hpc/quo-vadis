/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c)      2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvb-mpi-msg-rate.cc
 *
 * Closed-loop message-rate (throughput) benchmark for quo-vadisd, measured
 * through the public quo-vadis API so that the reported numbers reflect the
 * effective message rate a real user should expect.
 *
 * Concurrency is launcher-driven: each MPI process is an independent client
 * (its own qv_mpi_scope, hence its own connection to quo-vadisd), so the
 * number of concurrent clients N equals the communicator size chosen with
 * `mpiexec -n N`. Sweep the client count by re-running the binary at several
 * process counts.
 *
 * The driven primitive is the qv_bind_push()/qv_bind_pop() pair. On the daemon
 * path each of these is exactly one synchronous RMI round-trip, so each timed
 * iteration issues exactly two daemon messages. The pair is kept balanced every
 * iteration to preserve the bind stack. (Note: many public calls such as
 * qv_hw_count, qv_group_size, qv_bind_string, and qv_barrier are served
 * from the client-local hwloc cache and do NOT contact the daemon; they are
 * therefore unsuitable for a message-rate measurement.)
 *
 * Methodology (standard closed-loop server benchmark):
 *   - Every process connects once, warms up (untimed), then all processes align
 *     on a barrier before opening a fixed measurement window (either wall-clock
 *     duration or a fixed request count). A closing barrier bounds the window
 *     so the per-process windows overlap and true concurrency is measured.
 *   - Per-message round-trip latencies are accumulated into a fixed-bucket
 *     log-spaced histogram (bounded memory) for percentile estimation.
 *   - Results are reduced across ALL processes with MPI collectives; only the
 *     root process prints. Aggregate throughput = sum(messages) / max(window);
 *     this is the headline message rate at the given concurrency.
 *   - With --trials k the whole measurement is repeated and the root process
 *     reports the mean +/- sample standard deviation (and 95% CI) of the
 *     throughput.
 *
 * A quo-vadisd must be reachable (see docs/mpi-msg-rate.md). Under ctest the
 * daemon is provided by tests/run-dtest.sh.
 */

#include "quo-vadis-mpi.h"
#include "qvb.h"

#include "mpi.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

/**
 * Fixed log2-ns latency histogram. Bounded memory, no per-sample allocation.
 */
struct qvb_hist {
    /**
     * Latency histogram. Buckets are log-spaced over [1ns, ~1s] so a handful of
     * buckets cover the whole plausible RMI round-trip range with bounded memory
     * and no per-sample allocation. Bucket i covers [2^i ns, 2^(i+1) ns).
     */
    static constexpr int nbuckets = 40;

    std::vector<uint64_t> count;
    uint64_t nsamples = 0;
    uint64_t min_ns = UINT64_MAX;
    uint64_t max_ns = 0;

    qvb_hist(void) : count(nbuckets, 0) { }

    /** Returns the bucket index for a latency sample in nanoseconds. */
    static int
    bucket(uint64_t ns)
    {
        int b = 0;
        uint64_t v = ns;
        while (v > 1 && b < nbuckets - 1) {
            v >>= 1;
            ++b;
        }
        return b;
    }

    void
    add(uint64_t ns)
    {
        count[bucket(ns)]++;
        nsamples++;
        if (ns < min_ns) min_ns = ns;
        if (ns > max_ns) max_ns = ns;
    }

    /**
     * Estimates the p-th percentile latency (ns) from the cumulative bucket
     * counts, returning the containing bucket's geometric midpoint so the
     * estimate lies inside the [2^b, 2^(b+1)) range rather than at an edge.
     */
    double
    percentile(double p) const
    {
        if (nsamples == 0) return 0.0;
        const uint64_t target = static_cast<uint64_t>(
            std::ceil((p / 100.0) * static_cast<double>(nsamples))
        );
        uint64_t total = 0;
        for (int b = 0; b < nbuckets; ++b) {
            total += count[b];
            if (total >= target) {
                const double lo = std::ldexp(1.0, b);     // 2^b
                const double hi = std::ldexp(1.0, b + 1); // 2^(b+1)
                return std::sqrt(lo * hi);                // geometric midpoint
            }
        }
        return static_cast<double>(max_ns);
    }
};

/** Benchmark configuration parsed from argv. */
struct qvb_cfg {
    /**< Window seconds (0 => use requests). */
    double duration_s = 5.0;
    /**< Fixed request count/process (if > 0). */
    long requests = 0;
    /**< Untimed warmup seconds. */
    double warmup_s = 1.0;
    /**< Number of measurement repetitions. */
    long trials = 5;
    /**< Optional CSV output path (root process). */
    std::string csv_path;
};

static void
qvb_usage(
    const char *argv0
) {
    fprintf(
        stderr,
        "Usage: %s [--duration SEC | --requests N] [--warmup SEC]\n"
        "          [--trials K] [--csv PATH]\n"
        "\n"
        "  --duration SEC   Timed window length per trial (default 5.0).\n"
        "  --requests N     Fixed push/pop iterations per process per trial.\n"
        "                   Overrides --duration when > 0.\n"
        "  --warmup SEC     Untimed warmup before each trial (default 1.0).\n"
        "  --trials K       Repeat the measurement K times (default 5).\n"
        "  --csv PATH       Append per-trial results as CSV (root process).\n"
        "\n"
        "Concurrency (number of clients) is the MPI communicator size; set it\n"
        "with `mpiexec -n N`. A running quo-vadisd is required.\n",
        argv0
    );
}

static double
qvb_parse_double(
    const char *s,
    const char *what
) {
    char *end = nullptr;
    const double v = strtod(s, &end);
    if (end == s || *end != '\0') {
        ctu_panic("invalid value for %s: %s", what, s);
    }
    return v;
}

static long
qvb_parse_long(
    const char *s,
    const char *what
) {
    char *end = nullptr;
    const long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') ctu_panic("invalid value for %s: %s", what, s);
    return v;
}

static void
qvb_parse_args(
    int argc,
    char **argv,
    qvb_cfg &cfg
) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            cfg.duration_s = qvb_parse_double(argv[++i], "--duration");
        }
        else if (strcmp(argv[i], "--requests") == 0 && i + 1 < argc) {
            cfg.requests = qvb_parse_long(argv[++i], "--requests");
        }
        else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            cfg.warmup_s = qvb_parse_double(argv[++i], "--warmup");
        }
        else if (strcmp(argv[i], "--trials") == 0 && i + 1 < argc) {
            cfg.trials = qvb_parse_long(argv[++i], "--trials");
        }
        else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            cfg.csv_path = argv[++i];
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            qvb_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else {
            qvb_usage(argv[0]);
            ctu_panic("unrecognized argument: %s", argv[i]);
        }
    }
    if (cfg.trials <= 0) ctu_panic("--trials must be > 0");
    if (cfg.requests <= 0 && cfg.duration_s <= 0.0) {
        ctu_panic("one of --duration (> 0) or --requests (> 0) is required");
    }
}

/**
 * Result of a single trial as reduced across all processes (valid on the root
 * process).
 */
struct qvb_trial_result {
    /**< Aggregate messages/second. */
    double throughput = 0.0;
    /**< Mean per-message round-trip latency (ns). */
    double mean_ns = 0.0;
    double p50_ns = 0.0;
    double p95_ns = 0.0;
    double p99_ns = 0.0;
    uint64_t min_ns = 0;
    uint64_t max_ns = 0;
    /**< Total messages across all processes. */
    uint64_t messages = 0;
    /**< Longest per-process measurement window (s). */
    double window_s = 0.0;
};

/**
 * Runs one measurement trial. Every process participates in all collectives.
 * The result is meaningful only on the root process.
 */
static void
qvb_run_trial(
    qv_scope_t *scope,
    MPI_Comm comm,
    const qvb_cfg &cfg,
    qvb_trial_result &out
) {
    static constexpr size_t nbarrier = 2;
    // Untimed warm-up to amortize first-call and connection-cache effects.
    const uint64_t warm_end = qvb_now_ns() +
        static_cast<uint64_t>(cfg.warmup_s * 1e9);
    while (qvb_now_ns() < warm_end) {
        ctu_check(qv_bind_push(scope), "qv_bind_push");
        ctu_check(qv_bind_pop(scope), "qv_bind_pop");
    }

    qvb_hist hist;
    uint64_t local_msgs = 0;
    uint64_t total_ns = 0;

    // Align all processes so their measurement windows overlap.
    for (size_t i = 0; i < nbarrier; ++i) {
        ctu_mpi_check(
            MPI_Barrier(comm),
            "MPI_Reduce(messages)"
        );
    }

    const uint64_t t_start = qvb_now_ns();
    const uint64_t t_deadline = (cfg.requests > 0)
        ? UINT64_MAX
        : t_start + static_cast<uint64_t>(cfg.duration_s * 1e9);
    long iters_done = 0;

    while (true) {
        if (cfg.requests > 0) {
            if (iters_done >= cfg.requests) break;
        }
        else if (qvb_now_ns() >= t_deadline) {
            break;
        }

        // Number of messages issued per timed iteration (push + pop).
        static constexpr int msgs_per_iter = 2;

        const uint64_t a0 = qvb_now_ns();
        ctu_check(qv_bind_push(scope), "qv_bind_push");
        const uint64_t a1 = qvb_now_ns();
        ctu_check(qv_bind_pop(scope), "qv_bind_pop");
        const uint64_t a2 = qvb_now_ns();

        // Each call is one daemon round-trip; record both latencies.
        hist.add(a1 - a0);
        hist.add(a2 - a1);
        total_ns += (a2 - a0);
        local_msgs += msgs_per_iter;
        ++iters_done;
    }
    const uint64_t t_end = qvb_now_ns();
    const uint64_t local_window_ns = t_end - t_start;

    // Close the window; keeps processes in lock-step before the reductions.
    for (size_t i = 0; i < nbarrier; ++i) {
        ctu_mpi_check(
            MPI_Barrier(comm),
            "MPI_Reduce(messages)"
        );
    }

    // Reduce scalar aggregates across all processes.
    uint64_t total_msgs = 0, sum_total_ns = 0, max_window_ns = 0;
    uint64_t global_min = 0, global_max = 0;
    ctu_mpi_check(
        MPI_Reduce(
            &local_msgs, &total_msgs, 1,
            MPI_UINT64_T, MPI_SUM, 0, comm
        ),
        "MPI_Reduce(messages)"
    );
    ctu_mpi_check(
        MPI_Reduce(
            &total_ns, &sum_total_ns, 1,
            MPI_UINT64_T, MPI_SUM, 0, comm
        ),
        "MPI_Reduce(total_ns)"
    );
    ctu_mpi_check(
        MPI_Reduce(
            &local_window_ns, &max_window_ns, 1,
            MPI_UINT64_T, MPI_MAX, 0, comm
        ),
        "MPI_Reduce(window)"
    );
    ctu_mpi_check(
        MPI_Reduce(
            &hist.min_ns, &global_min, 1,
            MPI_UINT64_T, MPI_MIN, 0, comm
        ),
        "MPI_Reduce(min)"
    );
    ctu_mpi_check(
        MPI_Reduce(
            &hist.max_ns, &global_max, 1,
            MPI_UINT64_T, MPI_MAX, 0, comm
        ),
        "MPI_Reduce(max)"
    );

    // Reduce the latency histogram bucket-wise for global percentiles.
    qvb_hist global_hist;
    ctu_mpi_check(
        MPI_Reduce(
            hist.count.data(),
            global_hist.count.data(),
            qvb_hist::nbuckets,
            MPI_UINT64_T, MPI_SUM,
            0, comm
        ),
        "MPI_Reduce(histogram)"
    );

    // nsamples for the reduced histogram == total messages.
    global_hist.nsamples = total_msgs;
    global_hist.min_ns = global_min;
    global_hist.max_ns = global_max;

    const double window_s = static_cast<double>(max_window_ns) / 1e9;
    const double total_msgs_d = static_cast<double>(total_msgs);
    out.messages = total_msgs;
    out.window_s = window_s;
    out.throughput = window_s > 0.0 ? total_msgs_d / window_s : 0.0;
    out.mean_ns = total_msgs > 0
                ? static_cast<double>(sum_total_ns) / total_msgs_d
                : 0.0;
    out.p50_ns = global_hist.percentile(50.0);
    out.p95_ns = global_hist.percentile(95.0);
    out.p99_ns = global_hist.percentile(99.0);
    out.min_ns = global_min;
    out.max_ns = global_max;
}

/** Prints the reproducibility metadata banner (root process). */
static void
qvb_print_metadata(
    int nranks,
    const qvb_cfg &cfg
) {
    char host[256] = {0};
    if (gethostname(host, sizeof(host) - 1) != 0) {
        snprintf(host, sizeof(host), "unknown");
    }

    int vmaj = 0, vmin = 0, vpatch = 0;
    ctu_check(qv_version(&vmaj, &vmin, &vpatch), "qv_version");

    char mpiver[MPI_MAX_LIBRARY_VERSION_STRING] = {0};
    int mpiverlen = 0;
    if (MPI_Get_library_version(mpiver, &mpiverlen) != MPI_SUCCESS) {
        snprintf(mpiver, sizeof(mpiver), "unknown");
    }
    // Keep only the first line of the (often multi-line) MPI version string.
    char *nl = strchr(mpiver, '\n');
    if (nl) *nl = '\0';

    const char *port = getenv("QV_PORT");

    printf("# quo-vadisd message-rate benchmark (public API, MPI clients)\n");
    printf("# host              : %s\n", host);
    printf("# online processors : %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
    printf("# concurrent clients: %d (MPI processes)\n", nranks);
    printf("# quo-vadis version : %d.%d.%d\n", vmaj, vmin, vpatch);
    printf("# mpi library       : %s\n", mpiver);
    printf("# QV_PORT           : %s\n", port ? port : "(unset; discovered)");
    printf("# message primitive : push/pop (1 daemon message each)\n");
    if (cfg.requests > 0) {
        printf("# window            : %ld requests/process\n", cfg.requests);
    }
    else {
        printf("# window            : %.3f s\n", cfg.duration_s);
    }
    printf("# warmup            : %.3f s\n", cfg.warmup_s);
    printf("# trials            : %ld\n", cfg.trials);
    printf(
        "#\n"
        "# %-6s %14s %14s %14s %14s %14s %14s\n",
        "trial", "msg/s", "mean(ns)", "p50(ns)", "p95(ns)", "p99(ns)",
        "max(ns)"
    );
    fflush(stdout);
}

int
main(
    int argc, char **argv
) {
    const MPI_Comm target_comm = MPI_COMM_WORLD;
    ctu_mpi_check(MPI_Init(&argc, &argv), "MPI_Init");

    int wrank = 0, nranks = 0;
    ctu_mpi_check(MPI_Comm_rank(target_comm, &wrank), "MPI_Comm_rank");
    ctu_mpi_check(MPI_Comm_size(target_comm, &nranks), "MPI_Comm_size");
    const bool reporting = (wrank == 0);

    qvb_cfg cfg;
    qvb_parse_args(argc, argv, cfg);

    // Each process is an independent client:
    // its own scope means its own connection.
    qv_scope_t *scope = nullptr;
    ctu_check(
        qv_mpi_scope(target_comm, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &scope),
        "qv_mpi_scope"
    );

    if (reporting) qvb_print_metadata(nranks, cfg);

    FILE *csv = nullptr;
    if (reporting && !cfg.csv_path.empty()) {
        csv = fopen(cfg.csv_path.c_str(), "a");
        if (!csv) ctu_panic("could not open CSV file: %s", cfg.csv_path.c_str());
        // Header only if the file is empty.
        if (ftell(csv) == 0) {
            fprintf(
                csv,
                "clients,trial,throughput_msgs_per_s,mean_ns,p50_ns,"
                "p95_ns,p99_ns,min_ns,max_ns,messages,window_s\n"
            );
        }
    }

    // Running stats over trials (Welford) for throughput mean/stddev.
    double thr_mean = 0.0, thr_m2 = 0.0;
    // Running mean of per-trial mean latency, for the Little's Law check.
    double lat_mean_ns = 0.0;
    int exit_status = EXIT_SUCCESS;
    long thr_n = 0;

    for (long t = 0; t < cfg.trials; ++t) {
        qvb_trial_result r;
        qvb_run_trial(scope, target_comm, cfg, r);

        if (reporting) {
            printf(
                "  %-6ld %14.1f %14.1f %14.1f %14.1f %14.1f %14llu\n",
                t, r.throughput, r.mean_ns, r.p50_ns, r.p95_ns, r.p99_ns,
                static_cast<unsigned long long>(r.max_ns)
            );
            fflush(stdout);

            if (csv) {
                fprintf(
                    csv,
                    "%d,%ld,%.1f,%.1f,%.1f,%.1f,%.1f,%llu,%llu,%llu,%.6f\n",
                    nranks, t, r.throughput, r.mean_ns, r.p50_ns, r.p95_ns,
                    r.p99_ns, static_cast<unsigned long long>(r.min_ns),
                    static_cast<unsigned long long>(r.max_ns),
                    static_cast<unsigned long long>(r.messages), r.window_s
                );
                fflush(csv);
            }
            // Welford update for throughput across trials.
            ++thr_n;
            const double d = r.throughput - thr_mean;
            const double thr_nasd = static_cast<double>(thr_n);
            thr_mean += d / thr_nasd;
            thr_m2 += d * (r.throughput - thr_mean);
            lat_mean_ns += (r.mean_ns - lat_mean_ns) / thr_nasd;
        }
    }

    if (reporting) {
        const double thr_nasd = static_cast<double>(thr_n);
        const double var = (thr_n > 1) ? thr_m2 / thr_nasd - 1 : 0.0;
        const double sd = std::sqrt(var);
        // 95% CI half-width using the normal approximation (z=1.96).
        const double ci95 = (thr_n > 1) ? 1.96 * sd / std::sqrt(thr_nasd) : 0.0;
        printf(
            "#\n"
            "# summary: clients=%d throughput_mean=%.1f msg/s"
            " stddev=%.1f ci95=+/-%.1f (n=%ld trials)\n",
            nranks, thr_mean, sd, ci95, thr_n
        );
        // Little's Law sanity check: N ~= throughput * mean_latency.
        // Each client keeps exactly one message in flight, so the estimated
        // concurrency L should track the client count N (from below, since
        // the timed window also includes barriers and per-call overhead).
        static constexpr double little_lo = 0.70;
        static constexpr double little_hi = 1.10;
        const double lat_s = lat_mean_ns / 1e9;
        const double L = thr_mean * lat_s;
        const double ratio = (nranks > 0) ? L / nranks : 0.0;
        if (thr_n > 0 && lat_s > 0.0) {
            printf(
                "# little's law: L=%.2f (throughput_mean * mean_latency)"
                " N=%d L/N=%.3f\n",
                L, nranks, ratio
            );
            if (ratio < little_lo || ratio > little_hi) {
                printf(
                    "# Warning: Little's Law check failed: L/N=%.3f"
                    " outside [%.2f, %.2f]\n",
                    ratio, little_lo, little_hi
                );
                exit_status = EXIT_FAILURE;
            }
        }
        fflush(stdout);
        if (csv) fclose(csv);
    }

    ctu_check(qv_free(scope), "qv_free");

    ctu_mpi_check(MPI_Finalize(), "MPI_Finalize");
    return exit_status;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
