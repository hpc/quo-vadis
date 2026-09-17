/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022-2024 Inria
 *                         All rights reserved.
 *
 * Copyright (c) 2022-2024 Bordeaux INP
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis-thread.h
 */

#ifndef QUO_VADIS_THREAD_H
#define QUO_VADIS_THREAD_H

#include "quo-vadis.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Automatic grouping options for qv_thread_split() and
 * qv_thread_split_at(). The following values can be used instead of
 * group_id to influence how automatic task grouping is accomplished.
 *
 * See quo-vadis.h for further details.
 */
#define QV_THREAD_SPLIT_AUTO      ((int *)0x00000001)
#define QV_THREAD_SPLIT_PACKED    ((int *)0x00000002)
#define QV_THREAD_SPLIT_SPREAD    ((int *)0x00000003)
#define QV_THREAD_SPLIT_CLOSE     ((int *)0x00000004)

/**
 * Splits the provided scope into a number of pieces, returning a subscope for
 * each of the k threads that will share the scope's resources.
 *
 * This is the thread analog of qv_split(). Whereas qv_split() is called
 * collectively and returns to each caller the single subscope it belongs to,
 * qv_thread_split() is called once and produces all k subscopes at once, one
 * per prospective thread.
 *
 * @param[in] scope The scope to split.
 *
 * @param[in] npieces The number of pieces to split the scope into.
 *
 * @param[in] kcolors The per-thread colors: an array of length k whose i-th
 * entry selects which piece the i-th thread joins (analogous to the group_id
 * of qv_split()). Alternatively, pass one of the automatic grouping constants
 * (QV_THREAD_SPLIT_AUTO, QV_THREAD_SPLIT_PACKED, QV_THREAD_SPLIT_SPREAD,
 * QV_THREAD_SPLIT_CLOSE) in place of the array to apply that automatic
 * grouping to all k threads. Passing NULL is equivalent to
 * QV_THREAD_SPLIT_AUTO.
 *
 * @param[in] k The number of threads to split among, i.e. the number of
 * subscopes to produce and the length of the kcolors array.
 *
 * @param[out] subscopes Address of a pointer that will receive a newly
 * allocated array of k subscopes, where the i-th element corresponds to the
 * i-th thread. The caller is responsible for freeing this array with
 * qv_thread_free().
 *
 * @retval QV_SUCCESS if the operation completed successfully.
 */
int
qv_thread_split(
    qv_scope_t *scope,
    int npieces,
    int *kcolors,
    int k,
    qv_scope_t ***subscopes
);

/**
 * Splits the provided scope at the given hardware object type, returning a
 * subscope for each of the k threads that will share the scope's resources.
 *
 * This is the thread analog of qv_split_at(). Whereas qv_split_at() is called
 * collectively and returns to each caller the single subscope it belongs to,
 * qv_thread_split_at() is called once and produces all k subscopes at once,
 * one per prospective thread. It is to qv_thread_split() what qv_split_at() is
 * to qv_split(): the number of pieces is determined by the number of hardware
 * objects of the given type rather than by an explicit npieces count.
 *
 * @param[in] scope The scope to split.
 *
 * @param[in] type The hardware object type to split at.
 *
 * @param[in] kcolors The per-thread colors: an array of length k whose i-th
 * entry selects which piece the i-th thread joins (analogous to the group_id
 * of qv_split_at()). Alternatively, pass one of the automatic grouping
 * constants (QV_THREAD_SPLIT_AUTO, QV_THREAD_SPLIT_PACKED,
 * QV_THREAD_SPLIT_SPREAD, QV_THREAD_SPLIT_CLOSE) in place of the array to apply
 * that automatic grouping to all k threads. Passing NULL is equivalent to
 * QV_THREAD_SPLIT_AUTO.
 *
 * @param[in] k The number of threads to split among, i.e. the number of
 * subscopes to produce and the length of the kcolors array.
 *
 * @param[out] subscopes Address of a pointer that will receive a newly
 * allocated array of k subscopes, where the i-th element corresponds to the
 * i-th thread. The caller is responsible for freeing this array with
 * qv_thread_free().
 *
 * @retval QV_SUCCESS if the operation completed successfully.
 */
int
qv_thread_split_at(
    qv_scope_t *scope,
    qv_hw_type_t type,
    int *kcolors,
    int k,
    qv_scope_t ***subscopes
);

/**
 * Frees an array of subscopes produced by qv_thread_split() or
 * qv_thread_split_at().
 *
 * This frees both the k individual subscopes and the array itself, and is the
 * counterpart to those calls (analogous to how qv_free() releases a single
 * scope). It should be used instead of qv_free() for arrays obtained from the
 * thread split functions.
 *
 * @param[in] kscopes The array of k subscopes to free, as returned via the
 * subscopes parameter of qv_thread_split() or qv_thread_split_at(). Passing
 * NULL is a no-op.
 *
 * @param[in] k The number of subscopes in the array. Must be non-negative and
 * match the k passed to the originating split call.
 *
 * @retval QV_SUCCESS if the operation completed successfully.
 */
int
qv_thread_free(
    qv_scope_t **kscopes,
    int k
);

/* ////////////////////////////////////////////////////////////////////////// */
/* Pthread-specific calls.                                                    */
/* ////////////////////////////////////////////////////////////////////////// */

/**
 * Creates a new thread bound to the resources of the provided scope.
 *
 * This behaves like pthread_create(3): it starts a new thread running
 * thread_routine(arg) using the given attributes, and returns 0 on success or
 * a POSIX errno-style value on failure (e.g. ENOMEM). The distinguishing
 * behavior is that the created thread is automatically bound to scope's
 * hardware resources for the duration of thread_routine.
 *
 * @param[out] thread On success, receives the identifier of the created
 * thread. See pthread_create(3).
 *
 * @param[in] attr Thread attributes, or NULL for the defaults. See
 * pthread_create(3).
 *
 * @param[in] thread_routine The function the new thread runs; it is passed arg
 * and its return value is made available to a subsequent join.
 *
 * @param[in] arg The sole argument passed to thread_routine.
 *
 * @param[in] scope The scope whose resources the new thread is bound to. This
 * must be one of the subscopes produced by qv_thread_split() or
 * qv_thread_split_at(); its underlying group is what makes the per-thread
 * binding possible.
 *
 * @retval 0 on success, or a POSIX error number on failure.
 */
int
qv_pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*thread_routine)(void *arg),
    void *arg,
    qv_scope_t *scope
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
