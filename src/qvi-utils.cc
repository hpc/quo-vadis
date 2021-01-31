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
 * @file qvi-utils.cc
 */

#include "private/qvi-common.h"
#include "private/qvi-utils.h"
#include "private/qvi-macros.h"

#include <chrono>

char *
qvi_strerr(int ec)
{
    static thread_local char sb[4096];
    return strerror_r(ec, sb, sizeof(sb));
}

pid_t
qvi_gettid(void) {
    return (pid_t)syscall(SYS_gettid);
}

double
qvi_time(void)
{
    using namespace std::chrono;

    const auto n = steady_clock::now();
    auto tse_ms = time_point_cast<microseconds>(n).time_since_epoch().count();
    return double(tse_ms) / 1e6;
}

struct qvi_byte_buffer_s {
    // Current capacity of buffer.
    size_t capacity = 0;
    // Amount of data already stored.
    size_t size = 0;
    // Pointer to data backing store.
    void *data = nullptr;
    // Buffer constants.
    enum constants {
        // Minimum growth for resizes, etc.
        min_growth = 256
    };
};

int
qvi_byte_buffer_construct(
    qvi_byte_buffer_t **buff
) {
    qvi_byte_buffer_t *ibuff = qvi_new qvi_byte_buffer_t;
    if (!ibuff) {
        *buff = nullptr;
        return QV_ERR_OOR;
    }

    ibuff->capacity = ibuff->min_growth;
    ibuff->data = calloc(ibuff->capacity, sizeof(uint8_t));
    if (!ibuff->data) {
        qvi_byte_buffer_destruct(ibuff);
        return QV_ERR_OOR;
    }

    *buff = ibuff;
    return QV_SUCCESS;
}

void
qvi_byte_buffer_destruct(
    qvi_byte_buffer_t *buff
) {
    if (!buff) return;
    if (buff->data) free(buff->data);
    delete buff;
}

void *
qvi_byte_buffer_data(
    qvi_byte_buffer_t *buff
) {
    return buff->data;
}

size_t
qvi_byte_buffer_size(
    const qvi_byte_buffer_t *buff
) {
    return buff->size;
}

int
qvi_byte_buffer_append(
    qvi_byte_buffer_t *buff,
    void *data,
    size_t size
) {
    const size_t req_capacity = size + buff->size;
    if (req_capacity > buff->capacity) {
        // New capacity.
        const size_t new_capacity = req_capacity + buff->min_growth;
        void *new_data = calloc(new_capacity, sizeof(uint8_t));
        if (!new_data) return QV_ERR_OOR;
        // Memory allocation successful.
        memcpy(new_data, buff->data, buff->size);
        free(buff->data);
        buff->capacity = new_capacity;
        buff->data = new_data;
    }
    uint8_t *dest = (uint8_t *)buff->data;
    dest += buff->size + 1;
    memcpy(dest, data, size);
    buff->size += size;
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
