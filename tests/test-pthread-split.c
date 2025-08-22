/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "quo-vadis-thread.h"
#include "common-test-utils.h"

// A convenience structure to hold thread arguments.
typedef struct {
    qv_scope_t *scope;
    int answer;
} thargs_t;

void *
thread_work(
    void *arg
) {
    thargs_t *thargs = (thargs_t *)arg;

    if (thargs->answer != 42) {
        char const *ers = "user arguments not forwarded!";
        ctu_panic("%s", ers);
    }
    printf("Hello from pid=%d,tid=%d\n", getpid(), ctu_gettid());
    ctu_emit_task_bind(thargs->scope);
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
        QV_SCOPE_PROCESS, QV_SCOPE_FLAG_NONE, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_process_scope_get() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ncores = 0;
    rc = qv_scope_hw_obj_count(base_scope, QV_HW_OBJ_CORE, &ncores);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_task_bind(base_scope);
    //
    // Test qv_pthread_scope_split()
    //
    const int npieces = 2;
    const int nthreads = ncores;

    printf(
        "[%d] Testing thread_scope_split (nthreads=%d, npieces=%d)\n",
        tid, nthreads, npieces
    );

    qv_scope_t **th_scopes = NULL;
    rc = qv_thread_scope_split(
        base_scope, npieces,
        QV_THREAD_SCOPE_SPLIT_PACKED,
        nthreads, &th_scopes
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
    rc = qv_thread_scopes_free(nthreads, th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    //
    //Test qv_pthread_scope_split_at
    //
    printf(
        "[%d] Testing thread_scope_split_at (nthreads=%d, npieces=%d)\n",
        tid, nthreads, npieces
    );

    rc = qv_thread_scope_split_at(
        base_scope, QV_HW_OBJ_CORE,
        QV_THREAD_SCOPE_SPLIT_PACKED,
        nthreads, &th_scopes
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
    rc = qv_thread_scopes_free(nthreads, th_scopes);
    if (rc != QV_SUCCESS) {
        ers = "qv_pthread_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

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
