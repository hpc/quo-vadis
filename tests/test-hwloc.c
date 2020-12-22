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

#include "private/qvi-macros.h"
#include "private/qvi-hwloc.h"

#include "quo-vadis.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct hw_name_type_s {
    char const *name;
    qv_hwloc_obj_type_t type;
} hw_name_type_t;

static const hw_name_type_t nts[] = {
    {QVI_STRINGIFY(QV_HWLOC_OBJ_MACHINE),  QV_HWLOC_OBJ_MACHINE},
    {QVI_STRINGIFY(QV_HWLOC_OBJ_PACKAGE),  QV_HWLOC_OBJ_PACKAGE},
    {QVI_STRINGIFY(QV_HWLOC_OBJ_CORE),     QV_HWLOC_OBJ_CORE},
    {QVI_STRINGIFY(QV_HWLOC_OBJ_PU),       QV_HWLOC_OBJ_PU},
    {QVI_STRINGIFY(QV_HWLOC_OBJ_L1CACHE),  QV_HWLOC_OBJ_L1CACHE},
    {QVI_STRINGIFY(QV_HWLOC_OBJ_L2CACHE),  QV_HWLOC_OBJ_L2CACHE},
    {QVI_STRINGIFY(QV_HWLOC_OBJ_L3CACHE),  QV_HWLOC_OBJ_L3CACHE},
    {QVI_STRINGIFY(QV_HWLOC_OBJ_L4CACHE),  QV_HWLOC_OBJ_L4CACHE},
    {QVI_STRINGIFY(QV_HWLOC_OBJ_L5CACHE),  QV_HWLOC_OBJ_L5CACHE},
    {QVI_STRINGIFY(QV_HWLOC_OBJ_NUMANODE), QV_HWLOC_OBJ_NUMANODE}
};

static int
echo_hw_info(
    qvi_hwloc_t *hwl
) {
    const int num_nts = sizeof(nts) / sizeof(hw_name_type_t);

    printf("# System Hardware Overview --------------\n");
    for (int i = 0; i < num_nts; ++i) {
        int n;
        int rc = qvi_hwloc_get_nobjs_by_type(hwl, nts[i].type, &n);
        if (rc != QV_SUCCESS) {
            fprintf(
                stderr,
                "qvi_hwloc_get_nobjs_by_type(%s) failed\n",
                nts[i].name
            );
            return rc;
        }
        printf("# %s=%d\n", nts[i].name, n);
    }
    printf("# ---------------------------------------\n");
    return QV_SUCCESS;
}

int
echo_task_intersections(
    qvi_hwloc_t *hwl,
    char *bitmap_str
) {
    const int num_nts = sizeof(nts) / sizeof(hw_name_type_t);
    const pid_t me = getpid();

    printf("# Task Intersection Overview ------------\n");
    for (int i = 0; i < num_nts; ++i) {
        int nobj;
        int rc = qvi_hwloc_get_nobjs_by_type(hwl, nts[i].type, &nobj);
        if (rc != QV_SUCCESS) {
            fprintf(
                stderr,
                "qvi_hwloc_get_nobjs_by_type(%s) failed\n",
                nts[i].name
            );
            return rc;
        }
        for (int objid = 0; objid < nobj; ++objid) {
            int intersects;
            rc = qvi_hwloc_task_intersects_obj_by_type_id(
                hwl,
                nts[i].type,
                me,
                objid,
                &intersects
            );
            if (rc != QV_SUCCESS) {
                fprintf(
                    stderr,
                    "qvi_hwloc_task_intersects_obj_by_type_id(%s) failed\n",
                    nts[i].name
                );
                return rc;
            }
            printf(
                "# %s Intersects With %s %d: %s\n",
                bitmap_str,
                nts[i].name,
                objid,
                intersects ? "Yes" : "No"
            );
        }
    }
    printf("# ---------------------------------------\n");
    return QV_SUCCESS;
}

int
main(void)
{
    printf("# Starting hwloc test\n");

    char const *ers = NULL;
    qvi_hwloc_t *hwl;

    int rc = qvi_hwloc_construct(&hwl);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_construct() failed";
        goto out;
    }

    rc = qvi_hwloc_topology_load(hwl);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_load() failed";
        goto out;
    }

    rc = echo_hw_info(hwl);
    if (rc != QV_SUCCESS) {
        ers = "echo_hw_info() failed";
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

    char *binds;
    rc = qvi_hwloc_bitmap_asprintf(&binds, bitmap);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_bitmap_asprintf() failed";
        goto out;
    }
    printf("\n# cpuset=%s\n", binds);

    rc = echo_task_intersections(hwl, binds);
    if (rc != QV_SUCCESS) {
        ers = "echo_task_intersections() failed";
        goto out;
    }
out:
    if (binds) free(binds);
    hwloc_bitmap_free(bitmap);
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
