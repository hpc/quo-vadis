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
 * @file qvi-bbuff.cc
 */

/*
 * Picture Reference:
 * b = void *, size_t (two arguments)
 * i = int
 * s = char *
 * z = empty (zero arguments)
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
    enum constants {
        /** Minimum growth for resizes, etc. */
        min_growth = 256
    };
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
    qvi_bbuff_t *ibuff = *buff;
    if (!ibuff) return;
    if (ibuff->data) free(ibuff->data);
    delete ibuff;
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
    void *data,
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

int
qvi_bbuff_vsprintf(
    qvi_bbuff_t *buff,
    const char *picture,
    va_list args
) {
    int rc = QV_SUCCESS;

    const int npic = strlen(picture);
    for (int i = 0; i < npic; ++i) {
        if (picture[i] == 'b') {
            void *data = va_arg(args, void *);
            size_t dsize = va_arg(args, size_t);
            // We store size then data so unpack has an easier time, but keep
            // the user interface order as data then size.
            rc = qvi_bbuff_append(buff, &dsize, sizeof(dsize));
            if (rc != QV_SUCCESS) break;
            rc = qvi_bbuff_append(buff, data, dsize);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'i') {
            int data = va_arg(args, int);
            rc = qvi_bbuff_append(buff, &data, sizeof(data));
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 's') {
            char *data = va_arg(args, char *);
            rc = qvi_bbuff_append(buff, data, strlen(data) + 1);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'z') {
            continue;
        }
        else {
            rc = QV_ERR_INVLD_ARG;
            break;
        }
    }
    return rc;
}

int
qvi_bbuff_sprintf(
    qvi_bbuff_t *buff,
    const char *picture,
    ...
) {
    va_list vl;
    va_start(vl, picture);
    int rc = qvi_bbuff_vsprintf(buff, picture, vl);
    va_end(vl);
    return rc;
}

int
qvi_data_vsscanf(
    void *data,
    const char *picture,
    va_list args
) {
    int rc = QV_SUCCESS;
    byte *pos = (byte *)data;

    const int npic = strlen(picture);
    for (int i = 0; i < npic; ++i) {
        if (picture[i] == 'b') {
            void **data = va_arg(args, void **);
            size_t *dsize = va_arg(args, size_t *);
            memmove(dsize, pos, sizeof(*dsize));
            pos += sizeof(*dsize);
            *data = calloc(*dsize, sizeof(byte));
            if (!*data) {
                rc = QV_ERR_OOR;
                break;
            }
            memmove(*data, pos, *dsize);
            pos += *dsize;
            continue;
        }
        if (picture[i] == 'i') {
            memmove(va_arg(args, int *), pos, sizeof(int));
            pos += sizeof(int);
            continue;
        }
        if (picture[i] == 's') {
            const int nw = asprintf(va_arg(args, char **), "%s", pos);
            if (nw == -1) {
                rc = QV_ERR_OOR;
                break;
            }
            pos += nw + 1;
            continue;
        }
        if (picture[i] == 'z') {
            continue;
        }
        else {
            rc = QV_ERR_INVLD_ARG;
            break;
        }
    }
    return rc;
}

int
qvi_data_sscanf(
    void *data,
    const char *picture,
    ...
) {
    va_list vl;
    va_start(vl, picture);
    int rc = qvi_data_vsscanf(data, picture, vl);
    va_end(vl);
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
