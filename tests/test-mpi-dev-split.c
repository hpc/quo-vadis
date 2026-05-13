/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-mpi-phases.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

int
main(
    int argc,
    char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

    /* Initialization */
    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    int wsize;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    int wrank;
    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    setbuf(stdout, NULL);

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

    int ngpus;
    rc = qv_scope_hw_obj_count(base_scope, QV_HW_OBJ_GPU, &ngpus);
    if (rc != QV_SUCCESS) {
      ers = "qv_scope_hw_obj_count() failed";
      ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (wrank == 0 && ngpus == 0) {
        printf("Skipping: no GPUs found!\n");
        goto done;
    }

    qv_scope_t *gpu_scope;
    rc = qv_scope_split_at(
        base_scope,
        QV_HW_OBJ_GPU,
        QV_SCOPE_SPLIT_AFFINITY_PRESERVING,
        &gpu_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_gpu_rank;
    rc = qv_scope_group_rank(
        gpu_scope,
        &my_gpu_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_bind_push(gpu_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_ngpus;
    rc = qv_scope_hw_obj_count(
            gpu_scope,
            QV_HW_OBJ_GPU,
            &my_ngpus);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Where did I end up? */
    char *binds;
    rc = qv_scope_bind_string(gpu_scope, QV_BIND_STRING_LOGICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf(
        "=> [%d] Split@GPU: got %d GPUs, running on %s\n",
        wrank, my_ngpus, binds
    );
    free(binds);

    for (int i = 0; i < my_ngpus; ++i) {
        char *gpu;
        qv_scope_device_id_get(
            gpu_scope, QV_HW_OBJ_GPU, i, QV_DEVICE_ID_PCI_BUS_ID, &gpu
        );
        printf("   [%d] GPU %d PCI Bus ID = %s\n", wrank, i, gpu);
        free(gpu);
    }

    rc = qv_scope_free(gpu_scope);
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
