/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
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

struct qvi_bbuff_s {
    /** Current capacity of buffer. */
    size_t capacity = 0;
    /** Amount of data already stored. */
    size_t size = 0;
    /** Pointer to data backing store. */
    void *data = nullptr;
    /** Buffer constants. */
    enum {
        /** Minimum growth for resizes, etc. */
        min_growth = 256
    } constants;
};

int
qvi_bbuff_new(
    qvi_bbuff_t **buff
) {
    int rc = QV_SUCCESS;

    qvi_bbuff_t *ibuff = qvi_new qvi_bbuff_t;
    if (!ibuff) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ibuff->capacity = ibuff->min_growth;
    ibuff->data = calloc(ibuff->capacity, sizeof(byte));
    if (!ibuff->data) {
        rc = QV_ERR_OOR;
        goto out;
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_bbuff_free(&ibuff);
    }
    *buff = ibuff;
    return rc;
}

void
qvi_bbuff_free(
    qvi_bbuff_t **buff
) {
    if (!buff) return;
    qvi_bbuff_t *ibuff = *buff;
    if (!ibuff) goto out;
    if (ibuff->data) free(ibuff->data);
    delete ibuff;
out:
    *buff = nullptr;
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
        void *new_data = calloc(new_capacity, sizeof(byte));
        if (!new_data) return QV_ERR_OOR;
        // Memory allocation successful.
        memmove(new_data, buff->data, buff->size);
        free(buff->data);
        buff->capacity = new_capacity;
        buff->data = new_data;
    }
    byte *dest = (byte *)buff->data;
    dest += buff->size;
    memmove(dest, data, size);
    buff->size += size;
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
