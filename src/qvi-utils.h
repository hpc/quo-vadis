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
 * Constructs a new object of a given type. *t will be valid if successful,
 * undefined otherwise. Returns QV_SUCCESS if successful.
 */
// TODO(skg) Rename to qvi_new
template <class T>
int
qvi_new(
    T **t
) {
    try {
        *t = new T();
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

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
 * Simple wrapper that duplicates the provided instance.
 */
template <class T>
int
qvi_dup(
    const T &t,
    T **dup
) {
    try {
        *dup = new T(t);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

/**
 *
 */
int
qvi_url(
    std::string &url
);

#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
cstr_t
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
    const cstr_t path,
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
 *
 */
cstr_t
qvi_conn_ers(void);

/**
 *
 */
cstr_t
qvi_tmpdir(void);

/**
 *
 */
cstr_t
qvi_whoami(void);

/**
 *
 */
int
qvi_file_size(
    const cstr_t path,
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
