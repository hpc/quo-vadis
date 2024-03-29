/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-utils.h
 */

#ifndef QVI_UTILS_H
#define QVI_UTILS_H

#include "qvi-common.h"

#ifdef __cplusplus

/**
 * Simple wrapper around delete that also nullifies the input pointer.
 */
template <class T>
void
qvi_delete(
    T **t
) {
    if (!t) return;
    T *it = *t;
    if (!it) goto out;
    delete it;
out:
    *t = nullptr;
}

/**
 * Returns the code captured by a constructor. Since we are trying to avoid the
 * use of exceptions, we instead use this method for checking the state of an
 * object after its construction. This method isn't perfect, and requires that a
 * class member named qvim_rc (read as (qvi) (m)ember (r)eturn (c)ode) is
 * present in the provided class. Compilation will fail otherwise.
 */
template <class T>
int
qvi_construct_rc(
    const T &t
) {
    return t.qvim_rc;
}

template <class T>
int
qvi_construct_rc(
    const std::shared_ptr<T> &t
) {
    return qvi_construct_rc(*t.get());
}

/**
 * Constructs a new object of a given type. *t will be valid if successful,
 * nullptr otherwise. Returns QV_SUCCESS if successful.
 */
template <class T>
int
qvi_new_rc(
    T **t
) {
    T *it = qvi_new T();
    if (!it) return QV_ERR_OOR;

    const int rc = qvi_construct_rc(*it);
    if (rc != QV_SUCCESS) {
        qvi_delete(&it);
    }
    *t = it;
    return rc;
}

#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
char *
qvi_strerr(int ec);

/**
 *
 */
pid_t
qvi_gettid(void);

/**
 *
 */
double
qvi_time(void);

/**
 *
 */
bool
qvi_path_usable(
    const char *path,
    int *errc
);

/**
 *
 */
int
qvi_atoi(
    cstr_t str,
    int *maybe_val
);

/**
 * @note: Caller is responsible for freeing URL.
 */
int
qvi_url(
    char **url
);

/**
 *
 */
const char *
qvi_conn_ers(void);

/**
 *
 */
const char *
qvi_tmpdir(void);

/**
 *
 */
const char *
qvi_whoami(void);

/**
 *
 */
int
qvi_file_size(
    const char *path,
    size_t *size
);

/**
 * Cantor pairing function.
 */
int64_t
qvi_cantor_pairing(
    int a,
    int b
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
