/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadisd.cc
 *
 * Implements the quo-vadis daemon.
 */

// TODO(skg)
// * Add something like QV_SHUTDOWN_ON_DISCONNECT or QV_DAEMON_KEEP_ALIVE

#include "qvi-utils.h"
#include "qvi-hwloc.h"
#include "qvi-rmi.h"

static const cstr_t app_name = "quo-vadisd";

using option_help = std::map<std::string, std::string>;

struct qvid_context {
    qvi_rmi_config rmic;
    qvi_hwloc_t *hwloc = nullptr;
    qvi_rmi_server rmi;
    /** Run as a daemon flag. */
    bool daemonized = true;
    /** Constructor. */
    qvid_context(void)
    {
        const int rc = qvi_hwloc_new(&hwloc);
        if (rc != QV_SUCCESS) {
            throw qvi_runtime_error();
        }
    }
    /** Destructor */
    ~qvid_context(void)
    {
        qvi_hwloc_delete(&hwloc);
    }
};

static void
closefds(
    qvid_context &
) {
    qvi_log_debug("Closing FDs");
    // Determine the max number of file descriptors.
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        static cstr_t ers = "Cannot determine RLIMIT_NOFILE";
        const int err = errno;
        qvi_panic_log_error("{} (rc={}, {})", ers, err, strerror(err));
    }
    // Default: no limit on this resource, so pick one.
    int64_t maxfd = 1024;
    if (rl.rlim_max != RLIM_INFINITY) {
        // Not RLIM_INFINITY, so set to resource limit.
        maxfd = (int64_t)rl.rlim_max;
    }
    // Close all the file descriptors.
    for (int64_t fd = 0; fd < maxfd; ++fd) {
        (void)close(fd);
    }
}

static void
become_session_leader(
    qvid_context &
) {
    qvi_log_debug("Becoming session leader");

    pid_t pid = 0;
    if ((pid = fork()) < 0) {
        static cstr_t ers = "fork() failed";
        const int err = errno;
        qvi_panic_log_error("{} (rc={}, {})", ers, err, strerror(err));
    }
    // Parent
    if (pid != 0) {
        // _exit(2) used to match daemon(3) behavior.
        _exit(EXIT_SUCCESS);
    }
    // Child
    const pid_t pgid = setsid();
    if (pgid < 0) {
        static cstr_t ers = "setsid() failed";
        const int err = errno;
        qvi_panic_log_error("{} (rc={}, {})", ers, err, strerror(err));
    }
}

static void
rmi_config(
    qvid_context &ctx
) {
    qvi_log_debug("Starting RMI");

    ctx.rmic.hwloc = ctx.hwloc;

    int rc = qvi_url(ctx.rmic.url);
    if (rc != QV_SUCCESS) {
        qvi_panic_log_error(qvi_conn_ers());
        return;
    }

    rc = ctx.rmi.configure(ctx.rmic);
    if (rc != QV_SUCCESS) {
        qvi_panic_log_error("rmi->configure() failed");
        return;
    }

    qvi_log_debug("URL: {}", ctx.rmic.url);
    qvi_log_debug("hwloc XML: {}", ctx.rmic.hwtopo_path);
}

static void
rmi_start(
    qvid_context &ctx
) {
    qvi_log_debug("Starting RMI");

    cstr_t ers = nullptr;
    // TODO(skg) Add flags option
    const int rc = ctx.rmi.start();
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        ers = "qvi_rmi_server_start() failed";
    }
    if (qvi_unlikely(ers)) {
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }
}

static void
hwtopo_load(
    qvid_context &ctx
) {
    qvi_log_debug("Loading hardware information");

    int rc = qvi_hwloc_topology_init(ctx.hwloc, nullptr);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        static cstr_t ers = "qvi_hwloc_topology_init() failed";
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }

    rc = qvi_hwloc_topology_load(ctx.hwloc);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        static cstr_t ers = "qvi_hwloc_topology_load() failed";
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }

    rc = qvi_hwloc_discover_devices(ctx.hwloc);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        static cstr_t ers = "qvi_hwloc_discover_devices() failed";
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }
}

static void
hwtopo_export(
    qvid_context &ctx
) {
    qvi_log_debug("Publishing hardware information");

    const std::string basedir = qvi_tmpdir();
    char *path = nullptr;
    const int rc = qvi_hwloc_topology_export(
        ctx.hwloc, basedir.c_str(), &path
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        static cstr_t ers = "qvi_hwloc_topology_export() failed";
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }
    ctx.rmic.hwtopo_path = std::string(path);
    free(path);
}

static void
show_usage(
    const option_help &opt_help
) {
    printf(
        "\nUsage:\n"
        "%s [OPTIONS]\n"
        "Options:\n"
        , app_name
    );

    for (auto &i : opt_help) {
        printf("  %s %s\n", i.first.c_str(), i.second.c_str());
    }
}

static int
parse_args(
    int argc,
    char **argv,
    qvid_context &ctx
) {
    enum {
        FLOOR = 256,
        NO_DAEMONIZE,
        HELP,
    };

    const cstr_t opts = "";
    const struct option lopts[] = {
        {"no-daemonize"    , no_argument, nullptr         , NO_DAEMONIZE      },
        {"help"            , no_argument, nullptr         , HELP              },
        {nullptr           , 0          , nullptr         , 0                 }
    };
    static const option_help opt_help = {
        {"[--no-daemonize]     ", "Do not run as a daemon."                   },
        {"[--help]             ", "Show this message and exit."               }
    };

    int opt;
    while (-1 != (opt = getopt_long_only(argc, argv, opts, lopts, nullptr))) {
        switch (opt) {
            case NO_DAEMONIZE:
                ctx.daemonized = false;
                break;
            case HELP:
                show_usage(opt_help);
                return QV_SUCCESS_SHUTDOWN;
            default:
                show_usage(opt_help);
                return QV_ERR_INVLD_ARG;
        }
    }
    return QV_SUCCESS;
}

static int
start(
    int argc,
    char **argv
) {
    try {
        qvid_context ctx;

        int rc = parse_args(argc, argv, ctx);
        if (rc != QV_SUCCESS) {
            if (rc == QV_SUCCESS_SHUTDOWN) {
                rc = QV_SUCCESS;
            }
            return rc;
        }

        if (ctx.daemonized) {
            // Redirect all console output to syslog.
            qvi_logger::console_to_syslog();
            // Clear umask. Note: this system call always succeeds.
            umask(0);
            // Become a session leader to lose controlling TTY.
            become_session_leader(ctx);
            // Close all file descriptors.
            closefds(ctx);
        }
        // Gather and publish hardware information.
        hwtopo_load(ctx);
        hwtopo_export(ctx);
        // Configure RMI, start listening for commands.
        rmi_config(ctx);
        rmi_start(ctx);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

int
main(
    int argc,
    char **argv
) {
    const int rc = start(argc, argv);
    return (rc == QV_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
