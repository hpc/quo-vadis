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
 * @file qvi-bbuff.h
 */

#ifndef QVI_BBUFF_H
#define QVI_BBUFF_H

#include "qvi-common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qvi_bbuff_s;
typedef struct qvi_bbuff_s qvi_bbuff_t;

/**
 *
 */
int
qvi_bbuff_new(
    qvi_bbuff_t **buff
);

/**
 *
 */
void
qvi_bbuff_free(
    qvi_bbuff_t **buff
);

/**
 *
 */
void *
qvi_bbuff_data(
    qvi_bbuff_t *buff
);

/**
 *
 */
size_t
qvi_bbuff_size(
    const qvi_bbuff_t *buff
);

/**
 *
 */
int
qvi_bbuff_append(
    qvi_bbuff_t *buff,
    void *data,
    size_t size
);

/**
 *
 */
int
qvi_bbuff_vasprintf(
    qvi_bbuff_t *buff,
    const char *picture,
    va_list args
);

/**
 *
 */
int
qvi_bbuff_asprintf(
    qvi_bbuff_t *buff,
    const char *picture,
    ...
);

/**
 *
 */
int
qvi_data_vsscanf(
    void *data,
    const char *picture,
    va_list args
);

/**
 *
 */
int
qvi_data_sscanf(
    void *data,
    const char *picture,
    ...
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
