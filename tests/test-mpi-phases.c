/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-phases.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

static int
do_omp_things(
    int rank,
    int npus
) {
    printf("[%d] Doing OpenMP things with %d PUs\n", rank, npus);
    return 0;
}

static void
do_pthread_things(
    qv_scope_t *scope,
    char *scope_name,
    int rank
) {
    int ncores;
    int rc = qv_hw_obj_count(scope, QV_HW_OBJ_CORE, &ncores);
    if (rc != QV_SUCCESS) {
        char const *ers = "qv_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf(
        "[%d] %s: doing Pthread things on %d cores...\n",
        rank, scope_name, ncores
    );
    fflush(stdout);
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
    // Get base scope.
    qv_scope_t *base_scope;
    rc = qv_mpi_scope(
        comm,
        QV_SCOPE_USER,
        QV_SCOPE_FLAG_NONE,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get my base_scope's size and my rank.
    int base_scope_size;
    rc = qv_group_size(
        base_scope,
        &base_scope_size
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int base_scope_rank;
    rc = qv_group_rank(
        base_scope,
        &base_scope_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Report base_scope information, pre-split.
    ctu_emit_scope_report(
        base_scope, CTU_SCOPE_KIND_MPI, "base_scope"
    );
    // Split the base scope evenly across workers.
    qv_scope_t *sub_scope;
    rc = qv_split(
        base_scope,
        base_scope_size, // Number of workers
        base_scope_rank, // My group color
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // What resources did I get?
    ctu_pemit(
        base_scope,
        CTU_SCOPE_KIND_MPI,
        base_scope_rank == 0,
        "\n# Phase 1: Regular split\n"
    );
    ctu_emit_scope_report(
        sub_scope, CTU_SCOPE_KIND_MPI, " sub_scope"
    );
    ////////////////////////////////////////////////////////////////////////////
    // Phase 1: Everybody works.
    ////////////////////////////////////////////////////////////////////////////
    ctu_pemit(
        base_scope,
        CTU_SCOPE_KIND_MPI,
        base_scope_rank == 0,
        "\n# Pthread launch on sub_scope(s)\n"
    );
    // E.g., launch Pthreads on respective sub_scope resources.
    do_pthread_things(sub_scope, " sub_scope", base_scope_rank);
    // Not needed in practice. Used for tidy example output.
    rc = qv_barrier(base_scope);
    if (rc != QV_SUCCESS) {
      ers = "qv_context_barrier() failed";
      ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Launch one kernel per GPU, if GPUs are available.
    int ngpus;
    rc = qv_hw_obj_count(
        sub_scope,
        QV_HW_OBJ_GPU,
        &ngpus
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_pemit(
        base_scope,
        CTU_SCOPE_KIND_MPI,
        base_scope_rank == 0,
        "\n# GPU launch on sub_scope(s)\n"
    );

    ctu_emit(
        base_scope,
        CTU_SCOPE_KIND_MPI,
        "[%d] %s: launching %d GPU kernels...\n",
        base_scope_rank, " sub_scope", ngpus
    );

    char *gpu;
    for (int i = 0; i < ngpus; i++) {
        qv_device_id(
            sub_scope, QV_HW_OBJ_GPU, i,
            QV_DEVICE_ID_PCI_BUS_ID, &gpu
        );
        ctu_emit(
            base_scope, CTU_SCOPE_KIND_MPI,
            "[%d] GPU %d PCI Bus ID = %s\n", base_scope_rank, i, gpu
        );
        //cudaDeviceGetByPCIBusId(&dev, gpu);
        //cudaSetDevice(dev);
        // ** Launch GPU kernels here ** //
        free(gpu);
    }

    ctu_emit(
        base_scope,
        CTU_SCOPE_KIND_MPI,
        "\n"
    );

    char *binds;
    rc = qv_bind_string(base_scope, QV_BIND_STRING_LOGICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Popped up to %s\n", base_scope_rank, binds);
    free(binds);

    /* Keep printouts separate for each phase */
    rc = qv_barrier(base_scope);
    if (rc != QV_SUCCESS) {
      ers = "qv_context_barrier() failed";
      ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /***************************************
     * Phase 2: One master per resource,
     *          others sleep, ay.
     ***************************************/

    /* We could also do this by finding how many
       NUMA objects are there in the scope, and
       then splitting over that number. Then,
       we could ask for a leader of each subscope.
       However, this does not guarantee a NUMA split.
       Thus, we use qv_split_at. */
    if (base_scope_rank == 0)
      printf("\n===Phase 2: NUMA split===\n");

#if 1
    int nnumas, my_numa_rank;
    qv_scope_t *numa_scope;

    /* Get the number of NUMA domains so that we can
       specify the color/groupid of split_at */
    rc = qv_hw_obj_count(
        base_scope,
        QV_HW_OBJ_NUMANODE,
        &nnumas
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Split at NUMA domains */
    rc = qv_split_at(
        base_scope,
        QV_HW_OBJ_NUMANODE,
        base_scope_rank % nnumas, // color or group id
        &numa_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Allow selecting a leader per NUMA */
    rc = qv_group_rank(
        numa_scope,
        &my_numa_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    printf("[%d]: #NUMAs=%d numa_scope_id=%d\n",
       base_scope_rank, nnumas, my_numa_rank);

    rc = qv_bind_push(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_nnumas;
    rc = qv_hw_obj_count(
            numa_scope,
            QV_HW_OBJ_NUMANODE,
            &my_nnumas);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Where did I end up? */
    rc = qv_bind_string(numa_scope, QV_BIND_STRING_LOGICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("=> [%d] Split@NUMA: got %d NUMAs, running on %s\n",
       base_scope_rank, my_nnumas, binds);
    free(binds);


    int npus;
    if (my_numa_rank == 0) {
        /* I am the process lead */
        rc = qv_hw_obj_count(
            numa_scope,
            QV_HW_OBJ_PU,
            &npus
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_hw_obj_count() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
        printf("=> [%d] NUMA leader: Launching OMP region\n", base_scope_rank);
        do_omp_things(base_scope_rank, npus);
    }

    /* Everybody else waits... */
    rc = qv_barrier(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_context_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_bind_pop(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_bind_string(base_scope, QV_BIND_STRING_LOGICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Popped up to %s\n", base_scope_rank, binds);
    free(binds);

    /* Keep printouts separate for each phase */
    rc = qv_barrier(base_scope);
    if (rc != QV_SUCCESS) {
      ers = "qv_context_barrier() failed";
      ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#endif

    /***************************************
     * Phase 3:
     *   GPU work!
     ***************************************/
    if (base_scope_rank == 0)
        printf("\n===Phase 3: GPU split===\n");

    int my_gpu_rank;
    qv_scope_t *gpu_scope;

    /* Get the number of GPUs so that we can
       specify the color/groupid of split_at */
    rc = qv_hw_obj_count(base_scope,
            QV_HW_OBJ_GPU, &ngpus);
    if (rc != QV_SUCCESS) {
      ers = "qv_hw_obj_count() failed";
      ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (ngpus == 0) {
        if (base_scope_rank == 0) printf("Skipping: no GPUs found\n");
        goto done;
    }

    /* Split at GPUs */
    rc = qv_split_at(
        base_scope,
        QV_HW_OBJ_GPU,
        base_scope_rank % ngpus, // color or group id
        &gpu_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Allow selecting a leader per NUMA */
    rc = qv_group_rank(
        gpu_scope,
        &my_gpu_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_bind_push(gpu_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_ngpus;
    rc = qv_hw_obj_count(
            gpu_scope,
            QV_HW_OBJ_GPU,
            &my_ngpus);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Where did I end up? */
    rc = qv_bind_string(gpu_scope, QV_BIND_STRING_LOGICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("=> [%d] Split@GPU: got %d GPUs, running on %s\n",
       base_scope_rank, my_ngpus, binds);
    free(binds);

    for (int i=0; i<my_ngpus; i++) {
      qv_device_id(gpu_scope, QV_HW_OBJ_GPU,
              i, QV_DEVICE_ID_PCI_BUS_ID, &gpu);
      printf("   [%d] GPU %d PCI Bus ID = %s\n", base_scope_rank, i, gpu);
      free(gpu);
    }

    /***************************************
     * Clean up
     ***************************************/
    rc = qv_free(gpu_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

done:
    rc = qv_free(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
