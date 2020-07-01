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

#include <cstdlib>

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

static int
maxfd(void)
{
    // TODO(skg)
    return 0;
}

int
main(int argc, char **argv)
{
    // Clear umask.
    // Note: this system call always succeeds.
    umask(0);
    // Determine max file descriptor.
    const int mfd = maxfd();
    // Determine max number of file descriptors.
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
