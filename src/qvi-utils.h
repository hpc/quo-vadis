/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
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
extern "C" {
#endif

// Forward declarations.
struct qvi_byte_buffer_s;
typedef struct qvi_byte_buffer_s qvi_byte_buffer_t;

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
int
qvi_byte_buffer_construct(
    qvi_byte_buffer_t **buff
);

/**
 *
 */
void
qvi_byte_buffer_destruct(
    qvi_byte_buffer_t **buff
);

/**
 *
 */
void *
qvi_byte_buffer_data(
    qvi_byte_buffer_t *buff
);

/**
 *
 */
size_t
qvi_byte_buffer_size(
    const qvi_byte_buffer_t *buff
);

/**
 *
 */
int
qvi_byte_buffer_append(
    qvi_byte_buffer_t *buff,
    void *data,
    size_t size
);

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
    const char *str,
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
whoami(void);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
