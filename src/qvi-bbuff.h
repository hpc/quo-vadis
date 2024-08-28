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
 * Byte buffer infrastructure.
 */

#ifndef QVI_BBUFF_H
#define QVI_BBUFF_H

#include "qvi-common.h"

struct qvi_bbuff {
private:
    /** Minimum growth in bytes for resizes, etc. */
    static constexpr size_t s_min_growth = 256;
    /** Current capacity of buffer. */
    size_t m_capacity = 0;
    /** Amount of data already stored. */
    size_t m_size = 0;
    /** Pointer to data backing store. */
    void *m_data = nullptr;
public:
    /** Constructor. */
    qvi_bbuff(void);
    /** Copy constructor. */
    qvi_bbuff(
        const qvi_bbuff &src
    );
    /** Destructor. */
    ~qvi_bbuff(void);
    /** Assignment operator. */
    void
    operator=(const qvi_bbuff &src);
    /** Returns the size of the data stored in the byte buffer. */
    size_t
    size(void) const;
    /** Appends data to the buffer. */
    int
    append(
        const void *const data,
        size_t size
    );
    /**
     * Returns a raw pointer to the flat data buffer
     * maintained internally by the byte buffer.
     */
    void *
    data(void);
};

int
qvi_bbuff_new(
    qvi_bbuff **buff
);

int
qvi_bbuff_dup(
    const qvi_bbuff &src,
    qvi_bbuff **buff
);

void
qvi_bbuff_delete(
    qvi_bbuff **buff
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
