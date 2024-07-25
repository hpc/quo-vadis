/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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
 * The quo-vadis daemon.
 */

// TODO(skg)
// * Add something like QV_SHUTDOWN_ON_DISCONNECT or QV_DAEMON_KEEP_ALIVE
// * Add daemonize option.

#include "qvi-utils.h"
#include "qvi-hwloc.h"
#include "qvi-rmi.h"

struct context_s {
    qvi_rmi_config_s rmic;
    qvi_hwloc_t *hwloc = nullptr;
    qvi_rmi_server_t *rmi = nullptr;
    bool daemonized = false;
    /** Constructor. */
    context_s(void)
    {
        int rc = qvi_hwloc_new(&hwloc);
        if (rc != QV_SUCCESS) {
            throw qvi_runtime_error();
        }

        rc = qvi_rmi_server_new(&rmi);
        if (rc != QV_SUCCESS) {
            throw qvi_runtime_error();
        }
    }
    /** Destructor */
    ~context_s(void)
    {
        qvi_rmi_server_delete(&rmi);
        qvi_hwloc_delete(&hwloc);
    }
};

static void
closefds(
    context_s &
) {
    qvi_log_debug("Closing FDs");
    // Determine the max number of file descriptors.
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        static cstr_t ers = "Cannot determine RLIMIT_NOFILE";
        const int err = errno;
        qvi_panic_log_error("{} (rc={}, {})", ers, err, qvi_strerr(err));
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
    context_s &
) {
    qvi_log_debug("Becoming session leader");

    pid_t pid = 0;
    if ((pid = fork()) < 0) {
        static cstr_t ers = "fork() failed";
        const int err = errno;
        qvi_panic_log_error("{} (rc={}, {})", ers, err, qvi_strerr(err));
    }
    // Parent
    if (pid != 0) {
        // _exit(2) used to match daemon(3) behavior.
        _exit(EXIT_SUCCESS);
    }
    // Child
    pid_t pgid = setsid();
    if (pgid < 0) {
        static cstr_t ers = "setsid() failed";
        const int err = errno;
        qvi_panic_log_error("{} (rc={}, {})", ers, err, qvi_strerr(err));
    }
}

static void
rmi_config(
    context_s &ctx
) {
    qvi_log_debug("Starting RMI");

    ctx.rmic.hwloc = ctx.hwloc;

    int rc = qvi_url(ctx.rmic.url);
    if (rc != QV_SUCCESS) {
        qvi_panic_log_error(qvi_conn_ers());
        return;
    }

    rc = qvi_rmi_server_config(ctx.rmi, &ctx.rmic);
    if (rc != QV_SUCCESS) {
        qvi_panic_log_error("qvi_rmi_server_config() failed");
        return;
    }

    qvi_log_debug("URL: {}", ctx.rmic.url);
    qvi_log_debug("hwloc XML: {}", ctx.rmic.hwtopo_path);
}

static void
rmi_start(
    context_s &ctx
) {
    qvi_log_debug("Starting RMI");

    cstr_t ers = nullptr;

    static const bool blocks = true;
    int rc = qvi_rmi_server_start(ctx.rmi, blocks);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_start() failed";
    }
    // TODO(skg) Add flags option
    if (ers) {
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }
}

static void
hwtopo_load(
    context_s &ctx
) {
    qvi_log_debug("Loading hardware information");

    int rc = qvi_hwloc_topology_init(ctx.hwloc, nullptr);
    if (rc != QV_SUCCESS) {
        static cstr_t ers = "qvi_hwloc_topology_init() failed";
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }

    rc = qvi_hwloc_topology_load(ctx.hwloc);
    if (rc != QV_SUCCESS) {
        static cstr_t ers = "qvi_hwloc_topology_load() failed";
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }

    rc = qvi_hwloc_discover_devices(ctx.hwloc);
    if (rc != QV_SUCCESS) {
        static cstr_t ers = "qvi_hwloc_discover_devices() failed";
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }
}

static void
hwtopo_export(
    context_s &ctx
) {
    qvi_log_debug("Publishing hardware information");

    cstr_t basedir = qvi_tmpdir();
    char *path = nullptr;
    int rc = qvi_hwloc_topology_export(
        ctx.hwloc, basedir, &path
    );
    if (rc != QV_SUCCESS) {
        static cstr_t ers = "qvi_hwloc_topology_export() failed";
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }
    ctx.rmic.hwtopo_path = std::string(path);
    free(path);
}

static int
start(
    int,
    char **
) {
    try {
        context_s ctx;

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
