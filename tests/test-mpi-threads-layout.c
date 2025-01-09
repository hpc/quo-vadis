/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022-2023 Inria.
 *                         All rights reserved.
 *
 * Copyright (c) 2022-2023 Bordeaux INP.
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-mpi-threads-layout.c
 */

#include "quo-vadis-mpi.h"
#include "quo-vadis-pthread.h"
#include "common-test-utils.h"

#include <sys/syscall.h>
#include <omp.h>

#ifndef _OPENMP
#warning "OpenMP support NOT activated"
#endif

int
main(void)
{
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

    qv_context_t *mpi_ctx;
    rc = qv_mpi_context_create(comm, &mpi_ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_create() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *mpi_scope;
    rc = qv_scope_get(
            mpi_ctx,
            QV_SCOPE_JOB,
            &mpi_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_PROCESS) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    ctu_scope_report(mpi_ctx, mpi_scope, "mpi_process_scope");

    rc = qv_scope_nobjs(
            mpi_ctx,
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
            mpi_ctx,
            mpi_scope,
            QV_HW_OBJ_NUMANODE,
            wrank % n_numa, // this seems to imply that MPI process are linearly launched. A RR policy would make this fail.
            &mpi_numa_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    ctu_scope_report(mpi_ctx, mpi_numa_scope, "mpi_numa_scope");

    ctu_bind_push(mpi_ctx, mpi_numa_scope);

    rc = qv_scope_taskid(
            mpi_ctx,
            mpi_numa_scope,
            &my_numa_id
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

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
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }

        omp_set_num_threads(n_pu);
        printf("[%d] Spawing OpenMP Parallel Section with %d threads\n", wrank, n_pu);

        /* Bind to PUs with a stride of 1 (= resources with consecutive indices */
        qv_layout_t *thread_layout = NULL;
        qv_layout_params_t params = {QV_POLICY_PACKED, QV_HW_OBJ_PU, 1};

        qv_thread_layout_create(mpi_ctx,params,&thread_layout);

#pragma omp parallel private(ers, rc)
        {
            qv_thread_args_t args;
            int tid = omp_get_thread_num();
            int num = omp_get_num_threads();

            printf("[%d][%d] Binding to PUS \n", wrank, tid);

            rc = qv_thread_args_set(mpi_ctx,mpi_numa_scope,thread_layout,tid,num,&args);
            if (rc != QV_SUCCESS) {
                ers = "qv_thread_args_set() failed";
                ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
            }

            rc = qv_thread_layout_apply(args);
            if (rc != QV_SUCCESS) {
                ers = "qv_thread_layout_apply failed";
                ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
            }

            /* Do some work here */
        }

        omp_set_num_threads(n_pu/4);
        printf("[%d] Spawing OpenMP Parallel Section with %d threads\n", wrank, n_pu/4);

        /* Bind to cores with a stride of 2 */
        qv_thread_layout_set_obj_type(thread_layout,QV_HW_OBJ_CORE);
        qv_thread_layout_set_stride(thread_layout,2);

#pragma omp parallel private(ers, rc)
        {
            qv_thread_args_t args;
            int tid = omp_get_thread_num();
            int num = omp_get_num_threads();

            printf("[%d][%d] Binding to CORES \n", wrank, tid);

            rc = qv_thread_args_set(mpi_ctx,mpi_numa_scope,thread_layout,tid,num,&args);
            if (rc != QV_SUCCESS) {
                ers = "qv_thread_args_set() failed";
                ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
            }

            rc = qv_thread_layout_apply(args);
            if (rc != QV_SUCCESS) {
                ers = "qv_thread_layout_apply failed";
                ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
            }

            /* Do some work here */
        }
        qv_thread_layout_free(thread_layout);
    }
    else {
        printf("[%d] Waiting for Master thread \n", wrank);
    }

    ctu_bind_pop(mpi_ctx, mpi_numa_scope);

    rc = qv_context_barrier(mpi_ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_context_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(mpi_ctx, mpi_numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(mpi_ctx, mpi_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (qv_mpi_context_free(mpi_ctx) != QV_SUCCESS) {
        ers = "qv_mpi_context_free() failed";
        ctu_panic("%s", ers);
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
