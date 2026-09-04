/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file example-mpi-phases.c
 */

#include "quo-vadis-mpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define panic(...)                                                             \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)

void
sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;              // Whole seconds
    ts.tv_nsec = (ms % 1000) * 1000000; // Remaining nanoseconds

    nanosleep(&ts, NULL);
}

static char
get_phase_id(void) {
    static char phase = 'A';
    return phase++;
}

static void
do_omp_things(qv_scope_t *scope, int rank, char phase)
{
    int npus;
    int rc = qv_hw_obj_count(scope, QV_HW_OBJ_PU, &npus);
    if (rc != QV_SUCCESS) {
        char const *ers = "qv_hw_obj_count() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%c%d]-> Doing OpenMP things on %d PUs...\n",
           phase, rank, npus);
}

static void
do_pthread_things(qv_scope_t *scope, int rank, char phase)
{
    int ncores;
    int rc = qv_hw_obj_count(scope, QV_HW_OBJ_CORE, &ncores);
    if (rc != QV_SUCCESS) {
        char const *ers = "qv_hw_obj_count() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%c%d]-> Doing Pthread things on %d Cores...\n",
           phase, rank, ncores);
}

static void
print_resources(int rank, char *header, qv_scope_t *scope, char phase)
{
    int nc = 0;
    char str[256];

    if (rank == 0)
        printf("%c### %s\n", phase, header);

    char *binds;
    char const *ers = NULL;
    int rc = qv_bind_string(scope, QV_BIND_STRING_PHYSICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    nc += snprintf(str+nc, sizeof(str)-nc,
                   "[%c%d] Running on CPUs %s\n",
                   phase, rank, binds);
    free(binds);

    int nnumas;
    rc = qv_hw_obj_count(scope, QV_HW_OBJ_NUMANODE, &nnumas);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ncores;
    rc = qv_hw_obj_count(scope, QV_HW_OBJ_CORE, &ncores);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ngpus;
    rc = qv_hw_obj_count(scope, QV_HW_OBJ_GPU, &ngpus);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    nc += snprintf(str+nc, sizeof(str)-nc,
                   "[%c%d] Got %d NUMAs %d Cores %d GPUs\n",
                   phase, rank, nnumas, ncores, ngpus);

    if (ngpus > 0) {
        nc += snprintf(str+nc, sizeof(str)-nc,
                       "[%c%d] GPUs ",
                       phase, rank);
        char *gpu;
        for (int i = 0; i < ngpus; i++) {
            qv_device_id(scope, QV_HW_OBJ_GPU, i,
                         QV_DEVICE_ID_PCI_BUS_ID, &gpu);
            nc += snprintf(str+nc, sizeof(str)-nc,
                           "%s ",
                           gpu);
        }
        free(gpu);
    }

    printf("%s\n", str);
}

static void
split_at_device(int rank, qv_scope_t *scope,
                qv_hw_obj_type_t device, int color,
                char phase, char *header)
{
    qv_scope_t *dev_scope;
    int rc = qv_split_at(
        scope,
        device,
        color,
        &dev_scope
    );
    char const *ers = NULL;
    if (rc != QV_SUCCESS) {
        ers = "qv_split_at() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Move into my scope
    rc = qv_bind_push(dev_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    print_resources(rank, header, dev_scope, phase);

    rc = qv_bind_pop(dev_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(dev_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}


int main(int argc, char **argv)
{
    MPI_Comm comm = MPI_COMM_WORLD;
    char const *ers = NULL;

    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    // My communicator's size and rank.
    int comm_size;
    rc = MPI_Comm_size(
        comm,
        &comm_size
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    int comm_rank;
    rc = MPI_Comm_rank(
        comm,
        &comm_rank
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        panic("%s (rc=%d)", ers, rc);
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
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // What resources do I have in the given scope
    print_resources(comm_rank, "Base scope",
                    base_scope, get_phase_id());



    ///////////////////////////////////////////////////////////////////////
    // Phase 1: Everybody works via regular split
    ///////////////////////////////////////////////////////////////////////

    // Split the base scope evenly across workers.
    qv_scope_t *sub_scope;
    rc = qv_split(
        base_scope,
        comm_size, // Number of pieces
        comm_rank, // My color/piece
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_split() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Push into my sub_scope */
    rc = qv_bind_push(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // What resources did I get?
    print_resources(comm_rank, "Phase 1: Regular split",
                    sub_scope, get_phase_id());

    char phase = get_phase_id();
    if (comm_rank == 0)
        printf("%c###-> Pthread launch on sub_scope(s)\n", phase);

    // Launch Pthreads on respective sub_scope resources.
    do_pthread_things(sub_scope, comm_rank, phase);

    // Need to finish Pthread work before GPU work
    rc = qv_barrier(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ngpus;
    rc = qv_hw_obj_count(sub_scope, QV_HW_OBJ_GPU, &ngpus);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Launch one kernel per GPU, if GPUs are available.
    if (ngpus > 0) {
        phase = get_phase_id();
        if (comm_rank == 0)
            printf("%c###-> GPU launch on sub_scope(s)\n", phase);
        printf("[%c%d]--> Launching %d GPU kernel(s)%s\n",
               phase, comm_rank, ngpus, !!ngpus ? " on:" : ".");
    }

    for (int i = 0; i < ngpus; i++) {
        char *gpu;
        qv_device_id(sub_scope, QV_HW_OBJ_GPU, i,
                     QV_DEVICE_ID_PCI_BUS_ID, &gpu);
        printf("[%c%d]--> PCI Bus ID = %s\n", phase, comm_rank, gpu);
        // Here are examples on how a user might
        // target the GPUs they got in a QV scope.
        //cudaDeviceGetByPCIBusId(&dev, gpu);
        //cudaSetDevice(dev);
        // ** Launch GPU kernels here ** //
        free(gpu);
    }

    // Pop back up to the base scope
    rc = qv_bind_pop(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Make sure everbody finishes GPU work
    // Todo: Is there an implicit barrier associated with pop()?
    rc = qv_barrier(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }


    ///////////////////////////////////////////////////////////////////////
    // Phase 2: One master per NUMA, others sleep.
    ///////////////////////////////////////////////////////////////////////
    // We could also do this by finding how many NUMA objects are there in the
    // scope, and then splitting over that number. Then, we could ask for a
    // leader of each subscope. However, this may not guarantee respecting
    // NUMA boundaries. Thus, we use qv_split_at.

    // Split at NUMA domains
    qv_scope_t *numa_scope;
    rc = qv_split_at(
        base_scope,
        QV_HW_OBJ_NUMANODE,
        QV_SPLIT_AUTO,
        &numa_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_split_at() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Push into my sub_scope */
    rc = qv_bind_push(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    print_resources(comm_rank, "Phase 2: NUMA split w/SPLIT_AUTO",
                    numa_scope, get_phase_id());

    // Allow selecting a leader per NUMA.
    int my_numa_rank;
    rc = qv_group_rank(
        numa_scope,
        &my_numa_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_group_rank() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
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
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    phase = get_phase_id();
    if (my_numa_rank == 0) {
        printf("[%c%d]-> NUMA ID %d: Launching OMP region\n",
               phase, comm_rank, my_numa_rank);
        do_omp_things(numa_scope, comm_rank, phase);
    } else {
        printf("[%c%d]-> NUMA ID %d: Resting\n",
               phase, comm_rank, my_numa_rank);
    }

    // NUMA leader works; everybody else waits
    rc = qv_barrier(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Pop back up to the base scope
    rc = qv_bind_pop(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Test NUMA split using ordinal color
    rc = qv_hw_obj_count(
        base_scope,
        QV_HW_OBJ_NUMANODE,
        &nnumas
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    split_at_device(comm_rank, base_scope, QV_HW_OBJ_NUMANODE,
                    comm_rank % nnumas, get_phase_id(),
                    "Phase 2: NUMA split w/comm_rank % nnumas");


    ///////////////////////////////////////////////////////////////////////
    // Phase 3: GPU work
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
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (ngpus == 0) {
        phase = get_phase_id();
        if (comm_rank == 0)
            printf("%c### Phase 3: No GPUs found\n", phase);
        goto done;
    }

    // Split 1: Use ordinal color
    split_at_device(comm_rank, base_scope, QV_HW_OBJ_GPU,
                    comm_rank % ngpus, get_phase_id(),
                    "Phase 3: GPU split w/comm_rank % ngpus");
    // Split 2: Use QV_SPLIT_SPREAD
    split_at_device(comm_rank, base_scope, QV_HW_OBJ_GPU,
                    QV_SPLIT_SPREAD, get_phase_id(),
                    "Phase 3: GPU split w/QV_SPLIT_SPREAD");
    // Split 3: Use QV_SPLIT_PACKED
    split_at_device(comm_rank, base_scope, QV_HW_OBJ_GPU,
                    QV_SPLIT_PACKED, get_phase_id(),
                    "Phase 3: GPU split w/QV_SPLIT_PACKED");
    // Split 4: Use QV_SPLIT_AUTO
    split_at_device(comm_rank, base_scope, QV_HW_OBJ_GPU,
                    QV_SPLIT_AUTO, get_phase_id(),
                    "Phase 3: GPU split w/QV_SPLIT_AUTO");

    ///////////////////////////////////////////////////////////////////////
    // Clean up.
    ///////////////////////////////////////////////////////////////////////

done:
    rc = qv_free(numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
