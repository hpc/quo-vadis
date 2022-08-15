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
 * @file test-threads.c
 */

#include "qvi-macros.h"
#include "quo-vadis-thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef _OPENMP
#include <omp.h>
#warning "Using OpenMP support"
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
    printf("[%d] New cpubind is     %s\n", pid, bind1s);
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
   char const *ers = NULL;
   int rc = QV_SUCCESS;
   int pid = getpid();
   qv_context_t *ctx = NULL;
   qv_scope_t *base_scope = NULL;
   
   fprintf(stdout,"# Starting test\n");

#pragma omp parallel private(ctx, base_scope)
   {
     rc = qv_thread_context_create(&ctx);
     if (rc != QV_SUCCESS) {
       ers = "qv_process_context_create() failed";
       panic("%s (rc=%s)", ers, qv_strerr(rc));
     }     
     
     fprintf(stdout,"ctx addr is %p\n",ctx);

     rc = qv_scope_get(
		       ctx,
		       QV_SCOPE_USER,
		       &base_scope
		       );
     if (rc != QV_SUCCESS) {
       ers = "qv_scope_get() failed";
       panic("%s (rc=%s)", ers, qv_strerr(rc));
     }
     
     fprintf(stdout,"Entering scope report ...\n");
     
     scope_report(ctx, pid, base_scope, "base_scope");
     
     fprintf(stdout,"Entering scope free ...\n");

     rc = qv_scope_free(ctx, base_scope);
       if (rc != QV_SUCCESS) {
	 ers = "qv_scope_free() failed";
	 panic("%s (rc=%s)", ers, qv_strerr(rc));
       }
     
     fprintf(stdout,"Entering barrier ...\n");

     rc = qv_context_barrier(ctx);
     if (rc != QV_SUCCESS) {
       ers = "qv_context_barrier() failed";
       panic("%s (rc=%s)", ers, qv_strerr(rc));
     }

     rc = qv_thread_context_free(ctx);
     if (rc != QV_SUCCESS) {
       ers = "qv_thread_context_free failed";
       panic("%s (rc=%s)", ers, qv_strerr(rc));
     }
     
   }   
   return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
