/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
#include <stdio.h>
#include <pthread.h>
#include "quo-vadis-mpi.h"
#include "quo-vadis-thread.h"
#include "qvi-test-common.h"
#include <assert.h>

void *thread_work(void *arg)
{
    qv_context_t *ctx = (qv_context_t *) arg;
    char *binds;
    char const *ers = NULL;

    int rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);

    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    fprintf(stdout,"Thread running on %s\n", binds);
    free(binds);


    return NULL;
}


//int main(int argc, char *argv[])
int
main(void)
{
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;
    int wrank, wsize;
    int n_numas, n_cores, n_pus, my_numa_id;
    int rc = QV_SUCCESS;

    fprintf(stdout,"# Starting Hybrid MPI + Pthreads test\n");

    rc = MPI_Init(NULL, NULL);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    qv_context_t *mpi_ctx;
    rc = qv_mpi_context_create(comm, &mpi_ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_create() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *mpi_scope;
    rc = qv_scope_get(mpi_ctx, QV_SCOPE_JOB, &mpi_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    qvi_test_scope_report(mpi_ctx, mpi_scope, "mpi_job_scope");

    rc = qv_scope_nobjs(mpi_ctx, mpi_scope, QV_HW_OBJ_NUMANODE, &n_numas);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    fprintf(stdout,"[%d] Number of NUMANodes in mpi_scope is %d\n", wrank, n_numas);

    qv_scope_t *mpi_numa_scope;
    rc = qv_scope_split_at(mpi_ctx, mpi_scope, QV_HW_OBJ_NUMANODE, wrank % n_numas, &mpi_numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    qvi_test_scope_report(mpi_ctx, mpi_numa_scope, "mpi_numa_scope");

    qvi_test_bind_push(mpi_ctx, mpi_numa_scope);

    rc = qv_scope_taskid(mpi_ctx, mpi_numa_scope, &my_numa_id);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    fprintf(stdout,"[%d] NUMA id is %d\n", wrank, my_numa_id);

    rc = qv_scope_nobjs(mpi_ctx, mpi_numa_scope, QV_HW_OBJ_CORE, &n_cores);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    fprintf(stdout,"[%d] Number of Cores in mpi_numa_scope is %d\n", wrank, n_cores);

    rc = qv_scope_nobjs(mpi_ctx, mpi_numa_scope, QV_HW_OBJ_PU, &n_pus);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    fprintf(stdout,"[%d] Number of PUs in mpi_numa_scope is %d\n", wrank, n_pus);

    int nthreads = 1*n_cores;
    assert(nthreads <= n_pus);

    fprintf(stdout,"[%d] Number of threads : %i\n", wrank, nthreads);

    int colors[nthreads];
    for(int i = 0 ; i < nthreads ; i++)
        colors[i] = i%n_cores;

    qv_scope_t **th_scopes = NULL;

    rc = qv_thread_scope_split_at(mpi_ctx, mpi_numa_scope, QV_HW_OBJ_CORE, colors, nthreads, &th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_thread_scope_split_at() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    assert(th_scopes);

    pthread_t thid[nthreads];
    pthread_attr_t *attr = NULL;

    for(int i  = 0 ; i < nthreads; i ++){
        //sleep(1);
        if (qv_pthread_create(&thid[i], attr, thread_work, (void *)(mpi_ctx), mpi_ctx, th_scopes[i]) != 0) {
            perror("pthread_create() error");
            exit(1);
        }
    }

    void *ret;
    for(int i  = 0 ; i < nthreads; i ++){
        if (pthread_join(thid[i], &ret) != 0) {
            perror("pthread_create() error");
            exit(3);
        }
        fprintf(stdout,"Thread finished with '%s'\n", (char *)ret);
    }

    fprintf(stdout,"===================== Coucou\n");

    /* Clean up */
    for(int i  = 0 ; i < nthreads; i ++){
        rc = qv_scope_free(mpi_ctx, th_scopes[i]);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_free() failed";
            qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }
    free(th_scopes);

    fprintf(stdout,"===================== Coucou 2\n");

    rc = qv_scope_free(mpi_ctx, mpi_numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    fprintf(stdout,"===================== Coucou 3\n");

    rc = qv_scope_free(mpi_ctx, mpi_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    fprintf(stdout,"===================== Coucou 4\n");

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
