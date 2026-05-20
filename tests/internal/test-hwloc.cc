/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-hwloc.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-hwloc.h"
#include "qvi-utils.h"

#include "quo-vadis.h"
#include "common-test-utils.h"

static int
echo_hw_info(
    qvi_hwloc &hwl
) {
    printf("\n# System Hardware Overview --------------\n");
    for (size_t i = 0; i < ctu_hw_obj_name_to_type_tab_size; ++i) {
        int n;
        int rc = hwl.get_nobjs_by_type(ctu_hw_obj_name_to_type_tab[i].type, &n);
        if (rc != QV_SUCCESS) {
            fprintf(
                stderr, "qvi_hwloc_get_nobjs_by_type(%s) failed\n",
                ctu_hw_obj_name_to_type_tab[i].name
            );
            return rc;
        }
        printf("# %s=%d\n", ctu_hw_obj_name_to_type_tab[i].name, n);
    }
    printf("# ---------------------------------------\n");
    return QV_SUCCESS;
}

static int
echo_task_intersections(
    qvi_hwloc &hwl,
    const char *bitmap_str
) {
    const pid_t me = qvi_gettid();

    printf("\n# Task Intersection Overview ------------\n");
    for (size_t i = 0; i < ctu_hw_obj_name_to_type_tab_size; ++i) {
        int nobj;
        int rc = hwl.get_nobjs_by_type(ctu_hw_obj_name_to_type_tab[i].type, &nobj);
        if (rc != QV_SUCCESS) {
            fprintf(
                stderr, "hwloc.get_nobjs_by_type(%s) failed\n", ctu_hw_obj_name_to_type_tab[i].name
            );
            return rc;
        }
        for (int objid = 0; objid < nobj; ++objid) {
            int intersects;
            rc = hwl.task_intersects_obj_by_type_id(
                ctu_hw_obj_name_to_type_tab[i].type, me, objid, &intersects
            );
            if (rc != QV_SUCCESS) {
                fprintf(
                    stderr,
                    "hwloc.task_intersects_obj_by_type_id(%s) failed\n",
                    ctu_hw_obj_name_to_type_tab[i].name
                );
                return rc;
            }
            printf(
                "# %s Intersects With %s %d: %s\n",
                bitmap_str, ctu_hw_obj_name_to_type_tab[i].name, objid,
                intersects ? "Yes" : "No"
            );
        }
    }
    printf("# ---------------------------------------\n");
    return QV_SUCCESS;
}

static int
echo_gpu_info(
    qvi_hwloc &hwl
) {
    printf("\n# Discovered GPU Devices --------------\n");

    size_t ngpus = 0;
    int rc = hwl.get_nobjs_in_cpuset(
        QV_HW_OBJ_GPU, hwl.topology_get_cpuset(), ngpus
    );
    if (rc != QV_SUCCESS) return rc;

    printf("# Number of GPUs: %zu\n", ngpus);

    rc = hwl.devices_emit(QV_HW_OBJ_GPU);
    if (rc != QV_SUCCESS) return rc;

    for (size_t i = 0; i < ngpus; ++i) {
        for (size_t j = 0; j < ctu_devid_name_to_id_tab_size; ++j) {
            std::string devids;
            rc = hwl.get_device_id_in_cpuset(
                QV_HW_OBJ_GPU, i,
                hwl.topology_get_cpuset(),
                ctu_devid_name_to_id_tab[j].devid, devids
            );
            if (rc != QV_SUCCESS) return rc;
            printf(
                "# Device %zu %s = %s\n", i,
                ctu_devid_name_to_id_tab[j].name,
                devids.c_str()
            );
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
    qvi_hwloc hwl;
    qvi_hwloc_bitmap bitmap;
    pid_t who = qvi_gettid();

    int rc = hwl.topology_init(QVI_HWLOC_FLAG_TOPO_FULL);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_init() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = hwl.topology_load();
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

    rc = hwl.task_get_cpubind(who, bitmap);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_task_get_cpubind() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    std::string binds = qvi_hwloc::bitmap_string(bitmap);
    printf("\n# cpuset=%s\n", binds.c_str());

    rc = echo_task_intersections(hwl, binds.c_str());
    if (rc != QV_SUCCESS) {
        ers = "echo_task_intersections() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    printf("# Done\n");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
