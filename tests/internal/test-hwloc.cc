/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
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
 * @file test-hwloc.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-hwloc.h"
#include "qvi-utils.h"

#include "quo-vadis.h"
#include "common-test-utils.h"

typedef struct hw_name_type_s {
    char const *name;
    qv_hw_obj_type_t type;
} hw_name_type_t;

static const hw_name_type_t nts[] = {
    {CTU_TOSTRING(QV_HW_OBJ_MACHINE),  QV_HW_OBJ_MACHINE},
    {CTU_TOSTRING(QV_HW_OBJ_PACKAGE),  QV_HW_OBJ_PACKAGE},
    {CTU_TOSTRING(QV_HW_OBJ_CORE),     QV_HW_OBJ_CORE},
    {CTU_TOSTRING(QV_HW_OBJ_PU),       QV_HW_OBJ_PU},
    {CTU_TOSTRING(QV_HW_OBJ_L1CACHE),  QV_HW_OBJ_L1CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L2CACHE),  QV_HW_OBJ_L2CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L3CACHE),  QV_HW_OBJ_L3CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L4CACHE),  QV_HW_OBJ_L4CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L5CACHE),  QV_HW_OBJ_L5CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_NUMANODE), QV_HW_OBJ_NUMANODE}
};

typedef struct device_name_type_s {
    char const *name;
    qv_device_id_type_t type;
} device_name_type_t;

static const device_name_type_t devnts[] = {
    {CTU_TOSTRING(QV_DEVICE_ID_UUID),       QV_DEVICE_ID_UUID},
    {CTU_TOSTRING(QV_DEVICE_ID_PCI_BUS_ID), QV_DEVICE_ID_PCI_BUS_ID},
    {CTU_TOSTRING(QV_DEVICE_ID_ORDINAL),    QV_DEVICE_ID_ORDINAL}
};

static int
echo_hw_info(
    qvi_hwloc *hwl
) {
    const int num_nts = sizeof(nts) / sizeof(hw_name_type_t);

    printf("\n# System Hardware Overview --------------\n");
    for (int i = 0; i < num_nts; ++i) {
        int n;
        int rc = hwl->get_nobjs_by_type(nts[i].type, &n);
        if (rc != QV_SUCCESS) {
            fprintf(
                stderr, "qvi_hwloc_get_nobjs_by_type(%s) failed\n", nts[i].name
            );
            return rc;
        }
        printf("# %s=%d\n", nts[i].name, n);
    }
    printf("# ---------------------------------------\n");
    return QV_SUCCESS;
}

static int
echo_task_intersections(
    qvi_hwloc *hwl,
    char *bitmap_str
) {
    const int num_nts = sizeof(nts) / sizeof(hw_name_type_t);
    const pid_t me = qvi_gettid();

    printf("\n# Task Intersection Overview ------------\n");
    for (int i = 0; i < num_nts; ++i) {
        int nobj;
        int rc = hwl->get_nobjs_by_type(nts[i].type, &nobj);
        if (rc != QV_SUCCESS) {
            fprintf(
                stderr, "qvi_hwloc_get_nobjs_by_type(%s) failed\n", nts[i].name
            );
            return rc;
        }
        for (int objid = 0; objid < nobj; ++objid) {
            int intersects;
            rc = hwl->task_intersects_obj_by_type_id(
                nts[i].type, me, objid, &intersects
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
                bitmap_str, nts[i].name, objid,
                intersects ? "Yes" : "No"
            );
        }
    }
    printf("# ---------------------------------------\n");
    return QV_SUCCESS;
}

static int
echo_gpu_info(
    qvi_hwloc *hwl
) {
    printf("\n# Discovered GPU Devices --------------\n");

    int ngpus = 0;
    int rc = hwl->get_nobjs_in_cpuset(
        QV_HW_OBJ_GPU, hwloc_get_root_obj(hwl->topology_get())->cpuset, &ngpus
    );
    if (rc != QV_SUCCESS) return rc;

    printf("# Number of GPUs: %u\n", ngpus);

    rc = hwl->devices_emit(QV_HW_OBJ_GPU);
    if (rc != QV_SUCCESS) return rc;

    const unsigned ndevids = sizeof(devnts) / sizeof(device_name_type_t);
    for (int i = 0; i < ngpus; ++i) {
        for (unsigned j = 0; j < ndevids; ++j) {
            char *devids = nullptr;
            rc = hwl->get_device_id_in_cpuset(
                QV_HW_OBJ_GPU, i,
                hwloc_get_root_obj(hwl->topology_get())->cpuset,
                devnts[j].type, &devids
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

    char const *ers = nullptr;
    char *binds = nullptr;
    qvi_hwloc *hwl;
    hwloc_bitmap_t bitmap = nullptr;
    pid_t who = qvi_gettid();

    int rc = qvi_new(&hwl);
    if (rc != QV_SUCCESS) {
        ers = "qvi_new() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = hwl->topology_init();
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_init() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = hwl->topology_load();
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_load() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = echo_hw_info(hwl);
    if (rc != QV_SUCCESS) {
        ers = "echo_hw_info() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = echo_gpu_info(hwl);
    if (rc != QV_SUCCESS) {
        ers = "echo_gpu_info() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = hwl->task_get_cpubind(who, &bitmap);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_task_get_cpubind() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qvi_hwloc::bitmap_asprintf(bitmap, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc::bitmap_asprintf() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("\n# cpuset=%s\n", binds);

    rc = echo_task_intersections(hwl, binds);
    if (rc != QV_SUCCESS) {
        ers = "echo_task_intersections() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (binds) free(binds);
    if (bitmap) hwloc_bitmap_free(bitmap);
    qvi_delete(&hwl);

    printf("# Done\n");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
