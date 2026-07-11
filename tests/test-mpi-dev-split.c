/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

int
main(
    int argc,
    char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;
    qv_hw_obj_type_t target_dev = QV_HW_OBJ_GPU;
    char const *dev_name = ctu_obj_name(target_dev);

    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }
    // Get base scope.
    qv_scope_t *base_scope;
    rc = qv_mpi_scope_get(
        comm,
        QV_SCOPE_JOB,
        QV_SCOPE_FLAG_NONE,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope_get() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get my base_scope's size and my rank.
    int base_scope_size;
    rc = qv_scope_group_size(
        base_scope,
        &base_scope_size
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int base_scope_rank;
    rc = qv_scope_group_rank(
        base_scope,
        &base_scope_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ndevs;
    rc = qv_scope_hw_obj_count(base_scope, target_dev, &ndevs);
    if (rc != QV_SUCCESS) {
      ers = "qv_scope_hw_obj_count() failed";
      ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (ndevs == 0) {
        ctu_pemit(
            base_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0,
            "# Skipping: no %ss found!\n", dev_name
        );
        goto done;
    }
    else {
        ctu_pemit(
            base_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0,
            "# Total number of %ss found: %d\n", dev_name, ndevs
        );
    }

    qv_scope_t *dev_scope;
    rc = qv_scope_split_at(
        base_scope,
        target_dev,
        QV_SCOPE_SPLIT_AFFINITY_PRESERVING,
        &dev_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_dev_rank;
    rc = qv_scope_group_rank(
        dev_scope,
        &my_dev_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_ndevs;
    rc = qv_scope_hw_obj_count(
        dev_scope,
        target_dev,
        &my_ndevs
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Completely order device output.
    for (int i = 0; i < base_scope_size; ++i) {
        if (base_scope_rank == i) {
            ctu_emit_device_info(
                dev_scope, CTU_SCOPE_KIND_MPI, target_dev, "Device Scope"
            );
            ctu_emit(dev_scope, CTU_SCOPE_KIND_MPI, "\n");
        }
        else {
            ctu_emit(dev_scope, CTU_SCOPE_KIND_MPI, "");
            ctu_emit(dev_scope, CTU_SCOPE_KIND_MPI, "");
        }
        rc = qv_scope_barrier(base_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_barrier() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

    rc = qv_scope_free(dev_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
done:
    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
