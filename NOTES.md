# NOTES

Unmanaged Context Initialization
================================
* Use Case:
    - Utility thread placement in low-level infrastructure.
* Naming
    - Perhaps ask the caller for a name and ID at each call site?
* Out out band communication
    * POSIX message queues?
    * Shared memory segment?

Scopes
======
* A scope describes a collection of hardware resources.
* Scopes can dynamically 'change shape' during execution, and have transferable
  ownership, e.g.,  a parent can ask for n scopes and distribute them as needed.
  This will help with dynamic load balancing, for example.
* A scope modification call can fail. For example, asking for more resources.

Hardware Topologies
===================
* We may have to add additional capabilities to support GPUs. It may be that
  hwloc_topology_restrict() cannot support what we might need.
* We probably don't need multiple topologies. Just use scopes!

Splitting
=========
* Perhaps it would be useful to have a 'distance' call to help callers group
  tasks. Guillaume has some ideas. See: MPIX_Get_hw_resource_min(),
  MPIX_Get_hw_domain_neighbours(), MPIX_Get_hw_topo_group(). Also, see
  https://www.mpi-forum.org/docs/mpi-4.0/mpi40-report.pdf

Threading
=========
* Two cases:
    * 1. Spawn threads, register
        Use-case
        - Main thread running
        - qv_thread_context_create()
            - Setup zgroup with initial group size, etc.
        - Other operations: group split, etc.
            - Set up what the underlying structures would look like, etc.
        - Spawn (look at pthread_create interface, be similar).
            - Run until completion, maybe keep thread pool around, or just have
              multiple instances.
    * 2. Register threads (for Pthreads especially): in the thread-specific
         interface (for now).

Example:
    Spwaning a bunch of threads
        - Register threads, resuling in the thread_comm-like structure
          (internally keep some unique ID to structure?)
        - qv_thread_context_create(&ctx, with returned structure reference (think
          MPI_Comm))
        - Create a zgroup will just follow because we know the shape of the thread
          group, etc.
