/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "mpi.h"
#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

int
main(
    int argc,
    char **argv
) {
    MPI_Comm comm = MPI_COMM_WORLD;

    const int ndevs_tested = 2;
    const qv_hw_type_t devs_tested[2] = {
        QV_HW_GPU,
        QV_HW_NIC
    };

    int base_ndev[ndevs_tested];
    int rank_ndev[ndevs_tested];

    ctu_mpi_check(MPI_Init(&argc, &argv));
    // Get base scope.
    qv_scope_t *base_scope;
    ctu_check(
        qv_mpi_scope(comm, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &base_scope)
    );
    // Get my base_scope's size and my rank.
    int base_scope_size;
    ctu_check(qv_group_size(base_scope, &base_scope_size));

    int base_scope_rank;
    ctu_check(qv_group_rank(base_scope, &base_scope_rank));

    if (base_scope_rank == 0) {
        ctu_emit_device_info(
            base_scope, CTU_SCOPE_KIND_MPI,
            QV_HW_GPU, "base_scope"
        );
        ctu_emit_device_info(
            base_scope, CTU_SCOPE_KIND_MPI,
            QV_HW_NIC, "base_scope"
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
    ctu_check(
        qv_split(base_scope, base_scope_size, base_scope_rank, &rank_scope)
    );
    // Get number of tested devices in my rank_scope.
    for (int i = 0; i < ndevs_tested; ++i) {
        ctu_check(qv_hw_count(rank_scope, devs_tested[i], &rank_ndev[i]));
    }

    ctu_emit_device_info(
        rank_scope, CTU_SCOPE_KIND_MPI,
        QV_HW_GPU, "rank_scope"
    );
    ctu_pemit(rank_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0, "\n");

    ctu_emit_device_info(
        rank_scope, CTU_SCOPE_KIND_MPI,
        QV_HW_NIC, "rank_scope"
    );
    ctu_pemit(rank_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0, "\n");

    // Verify results.
    for (int i = 0; i < ndevs_tested; ++i) {
        // Get total number of GPUs in base_scope.
        ctu_check(qv_hw_count(base_scope, devs_tested[i], &base_ndev[i]));
        int total_ndevs;
        ctu_mpi_check(
            MPI_Reduce(
                &rank_ndev[i], &total_ndevs, 1, MPI_INT, MPI_SUM, 0, comm
            )
        );

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
    qv_free(rank_scope);
    qv_free(base_scope);

    ctu_mpi_check(MPI_Finalize());

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
