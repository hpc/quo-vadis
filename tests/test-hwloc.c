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
 * @file test-hwloc.c
 */

#include "qvi-macros.h"
#include "qvi-hwloc.h"

#include "quo-vadis.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct hw_name_type_s {
    char const *name;
    qv_hw_obj_type_t type;
} hw_name_type_t;

static const hw_name_type_t nts[] = {
    {QVI_STRINGIFY(QV_HW_OBJ_MACHINE),  QV_HW_OBJ_MACHINE},
    {QVI_STRINGIFY(QV_HW_OBJ_PACKAGE),  QV_HW_OBJ_PACKAGE},
    {QVI_STRINGIFY(QV_HW_OBJ_CORE),     QV_HW_OBJ_CORE},
    {QVI_STRINGIFY(QV_HW_OBJ_PU),       QV_HW_OBJ_PU},
    {QVI_STRINGIFY(QV_HW_OBJ_L1CACHE),  QV_HW_OBJ_L1CACHE},
    {QVI_STRINGIFY(QV_HW_OBJ_L2CACHE),  QV_HW_OBJ_L2CACHE},
    {QVI_STRINGIFY(QV_HW_OBJ_L3CACHE),  QV_HW_OBJ_L3CACHE},
    {QVI_STRINGIFY(QV_HW_OBJ_L4CACHE),  QV_HW_OBJ_L4CACHE},
    {QVI_STRINGIFY(QV_HW_OBJ_L5CACHE),  QV_HW_OBJ_L5CACHE},
    {QVI_STRINGIFY(QV_HW_OBJ_NUMANODE), QV_HW_OBJ_NUMANODE}
};

typedef struct device_name_type_s {
    char const *name;
    qv_device_id_type_t type;
} device_name_type_t;

static const device_name_type_t devnts[] = {
    {QVI_STRINGIFY(QV_DEVICE_ID_UUID),       QV_DEVICE_ID_UUID},
    {QVI_STRINGIFY(QV_DEVICE_ID_PCI_BUS_ID), QV_DEVICE_ID_PCI_BUS_ID},
    {QVI_STRINGIFY(QV_DEVICE_ID_ORDINAL),    QV_DEVICE_ID_ORDINAL}
};

static int
echo_hw_info(
    qvi_hwloc_t *hwl
) {
    const int num_nts = sizeof(nts) / sizeof(hw_name_type_t);

    printf("\n# System Hardware Overview --------------\n");
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

    printf("\n# Task Intersection Overview ------------\n");
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
echo_gpu_info(
    qvi_hwloc_t *hwl
) {
    printf("\n# Discovered GPU Devices --------------\n");

    unsigned ngpus = 0;
    int rc = qvi_hwloc_get_nobjs_in_cpuset(
        hwl,
        QV_HW_OBJ_GPU,
        hwloc_get_root_obj(qvi_hwloc_topo_get(hwl))->cpuset,
        &ngpus
    );
    if (rc != QV_SUCCESS) return rc;
    printf("# Number of GPUs: %u\n", ngpus);

    rc = qvi_hwloc_devices_emit(hwl, QV_HW_OBJ_GPU);
    if (rc != QV_SUCCESS) return rc;

    const unsigned ndevids = sizeof(devnts) / sizeof(device_name_type_t);
    for (unsigned i = 0; i < ngpus; ++i) {
        for (unsigned j = 0; j < ndevids; ++j) {
            char *devids = NULL;
            rc = qvi_hwloc_get_device_in_cpuset(
                hwl,
                QV_HW_OBJ_GPU,
                i,
                hwloc_get_root_obj(qvi_hwloc_topo_get(hwl))->cpuset,
                devnts[j].type,
                &devids
            );
            if (rc != QV_SUCCESS) return rc;
            printf("# Device %u %s = %s\n", i, devnts[j].name, devids);
            free(devids);
        }
    }

    printf("# -------------------------------------\n");
    return rc;
}

int
main(void)
{
    printf("\n# Starting hwloc test\n");

    char const *ers = NULL;
    char *binds = NULL;
    qvi_hwloc_t *hwl;
    hwloc_bitmap_t bitmap = NULL;

    int rc = qvi_hwloc_new(&hwl);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_new() failed";
        goto out;
    }

    rc = qvi_hwloc_topology_init(hwl, NULL);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_init() failed";
        goto out;
    }

    rc = qvi_hwloc_topology_load(hwl);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_load() failed";
        goto out;
    }

    rc = qvi_hwloc_discover_devices(hwl);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_discover_devices() failed";
        goto out;
    }

    rc = echo_hw_info(hwl);
    if (rc != QV_SUCCESS) {
        ers = "echo_hw_info() failed";
        goto out;
    }

    rc = echo_gpu_info(hwl);
    if (rc != QV_SUCCESS) {
        ers = "echo_gpu_info() failed";
        goto out;
    }

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
    printf("\n# cpuset=%s\n", binds);

    rc = echo_task_intersections(hwl, binds);
    if (rc != QV_SUCCESS) {
        ers = "echo_task_intersections() failed";
        goto out;
    }
out:
    if (binds) free(binds);
    if (bitmap) hwloc_bitmap_free(bitmap);
    qvi_hwloc_free(&hwl);
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
