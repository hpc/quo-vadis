/*
 * Copyright (c) 2020-2023 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-mpi.c
 */

#include "qvi-task.h"
#include "qvi-mpi.h"
#include "qvi-utils.h"

#include "quo-vadis.h"
#include "qvi-test-common.h"

int
main(
    int argc,
    char **argv
) {
    char const *ers = NULL;
    int *evens = NULL;

    MPI_Comm comm = MPI_COMM_WORLD;

    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    int wsize;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    qvi_mpi_t *mpi = NULL;
    rc = qvi_mpi_new(&mpi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_new() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qvi_mpi_init(mpi, comm);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_init() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_mpi_group_t *node_group;
    rc = qvi_mpi_group_new(&node_group);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_group_new() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qvi_mpi_group_lookup_by_id(mpi, QVI_MPI_GROUP_NODE, node_group);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_group_lookup_by_id() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int nsize = qvi_mpi_group_size(node_group);
    int group_id = qvi_mpi_group_id(node_group);

    qvi_task_t *task = qvi_mpi_task_get(mpi);
    int64_t task_gid = qvi_task_gid(task);
    int task_lid = qvi_task_lid(task);

    printf(
        "Hello from %s gid=%" PRId64
        " (lid=%d, nsize=%d, node_gid=%d) of wsize=%d\n",
        qvi_task_type(task) == QV_TASK_TYPE_PROCESS ? "process" : "thread",
        task_gid, task_lid, nsize, group_id, wsize
    );

    qvi_mpi_group_t *node_even_group = NULL;
    const int n_evens = (wsize + 1) / 2;
    evens = calloc(n_evens, sizeof(*evens));
    if (!evens) {
        ers = "calloc() failed";
        qvi_test_panic("%s", ers);
    }

    for (int r = 0, i = 0; r < wsize; ++r) {
        if (r % 2 == 0) evens[i++] = r;
    }

    rc = qvi_mpi_group_create_from_ids(
        mpi, node_group, n_evens, evens, &node_even_group
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_group_create_from_ids() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (node_even_group) {
        printf(
            "GID=%" PRId64 " Task %d says hello from node even group!\n",
            task_gid, task_lid
        );
    }

    qvi_mpi_group_t *world_group;
    rc = qvi_mpi_group_create_from_mpi_comm(mpi, comm, &world_group);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_group_create_from_mpi_comm() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int world_group_id = qvi_mpi_group_id(world_group);
    int world_group_size = qvi_mpi_group_size(world_group);
    printf(
        "GID=%" PRId64 " World group task %d of %d says hello!\n",
        task_gid, world_group_id, world_group_size
    );

    rc = qvi_mpi_finalize(mpi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_finalize() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_mpi_group_free(&node_even_group);
    qvi_mpi_group_free(&node_group);
    qvi_mpi_group_free(&world_group);
    qvi_mpi_free(&mpi);
    if (evens) free(evens);

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
