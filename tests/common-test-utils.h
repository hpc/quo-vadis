/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file common-test-utils.h
 *
 * Common test infrastructure.
 */

#ifndef COMMON_TEST_UTILS_H
#define COMMON_TEST_UTILS_H

#include "quo-vadis.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define CTU_STRINGIFY(x) #x
#define CTU_TOSTRING(x)  CTU_STRINGIFY(x)

#define ctu_panic(...)                                                         \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)

// We assume that the quo-vadis.h is included before us.
#ifdef QUO_VADIS

static inline pid_t
ctu_gettid(void)
{
    return (pid_t)syscall(SYS_gettid);
}

static inline void
ctu_emit_task_bind(
    qv_scope_t *scope
) {
    const int pid = ctu_gettid();
    char const *ers = NULL;
    // Get new, current binding.
    char *binds = NULL;
    int rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] cpubind (physical) = %s\n", pid, binds);
    free(binds);
}

static inline void
ctu_scope_report(
    qv_scope_t *scope,
    const char *const scope_name
) {
    char const *ers = NULL;

    const int pid = ctu_gettid();

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int sgsize;
    rc = qv_scope_group_size(scope, &sgsize);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    printf(
        "[%d] %s scope group rank is %d\n"
        "[%d] %s scope group size is %d\n",
        pid, scope_name, sgrank,
        pid, scope_name, sgsize
    );

    rc = qv_scope_barrier(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

/**
 * A verbose version of qv_bind_push().
 */
static inline void
ctu_bind_push(
    qv_scope_t *scope
) {
    char const *ers = NULL;
    const int pid = ctu_gettid();

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get current binding.
    char *bind0s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind0s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Current cpubind before qv_bind_push() is %s\n", pid, bind0s);

    // Change binding.
    rc = qv_scope_bind_push(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Get new, current binding.
    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] New cpubind after qv_bind_push() is %s\n", pid, bind1s);

    free(bind0s);
    free(bind1s);
}

/**
 * A verbose version of qv_bind_pop().
 */
static inline void
ctu_bind_pop(
    qv_scope_t *scope
) {
    char const *ers = NULL;

    const int pid = ctu_gettid();

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get current binding.
    char *bind0s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind0s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Current cpubind before qv_bind_pop() is %s\n", pid, bind0s);

    // Change binding.
    rc = qv_scope_bind_pop(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get new, current binding.
    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
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
ctu_change_bind(
    qv_scope_t *scope
) {
    char const *ers = NULL;

    const int pid = ctu_gettid();

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get current binding.
    char *bind0s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind0s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Current cpubind is %s\n", pid, bind0s);

    // Change binding.
    rc = qv_scope_bind_push(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Get new, current binding.
    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_AS_LIST, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] New cpubind is %s\n", pid, bind1s);

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
    printf("[%d] Popped cpubind is %s\n", pid, bind2s);

    if (strcmp(bind0s, bind2s)) {
        ers = "bind push/pop mismatch";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    free(bind0s);
    free(bind1s);
    free(bind2s);

    rc = qv_scope_barrier(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
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
