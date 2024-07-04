/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2023 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-test-common.h
 *
 * Common test infrastructure.
 */

#ifndef QVI_TEST_COMMON_H
#define QVI_TEST_COMMON_H

#include "quo-vadis.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define QVI_TEST_STRINGIFY(x) #x
#define QVI_TEST_TOSTRING(x)  QVI_TEST_STRINGIFY(x)

#define qvi_test_panic(...)                                                    \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)

// We assume that the quo-vadis.h is included before us.
#ifdef QUO_VADIS

static inline void
qvi_test_scope_report(
    qv_scope_t *scope,
    const char *const scope_name
) {
    char const *ers = NULL;

    const int pid = getpid();

    int taskid;
    int rc = qv_scope_taskid(scope, &taskid);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ntasks;
    rc = qv_scope_ntasks(scope, &ntasks);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_ntasks() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    printf(
        "[%d] %s taskid is %d\n"
        "[%d] %s ntasks is %d\n",
        pid, scope_name, taskid,
        pid, scope_name, ntasks
    );

    rc = qv_scope_barrier(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

/**
 * A verbose version of qv_bind_push().
 */
static inline void
qvi_test_bind_push(
    qv_scope_t *scope
) {
    char const *ers = NULL;

    const int pid = getpid();

    int taskid;
    int rc = qv_scope_taskid(scope, &taskid);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (getenv("HWLOC_XMLFILE")) {
        if (taskid == 0) {
            printf("*** Using synthetic topology. "
                   "Skipping change_bind tests. ***\n");
        }
        return;
    }

    // Get current binding.
    char *bind0s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind0s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Current cpubind before qv_bind_push() is %s\n", pid, bind0s);

    // Change binding.
    rc = qv_scope_bind_push(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Get new, current binding.
    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] New cpubind after qv_bind_push() is %s\n", pid, bind1s);

    free(bind0s);
    free(bind1s);
}

/**
 * A verbose version of qv_bind_pop().
 */
static inline void
qvi_test_bind_pop(
    qv_scope_t *scope
) {
    char const *ers = NULL;

    const int pid = getpid();

    int taskid;
    int rc = qv_scope_taskid(scope, &taskid);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (getenv("HWLOC_XMLFILE")) {
        if (taskid == 0) {
            printf("*** Using synthetic topology. "
                   "Skipping change_bind tests. ***\n");
        }
        return;
    }

    // Get current binding.
    char *bind0s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind0s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Current cpubind before qv_bind_pop() is %s\n", pid, bind0s);

    // Change binding.
    rc = qv_scope_bind_pop(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Get new, current binding.
    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] New cpubind after qv_bind_pop() is %s\n", pid, bind1s);

    free(bind0s);
    free(bind1s);
}

/**
 * Collective call over the provided scope that tests pushing and popping of
 * binding policies.
 */
static inline void
qvi_test_change_bind(
    qv_scope_t *scope
) {
    char const *ers = NULL;

    const int pid = getpid();

    int taskid;
    int rc = qv_scope_taskid(scope, &taskid);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (getenv("HWLOC_XMLFILE")) {
        if (taskid == 0) {
            printf("*** Using synthetic topology. "
                   "Skipping change_bind tests. ***\n");
        }
        return;
    }

    // Get current binding.
    char *bind0s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind0s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Current cpubind is %s\n", pid, bind0s);

    // Change binding.
    rc = qv_scope_bind_push(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Get new, current binding.
    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] New cpubind is %s\n", pid, bind1s);

    rc = qv_scope_bind_pop(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    char *bind2s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind2s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Popped cpubind is %s\n", pid, bind2s);

    if (strcmp(bind0s, bind2s)) {
        ers = "bind push/pop mismatch";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    free(bind0s);
    free(bind1s);
    free(bind2s);

    rc = qv_scope_barrier(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        qvi_test_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

#endif // #ifdef QUO_VADIS

// We assume that the infrastructure-specific headers are included before us.
#ifdef QUO_VADIS_MPI

#endif // #ifdef QUO_VADIS_MPI

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
