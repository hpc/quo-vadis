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
 * @file test-mpi-phases.c
 */

#include "quo-vadis-mpi.h"
#include "qvi-rmi.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define panic(vargs...)                                                        \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, vargs);                                                    \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)



static int
do_pthread_things(int ncores)
{
    printf("# Doing pthread_things with %d cores\n", ncores);
    return 0;
}

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
        panic("%s (rc=%d)", ers, rc);
    }

    int wsize;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    int wrank;
    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    setbuf(stdout, NULL);

    /* Create a QV context */
    qv_context_t *ctx;
    rc = qv_mpi_create(&ctx, comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_create() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Get base scope: RM-given resources */
    qv_scope_t *base_scope;
    rc = qv_scope_get(
        ctx,
        QV_SCOPE_SYSTEM,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ncores;
    rc = qv_scope_nobjs(
        ctx,
        base_scope,
        QV_HW_OBJ_CORE,
        &ncores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Base scope has %d cores\n", wrank, ncores);



    char *binds;
    rc = qv_bind_get_as_string(ctx, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Initially I am running on %s\n", wrank, binds);
    free(binds);

    /* Split the base scope evenly across workers */
    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        ctx,
        base_scope,
        wsize,        // Number of workers
        wrank,        // My group color
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* What resources did I get? */
    rc = qv_scope_nobjs(
        ctx,
        sub_scope,
        QV_HW_OBJ_CORE,
        &ncores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("=> [%d] I got %d cores\n", wrank, ncores);

    /***************************************
     * Phase 1: Everybody works
     ***************************************/

    rc = qv_bind_push(ctx, sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Where did I end up? */
    char *binds1;
    rc = qv_bind_get_as_string(ctx, &binds1);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("=> [%d] I got to run on %s\n", wrank, binds1);
    free(binds1);

#if 1
    /* Launch one thread per core */
    do_pthread_things(ncores);

    /* Launch one kernel per GPU */
    int ngpus;
    rc = qv_scope_nobjs(
        ctx,
        sub_scope,
        QV_HW_OBJ_GPU,
        &ngpus
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Launching %d kernels\n", wrank, ngpus);

    int i, dev;
    char *gpu;
    for (i=0; i<ngpus; i++) {
        qv_scope_get_device(ctx, sub_scope, QV_HW_OBJ_GPU, i, QV_DEVICE_ID_PCI_BUS_ID, &gpu);
        printf("GPU %d PCI Bus ID = %s\n", i, gpu);
        //cudaDeviceGetByPCIBusId(&dev, gpu);
        //cudaSetDevice(dev);
        /* Launch GPU kernels here */
        free(gpu);
    }
#endif

    rc = qv_bind_pop(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    char *binds2;
    rc = qv_bind_get_as_string(ctx, &binds2);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Popped up to %s\n", wrank, binds2);
    free(binds2);

    /***************************************
     * Phase 2: One master per resource,
     *          others sleep, ay.
     ***************************************/

    /* We could also do this by finding how many
       NUMA objects are there in the scope, and
       then splitting over that number. Then,
       we could ask for a leader of each subscope.
       However, this does not guarantee a NUMA split.
       Thus, we use qv_scope_split_at. */
#if 0
    int nnumas, my_numa_id;
    qv_scope_t *numa_scope;

    /* Get the number of NUMA domains so that we can
       specify the color/groupid of split_at */
    rc = qv_scope_nobjs(
        ctx,
        base_scope,
	QV_HW_OBJ_NUMANODE,
        &nnumas
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Split at NUMA domains */
    rc = qv_scope_split_at(
        ctx,
        base_scope,
        QV_HW_OBJ_NUMANODE,
	wrank % nnumas,          // color or group id
        &numa_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Allow selecting a leader per NUMA */
    rc = qv_scope_taskid(
        ctx,
        numa_scope,
        &my_numa_id
    );

    rc = qv_bind_push(ctx, numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int pus;
    if (my_numa_id == 0) {
        /* I am the process lead */
        rc = qv_scope_nobjs(
            ctx,
            numa_scope,
            QV_HW_OBJ_PU,
            &npus
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_nobjs() failed";
            panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
        printf("=> [%d] Launching OMP region with %d threads\n",
            wrank, npus);
        do_omp_things(npus);
    }

    /* Everybody else waits... */
    rc = qv_scope_barrier(ctx, numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_bind_pop(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#endif

    /***************************************
     * Clean up
     ***************************************/

#if 0
    rc = qv_scope_free(ctx, numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#endif

    rc = qv_scope_free(ctx, sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(ctx, base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_barrier(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
out:
    if (qv_mpi_free(ctx) != QV_SUCCESS) {
        ers = "qv_free() failed";
        panic("%s", ers);
    }
    MPI_Finalize();
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
