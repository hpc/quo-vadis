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

#include "qvi-common.h"
#include "qvi-bbuff.h"
#include "qvi-utils.h"

struct qvi_bbuff_s {
    int qvim_rc = QV_ERR_INTERNAL;
    /** Current capacity of buffer. */
    size_t capacity = 0;
    /** Amount of data already stored. */
    size_t size = 0;
    /** Pointer to data backing store. */
    void *data = nullptr;
    /** Buffer constants. */
    enum {
        /** Minimum growth in bytes for resizes, etc. */
        min_growth = 256
    } constants;
    /** Constructor */
    qvi_bbuff_s(void)
    {
        capacity = min_growth;
        data = calloc(capacity, sizeof(byte_t));
        if (!data) {
            qvim_rc = QV_ERR_OOR;
            return;
        }
        qvim_rc = QV_SUCCESS;
    }
    /** Destructor */
    ~qvi_bbuff_s(void)
    {
        if (data) free(data);
    }
};

int
qvi_bbuff_new(
    qvi_bbuff_t **buff
) {
    return qvi_new_rc(buff);
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
        if (!new_data) return QV_ERR_OOR;
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
