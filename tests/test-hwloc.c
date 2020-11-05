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
 * @file test-hwloc.c
 */

#include "private/qvi-hwloc.h"

#include "quo-vadis.h"

#include <stdlib.h>
#include <stdio.h>

int
main(void)
{
    printf("# Starting hwloc test\n");

    char const *ers = NULL;
    qvi_hwloc_t *hwl;
    char *binds = NULL;

    int rc = qvi_hwloc_construct(&hwl);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_construct() failed";
        goto out;
    }

    rc = qvi_hwloc_init(hwl);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_init() failed";
        goto out;
    }

    rc = qvi_hwloc_topology_load(hwl);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_load() failed";
        goto out;
    }

    hwloc_bitmap_t bitmap;
    rc = qvi_hwloc_task_get_cpubind(
        hwl,
        getpid(),
        &bitmap
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_task_get_cpubind() failed";
        goto out;
    }

    rc = qvi_hwloc_bitmap_asprintf(&binds, bitmap);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_bitmap_asprintf() failed";
        goto out;
    }
    printf("# cpuset=%s\n", binds);
    free(binds);

    hwloc_bitmap_free(bitmap);
out:
    qvi_hwloc_destruct(hwl);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return EXIT_FAILURE;
    }
    printf("# Done\n");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
