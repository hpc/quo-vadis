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
 * @file qvi-bbuff.cc
 */

#include "qvi-bbuff.h"
#include "qvi-utils.h"

struct qvi_bbuff_s {
    /** Minimum growth in bytes for resizes, etc. */
    static constexpr size_t min_growth = 256;
    /** Current capacity of buffer. */
    size_t capacity = 0;
    /** Amount of data already stored. */
    size_t size = 0;
    /** Pointer to data backing store. */
    void *data = nullptr;
    /** Constructor. */
    qvi_bbuff_s(void)
    {
        capacity = min_growth;
        data = calloc(capacity, sizeof(byte_t));
        if (qvi_unlikely(!data)) throw qvi_runtime_error();
    }
    /** Copy constructor. */
    qvi_bbuff_s(
        const qvi_bbuff_s &src
    ) : qvi_bbuff_s()
    {
        const int rc = qvi_bbuff_append(this, src.data, src.size);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    }
    /** Destructor. */
    ~qvi_bbuff_s(void)
    {
        if (data) free(data);
    }
};

int
qvi_bbuff_new(
    qvi_bbuff_t **buff
) {
    return qvi_new(buff);
}

int
qvi_bbuff_dup(
    const qvi_bbuff_t *const src,
    qvi_bbuff_t **buff
) {
    return qvi_dup(*src, buff);
}

void
qvi_bbuff_free(
    qvi_bbuff_t **buff
) {
    qvi_delete(buff);
}

void *
qvi_bbuff_data(
    qvi_bbuff_t *buff
) {
    return buff->data;
}

size_t
qvi_bbuff_size(
    const qvi_bbuff_t *buff
) {
    return buff->size;
}

int
qvi_bbuff_append(
    qvi_bbuff_t *buff,
    const void *data,
    size_t size
) {
    const size_t req_capacity = size + buff->size;
    if (req_capacity > buff->capacity) {
        // New capacity.
        const size_t new_capacity = req_capacity + buff->min_growth;
        void *new_data = calloc(new_capacity, sizeof(byte_t));
        if (qvi_unlikely(!new_data)) return QV_ERR_OOR;
        // Memory allocation successful.
        memmove(new_data, buff->data, buff->size);
        free(buff->data);
        buff->capacity = new_capacity;
        buff->data = new_data;
    }
    byte_t *dest = (byte_t *)buff->data;
    dest += buff->size;
    memmove(dest, data, size);
    buff->size += size;
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
