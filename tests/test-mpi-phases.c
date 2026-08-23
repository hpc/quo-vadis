/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-phases.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"
#include <time.h>

static void
sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;              // Whole seconds
    ts.tv_nsec = (ms % 1000) * 1000000; // Remaining nanoseconds

    nanosleep(&ts, NULL);
}

static void
do_omp_things(
    qv_scope_t *scope,
    char *scope_name,
    int base_rank
) {
    int npus;
    int rc = qv_hw_obj_count(scope, QV_HW_OBJ_PU, &npus);
    if (rc != QV_SUCCESS) {
        char const *ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    ctu_logf(
        "[%d] %s: doing OpenMP things on %d PUs...\n",
        base_rank, scope_name, npus
    );
}

static void
do_pthread_things(
    qv_scope_t *scope,
    char *scope_name,
    int base_rank
) {
    int ncores;
    int rc = qv_hw_obj_count(scope, QV_HW_OBJ_CORE, &ncores);
    if (rc != QV_SUCCESS) {
        char const *ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    ctu_logf(
        "[%d] %s: doing Pthread things on %d Cores...\n",
        base_rank, scope_name, ncores
    );
}

#if 0
static void
print_cpus(int rank, char *header, qv_scope_t *scope, char *scope_name)
{
    if (rank == 0) ctu_logf("\n# %s\n", header);
    // Not needed; used so that header prints first
    sleep_ms(1);

    char *binds;
    char const *ers = NULL;
    int rc = qv_bind_string(scope, QV_BIND_STRING_PHYSICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    ctu_logf("[%d] %s: Running on CPUs %s\n",
             rank, scope_name, binds);
    free(binds);
}
#endif

static void
print_resources(int rank, char *header, qv_scope_t *scope, char *scope_name)
{
    ctu_str_t *str = ctu_str_new();
    if (rank == 0)
        ctu_str_appendf(
            str, "\n# %s\n", header
        );
    else
        // Not needed; used so that header prints first
        sleep_ms(1);

    char *binds;
    char const *ers = NULL;
    int rc = qv_bind_string(scope, QV_BIND_STRING_PHYSICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    ctu_str_appendf(
        str, "[%d] %s: Running on CPUs %s\n", rank, scope_name, binds
    );
    free(binds);

    int nnumas;
    rc = qv_hw_obj_count(
        scope,
        QV_HW_OBJ_NUMANODE,
        &nnumas
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ncores;
    rc = qv_hw_obj_count(
        scope,
        QV_HW_OBJ_CORE,
        &ncores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ngpus;
    rc = qv_hw_obj_count(
        scope,
        QV_HW_OBJ_GPU,
        &ngpus
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_str_appendf(
        str,
        "[%d] %s: Got %d NUMAs %d Cores %d GPUs",
        rank, scope_name, nnumas, ncores, ngpus
    );

    if (ngpus > 0) {
        ctu_str_appendf(
            str, "\n[%d] %s: GPUs ", rank, scope_name
        );
        char *gpu;
        for (int i = 0; i < ngpus; i++) {
            qv_device_id(
                 scope, QV_HW_OBJ_GPU, i, QV_DEVICE_ID_PCI_BUS_ID, &gpu
            );
            ctu_str_appendf(
                str, "%s ", gpu
            );
        }
        free(gpu);
    }

    ctu_logf(
        "%s\n", ctu_str_cstr(str)
    );
    ctu_str_del(str);
}

// Split at GPUs.
void split_at_gpu(int rank, qv_scope_t *scope, int color, char *header)
{
    // Split 1: Use a color
    // Split 2: Use flag QV_SPLIT_BLOCK
    // Split 3: Use flag QV_SPLIT_SPREAD
    // Split 4: Use flag QV_SPLIT_USE_ALL

    qv_scope_t *gpu_scope;
    int rc = qv_split_at(
        scope,
        QV_HW_OBJ_GPU,
        color,
        &gpu_scope
    );
    char const *ers = NULL;
    if (rc != QV_SUCCESS) {
        ers = "qv_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Move into my scope
    rc = qv_bind_push(gpu_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    print_resources(rank, header, gpu_scope, "gpu_scope");

    int my_gpu_rank;
    rc = qv_group_rank(
        gpu_scope,
        &my_gpu_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_ngpus;
    rc = qv_hw_obj_count(
        gpu_scope,
        QV_HW_OBJ_GPU,
        &my_ngpus
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_bind_pop(gpu_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(gpu_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}


int
main(
    int argc,
    char **argv
) {
    MPI_Comm comm = MPI_COMM_WORLD;
    char const *ers = NULL;

    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    // Get my communicator's size and rank.
    int comm_size;
    rc = MPI_Comm_size(
        comm,
        &comm_size
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    int comm_rank;
    rc = MPI_Comm_rank(
        comm,
        &comm_rank
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
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

    print_resources(comm_rank, "Base scope",
               base_scope, "base_scope");


    ///////////////////////////////////////////////////////////////////////
    // Phase 1: Everybody works via regular split
    ///////////////////////////////////////////////////////////////////////

    // Split the base scope evenly across workers.
    qv_scope_t *sub_scope;
    rc = qv_split(
        base_scope,
        comm_size, // Number of workers.
        comm_rank, // My group color.
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Push into my sub_scope */
    rc = qv_bind_push(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    print_resources(comm_rank, "Phase 1: Regular split:",
               sub_scope, "sub_scope");

    // What resources did I get?
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

    if (comm_rank == 0)
        ctu_logf(
            "\n# Pthread launch on sub_scope(s)\n"
        );

    // Launch Pthreads on respective sub_scope resources.
    do_pthread_things(sub_scope, " sub_scope", comm_rank);

    // Need to finish Pthread work before GPU work
    rc = qv_barrier(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // E.g., launch one kernel per GPU, if GPUs are available.
    if (comm_rank == 0)
        ctu_logf(
            "\n# GPU launch on sub_scope(s)\n"
        );

    ctu_str_t *gpustr = ctu_str_new();
    ctu_str_appendf(
        gpustr,
        "[%d] %s: launching %d GPU kernel(s)%s\n",
        comm_rank, " sub_scope",
        ngpus, !!ngpus ? " on:" : "."
    );

    for (int i = 0; i < ngpus; i++) {
        char *gpu;
        qv_device_id(
            sub_scope, QV_HW_OBJ_GPU, i,
            QV_DEVICE_ID_PCI_BUS_ID, &gpu
        );
        ctu_str_appendf(
            gpustr, "[%d]->PCI Bus ID = %s\n", comm_rank, gpu
        );
        // Here are examples on how a user might
        // target the GPUs they got in a QV scope.
        //cudaDeviceGetByPCIBusId(&dev, gpu);
        //cudaSetDevice(dev);
        // ** Launch GPU kernels here ** //
        free(gpu);
    }

    ctu_emit(base_scope, CTU_SCOPE_KIND_MPI, "%s", ctu_str_cstr(gpustr));
    ctu_str_del(gpustr);

    // Pop back up to the base scope
    // Todo: Do we need to pass a scope to the pop operation?
    rc = qv_bind_pop(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }


    ///////////////////////////////////////////////////////////////////////
    // Phase 2: One master per resource,
    //          others sleep, ay.
    ///////////////////////////////////////////////////////////////////////
    // We could also do this by finding how many NUMA objects are there in the
    // scope, and then splitting over that number. Then, we could ask for a
    // leader of each subscope. However, this does not guarantee a NUMA split.
    // Thus, we use qv_split_at.

    // Todo: It'd be nice to have a QV_SPLIT_DEFAULT flag
    // so that users don't have to think about about this,
    // unless there's specific requirements, e.g.,
    // SPREAD, BLOCK, USE_ALL
    // Implementation-wise, DEFAULT may simply be an alias
    // to another policy given the specific situation, e.g.,
    // USE_ALL if num_processes less than num_devices
    // Need to document all these flags, too.

    // Split at NUMA domains.
    qv_scope_t *numa_scope;
    rc = qv_split_at(
        base_scope,
        QV_HW_OBJ_NUMANODE,
        QV_SPLIT_SPREAD,
        &numa_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Push into my sub_scope */
    rc = qv_bind_push(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Todo: Need to add ordinal color and SPLIT_USE_ALL
    print_resources(comm_rank, "Phase 2: NUMA split w/SPLIT_SPREAD",
                    numa_scope, "numa_scope");

    // Allow selecting a leader per NUMA.
    int my_numa_rank;
    rc = qv_group_rank(
        numa_scope,
        &my_numa_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // How many NUMAs did I get
    int nnumas;
    rc = qv_hw_obj_count(
        numa_scope,
        QV_HW_OBJ_NUMANODE,
        &nnumas
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Barrier not needed; used for tidy output
    rc = qv_barrier(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Todo: What's the difference between ctu_emit and ctu_logf?
    if (my_numa_rank == 0) {
        ctu_logf("[%d]->NUMA ID %d: Launching OMP region\n",
                 comm_rank, my_numa_rank);
        do_omp_things(numa_scope, "numa_scope", comm_rank);
    } else {
        ctu_logf("[%d]->NUMA ID %d\n", comm_rank, my_numa_rank);
    }

    // Leader works; everybody else waits
    rc = qv_barrier(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Pop back up to the base scope
    // Todo: Do we need to pass a scope to the pop operation?
    rc = qv_bind_pop(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }


    ///////////////////////////////////////////////////////////////////////
    // Phase 3: GPU work!
    ///////////////////////////////////////////////////////////////////////

    // Get the number of GPUs so that we can
    // specify the color/groupid of split_at.
    rc = qv_hw_obj_count(
        base_scope,
        QV_HW_OBJ_GPU,
        &ngpus
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (ngpus == 0) {
        if (comm_rank == 0)
            ctu_logf("->Skipping: No GPUs found!\n");
        goto done;
    }

    // Todo: Create Github Issue
    // Todo: I think we should have the following policies:
    // SPLIT_SPREAD
    //   Only matters when num_tasks < num_scopes
    // SPLIT_COMPACT
    //   Only matters when num_tasks < num_scopes
    // SPLIT_USE_ALL
    //   Only matters when num_tasks < num_scopes
    // SPLIT_DEFAULT
    //   Wrapper pointing to an existing policy given certain conditions
    // Ordinal color

    // Todo: What's the difference between
    // SPLIT_CLOSE and SPLIT_PACKED
    // how about SPLIT_BLOCK?

    // Split 1: Use a color
    split_at_gpu(comm_rank, base_scope, comm_rank % ngpus,
        "Phase 3: GPU split using color comm_rank%ngpus"
    );
    // Split 2: Use QV_SPLIT_SPREAD
    split_at_gpu(comm_rank, base_scope, QV_SPLIT_SPREAD,
        "Phase 3: GPU split using color QV_SPLIT_SPREAD"
    );
    // Split 3: Use QV_SPLIT_PACKED
    // Split 4: Use QV_SPLIT_DEFAULT
    // Split 5: Use QV_SPLIT_USE_ALL

    ///////////////////////////////////////////////////////////////////////
    // Clean up.
    ///////////////////////////////////////////////////////////////////////

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
