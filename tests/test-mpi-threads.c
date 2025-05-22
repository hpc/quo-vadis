/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-threads.c
 */

#include "quo-vadis-mpi.h"
// TODO(skg)
#include "quo-vadis-pthread.h"

#include "common-test-utils.h"

#include "omp.h"

void *
thread_work(
    void *arg
) {
    // Do work.
    return arg;
}

int
main(
    int argc, char *argv[]
) {
  char const *ers = NULL;
  MPI_Comm comm = MPI_COMM_WORLD;

  /* Initialization */
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


  /************************************************
   * Use the process interface for NUMA
   ************************************************/

  /* Get base scope: RM-given resources */
  qv_scope_t *base_scope;
  rc = qv_mpi_scope_get(comm, QV_SCOPE_USER, &base_scope);

  if (rc != QV_SUCCESS) {
    ers = "qv_mpi_scope_get() failed";
    ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
  }

  int nnumas;
  rc = qv_scope_nobjs(base_scope, QV_HW_OBJ_NUMANODE, &nnumas);

  if (rc != QV_SUCCESS) {
    ers = "qv_scope_nobjs() failed";
    ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
  }

  /* Split at NUMA domains */
  qv_scope_t *numa;
  rc = qv_scope_split_at(base_scope, QV_HW_OBJ_NUMANODE,
             wrank % nnumas, &numa);

  if (rc != QV_SUCCESS) {
    ers = "qv_scope_split_at() failed";
    ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
  }

  /* When there's more tasks than NUMAs,
     make sure each task has exclusive resources */
  int lrank, ntasks_per_numa;
  qv_scope_t *subnuma;
  qv_scope_group_rank(numa, &lrank);
  qv_scope_group_size(numa, &ntasks_per_numa);

  rc = qv_scope_split(numa, ntasks_per_numa,
              lrank % ntasks_per_numa, &subnuma);

  if (rc != QV_SUCCESS) {
    ers = "qv_scope_split_at() failed";
    ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
  }

  /* Get the number of cores and pus per NUMA part */
  int ncores, npus;
  qv_scope_nobjs(subnuma, QV_HW_OBJ_CORE, &ncores);
  qv_scope_nobjs(subnuma, QV_HW_OBJ_PU, &npus);


  /********************************************
   * OpenMP: Launch one thread per core
   ********************************************/

  /* Use defaul thread color */
  int i, nthreads = ncores;
  int *th_color = NULL;
  qv_scope_t **th_scope;

  // TODO(skg)
  qv_pthread_scope_split_at(subnuma, QV_HW_OBJ_CORE,
               th_color, nthreads, &th_scope);

  omp_set_num_threads(nthreads);
#pragma omp parallel
  {
    int tid = omp_get_thread_num();
    qv_scope_bind_push(th_scope[tid]);

    /* Each thread works */
    thread_work(&tid);
  }

  /* When we are done, clean up */
  for (i=0; i<nthreads; i++)
    qv_scope_free(th_scope[i]);


  /************************************************
   * POSIX threads:
   *   Launch one thread per hardware thread
   *   Policy-based placement
   *   Note num_threads < num_places on SMT
   ************************************************/

  void *ret;
  th_color = QV_PTHREAD_SCOPE_SPLIT_PACKED;
  nthreads = ncores;
    // TODO(skg)
  qv_pthread_scope_split_at(subnuma, QV_HW_OBJ_PU,
               th_color, nthreads, &th_scope);

  pthread_t *thid = malloc(nthreads * sizeof(pthread_t));
  pthread_attr_t *attr = NULL;

  for (i=0; i<nthreads; i++)
    qv_pthread_create(&thid[i], attr, thread_work, &i, th_scope[i]);

  for (i=0; i<nthreads; i++)
    if (pthread_join(thid[i], &ret) != 0)
      printf("Thread exited with %p\n", ret);

  for (i=0; i<nthreads; i++)
    qv_scope_free(th_scope[i]);


  /************************************************
   * Clean up
   ************************************************/

  qv_scope_free(subnuma);
  qv_scope_free(numa);

  MPI_Finalize();

  return 0;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
