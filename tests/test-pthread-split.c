/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "quo-vadis-mpi.h"
#include "quo-vadis-pthread.h"
#include "qvi-test-common.h"

void *
thread_work(
    void *arg
) {
    char const *ers = NULL;
    const pid_t tid = qvi_test_gettid();
    qv_scope_t *scope = (qv_scope_t *)arg;

    char *binds = NULL;
    int rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    fprintf(stdout,"[%d] Thread running on %s\n", tid, binds);
    free(binds);

    return NULL;
}

int
main(void)
{
    char const *ers = NULL;
    const pid_t tid = qvi_test_gettid();
    MPI_Comm comm = MPI_COMM_WORLD;
    int wrank, wsize;
    int n_cores = 0;

    int rc = MPI_Init(NULL, NULL);
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

    if (wrank == 0) {
        fprintf(stdout,"# Starting Hybrid MPI + Pthreads test.\n");
    }

    qv_scope_t *mpi_scope;
    rc = qv_mpi_scope_get(comm, QV_SCOPE_JOB, &mpi_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_nobjs(mpi_scope, QV_HW_OBJ_CORE, &n_cores);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    //test qv_pthread_scope_split
    int npieces  = n_cores / 2;
    int nthreads = n_cores;

    fprintf(stdout,"[%d] ====== Testing thread_scope_split (number of threads : %i)\n", tid, nthreads);

    int colors[nthreads];
    for(int i = 0 ; i < nthreads ; i++) {
        colors[i] = i % npieces;
    }

    qv_scope_t **th_scopes = NULL;
    rc = qv_pthread_scope_split(mpi_scope, npieces, colors, nthreads, &th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_split() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    pthread_t thid[nthreads];
    pthread_attr_t *attr = NULL;

    for (int i = 0 ; i < nthreads; i ++) {
        //sleep(1);
        if (qv_pthread_create(&thid[i], attr, thread_work, (void *)(th_scopes[i]), th_scopes[i]) != 0) {
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
        //fprintf(stdout,"Thread finished with '%s'\n", (char *)ret);
    }

    /* Clean up */
    rc = qv_pthread_scopes_free(nthreads, th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    //Test qv_pthread_scope_split_at
    nthreads = 2*n_cores;

    fprintf(stdout,"[%d] ====== Testing thread_scope_split_at (number of threads : %i)\n", tid, nthreads);

    int colors2[nthreads];
    for(int i = 0 ; i < nthreads ; i++)
        colors2[i] = i%n_cores;

    rc = qv_pthread_scope_split_at(mpi_scope, QV_HW_OBJ_CORE, colors2, nthreads, &th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_split_at() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    pthread_t thid2[nthreads];
    for(int i  = 0 ; i < nthreads; i ++){
        if (qv_pthread_create(&thid2[i], attr, thread_work, (void *)(th_scopes[i]), th_scopes[i]) != 0) {
            perror("pthread_create() error");
            exit(1);
        }
    }

    for(int i  = 0 ; i < nthreads; i ++) {
        if (pthread_join(thid2[i], &ret) != 0) {
            perror("pthread_create() error");
            exit(3);
        }
        //fprintf(stdout,"Thread finished with '%s'\n", (char *)ret);
    }

    /* Clean up */
    rc = qv_pthread_scopes_free(nthreads, th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(mpi_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
