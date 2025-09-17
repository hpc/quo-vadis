---
layout: default
title: Concepts
---

Quo-Vadis is a runtime system that helps hybrid applications make efficient use
of heterogeneous hardware, ease programmability in the presence of multiple
programming abstractions, and enable portability across systems.

The runtime system abstracts out low-level details of the hardware and presents
an architecture-independent interface applications can use to leverage local
resources automatically and without user intervention. This interface enables
portability across computing platforms.

Conceptually, Quo-Vadis is centered around `scopes`, an abstraction representing
a collection of hardware resources and task groupings. Users interact with a
system through scopes and move in and out of scopes through `push` and `pop`
operations (similar to its predecessor Quo). Users can create scopes
automatically through `split` operations that split the resources into subsets
that compute workers (e.g., tasks or threads) can use. Each resource subset or
sub-scope maximizes locality for a given worker.


## Features

* Architecture-independent API.
* Hardware/software state queries including hardware topology and task
  configuration.
* Efficient partition of resources among workers.
* Efficient task reconfiguration including placement and affinity.
* Composable and general.
* Open-source implementation.
* C and Fortran interfaces.

## Main Concepts

### Scopes: Predefined and Derived

```C
// Predefined
qv_process_scope_get(QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &uscope);

// Derived
// See qv_scope_split() and qv_scope_split_at()
```

### Hardware and Software Queries

```C
// Hardware topology queries
rc = qv_scope_hw_obj_count(uscope, QV_HW_OBJ_GPU, &ngpus);

// Caller-state queries
qv_scope_bind_string(uscope, QV_BIND_STRING_LOGICAL, &bindstr);
```

### Split Operations: Distribute Resources to Workers

```C
// Splits are collective operations: Every caller gets its own subscope

// For example: comm_size as num_workers, comm_rank as color
qv_scope_split(ctx, base_scope, size, rank, &sub_scope);

// Or split by a specific resource type
qv_scope_split_at(ctx, base_scope, QV_HW_OBJ_NUMANODE, rank%nnumas, &numa_scope);

// including accelerators
qv_scope_split_at(ctx, base_scope, QV_HW_OBJ_GPU, rank%ngpus, &gpu_scope);
```

### Stack-Based Semantics to Map Workers to Hardware

```C
qv_bind_push(ctx, sub_scope);

a_library_call(in_args, &result);

qv_bind_pop(ctx);
```

### Leader selection through scope-based task IDs and scope-based barriers

```C
qv_scope_taskid(ctx, numa_scope, &my_numa_id);

// I'm the leader, taking over NUMA domain
if (my_numa_id == 0) {
   qv_bind_push(ctx, numa_scope);
   a_library_call(in_args, &result);
   qv_bind_pop(ctx);
}

// Everybody else waits
qv_scope_barrier(ctx, numa_scope);
```

### Accelerator support

```C
// Get the PCI bus ID of the ith GPU of a given scope
qv_scope_get_device(ctx, scope, QV_HW_OBJ_GPU, i, QV_DEVICE_ID_PCI_BUS_ID, &gpu);

// For HIP (similar for CUDA)
hipDeviceGetByPCIBusId(&device, gpu);

hipSetDevice(device);

launch_gpu_kernels(in_args, &result);
```

## A simple but powerful example

See [tests](../tests) for working examples.


```C
#include "quo-vadis-mpi.h"
#include "quo-vadis-thread.h"
#include "common-test-utils.h"
#include "omp.h"

void *
thread_work(
    void *arg
) {
    // Do work.
    printf("Hello from pid=%d,tid=%d\n", getpid(), ctu_gettid());
    ctu_emit_task_bind(arg);
    return NULL;
}

int
main(
    int argc, char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

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
    ////////////////////////////////////////////////////////////////////////////
    // Use the process interface for NUMA.
    ////////////////////////////////////////////////////////////////////////////
    // Get the base scope: RM-given resources.
    qv_scope_t *base_scope;
    rc = qv_mpi_scope_get(
        comm, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope_get() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int nnumas;
    rc = qv_scope_hw_obj_count(base_scope, QV_HW_OBJ_NUMANODE, &nnumas);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Split at NUMA domains.
    qv_scope_t *numa_scope;
    rc = qv_scope_split_at(
        base_scope, QV_HW_OBJ_NUMANODE,
        wrank % nnumas, &numa_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // When there's more tasks than NUMAs,
    // make sure each task has exclusive resources.
    int lrank;
    rc = qv_scope_group_rank(numa_scope, &lrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ntasks_per_numa;
    rc = qv_scope_group_size(numa_scope, &ntasks_per_numa);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *subnuma;
    rc = qv_scope_split(
        numa_scope, ntasks_per_numa,
        lrank % ntasks_per_numa, &subnuma
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get the number of cores and pus per NUMA part.
    int ncores;
    rc = qv_scope_hw_obj_count(subnuma, QV_HW_OBJ_CORE, &ncores);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int npus;
    rc = qv_scope_hw_obj_count(subnuma, QV_HW_OBJ_PU, &npus);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    ////////////////////////////////////////////////////////////////////////////
    // OpenMP: Launch one thread per core.
    ////////////////////////////////////////////////////////////////////////////
    const int nthreads = ncores;
    int *thread_coloring = NULL; // Default thread assignment.
    qv_scope_t **th_scopes;
    rc = qv_thread_scope_split_at(
        subnuma, QV_HW_OBJ_CORE, thread_coloring, nthreads, &th_scopes
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    omp_set_num_threads(nthreads);
    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        // Each thread works with its new hardware affinity.
        qv_scope_bind_push(th_scopes[tid]);
        thread_work(th_scopes[tid]);
    }
    // When we are done with the scope, clean up.
    rc = qv_thread_scopes_free(nthreads, th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scopes_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    ////////////////////////////////////////////////////////////////////////////
    // POSIX threads:
    // * Launch one thread per hardware thread
    // *   Policy-based placement
    // *   Note num_threads < num_places on SMT
    ////////////////////////////////////////////////////////////////////////////
    thread_coloring = QV_THREAD_SCOPE_SPLIT_PACKED,
    rc = qv_thread_scope_split_at(
        subnuma, QV_HW_OBJ_PU, thread_coloring, nthreads, &th_scopes
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    pthread_t *pthrds = calloc(nthreads, sizeof(pthread_t));
    if (!pthrds) {
        ers = "calloc() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    for (int i = 0; i < nthreads; i++) {
        qv_pthread_create(
            &pthrds[i], NULL, thread_work,
            th_scopes[i], th_scopes[i]
        );
    }

    for (int i = 0; i < nthreads; i++) {
        void *ret = NULL;
        if (pthread_join(pthrds[i], &ret) != 0) {
            printf("Thread exited with %p\n", ret);
        }
    }
    free(pthrds);
    // When we are done with the scope, clean up.
    rc = qv_thread_scopes_free(nthreads, th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scopes_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Clean up.
    qv_scope_free(subnuma);
    qv_scope_free(numa_scope);
    qv_scope_free(base_scope);

    MPI_Finalize();

    return EXIT_SUCCESS;
}
```
