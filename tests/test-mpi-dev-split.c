/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "mpi.h"
#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

int
main(
    int argc,
    char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

    const int ndevs_tested = 2;
    const qv_hw_obj_type_t devs_tested[2] = {
        QV_HW_OBJ_GPU,
        QV_HW_OBJ_NIC
    };

    int base_ndev[ndevs_tested];
    int rank_ndev[ndevs_tested];

    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }
    // Get base scope.
    qv_scope_t *base_scope;
    rc = qv_mpi_scope_get(
        comm,
        QV_SCOPE_USER,
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

    if (base_scope_rank == 0) {
        ctu_emit_device_info(
            base_scope, CTU_SCOPE_KIND_MPI,
            QV_HW_OBJ_GPU, "base_scope"
        );
        ctu_emit_device_info(
            base_scope, CTU_SCOPE_KIND_MPI,
            QV_HW_OBJ_NIC, "base_scope"
        );
        ctu_emit(base_scope, CTU_SCOPE_KIND_MPI, "\n");
    }
    else {
        // Match both ctu_emit*s to avoid deadlocks.
        ctu_emit(base_scope, CTU_SCOPE_KIND_MPI, "");
        ctu_emit(base_scope, CTU_SCOPE_KIND_MPI, "");
        ctu_emit(base_scope, CTU_SCOPE_KIND_MPI, "");
    }
    // Split the base scope evenly across workers.
    qv_scope_t *rank_scope;
    rc = qv_scope_split(
        base_scope,
        base_scope_size,
        base_scope_rank,
        &rank_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get number of tested devices in my rank_scope.
    for (int i = 0; i < ndevs_tested; ++i) {
        rc = qv_scope_hw_obj_count(
            rank_scope,
            devs_tested[i],
            &rank_ndev[i]
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_hw_obj_count() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }
    // Completely order device output.
    for (int i = 0; i < base_scope_size; ++i) {
        if (base_scope_rank == i) {
            ctu_emit_device_info(
                rank_scope, CTU_SCOPE_KIND_MPI,
                QV_HW_OBJ_GPU, "rank_scope"
            );
            ctu_emit_device_info(
                rank_scope, CTU_SCOPE_KIND_MPI,
                QV_HW_OBJ_NIC, "rank_scope"
            );
            ctu_emit(rank_scope, CTU_SCOPE_KIND_MPI, "\n");
        }
        else {
            ctu_emit(rank_scope, CTU_SCOPE_KIND_MPI, "");
            ctu_emit(rank_scope, CTU_SCOPE_KIND_MPI, "");
            ctu_emit(rank_scope, CTU_SCOPE_KIND_MPI, "");
        }
        rc = qv_scope_barrier(base_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_barrier() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }
    // Verify results.
    for (int i = 0; i < ndevs_tested; ++i) {
        // Get total number of GPUs in base_scope.
        rc = qv_scope_hw_obj_count(
            base_scope, devs_tested[i], &base_ndev[i]
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_hw_obj_count() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
        int total_ndevs;
        rc = MPI_Reduce(
            &rank_ndev[i], &total_ndevs, 1, MPI_INT, MPI_SUM, 0, comm
        );
        if (rc != MPI_SUCCESS) {
            ers = "MPI_Reduce() failed";
            ctu_panic("%s (rc=%d)", ers, rc);
        }

        if (base_ndev[i] == total_ndevs) {
            ctu_pemit(
                base_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0,
                "PASS: Number of %ss match!\n", ctu_obj_name(devs_tested[i])
            );
        }
        else {
            ctu_pemit(
                base_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0,
                "FAIL: Base GPUs=%d do not match aggregate GPUs=%d!\n",
                base_ndev[i], total_ndevs
            );
        }
    }
    // Cleanup.
    qv_scope_free(rank_scope);
    qv_scope_free(base_scope);

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
