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

void
qvi_bbuff::init(void)
{
    // Make sure we get rid of any data that may be present.
    if (m_data) {
        free(m_data);
        m_data = nullptr;
    }
    m_size = 0;
    m_capacity = s_min_growth;
    m_data = calloc(m_capacity, sizeof(byte_t));
    if (qvi_unlikely(!m_data)) throw qvi_runtime_error();
}

qvi_bbuff::qvi_bbuff(void)
{
    init();
}

qvi_bbuff::qvi_bbuff(
    const qvi_bbuff &src
) : qvi_bbuff()
{
    const int rc = append(src.m_data, src.m_size);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
}

qvi_bbuff::~qvi_bbuff(void)
{
    if (m_data) {
        free(m_data);
        m_data = nullptr;
    }
}

void
qvi_bbuff::operator=(
    const qvi_bbuff &src
) {
    init();
    const int rc = append(src.m_data, src.m_size);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
}

size_t
qvi_bbuff::size(void) const
{
    return m_size;
}

void *
qvi_bbuff::data(void)
{
    return m_data;
}

int
qvi_bbuff::append(
    const void *const data,
    size_t size
) {
    const size_t req_capacity = size + m_size;
    if (req_capacity > m_capacity) {
        // New capacity.
        const size_t new_capacity = req_capacity + s_min_growth;
        void *new_data = calloc(new_capacity, sizeof(byte_t));
        if (qvi_unlikely(!new_data)) return QV_ERR_OOR;
        // Memory allocation successful.
        memmove(new_data, m_data, m_size);
        free(m_data);
        m_capacity = new_capacity;
        m_data = new_data;
    }
    byte_t *dest = (byte_t *)m_data;
    dest += m_size;
    memmove(dest, data, size);
    m_size += size;
    return QV_SUCCESS;
}

int
qvi_bbuff_new(
    qvi_bbuff **buff
) {
    return qvi_new(buff);
}

int
qvi_bbuff_dup(
    const qvi_bbuff &src,
    qvi_bbuff **buff
) {
    return qvi_dup(src, buff);
}

void
qvi_bbuff_delete(
    qvi_bbuff **buff
) {
    qvi_delete(buff);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
