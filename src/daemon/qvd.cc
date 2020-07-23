/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvd.cc
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "core/common.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <cstdlib>

static void
closefds(void)
{
    // Determine the max number of file descriptors.
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        static const char *er = "Cannot determine RLIMIT_NOFILE";
        char *em = qvi_msg("%s (%s)", er, qvi_strerr(errno));
        qvi_panic(em);
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
become_session_leader(void)
{
    pid_t pid = 0;
    if ((pid = fork()) < 0) {
        qvi_panic(qvi_strerr(errno));
    }
    // Parent
    if (pid != 0) {
        // _exit(2) used to match daemon(3) behavior.
        _exit(EXIT_SUCCESS);
    }
    // Child
    pid_t pgid = setsid();
    if (pgid < 0) {
        qvi_panic(qvi_strerr(errno));
    }
}

static void
main_loop(void)
{
    while(true) {
        sleep(1);
    }
}

int
main(int, char **)
{
    // Clear umask. Note: this system call always succeeds.
    umask(0);
    // Become a session leader to lose controlling TTY.
    become_session_leader();
    // Close all file descriptors.
    closefds();
    // Enter the main processing loop.
    main_loop();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
