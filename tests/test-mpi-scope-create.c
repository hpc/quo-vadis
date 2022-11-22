/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define USE_AFFINITY_PRESERVING 1


#define panic(vargs...)                                                        \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, vargs);                                                    \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)


static int
do_omp_things(int rank, int npus)
{
  printf("[%d] Doing OpenMP things with %d PUs\n", rank, npus);
  return 0;
}

static int
do_pthread_things(int rank, int ncores)
{
  printf("[%d] Doing pthread_things with %d cores\n", rank, ncores);
  return 0;
}




void test_create_scope(qv_context_t *ctx, qv_scope_t *scope,
		       int wrank, int ncores, int free_scope)
{
  int rc;
  char *binds;
  char const *ers = NULL;
  
  if (wrank == 0)
    printf("\n===Scope w/%d cores===\n", ncores); 
  
  qv_scope_t *core_scope;
  rc = qv_scope_create(ctx, scope, QV_HW_OBJ_CORE,
		       ncores, 0, &core_scope);
  if (rc != QV_SUCCESS) {
    ers = "qv_scope_create() failed";
    panic("%s (rc=%s)", ers, qv_strerr(rc));
  }

  int res_ncores; 
  rc = qv_scope_nobjs(ctx, core_scope, QV_HW_OBJ_CORE, &res_ncores);
  if (rc != QV_SUCCESS) {
    ers = "qv_scope_nobjs() failed";
    panic("%s (rc=%s)", ers, qv_strerr(rc));
  }

  rc = qv_bind_push(ctx, core_scope);
  if (rc != QV_SUCCESS) {
    ers = "qv_bind_push() failed";
    panic("%s (rc=%s)", ers, qv_strerr(rc));
  }
  
  /* Where did I end up? */
  rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
  if (rc != QV_SUCCESS) {
    ers = "qv_bind_get_list_as_string() failed";
    panic("%s (rc=%s)", ers, qv_strerr(rc));
  }
  printf("=> [%d] Core scope: got %d cores, running on %s\n",
	 wrank, res_ncores, binds);
  free(binds);

  rc = qv_bind_pop(ctx);
  if (rc != QV_SUCCESS) {
    ers = "qv_bind_pop() failed";
    panic("%s (rc=%s)", ers, qv_strerr(rc));
  }

  rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
  if (rc != QV_SUCCESS) {
    ers = "qv_bind_get_list_as_string() failed";
    panic("%s (rc=%s)", ers, qv_strerr(rc));
  }
  printf("[%d] Popped up to %s\n", wrank, binds);
  free(binds);

  if (free_scope) { 
    rc = qv_scope_free(ctx, core_scope);
    if (rc != QV_SUCCESS) {
      ers = "qv_scope_free() failed";
      panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
  }
  
  /* Sync output */
  MPI_Barrier(MPI_COMM_WORLD);
  /* Context barrier doesn't seem to work */ 
#if 0
  rc = qv_context_barrier(ctx);
  if (rc != QV_SUCCESS) {
    ers = "qv_context_barrier() failed";
    panic("%s (rc=%s)", ers, qv_strerr(rc));
  }
#endif 
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
    rc = qv_mpi_context_create(&ctx, comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_create() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Get base scope: RM-given resources */
    qv_scope_t *base_scope;
    rc = qv_scope_get(
        ctx,
        QV_SCOPE_USER,
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

    char *binds;
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Base scope w/%d cores, running on %s\n",
       wrank, ncores, binds);
    free(binds);

    /* Sync output */
    MPI_Barrier(MPI_COMM_WORLD);

    /***************************************
     * Phase 1: Split base scope
     ***************************************/
    
    if (wrank == 0)
      printf("\n===Scope split===\n"); 
    
    /* Split the base scope evenly across workers */
    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        ctx,
        base_scope,
        wsize,        // Number of workers
#ifdef USE_AFFINITY_PRESERVING
	QV_SCOPE_SPLIT_AFFINITY_PRESERVING,
#else
	wrank,        // My group color
#endif 
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

    rc = qv_bind_push(ctx, sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Where did I end up? */
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("=> [%d] Split: got %d cores, running on %s\n",
       wrank, ncores, binds);
    free(binds);

    /* Sync output */
    MPI_Barrier(MPI_COMM_WORLD); 

    /***************************************
     * Phase 2: Create core scope 
     ***************************************/

    if (wrank == 0)
      printf("\n===Asking and not releasing 1-core and 10-core scopes===\n"); 

    int free_scope = 0;
    test_create_scope(ctx, sub_scope, wrank, 1, free_scope);
    test_create_scope(ctx, sub_scope, wrank, 10, free_scope);
    
    if (wrank == 0)
      printf("\n===Asking and releasing 5-core scopes===\n"); 
    
    free_scope = 1; 
    test_create_scope(ctx, sub_scope, wrank, 5, free_scope);
    test_create_scope(ctx, sub_scope, wrank, 5, free_scope);
    

    /***************************************
     * Clean up
     ***************************************/

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

    rc = qv_context_barrier(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_context_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
out:
    if (qv_mpi_context_free(ctx) != QV_SUCCESS) {
        ers = "qv_mpi_context_free() failed";
        panic("%s", ers);
    }
    MPI_Finalize();
    return EXIT_SUCCESS;
}




/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
