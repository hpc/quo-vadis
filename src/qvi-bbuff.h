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
 * @file qvi-bbuff.h
 *
 * Base byte buffer infrastructure.
 */

#ifndef QVI_BBUFF_H
#define QVI_BBUFF_H

#include "qvi-common.h"

/**
 *
 */
int
qvi_bbuff_new(
    qvi_bbuff_t **buff
);

int
qvi_bbuff_dup(
    const qvi_bbuff_t *const src,
    qvi_bbuff_t **buff
);

/**
 *
 */
void
qvi_bbuff_delete(
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
    const void *const data,
    size_t size
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
