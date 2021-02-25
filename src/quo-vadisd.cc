/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
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
 */

#include "qvi-common.h"
#include "qvi-utils.h"
#include "qvi-hwloc.h"
#include "qvi-rmi.h"

struct context {
    qvi_hwloc_t *hwloc = nullptr;
    qvi_rmi_server_t *rmi = nullptr;
    bool daemonized = false;
};

static void
context_destruct(
    context **ctx
) {
    context *ictx = *ctx;
    if (!ictx) return;
    // TODO(skg) Fix crash in hwloc teardown.
    qvi_rmi_server_destruct(&ictx->rmi);
    qvi_hwloc_destruct(&ictx->hwloc);
    delete ictx;
    *ctx = nullptr;
}

static void
context_construct(
    context **ctx
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    context *ictx = qvi_new context;
    if (!ictx) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_hwloc_construct(&ictx->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_construct() failed";
        goto out;
    }

    rc = qvi_rmi_server_construct(&ictx->rmi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_construct() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_panic_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    *ctx = ictx;
}

static void
closefds(
    context *
) {
    qvi_log_debug("Closing FDs");
    // Determine the max number of file descriptors.
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        static cstr ers = "Cannot determine RLIMIT_NOFILE";
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
    context *
) {
    qvi_log_debug("Becoming session leader");

    pid_t pid = 0;
    if ((pid = fork()) < 0) {
        static cstr ers = "fork() failed";
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
        static cstr ers = "setsid() failed";
        const int err = errno;
        qvi_panic_log_error("{} (rc={}, {})", ers, err, qvi_strerr(err));
    }
}

static void
start_rmi(
    context *ctx
) {
    qvi_log_debug("Starting RMI");

    cstr ers = nullptr;

    char *url = nullptr;
    int rc = qvi_url(&url);
    if (rc != QV_SUCCESS) {
        qvi_log_error(qvi_conn_ers());
        ers = "qvi_url() failed";
        goto out;
    }
    qvi_log_debug("Listening on {}", url);

    rc = qvi_rmi_server_start(ctx->rmi, url);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_start() failed";
        goto out;
    }
    // TODO(skg) Add flags option
out:
    if (ers) {
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }
    if (url) free(url);
}

static void
load_hwtopo(
    context *ctx
) {
    qvi_log_debug("Loading hardware information");

    int rc = qvi_hwloc_topology_load(ctx->hwloc);
    if (rc != QV_SUCCESS) {
        static cstr ers = "qvi_hwloc_topology_load() failed";
        qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }
}

// TODO(skg) Add daemonize option.
int
main(
    int,
    char **
) {
    context *ctx = nullptr;
    context_construct(&ctx);

    if (ctx->daemonized) {
        // Redirect all console output to syslog.
        qvi_logger::console_to_syslog();
        // Clear umask. Note: this system call always succeeds.
        umask(0);
        // Become a session leader to lose controlling TTY.
        become_session_leader(ctx);
        // Close all file descriptors.
        closefds(ctx);
    }
    // Gather hardware information.
    load_hwtopo(ctx);
    start_rmi(ctx);
    context_destruct(&ctx);
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
