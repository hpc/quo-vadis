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

#include "quo-vadis.h"
#include "quo-vadis/qv-hwloc.h"

#include <stdlib.h>
#include <stdio.h>

int
main(void)
{
    printf("# Starting hwloc test\n");

    char const *ers = NULL;
    qv_hwloc_t *hwl;
    char *binds = NULL;

    int rc = qv_hwloc_construct(&hwl);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_construct() failed";
        goto out;
    }

    rc = qv_hwloc_init(hwl);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_init() failed";
        goto out;
    }

    rc = qv_hwloc_topo_load(hwl);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_topo_load() failed";
        goto out;
    }

    qv_hwloc_bitmap_t bitmap;
    rc = qv_hwloc_task_get_cpubind(
        hwl,
        getpid(),
        &bitmap
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_task_get_cpubind() failed";
        goto out;
    }

    rc = qv_hwloc_bitmap_asprintf(bitmap, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_bitmap_asprintf() failed";
        goto out;
    }
    printf("# cpuset=%s\n", binds);
    free(binds);

    rc = qv_hwloc_bitmap_free(bitmap);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_bitmap_free() failed";
        goto out;
    }
out:
    qv_hwloc_destruct(hwl);
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
