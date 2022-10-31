/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Inria.
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Bordeaux INP.
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-mpi-threads.c
 */

#include "qvi-macros.h"
#include "quo-vadis-mpi.h"
#include "quo-vadis-thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <omp.h>

#ifndef _OPENMP
#warning "OpenMP support NOT activated"
#endif
  
#define panic(vargs...)                                                        \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, vargs);                                                    \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)

static void
scope_report(
    qv_context_t *ctx,
    int pid,
    qv_scope_t *scope,
    const char *scope_name
) {
    char const *ers = NULL;

    int taskid;
    int rc = qv_scope_taskid(ctx, scope, &taskid);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ntasks;
    rc = qv_scope_ntasks(ctx, scope, &ntasks);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_ntasks() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    printf("[%d] %s taskid is %d\n", pid, scope_name, taskid);
    printf("[%d] %s ntasks is %d\n", pid, scope_name, ntasks);

    rc = qv_scope_barrier(ctx, scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

static void
change_bind(
    qv_context_t *ctx,
    int pid,
    qv_scope_t *scope
) {
    char const *ers = NULL;

    if (getenv("HWLOC_XMLFILE")) {
        if (pid == 0) {
            printf("*** Using synthetic topology. "
                   "Skipping change_bind tests. ***\n");
        }
        return;
    }

    int rc = qv_bind_push(ctx, scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    char *bind1s;
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] New cpubind is  %s\n", pid,bind1s);
    free(bind1s);

    rc = qv_bind_pop(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    
    char *bind2s;
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &bind2s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Popped cpubind is  %s\n", pid, bind2s);
    free(bind2s);

    // TODO(skg) Add test to make popped is same as original.
    rc = qv_scope_barrier(ctx, scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

int
main(
     int argc,
     char *argv[]
){
   MPI_Comm comm = MPI_COMM_WORLD;
   char const *ers = NULL;
   int wrank, wsize;
   int n_numa, n_pu;
   int my_numa_id;
   int rc = QV_SUCCESS;
   
   fprintf(stdout,"# Starting Hybrid MPI + OpenMP test\n");

   rc = MPI_Init(NULL, NULL);
   if (rc != MPI_SUCCESS) {
     ers = "MPI_Init() failed";
     panic("%s (rc=%d)", ers, rc);
   }

   rc = MPI_Comm_size(comm, &wsize);
   if (rc != MPI_SUCCESS) {
     ers = "MPI_Comm_size() failed";
     panic("%s (rc=%d)", ers, rc);
   }
   
   rc = MPI_Comm_rank(comm, &wrank);
   if (rc != MPI_SUCCESS) {
     ers = "MPI_Comm_rank() failed";
     panic("%s (rc=%d)", ers, rc);
   }
 
   qv_context_t *mpi_ctx;
   rc = qv_mpi_context_create(&mpi_ctx, comm);
   if (rc != QV_SUCCESS) {
     ers = "qv_mpi_context_create() failed";
     panic("%s (rc=%s)", ers, qv_strerr(rc));
   }

   qv_scope_t *mpi_scope;
   rc = qv_scope_get(
       mpi_ctx,
       QV_SCOPE_JOB, 
       &mpi_scope
   );
   if (rc != QV_SUCCESS) {
     ers = "qv_scope_get(QV_SCOPE_PROCESS) failed";
     panic("%s (rc=%s)", ers, qv_strerr(rc));
   }
   scope_report(mpi_ctx, wrank, mpi_scope, "mpi_process_scope");

   rc = qv_scope_nobjs(
      mpi_ctx,
      mpi_scope,
      QV_HW_OBJ_NUMANODE,
      &n_numa
		       );
   if (rc != QV_SUCCESS) {
     ers = "qv_scope_nobjs() failed";
     panic("%s (rc=%s)", ers, qv_strerr(rc));
   }
   printf("[%d] Number of NUMA in mpi_process_scope is %d\n", wrank, n_numa);

   qv_scope_t *mpi_numa_scope;
   rc = qv_scope_split_at(
      mpi_ctx,
      mpi_scope,
      QV_HW_OBJ_NUMANODE,
      wrank % n_numa, // this seems to imply that MPI process are linearly launched. A RR policy would make this fail. 
      &mpi_numa_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    scope_report(mpi_ctx, wrank, mpi_numa_scope, "mpi_numa_scope");
    
    change_bind(mpi_ctx, wrank, mpi_numa_scope);

    rc = qv_scope_taskid(
       mpi_ctx,
       mpi_numa_scope,
       &my_numa_id
    );

    if (my_numa_id == 0)
      {

	rc = qv_scope_nobjs(
            mpi_ctx,
            mpi_numa_scope,
            QV_HW_OBJ_PU,
            &n_pu
	);
	if (rc != QV_SUCCESS) {
            ers = "qv_scope_nobjs() failed";
            panic("%s (rc=%s)", ers, qv_strerr(rc));
        }

	omp_set_num_threads(n_pu);
	printf("[%d] Spawing OpenMP Parallel Section with %d threads\n", wrank, n_pu);

	qv_layout_t thread_layout;
	
	/* Bind to PUs w/o stride */
	qv_thread_layout_init(&thread_layout,QV_POLICY_PACKED,QV_HW_OBJ_PU,0);
	
#pragma omp parallel private(ers, rc) 
	{
	  int tid = omp_get_thread_num();
	  printf("[%d][%d] Binding to PUS \n", wrank, tid); 
	  rc = qv_thread_layout_apply(
	     mpi_ctx,
	     mpi_numa_scope,
	     thread_layout
	  );
	}

	omp_set_num_threads(n_pu/4);
	printf("[%d] Spawing OpenMP Parallel Section with %d threads\n", wrank, n_pu/4);
	
	/* Bind to cores with a stride of 1 */	
	qv_thread_layout_init(&thread_layout,QV_POLICY_PACKED,QV_HW_OBJ_CORE,1);	
#pragma omp parallel private(ers, rc) 
	{
	  int tid = omp_get_thread_num();
	  printf("[%d][%d] Binding to CORES \n", wrank, tid); 
	  rc = qv_thread_layout_apply(
	     mpi_ctx,
	     mpi_numa_scope,
	     thread_layout
	  );
	}
      }
    else
      {
	printf("[%d] Waiting for Master thread \n", wrank);
      }
    
    rc = qv_context_barrier(mpi_ctx);
    if (rc != QV_SUCCESS) {
      ers = "qv_context_barrier() failed";
      panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    
    rc = qv_scope_free(mpi_ctx, mpi_numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(mpi_ctx, mpi_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (qv_mpi_context_free(mpi_ctx) != QV_SUCCESS) {
        ers = "qv_mpi_context_free() failed";
        panic("%s", ers);
    }
    
  
   MPI_Finalize();

   return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
