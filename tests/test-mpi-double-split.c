/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-double-split.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

static void
emit_gpu_info(
    qv_scope_t *base_scope,
    int base_scope_size,
    int base_scope_rank
) {
    char const *ers = NULL;

    qv_scope_t *split_at_gpu;
    int rc = qv_scope_split_at(
        base_scope,
        QV_HW_OBJ_GPU,
        QV_SCOPE_SPLIT_PACKED,
        &split_at_gpu
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    for (int i = 0; i < base_scope_size; ++i) {
        if (base_scope_rank == i) {
            ctu_emit_device_info(
                split_at_gpu, CTU_SCOPE_KIND_MPI,
                QV_HW_OBJ_GPU, "         split_at_gpu"
            );
        }
        else {
            ctu_emit(split_at_gpu, CTU_SCOPE_KIND_MPI, "");
        }
        rc = qv_scope_barrier(base_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_barrier() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

    rc = qv_scope_free(split_at_gpu);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

int
main(
    int argc,
    char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

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

    for (int i = 0; i < base_scope_size; ++i) {
        if (base_scope_rank == i) {
            ctu_emit_scope_report(
                base_scope, CTU_SCOPE_KIND_MPI, "           base_scope"
            );
        }
        else {
            ctu_emit(base_scope, CTU_SCOPE_KIND_MPI, "");
        }
        rc = qv_scope_barrier(base_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_barrier() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

    qv_scope_t *split_at_numa;
    rc = qv_scope_split_at(
        base_scope,
        QV_HW_OBJ_NUMANODE,
        QV_SCOPE_SPLIT_PACKED,
        &split_at_numa
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    for (int i = 0; i < base_scope_size; ++i) {
        if (base_scope_rank == i) {
            ctu_emit_scope_report(
                split_at_numa, CTU_SCOPE_KIND_MPI, "        split_at_numa"
            );
        }
        else {
            ctu_emit(split_at_numa, CTU_SCOPE_KIND_MPI, "");
        }
        rc = qv_scope_barrier(base_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_barrier() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

    int ntask_per_numa;
    rc = qv_scope_group_size(
        split_at_numa,
        &ntask_per_numa
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *split_cores_from_numa;
    rc = qv_scope_split(
        split_at_numa,
        ntask_per_numa,
        QV_SCOPE_SPLIT_PACKED,
        &split_cores_from_numa
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    for (int i = 0; i < base_scope_size; ++i) {
        if (base_scope_rank == i) {
            ctu_emit_scope_report(
                split_cores_from_numa, CTU_SCOPE_KIND_MPI, "split_cores_from_numa"
            );
        }
        else {
            ctu_emit(split_cores_from_numa, CTU_SCOPE_KIND_MPI, "");
        }
        rc = qv_scope_barrier(base_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_barrier() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }
    // How many GPUs do we have?
    int ngpus;
    rc = qv_scope_hw_obj_count(
        base_scope, QV_HW_OBJ_GPU, &ngpus

    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (ngpus > 0) {
        emit_gpu_info(base_scope, base_scope_size, base_scope_rank);
    }

    // Free base_scope first to test scope free out-of-order operations.
    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(split_at_numa);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(split_cores_from_numa);
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
