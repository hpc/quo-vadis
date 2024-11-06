/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "quo-vadis-mpi.h"
#include "quo-vadis-pthread.h"
#include "qvi-test-common.h"

typedef struct {
    qv_scope_t *scope;
    int answer;
} thargs_t;

void *
thread_work(
    void *arg
) {
    char const *ers = NULL;
    const pid_t tid = qvi_test_gettid();
    thargs_t *thargs = (thargs_t *)arg;

    qv_scope_t *scope = thargs->scope;
    if (thargs->answer != 42) {
        ers = "user arguments not forwarded!";
        qvi_test_panic("%s", ers);
    }

    int rank = 0;
    int rc = qv_scope_group_rank(scope, &rank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    
    qvi_test_scope_report(scope, "thread_scope_in_thread_routine");
    qvi_test_emit_task_bind(scope);
    
    fprintf(stdout,"[%d] ============ Thread %d splitting in two pieces\n", tid, rank);
    qv_scope_t *pthread_subscope = NULL;
    rc = qv_scope_split(scope, 2, rank, &pthread_subscope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_test_scope_report(pthread_subscope, "thread_subscope");
    qvi_test_emit_task_bind(pthread_subscope);

    
    rc = qv_scope_free(pthread_subscope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    return NULL;
}

int
main(void)
{
    char const *ers = NULL;
    const pid_t tid = qvi_test_gettid();
    int wrank, wsize;

    int rc = MPI_Init(NULL, NULL);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        qvi_test_panic("%s (rc=%d)", ers, rc);
    }

    MPI_Comm comm = MPI_COMM_WORLD;
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

    int ncores = 0;
    rc = qv_scope_nobjs(mpi_scope, QV_HW_OBJ_CORE, &ncores);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qvi_test_scope_report(mpi_scope, "mpi_scope");
    qvi_test_emit_task_bind(mpi_scope);

    //As Edgar pointed out, this will work only in the
    //single process case.
    //The mpi_scope need to be plsit in order to get
    //a new mpi_single_process_scope 
    
    //
    // Test qv_pthread_scope_split
    //
    int npieces = 2;
    int nthreads = ncores;
    int stride = 1;
    int colors[nthreads];
    
    printf("[%d] Testing thread_scope_split (nthreads=%i, npieces=%i)\n", tid, nthreads, npieces);
    
    for (int i = 0 ; i < nthreads ; i++) {
        colors[i] = i % npieces;
    }

    fprintf(stdout,"Array values :");
    for (int i = 0 ; i < nthreads ; i++) {
        fprintf(stdout,"val[%i]: %i |",i,colors[i]);
    }
    fprintf(stdout,"\n");
    

    rc = qv_pthread_colors_fill(colors, nthreads, QV_POLICY_PACKED, stride, npieces);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_colors_fill() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    fprintf(stdout,"Array values :");
    for (int i = 0 ; i < nthreads ; i++) {
        fprintf(stdout,"val[%i]: %i |",i,colors[i]);
    }
    fprintf(stdout,"\n");
    
    
    qv_scope_t **th_scopes = NULL;
    rc = qv_pthread_scope_split(
        mpi_scope, npieces, colors, nthreads, &th_scopes
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_split() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    thargs_t thargs[nthreads];
    for (int i = 0 ; i < nthreads; i ++) {
        thargs[i].scope = th_scopes[i];
        thargs[i].answer = 42;
    }

    pthread_t thid[nthreads];
    pthread_attr_t *attr = NULL;

    for (int i = 0 ; i < nthreads; ++i) {
        const int ptrc = qv_pthread_create(
            &thid[i], attr, thread_work, &thargs[i], th_scopes[i]
        );
        if (ptrc != 0) {
            ers = "qv_pthread_create() failed";
            qvi_test_panic("%s (rc=%s)", ers, strerror(rc));
        }
    }

    void *ret = NULL;
    for (int i  = 0 ; i < nthreads; i ++) {
        if (pthread_join(thid[i], &ret) != 0) {
            perror("pthread_create() error");
            exit(3);
        }
        //fprintf(stdout,"Thread finished with '%s'\n", (char *)ret);
    }
    // Clean up.
    rc = qv_pthread_scopes_free(nthreads, th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

#if 0
    //Test qv_pthread_scope_split_at
    nthreads = 2 * ncores;

    printf("[%d] Testing thread_scope_split_at (nthreads=%i)\n", tid, nthreads);

    int colors2[nthreads];    
    for (int i = 0 ; i < nthreads ; i++) {
        colors2[i] = i % ncores;
    }

    fprintf(stdout,"Array values :");
    for (int i = 0 ; i < nthreads ; i++) {
        fprintf(stdout,"val[%i]: %i |",i,colors2[i]);
    }
    fprintf(stdout,"\n");

    
    rc = qv_pthread_colors_fill(colors2, nthreads, QV_POLICY_PACKED, stride, ncores);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_colors_fill() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    fprintf(stdout,"Array values :");
    for (int i = 0 ; i < nthreads ; i++) {
        fprintf(stdout,"val[%i]: %i |",i,colors2[i]);
    }
    fprintf(stdout,"\n");

    rc = qv_pthread_scope_split_at(
        mpi_scope, QV_HW_OBJ_CORE, colors2, nthreads, &th_scopes
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_split_at() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    thargs_t thargs2[nthreads];
    for (int i = 0 ; i < nthreads; i++) {
        thargs2[i].scope = th_scopes[i];
        thargs2[i].answer = 42;
    }

    pthread_t thid2[nthreads];
    for (int i  = 0 ; i < nthreads; ++i) {
        const int ptrc = qv_pthread_create(
            &thid2[i], attr, thread_work, &thargs2[i], th_scopes[i]
        );
        if (ptrc != 0) {
            ers = "qv_pthread_create() failed";
            qvi_test_panic("%s (rc=%s)", ers, strerror(rc));
        }
    }

    for (int i  = 0 ; i < nthreads; ++i) {
        if (pthread_join(thid2[i], &ret) != 0) {
            perror("pthread_create() error");
            exit(3);
        }
        //fprintf(stdout,"Thread finished with '%s'\n", (char *)ret);
    }

    // Clean up.
    rc = qv_pthread_scopes_free(nthreads, th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_free() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#endif
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
