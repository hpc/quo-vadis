/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-double-split.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

int
main(
    int argc,
    char **argv
) {
    MPI_Comm comm = MPI_COMM_WORLD;

    ctu_mpi_check(MPI_Init(&argc, &argv), "MPI_Init");

    qv_scope_t *base_scope;
    ctu_check(
        qv_mpi_scope(
            comm,
            QV_SCOPE_USER,
            QV_SCOPE_FLAG_NONE,
            &base_scope
        ),
        "qv_mpi_scope"
    );

    ctu_emit_scope_report(
        base_scope, CTU_SCOPE_KIND_MPI, "           base_scope"
    );

    qv_scope_t *split_at_numa;
    ctu_check(
        qv_split_at(
            base_scope,
            QV_HW_OBJ_NUMANODE,
            QV_SPLIT_PACKED,
            &split_at_numa
        ),
        "qv_split_at"
    );

    ctu_emit_scope_report(
        split_at_numa, CTU_SCOPE_KIND_MPI, "        split_at_numa"
    );

    int ntask_per_numa;
    ctu_check(
        qv_group_size(
            split_at_numa,
            &ntask_per_numa
        ),
        "qv_group_size"
    );

    qv_scope_t *split_cores_from_numa;
    ctu_check(
        qv_split(
            split_at_numa,
            ntask_per_numa,
            QV_SPLIT_PACKED,
            &split_cores_from_numa
        ),
        "qv_split"
    );

    ctu_emit_scope_report(
        split_cores_from_numa, CTU_SCOPE_KIND_MPI, "split_cores_from_numa"
    );

    // How many GPUs do we have in the base scope?
    int ngpus;
    ctu_check(
        qv_hw_obj_count(
            base_scope, QV_HW_OBJ_GPU, &ngpus
        ),
        "qv_hw_obj_count"
    );

    if (ngpus > 0) {
        qv_scope_t *split_at_gpu;
        ctu_check(
            qv_split_at(
                base_scope,
                QV_HW_OBJ_GPU,
                QV_SPLIT_PACKED,
                &split_at_gpu
            ),
            "qv_split_at"
        );

        ctu_emit_device_info(
            split_at_gpu, CTU_SCOPE_KIND_MPI,
            QV_HW_OBJ_GPU, "         split_at_gpu"
        );

        ctu_check(qv_free(split_at_gpu), "qv_free");
    }

    // Free base_scope first to test scope free out-of-order operations.
    ctu_check(qv_free(base_scope), "qv_free");

    ctu_check(qv_free(split_at_numa), "qv_free");

    ctu_check(qv_free(split_cores_from_numa), "qv_free");

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
