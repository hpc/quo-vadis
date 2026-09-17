/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
#include <stdio.h>
#include <pthread.h>
#include "quo-vadis-mpi.h"
#include "quo-vadis-thread.h"
#include "common-test-utils.h"

/*
 * QV Todo:
 *
 * (1) QV_SCOPE_SYSTEM returns the same resources as
 * QV_SCOPE_USER. It would be better if it returned
 * no resources when it is not set to "system" cores.
 * Perhaps, we should rename SYSTEM to UTILITY.
 * In addition, we could have a call that defines the
 * UTILITY scope:
 * qv_scope_set_utility(context, char *lst);
 * Or
 * qv_scope_set_utility(context, input_scope);
 *
 * (2) Define qv_create_scope_hint for qv_create_scope.
 * We may need:
 * // Get PUs from the tail of the list
 * QV_SCOPE_CREATE_LAST
 * // Get PUs from the bottom SMT hardware threads
 * QV_SCOPE_CREATE_BOTTOM_SMT
 */


//#define MY_INTRINSIC_SCOPE QV_SCOPE_SYSTEM
#define MY_INTRINSIC_SCOPE QV_SCOPE_PROCESS

#if 0
void *thread_work(void *arg)
{
    qv_context_t *ctx = (qv_context_t *) arg;

    char *binds;
    char const *ers = NULL;
    int rc = qv_bind_string(ctx, QV_BIND_STRING_LOGICAL, &binds);

    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    printf("Progress thread running on %s\n", binds);
    free(binds);

    return NULL;
}
#endif

int main(int argc, char *argv[])
{
    MPI_Comm comm = MPI_COMM_WORLD;

    ctu_mpi_check(MPI_Init(&argc, &argv), "MPI_Init");

    int wsize;
    ctu_mpi_check(MPI_Comm_size(comm, &wsize), "MPI_Comm_size");

    int wrank;
    ctu_mpi_check(MPI_Comm_rank(comm, &wrank), "MPI_Comm_rank");

    qv_scope_t *user_scope;
    ctu_check(
        qv_mpi_scope(comm, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &user_scope),
        "qv_mpi_scope"
    );

    /* Split user scope evenly across tasks */
    qv_scope_t *task_scope;
    ctu_check(qv_split(user_scope, wsize, wrank, &task_scope), "qv_split");

    /* Push into my task scope */
    ctu_check(qv_bind_push(task_scope), "qv_bind_push");

    /* Where did I end up? */
    char *binds;
    ctu_check(
        qv_bind_string(task_scope, QV_BIND_STRING_LOGICAL, &binds),
        "qv_bind_string"
    );
    printf("[%d] Split: running on %s\n", wrank, binds);
    free(binds);

    /*
     * Todo:
     * Before calling the library with the progress threads,
     * how could we set aside cores/pus for the progress threads
     * and not use those resources for the application's work?
     * Currently, we could get around this by implementing
     * hints in qv_create_scope as detailed in this file.
     * Another way of doing this is by allowing the creation of
     * intrinsic *named* scopes that can be set by the application
     * and used by the external library.
     * For example, I could create an "SMT top" scope and an
     * "SMT bottom" scope...
     *
     * Todo:
     * Bug in scope create: asking/releasing of cores not honoring
     * resources that have already been used without releasing.
     * See test-mpi-scope-create. See GitHub Issue #4.
     * We could also create a hint QV_SCOPE_CREATE_EXCLUSIVE
     * to not give out these resources until the scope is
     * released.
     */

    int ncores;
    ctu_check(
        qv_hw_count(task_scope, QV_HW_CORE, &ncores),
        "qv_hw_count"
    );

    qv_scope_t *wk_scope;
    ctu_check(
        qv_create_scope(
            task_scope, QV_SCOPE_FLAG_NONE, QV_HW_CORE, ncores-1, &wk_scope
        ),
        "qv_create_scope"
    );

    qv_scope_t *ut_scope;
    ctu_check(
        qv_create_scope(
            task_scope, QV_SCOPE_FLAG_NONE, QV_HW_CORE, 1, &ut_scope
        ),
        "qv_create_scope"
    );

    /* Test work scope */
    ctu_check(qv_bind_push(wk_scope), "qv_bind_push");
    ctu_check(
        qv_bind_string(wk_scope, QV_BIND_STRING_LOGICAL, &binds),
        "qv_bind_string"
    );
    printf("[%d] Work scope: running on %s\n", wrank, binds);
    free(binds);
    ctu_check(qv_bind_pop(wk_scope), "qv_bind_pop");

    /* Test utility scope */
    ctu_check(qv_bind_push(ut_scope), "qv_bind_push");
    ctu_check(
        qv_bind_string(ut_scope, QV_BIND_STRING_LOGICAL, &binds),
        "qv_bind_string"
    );
    printf("[%d] Utility scope: running on %s\n", wrank, binds);
    free(binds);
    ctu_check(qv_bind_pop(ut_scope), "qv_bind_pop");

    /* Clean up for now */
    ctu_check(qv_free(ut_scope), "qv_free");

    ctu_check(qv_free(wk_scope), "qv_free");


    /***************************************
     * Emulate a progress thread scenario
     * within a library external to the application
     ***************************************/

    /* Where do I get the qv_context from? since
       this is likely a library with its own interface
       and we don't want to modify the library's interface.
       Shall I create a new context? */
#if 0
    qv_context_t *ctx2;
    rc = qv_mpi_context_create(comm, &ctx2);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_create() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Get scope from which to derive the progress thread.
       Since this would be called externally to the application
       using utility threads, then we can only derive the
       scope from the intrinsic scopes.
       I'd like to try QV_SCORE_SYSTEM and if
       there's nothing available, then use QV_PROCESS_SCOPE */
    qv_scope_t *base_scope;
    rc = qv_scope_get(ctx2, MY_INTRINSIC_SCOPE, &base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Test we have PUs to use in the base scope */
    int npus;
    rc = qv_hw_count(ctx2, base_scope, QV_HW_PU, &npus);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Base scope: npus=%d\n", wrank, npus);

    /* Create the progress thread scope */
    qv_scope_t *pt_scope = user_scope;
    if (npus > 0) {
        rc = qv_create_scope(ctx2, base_scope, QV_HW_PU, 1, 0, &pt_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_create_scope() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

    pthread_t thid;
    void *args = ctx2;
    if (qv_pthread_create(&thid, NULL, thread_work, args,
                ctx2, pt_scope) != 0) {
        perror("pthread_create() error");
        exit(1);
    }

    void *ret;
    if (pthread_join(thid, &ret) != 0) {
        perror("pthread_create() error");
        exit(3);
    }
    printf("Thread finished with '%s'\n", (char *)ret);


    /* Clean up */
    rc = qv_free(ctx2, pt_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(ctx2, base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }


    /***************************************
     * Back to the application
     ***************************************/

    rc = qv_free(ctx, task_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(ctx, user_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#endif
    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
