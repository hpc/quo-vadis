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
#include <sys/syscall.h>

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
   char const *ers = NULL;
   int rc = QV_SUCCESS;
   
   fprintf(stdout,"# Starting test\n");

#pragma omp parallel private(ers, rc) //num_threads(4)
   {    
     int pid = syscall(SYS_gettid);
     int n_cores;
     int n_pus;
 
     qv_context_t *ctx;
     rc = qv_thread_context_create(&ctx);
     if (rc != QV_SUCCESS) {
       ers = "qv_process_context_create() failed";
       panic("%s (rc=%s)", ers, qv_strerr(rc));
     }     
     
     qv_scope_t *base_scope;
     rc = qv_scope_get(
		       ctx,
		       QV_SCOPE_PROCESS,
		       &base_scope
		       );
     if (rc != QV_SUCCESS) {
       ers = "qv_scope_get() failed";
       panic("%s (rc=%s)", ers, qv_strerr(rc));
     }
     scope_report(ctx, pid, base_scope, "base_scope");

     rc = qv_scope_nobjs(
         ctx,
         base_scope,
         QV_HW_OBJ_CORE,
         &n_cores
     );
     if (rc != QV_SUCCESS) {
         ers = "qv_scope_nobjs() failed";
         panic("%s (rc=%s)", ers, qv_strerr(rc));
     }
     printf("[%d] Number of CORES in base_scope is %d\n", pid, n_cores);

     rc = qv_scope_nobjs(
         ctx,
         base_scope,
         QV_HW_OBJ_PU,
         &n_pus
     );
     if (rc != QV_SUCCESS) {
         ers = "qv_scope_nobjs() failed";
         panic("%s (rc=%s)", ers, qv_strerr(rc));
     }
     printf("[%d] Number of PUS   in base_scope is %d\n", pid, n_pus);

     rc = qv_scope_free(ctx, base_scope);
     if (rc != QV_SUCCESS) {
         ers = "qv_scope_free() failed";
         panic("%s (rc=%s)", ers, qv_strerr(rc));
     }
     
     qv_scope_t *self_scope;
     rc = qv_scope_get(
         ctx,
         QV_SCOPE_PROCESS,
         &self_scope
     );
     if (rc != QV_SUCCESS) {
         ers = "qv_scope_get(QV_SCOPE_PROCESS) failed";
         panic("%s (rc=%s)", ers, qv_strerr(rc));
     } 
     scope_report(ctx, pid, self_scope, "self_scope");
    
     rc = qv_scope_nobjs(
         ctx,
         self_scope,
         QV_HW_OBJ_CORE,
         &n_cores
     );
     if (rc != QV_SUCCESS) {
         ers = "qv_scope_nobjs() failed";
         panic("%s (rc=%s)", ers, qv_strerr(rc));
     }
     printf("[%d] Number of CORES in self_scope is %d\n", pid, n_cores);
    
     rc = qv_scope_nobjs(
         ctx,
         self_scope,
         QV_HW_OBJ_PU,
         &n_pus
     );
     if (rc != QV_SUCCESS) {
         ers = "qv_scope_nobjs() failed";
         panic("%s (rc=%s)", ers, qv_strerr(rc));
     }
     printf("[%d] Number of PUS   in self_scope is %d\n", pid, n_pus);
     
     char *binds;
     rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
     if (rc != QV_SUCCESS) {
         ers = "qv_bind_string() failed";
         panic("%s (rc=%s)", ers, qv_strerr(rc));
     }
     printf("[%d] Current cpubind is %s\n", pid, binds);
     free(binds);

     qv_scope_t *sub_scope;
     rc = qv_scope_split(
	 ctx,
         self_scope,
	 2,                        // npieces
         omp_get_thread_num() %2,  // group id
         &sub_scope
     );
     if (rc != QV_SUCCESS) {
         ers = "qv_scope_split() failed";
         panic("%s (rc=%s)", ers, qv_strerr(rc));
     }
     scope_report(ctx, pid, sub_scope, "sub_scope");
     
     change_bind(ctx, pid, sub_scope);

     rc = qv_scope_free(ctx, sub_scope);
     if (rc != QV_SUCCESS) {
         ers = "qv_scope_free() failed";
         panic("%s (rc=%s)", ers, qv_strerr(rc));
     }

     rc = qv_scope_free(ctx, self_scope);
       if (rc != QV_SUCCESS) {
	 ers = "qv_scope_free() failed";
	 panic("%s (rc=%s)", ers, qv_strerr(rc));
       }
     
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
