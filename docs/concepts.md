
# Quo Vadis
*Where runtime systems are going*.

Quo Vadis is a runtime system that helps hybrid applications make
efficient use of heterogeneous hardware, ease programmability in the
presence of multiple programming abstractions, and enable portability
across systems. 

The runtime system abstracts out low-level details of the hardware and
presents an architecture-independent interface applications can use to
leverage local resources automatically and without user
intervention. This interface enables portability across computing
platforms. 

Conceptually, Quo Vadis is centered around `scopes`, an abstraction
representing a set of hardware resources. Users interact with a system
through scopes and move in and out of scopes through `push` and `pop`
operations (similar to its predecessor Quo). Users can create scopes
automatically through `split` operations that split the resources into
subsets that compute workers (e.g., tasks or threads) can use. Each
resource subset or subscope maximizes locality for a given worker. 


## Features

* Architecture-independent API.

* Hardware/software state queries including hardware topology and task
  configuration. 

* Efficient partition of resources among workers.

* Efficient task reconfiguration including placement and affinity.

* Composable and general.

* Open-source library.

* C and Fortran (work in progress) interfaces.



## Main Concepts

### Contexts: Instance handles

```C
qv_mpi_context_create(&ctx, communicator);

qv_mpi_context_free(ctx);
```

### Scopes: Predefined, derived, and custom 

```C
// Predefined 
qv_scope_get(ctx, QV_SCOPE_USER, &base_scope);

// Derived
// See qv_scope_split() and qv_scope_split_at()

// Custom
// Work in progress
```

### Hardware and software environment queries

```C
// Hardware topology queries
qv_scope_nobjs(ctx, scope, QV_HW_OBJ_GPU, &ngpus);

// Caller-state queries
qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binding_list);
```




### Split operations: Distribute resources to workers

```C
// Splits are collective operations: Every caller gets its own subscope

// For example: comm_size as num_workers, comm_rank as color
qv_scope_split(ctx, base_scope, size, rank, &sub_scope);

// Or split by a specific resource type
qv_scope_split_at(ctx, base_scope, QV_HW_OBJ_NUMANODE, rank%nnumas, &numa_scope);

// including accelerators 
qv_scope_split_at(ctx, base_scope, QV_HW_OBJ_GPU, rank%ngpus, &gpu_scope);



```

### Stack-based semantics to map workers to hardware

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

int main(int argc, char **argv)
{
    /* Initialize MPI */ 
    /* Get MPI_COMM_WORLD (comm), size (wsize), and rank (wrank) */ 

    /* Create a QV context */
    qv_context_t *ctx;
    int rc = qv_mpi_context_create(&ctx, comm);

    /* Get base scope: My hardware resources */
    qv_scope_t *base_scope;
    rc = qv_scope_get(ctx, QV_SCOPE_USER, &base_scope);

    int ncores;
    rc = qv_scope_nobjs(ctx, base_scope, QV_HW_OBJ_CORE, &ncores);

    /* Split the base scope evenly across workers */
    qv_scope_t *sub_scope;
    rc = qv_scope_split(ctx, base_scope, wsize, wrank, &sub_scope);

    /* What resources did I get? */
    rc = qv_scope_nobjs(ctx, sub_scope, QV_HW_OBJ_CORE, &ncores);


    /******************************************
     * Phase 1: Everybody works
     *          Use Pthreads and GPU kernels
     ******************************************/

    /* Move into my scope to my assigned resources */ 
    rc = qv_bind_push(ctx, sub_scope);

    /* Where did I end up? */
    char *bind_list;
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &bind_list);
    printf("=> [%d] Split: Got %d cores, running on %s\n",
       wrank, ncores, bind_list);
    free(bind_list);

    /* Launch pthreads to do the work */ 
    do_pthread_things(wrank, ncores);

    /* Launch GPU kernels */
    int ngpus;
    rc = qv_scope_nobjs(ctx, sub_scope, QV_HW_OBJ_GPU, &ngpus);
    
    int i, dev;
    char *gpu;
    for (i=0; i<ngpus; i++) {
        qv_scope_get_device(ctx, sub_scope, QV_HW_OBJ_GPU, i, QV_DEVICE_ID_PCI_BUS_ID, &gpu);
        cudaDeviceGetByPCIBusId(&dev, gpu);
        cudaSetDevice(dev);
        launch_gpu_kernel(); 
        free(gpu);
    }

    /* We are done in this scope */ 
    rc = qv_bind_pop(ctx);


    /*******************************************************
     * Phase 2: One master per NUMA,
     *          others sleep, ay que flojos!
     *          Use OpenMP
     *******************************************************/

    int nnumas, my_numa_id;
    qv_scope_t *numa_scope;

    /* Get the number of NUMA domains so that we can
       specify the color/groupid to split_at() */
    rc = qv_scope_nobjs(ctx, base_scope, QV_HW_OBJ_NUMANODE, &nnumas);

    /* Split at NUMA domains */
    rc = qv_scope_split_at(ctx, base_scope, QV_HW_OBJ_NUMANODE, wrank%nnumas, &numa_scope);

    /* My NUMA-based ID */
    rc = qv_scope_taskid(ctx, numa_scope, &my_numa_id);

    int npus;
    if (my_numa_id == 0) {
        /* I am the lead */	
        rc = qv_bind_push(ctx, numa_scope);

        /* Launch OMP region on all the resources of my NUMA */ 
        rc = qv_scope_nobjs(ctx, numa_scope, QV_HW_OBJ_PU, &npus);
        do_omp_things(wrank, npus);

        rc = qv_bind_pop(ctx);
    }

    /* Everybody else waits... */
    rc = qv_scope_barrier(ctx, numa_scope);


    /************************************************
     * Phase 3: More GPU work
     *          Optimize placement/affinity for GPUs
     *************************************************/

    int my_gpu_id;
    qv_scope_t *gpu_scope;

    /* Get the number of GPUs so that we can
       pass the color/groupid to  split_at */
    rc = qv_scope_nobjs(ctx, base_scope, QV_HW_OBJ_GPU, &ngpus);

    /* Split at GPUs */
    rc = qv_scope_split_at(ctx, base_scope, QV_HW_OBJ_GPU, wrank%ngpus, &gpu_scope);

    /* My GPU-based ID */ 
    rc = qv_scope_taskid(ctx, gpu_scope, &my_gpu_id);

    /* I want no more than one worker per GPU submitting kernels */
    if (my_gpu_id == 0) {
        /* Worker placement is GPU-optimized */ 
        rc = qv_bind_push(ctx, gpu_scope);

        /* Get the first device assigned to me */ 
        qv_scope_get_device(ctx, gpu_scope, QV_HW_OBJ_GPU, 0, QV_DEVICE_ID_PCI_BUS_ID, &gpu);

        launch_kernels_on_gpu(wrank, gpu); 

        /* We are done in this scope */ 
        rc = qv_bind_pop(ctx);
        free(gpu);
    }


    /***************************************
     * Clean up
     ***************************************/

    rc = qv_scope_free(ctx, gpu_scope);
    
    rc = qv_scope_free(ctx, numa_scope);

    rc = qv_scope_free(ctx, sub_scope);

    rc = qv_scope_free(ctx, base_scope);

    rc = qv_context_barrier(ctx);

    rc = qv_mpi_context_free(ctx); 

    return 0;
}
```





