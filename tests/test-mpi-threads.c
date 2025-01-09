/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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

#include "quo-vadis-mpi.h"
#include "quo-vadis-pthread.h"

#include "common-test-utils.h"

#include <sys/syscall.h>
#include <omp.h>

#ifndef _OPENMP
#warning "OpenMP support NOT activated"
#endif

#if 0
static void
scope_report(
    int pid,
    qv_scope_t *scope,
    const char *scope_name
) {
    char const *ers = NULL;

    int taskid;
    int rc = qv_scope_taskid(scope, &taskid);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int sgsize;
    rc = qv_scope_group_size(scope, &sgsize);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    printf("[%d] %s taskid is %d\n", pid, scope_name, taskid);
    printf("[%d] %s sgsize is %d\n", pid, scope_name, sgsize);

    rc = qv_scope_barrier(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

static void
change_bind(
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

    int rc = qv_scope_bind_push(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] New cpubind is  %s\n", pid,bind1s);
    free(bind1s);

    rc = qv_scope_bind_pop(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    char *bind2s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind2s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Popped cpubind is  %s\n", pid, bind2s);
    free(bind2s);

    // TODO(skg) Add test to make popped is same as original.
    rc = qv_scope_barrier(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}
#endif

int
main(void)
{
#if 0
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
     ctu_panic("%s (rc=%d)", ers, rc);
   }

   rc = MPI_Comm_size(comm, &wsize);
   if (rc != MPI_SUCCESS) {
     ers = "MPI_Comm_size() failed";
     ctu_panic("%s (rc=%d)", ers, rc);
   }

   rc = MPI_Comm_rank(comm, &wrank);
   if (rc != MPI_SUCCESS) {
     ers = "MPI_Comm_rank() failed";
     ctu_panic("%s (rc=%d)", ers, rc);
   }

   qv_scope_t *mpi_scope;
   rc = qv_mpi_scope_get(
       comm,
       QV_SCOPE_JOB,
       &mpi_scope
   );
   if (rc != QV_SUCCESS) {
     ers = "qv_mpi_scope_get(QV_SCOPE_PROCESS) failed";
     ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
   }
   scope_report(wrank, mpi_scope, "mpi_process_scope");

   rc = qv_scope_nobjs(
      mpi_scope,
      QV_HW_OBJ_NUMANODE,
      &n_numa
               );
   if (rc != QV_SUCCESS) {
     ers = "qv_scope_nobjs() failed";
     ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
   }
   printf("[%d] Number of NUMA in mpi_process_scope is %d\n", wrank, n_numa);

   qv_scope_t *mpi_numa_scope;
   rc = qv_scope_split_at(
      mpi_scope,
      QV_HW_OBJ_NUMANODE,
      wrank % n_numa, // this seems to imply that MPI process are linearly launched. A RR policy would make this fail.
      &mpi_numa_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    scope_report(wrank, mpi_numa_scope, "mpi_numa_scope");

    change_bind(wrank, mpi_numa_scope);

    rc = qv_scope_taskid(
       mpi_numa_scope,
       &my_numa_id
    );

    if (my_numa_id == 0)
      {

    rc = qv_scope_nobjs(
            mpi_numa_scope,
            QV_HW_OBJ_PU,
            &n_pu
    );
    if (rc != QV_SUCCESS) {
            ers = "qv_scope_nobjs() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }

    omp_set_num_threads(n_pu);
    printf("[%d] Spawing OpenMP Parallel Section with %d threads\n", wrank, n_pu);
#pragma omp parallel private(ers, rc)
        {
     int tid = omp_get_thread_num();
      int n_cores;
      int n_pus;


      qv_context_t *omp_ctx;
      rc = qv_thread_context_create(&omp_ctx);
      if (rc != QV_SUCCESS) {
        ers = "qv_process_context_create() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
      }

      qv_scope_t *omp_self_scope;
      rc = qv_scope_get(
              omp_ctx,
              QV_SCOPE_JOB, // should be mpi_num_scope instead. PROCESS semantics not right here -> JOB
              &omp_self_scope
          );
      if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_PROCESS) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
      }
      scope_report(omp_ctx, wrank, omp_self_scope, "omp_self_scope");

      rc = qv_scope_nobjs(
              omp_ctx,
              omp_self_scope,
              QV_HW_OBJ_CORE,
              &n_cores
          );
      if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
      }
      printf("[%d][%d] Number of CORES in omp_self_scope is %d\n", wrank, tid, n_cores);

      rc = qv_scope_nobjs(
              omp_ctx,
              omp_self_scope,
              QV_HW_OBJ_PU,
              &n_pus
          );
      if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
      }
      printf("[%d][%d] Number of PUS   in omp_self_scope is %d\n", wrank, tid, n_pus);

      char *binds;
      rc = qv_bind_string(omp_ctx, QV_BIND_STRING_AS_LIST, &binds);
      if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
      }
      printf("[%d][%d] Current cpubind is %s\n", wrank, tid, binds);
      free(binds);

      qv_scope_t *omp_sub_scope;
      rc = qv_scope_split(
                  omp_ctx,
                  omp_self_scope,
                  omp_get_num_threads(),
                  omp_get_thread_num(),
                  &omp_sub_scope
                  );
      if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
      }
      scope_report(omp_ctx, wrank, omp_sub_scope, "omp_sub_scope");

      change_bind(omp_ctx, wrank, omp_sub_scope);

      rc = qv_scope_free(omp_ctx, omp_sub_scope);
      if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
      }

      rc = qv_scope_free(omp_ctx, omp_self_scope);
      if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
      }

      rc = qv_context_barrier(omp_ctx);
      if (rc != QV_SUCCESS) {
        ers = "qv_context_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
      }

      rc = qv_thread_context_free(omp_ctx);
      if (rc != QV_SUCCESS) {
        ers = "qv_thread_context_free failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
      }
    }
      }
    else
      {
    printf("[%d] Waiting for Master thread \n", wrank);
      }

    rc = qv_scope_free(mpi_numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(mpi_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

   MPI_Finalize();
#endif

   return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
