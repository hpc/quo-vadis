/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "quo-vadis-process.h"
#include "quo-vadis-pthread.h"
#include "common-test-utils.h"

typedef struct {
    qv_scope_t *scope;
    int answer;
} thargs_t;

void *
thread_work(
    void *arg
) {
    char const *ers = NULL;
    const pid_t tid = ctu_gettid();
    thargs_t *thargs = (thargs_t *)arg;

    qv_scope_t *scope = thargs->scope;
    if (thargs->answer != 42) {
        ers = "user arguments not forwarded!";
        ctu_panic("%s", ers);
    }

    int rank = 0;
    int rc = qv_scope_group_rank(scope, &rank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#if 0
    ctu_scope_report(scope, "thread_scope_in_thread_routine");
    ctu_emit_task_bind(scope);
#endif

    if (rank == 0) {
        printf("[%d] ============ Splitting thread scopes in two\n", tid);
    }
    qv_scope_t *pthread_subscope = NULL;
    rc = qv_scope_split(scope, 2, rank, &pthread_subscope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_scope_report(pthread_subscope, "thread_subscope");
    ctu_emit_task_bind(pthread_subscope);

#if 0
    rc = qv_scope_free(pthread_subscope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#endif

    return NULL;
}

int
main(void)
{
    char const *ers = NULL;
    const pid_t tid = ctu_gettid();

    fprintf(stdout,"# Starting Pthreads test.\n");

    qv_scope_t *base_scope;
    int rc = qv_process_scope_get(
        QV_SCOPE_PROCESS, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_process_scope_get() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ncores = 0;
    rc = qv_scope_nobjs(base_scope, QV_HW_OBJ_CORE, &ncores);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_task_bind(base_scope);
    //
    // Test qv_pthread_scope_split
    //
    const int npieces = 2;
    const int nthreads = ncores;
    int stride = 1;
    int colors[nthreads];

    printf(
        "[%d] Testing thread_scope_split (nthreads=%d, npieces=%d)\n",
        tid, nthreads, npieces
    );

    for (int i = 0 ; i < nthreads ; i++) {
        colors[i] = i % npieces;
    }
#if 0
    printf("Manual values: ");
    for (int i = 0 ; i < nthreads ; i++) {
        printf("val[%i]:%i | ",i,colors[i]);
    }
    printf("\n");
#endif
    rc = qv_pthread_colors_fill(colors, nthreads, QV_POLICY_PACKED, stride, npieces);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_colors_fill() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#if 0
    fprintf(stdout,"Filled values: ");
    for (int i = 0 ; i < nthreads ; i++) {
        fprintf(stdout,"val[%i]:%i | ",i,colors[i]);
    }
    fprintf(stdout,"\n");
#endif
    qv_scope_t **th_scopes = NULL;
    rc = qv_pthread_scope_split(
        base_scope, npieces, colors, nthreads, &th_scopes
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
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
            ctu_panic("%s (rc=%s)", ers, strerror(rc));
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
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
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
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
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
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
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
            ctu_panic("%s (rc=%s)", ers, strerror(rc));
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
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#endif
    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
